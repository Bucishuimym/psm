#include <cstring>
#include <iostream>
#include <string>
#include <cstdlib>
#include <windows.h>
#include <dpapi.h>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <conio.h>
#include <io.h>
#include <cwchar>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <unordered_set>
#include <map>
#include <random>
#include <stdio.h>
#include <algorithm>

#pragma comment(lib, "crypt32.lib")

#include "pwDataGroup.h"
#include "ListH.h"
#include "json.hpp"
#include "CLI11.hpp"
#include "psm.hpp"
#include "AesGcm.h"

using namespace std;
using namespace nlohmann;

bool LoadData(linked::LinkedList<pwDataGroup>& trandata,const std::string& FILE){
	std::ifstream in(FILE);
	if (!in.is_open()) {
		return true;   //文件不存在视为空数据
	}

	json json_data;
	try {
		in >> json_data;
	}
	catch (const std::exception& e) {
		cerr << "bad json in " << FILE << ": " << e.what() << "\n";
		return false;
	}
	in.close();

	pw_json.clear();
	for (const auto& item : json_data) {
		pwDataGroup data;
		data.FromJson(item);
		pw_json.push_back(data);
	}

	trandata = linked::LinkedList<pwDataGroup>(pw_json.data(), (int)pw_json.size());
	return true;
}

namespace {

std::string EscapeField(const std::string& text) {                             //转义字段
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string UnescapeField(const std::string& text) {                           //还原字段
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            char c = text[++i];
            switch (c) {
                case '\\': out += '\\'; break;
                case 't': out += '\t'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                default: out += c; break;
            }
        }
        else {
            out += text[i];
        }
    }
    return out;
}

std::string EscapeCustomPart(const std::string& text) {                        //转义自定义字段
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case ';': out += "\\;"; break;
            case '=': out += "\\="; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string UnescapeCustomPart(const std::string& text) {                      //还原自定义字段
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            char c = text[++i];
            switch (c) {
                case '\\': out += '\\'; break;
                case ';': out += ';'; break;
                case '=': out += '='; break;
                case 't': out += '\t'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                default: out += c; break;
            }
        }
        else {
            out += text[i];
        }
    }
    return out;
}

std::string CustomToString(
    const std::unordered_map<std::string, std::string>& custom) {              //自定义字段转字符串
    std::vector<std::pair<std::string, std::string>> items(custom.begin(), custom.end());
    std::sort(items.begin(), items.end(),
        [](const std::pair<std::string, std::string>& a,
           const std::pair<std::string, std::string>& b) {
            return a.first < b.first;
        });

    std::string out;
    bool first = true;
    for (const auto& item : items) {
        if (!first) {
            out += ';';
        }
        first = false;
        out += EscapeCustomPart(item.first);
        out += '=';
        out += EscapeCustomPart(item.second);
    }
    return out;
}

void ParseCustom(
    const std::string& text,
    std::unordered_map<std::string, std::string>& custom) {                    //字符串转自定义字段
    std::string part;

    auto flush = [&]() {
        if (part.empty()) {
            return;
        }

        size_t eq = std::string::npos;
        for (size_t i = 0; i < part.size(); ++i) {
            if (part[i] == '\\') {
                ++i;                    //跳过被转义的字符
                continue;
            }
            if (part[i] == '=') {
                eq = i;
                break;
            }
        }

        if (eq != std::string::npos) {
            custom[UnescapeCustomPart(part.substr(0, eq))] =
                UnescapeCustomPart(part.substr(eq + 1));
        }
        part.clear();
    };

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '\\' && i + 1 < text.size()) {
            part += c;
            part += text[++i];
        }
        else if (c == ';') {
            flush();
        }
        else {
            part += c;
        }
    }
    flush();
}

} // namespace

void AddData(linked::LinkedList<pwDataGroup>& trandata, const std::string& FILE) {     //追加记录到txt
    bool has_content = false;
    {
        std::ifstream probe(FILE);
        has_content = probe.good() && probe.peek() != std::char_traits<char>::eof();
    }

    std::ofstream out(FILE, std::ios::app);
    if (!out.is_open()) {
        cerr << "File failed to open!\n";
        return;
    }

    if (!has_content) {
        out << "#uid\top\taccount\turl\temail\tupdated_at\tkey\tcustom\n";
    }

    for (int i = 1; i <= trandata.GetLength(); i++) {
        const pwDataGroup& data = trandata.GetRef(i);
        out << data.GetUid() << '\t'
            << (data.GetOp() ? 1 : 0) << '\t'
            << EscapeField(data.GetAccount()) << '\t'
            << EscapeField(data.GetUrl()) << '\t'
            << EscapeField(data.GetEmail()) << '\t'
            << EscapeField(data.GetUpdated()) << '\t'
            << EscapeField(data.GetKey()) << '\t'
            << CustomToString(data.GetCustomCom()) << '\n';
    }
}

bool LoadTxt(linked::LinkedList<pwDataGroup>& trandata, const std::string& FILE) {     //从txt读取记录
    std::ifstream in(FILE);
    if (!in.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::vector<std::string> fields;
        size_t start = 0;
        while (true) {
            size_t tab = line.find('\t', start);
            fields.push_back(line.substr(start, tab == std::string::npos
                                                    ? std::string::npos
                                                    : tab - start));
            if (tab == std::string::npos) {
                break;
            }
            start = tab + 1;
        }

        if (fields.size() < 8) {
            continue;
        }

        pwDataGroup data;
        try {
            data.SetUid(std::stoi(fields[0]));
        }
        catch (const std::exception&) {
            continue;
        }

        data.SetOp(fields[1] == "1");
        data.SetUsername(UnescapeField(fields[2]));
        data.SetUrl(UnescapeField(fields[3]));
        data.SetEmail(UnescapeField(fields[4]));
        data.SetUpdated(UnescapeField(fields[5]));
        data.SetKey(UnescapeField(fields[6]));

        std::unordered_map<std::string, std::string> custom;
        ParseCustom(fields[7], custom);
        data.SetCustomCom(custom);

        trandata.PushBack(data);
    }

    return true;
}

void SaveData(linked::LinkedList<pwDataGroup>& trandata, const std::string& FILE) {
    json json_data = json::array();

    for (int i = 1;i <= trandata.GetLength();i++) {
        pwDataGroup curr = trandata.Get(i);
        json_data.push_back(curr.ToJson());
    }

    std::ofstream out(FILE);
    if (!out.is_open()) {
        cerr << "File failed to open!\n";
        return;
    }
    out << json_data.dump(4);
    out.close();
}

void GetTime() {       
	auto now = std::chrono::system_clock::now();
	std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm,&time);

	std::cout << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " ";
}

string CountTime() {
    time_t now = time(nullptr);
    tm local{};
    localtime_s(&local,&now);

    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local);
    return string(buf);
}

namespace {

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return std::string();
    }

    const int bytes = WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
        return std::string();
    }

    std::string result(bytes, '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
        result.data(), bytes, nullptr, nullptr);
    return result;
}

} // namespace

std::string getInput(const std::string& prompt) { 
    std::cout << prompt;
    std::string input;

    //stdin 被重定向时 _getch 读不到输入，改用 getline
    if (!_isatty(_fileno(stdin))) {
        if (!std::getline(std::cin, input)) {
            input.clear();
        }
        std::cout << std::endl;
        return input;
    }

    std::wstring wide;
    while (true) {
        wint_t ch = _getwch();
        if (ch == L'\r' || ch == L'\n' || ch == WEOF) {
            std::cout << std::endl;
            break;
        }
        if (ch == 8) { // backspace
            if (!wide.empty()) {
                wide.pop_back();
                std::cout << "\b \b";
            }
        }
        else if (ch >= 32) {
            wide.push_back(static_cast<wchar_t>(ch));
            std::cout << '*';
        }
    }
    return WideToUtf8(wide);
}

void ListDataTime(linked::LinkedList<pwDataGroup>& newData, bool ascending) {
    int len = newData.GetLength();
    if (len == 0) return;

    std::unordered_set<std::string> custom_keys_set;
    for (int i = 1; i <= len; i++) {
        for (const auto& pair : newData.Get(i).GetCustomCom()) {
            custom_keys_set.insert(pair.first); // 收集所有的自定义键名
        }
    }
    std::vector<std::string> dynamic_headers(custom_keys_set.begin(), custom_keys_set.end());
    std::sort(dynamic_headers.begin(), dynamic_headers.end());

    std::vector<std::string> headers = { "Username", "Url", "Key", "Update_Time", "UID" };

    // 再把自定义表头追加进去
    for (const auto& key : dynamic_headers) {
        headers.push_back("[" + key + "]");
    }

    std::vector<size_t> max_widths(headers.size(), 0);

    // 先算表头本身的宽度
    for (size_t i = 0; i < headers.size(); i++) {
        max_widths[i] = headers[i].length();
    }

    // 再遍历数据，算实际内容的宽度
    for (int i = 1; i <= len; i++) {
        pwDataGroup item = newData.Get(i);

        // 基础字段宽度
        max_widths[0] = std::max<size_t>(max_widths[0], item.GetAccount().length());
        max_widths[1] = std::max<size_t>(max_widths[1], item.GetUrl().length());
        max_widths[2] = std::max<size_t>(max_widths[2], item.GetKey().length());
        max_widths[3] = std::max<size_t>(max_widths[3], item.GetUpdated().length());
        max_widths[4] = std::max<size_t>(max_widths[4], std::to_string(item.GetUid()).length());

        // 动态字段宽度
        auto custom_map = item.GetCustomCom();
        for (size_t j = 0; j < dynamic_headers.size(); j++) {
            std::string val = "";
            if (custom_map.find(dynamic_headers[j]) != custom_map.end()) {
                val = custom_map[dynamic_headers[j]];
            }
            max_widths[5 + j] = std::max<size_t>(max_widths[5 + j], val.length());
        }
    }

    for (size_t i = 0; i < headers.size(); i++) {
        std::cout << std::left << std::setw(max_widths[i] + 2) << headers[i];
    }
    std::cout << "\n";

    // 打印分割线
    size_t total_len = 0;
    for (size_t w : max_widths) total_len += (w + 2);
    std::cout << std::string(total_len, '-') << "\n";

    for (int i = 1; i <= len; i++) {
        int index = ascending ? i : (len - i + 1); // 控制升降序
        pwDataGroup item = newData.Get(index);

        // 打印基础数据
        std::cout << std::left
            << std::setw(max_widths[0] + 2) << (item.GetAccount().empty() ? "-" : item.GetAccount())
            << std::setw(max_widths[1] + 2) << (item.GetUrl().empty() ? "-" : item.GetUrl())
            << std::setw(max_widths[2] + 2) << (item.GetKey().empty() ? "-" : item.GetKey())
            << std::setw(max_widths[3] + 2) << (item.GetUpdated().empty() ? "-" : item.GetUpdated())
            << std::setw(max_widths[4] + 2) << item.GetUid();

        auto custom_map = item.GetCustomCom();
        for (size_t j = 0; j < dynamic_headers.size(); j++) {
            std::string val = "-";
            if (custom_map.find(dynamic_headers[j]) != custom_map.end()) {
                val = custom_map[dynamic_headers[j]];
                if (val.empty()) val = "-";
            }
            std::cout << std::left << std::setw(max_widths[5 + j] + 2) << val;
        }
        std::cout << "\n";
    }
}

int randomInt(int max) {
    static std::mt19937 gen(random_device{}());
    std::uniform_int_distribution<int> dist(0, max - 1);
    return dist(gen);
}

int uidCount() {
    int token = pw_data.GetLength();
    int random = randomInt(10086);
    return token * 13 + random;
}

void addData(
    const std::string& username,
    const std::string& url,
    const std::string& password,
    const std::string& email,
    const std::vector <std::string>& custom_sets,
    bool op) {

    std::string encrypted_base64;
    if (!password.empty()) {
        std::string error_msg;
        bool success = psm::crypto::EncryptToString(
            g_master_password,
            password,
            "psm-pw-v1",
            encrypted_base64,
            error_msg
        );

        if (!success) {
            std::cerr << "Encrpty error.\n\tplease check the startup key and try again.\n";
            return;
        }
    }

    for (int i = 1; i <= pw_data.GetLength(); i++) {
        pwDataGroup& item = pw_data.GetRef(i);
        if (item.GetUrl() == url && item.GetAccount() == username) {
            if (!email.empty()) {
                item.SetEmail(email);
            }
            if (!encrypted_base64.empty()) {
                item.SetKey(encrypted_base64);
            }
            if (op) {
                item.SetOp(true);
            }
            item.SetUpdated(CountTime());

            for (const auto& cus_set : custom_sets) {
                size_t pos = cus_set.find("=");
                if (pos != string::npos) {
                    string k = cus_set.substr(0, pos);
                    string v = cus_set.substr(pos + 1);
                    item.addCustomField(k, v);
                }
                else {
                    cerr << "Warning: Parameter format error.\n";
                }
            }
            cout << "data update generated." << endl;
            return;
        }
    }

    pwDataGroup newData;
    newData.SetUsername(username);
    newData.SetUrl(url);
    newData.SetEmail(email);
    newData.SetKey(encrypted_base64);
    newData.SetUid(uidCount());
    newData.SetUpdated(CountTime());
    if (op) {
        newData.SetOp();
    }


    for (const auto& cus_set : custom_sets) {
        size_t pos = cus_set.find("=");
        if (pos != string::npos) {
            string k = cus_set.substr(0, pos);
            string v = cus_set.substr(pos + 1);
            newData.addCustomField(k, v);
        }
        else {
            cerr << "Warning: Parameter format error.\n";
        }
    }

    pw_data.PushBack(newData);
    cout << endl;
}

linked::LinkedList<pwDataGroup> sortLinkedTime(bool ascending) {
    int len = pw_data.GetLength();
    std::vector<pwDataGroup> newVec;
    for (int i = 1;i <= len;i++) {
        newVec.push_back(pw_data.Get(i));
    }

    std::sort(newVec.begin(), newVec.end(), [ascending](const pwDataGroup& a, const pwDataGroup& b) {
        string timeA = a.GetUpdated();
        string timeB = b.GetUpdated();

        if (ascending) {
            return timeA < timeB;//升序
        }
        else {
            return timeA > timeB;//降序
        }
        });
       
    linked::LinkedList<pwDataGroup> newData(newVec.data(), (int)newVec.size());
    return newData;
}

namespace {

constexpr const char* kMasterMagic = "#psm-master-v2";
constexpr const char* kVerifierPlain = "psm-master-verifier";
constexpr const char* kVerifierAad = "psm-verifier-v1";

std::string g_master_verifier;   //Base64

std::string MasterFilePath() {
    std::filesystem::path p(g_data_file);
    std::filesystem::path dir =
        p.has_parent_path() ? p.parent_path() : std::filesystem::path(".");
    return (dir / MASTER_FILE).string();
}

std::string TrimCR(std::string value) {
    if (!value.empty() && value.back() == '\r') {
        value.pop_back();
    }
    return value;
}

std::string ProtectForCurrentUser(const std::string& plain) {
    DATA_BLOB in;
    in.cbData = static_cast<DWORD>(plain.size());
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()));

    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"psm-master-password", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return std::string();
    }

    std::string result(reinterpret_cast<char*>(out.pbData), out.cbData);
    LocalFree(out.pbData);
    return result;
}

std::string UnprotectForCurrentUser(const std::string& blob) {
    if (blob.empty()) {
        return std::string();
    }

    DATA_BLOB in;
    in.cbData = static_cast<DWORD>(blob.size());
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(blob.data()));

    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return std::string();
    }

    std::string result(reinterpret_cast<char*>(out.pbData), out.cbData);
    LocalFree(out.pbData);
    return result;
}

std::string MakeVerifier(const std::string& password) {
    std::string verifier;
    std::string error;
    if (!psm::crypto::EncryptToString(
            password, kVerifierPlain, kVerifierAad, verifier, error)) {
        return std::string();
    }
    return verifier;
}

bool CheckVerifier(const std::string& password, const std::string& verifier) {
    if (verifier.empty()) {
        return false;
    }

    std::string plain;
    std::string error;
    if (!psm::crypto::DecryptFromString(
            password, verifier, kVerifierAad, plain, error)) {
        return false;
    }
    return plain == kVerifierPlain;
}

bool WriteMasterFile(const std::string& password) {
    const std::string protectedPassword = ProtectForCurrentUser(password);
    if (protectedPassword.empty()) {
        g_master_error = "DPAPI failed to protect the master password";
        return false;
    }

    const std::string verifier = MakeVerifier(password);
    if (verifier.empty()) {
        g_master_error = "failed to create the master password verifier";
        return false;
    }

    const psm::crypto::Bytes protectedBytes(
        protectedPassword.begin(), protectedPassword.end());
    const std::string protectedBase64 =
        psm::crypto::Base64Encode(protectedBytes);

    const std::string path = MasterFilePath();
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        g_master_error = "cannot open " + path;
        return false;
    }
    out << kMasterMagic << "\n"
        << protectedBase64 << "\n"
        << verifier << "\n";
    out.close();
    if (!out) {
        g_master_error = "failed to write " + path;
        return false;
    }

    g_master_verifier = verifier;
    return true;
}

} // namespace

bool ReadMasterpsm() {
    g_master_error.clear();
    g_master_verifier.clear();

    const std::string path = MasterFilePath();
    std::ifstream in(path);
    if (!in.is_open()) {
        return true;   //没有主密码使用环境变量或提示init
    }

    std::string first;
    if (!std::getline(in, first)) {
        std::cerr << path << " is empty; ignoring it.\n";
        return true;
    }
    first = TrimCR(first);

    if (first == kMasterMagic) {
        std::string protectedBase64;
        std::string verifier;
        if (!std::getline(in, protectedBase64) ||
            !std::getline(in, verifier)) {
            g_master_error = "master password file is incomplete: " + path;
            return false;
        }
        protectedBase64 = TrimCR(protectedBase64);
        verifier = TrimCR(verifier);
        in.close();

        psm::crypto::Bytes raw;
        std::string error;
        if (!psm::crypto::Base64Decode(protectedBase64, raw, error)) {
            g_master_error = "cannot decode " + path + ": " + error;
            return false;
        }

        const std::string password = UnprotectForCurrentUser(
            std::string(raw.begin(), raw.end()));
        if (password.empty()) {
            g_master_error = "cannot decrypt " + path + " with DPAPI";
            return false;
        }

        if (!CheckVerifier(password, verifier)) {
            g_master_error = "master password verifier check failed";
            return false;
        }

        g_master_password = password;
        g_master_verifier = verifier;
        return true;
    }

    //旧版明文文件：读取第一行并自动升级为 DPAPI + verifier
    in.close();
    if (first.empty()) {
        std::cerr << path << " is empty; ignoring it.\n";
        return true;
    }

    g_master_password = first;
    std::cerr << "warning: legacy plaintext " << path
              << ", upgrading to DPAPI.\n";
    if (!WriteMasterFile(g_master_password)) {
        std::cerr << "warning: failed to upgrade " << path
                  << ": " << g_master_error << "\n";
        g_master_error.clear();
    }
    return true;
}

bool ApplyMasterPasswordOverride(const std::string& password) {
    g_master_error.clear();
    if (!g_master_verifier.empty() &&
        !CheckVerifier(password, g_master_verifier)) {
        g_master_error = "PSM_MASTER_PASSWORD does not match the verifier in "
                         + MasterFilePath();
        return false;
    }
    g_master_password = password;
    return true;
}

bool initMasterpsm() {
    if (g_master_password != "") {
        cout << "Reinitialize existing PSM startup key in "
             << MasterFilePath() << endl;
    }

    const std::string first = getInput("Enter startup key : ");
    if (first.empty()) {
        cerr << "master password must not be empty.\n";
        return false;
    }
    const std::string second = getInput("Confirm startup key: ");
    if (first != second) {
        cerr << "passwords do not match.\n";
        return false;
    }

    //先把所有 key 用旧密码解出来，再用新密码加密；全部成功后才写文件
    const int len = pw_data.GetLength();
    std::vector<bool> hasKey(len + 1, false);
    std::vector<std::string> newKeys(len + 1);
    int reencrypted = 0;

    for (int i = 1; i <= len; i++) {
        const pwDataGroup& item = pw_data.GetRef(i);
        if (item.GetKey().empty()) {
            continue;
        }

        std::string plain;
        std::string error;
        if (!psm::crypto::DecryptFromString(
                g_master_password, item.GetKey(), "psm-pw-v1", plain, error)) {
            cerr << "cannot decrypt entry uid=" << item.GetUid()
                 << " with the current master password: " << error << "\n";
            cerr << "master password was not changed.\n";
            return false;
        }

        std::string encrypted;
        if (!psm::crypto::EncryptToString(
                first, plain, "psm-pw-v1", encrypted, error)) {
            cerr << "cannot re-encrypt entry uid=" << item.GetUid()
                 << ": " << error << "\n";
            cerr << "master password was not changed.\n";
            return false;
        }

        hasKey[i] = true;
        newKeys[i] = encrypted;
        reencrypted++;
    }

    if (!WriteMasterFile(first)) {
        cerr << "master password was not changed: " << g_master_error << "\n";
        return false;
    }

    for (int i = 1; i <= len; i++) {
        if (hasKey[i]) {
            pw_data.GetRef(i).SetKey(newKeys[i]);
        }
    }
    g_master_password = first;
    g_master_error.clear();

    cout << "master password updated";
    if (reencrypted > 0) {
        cout << ", " << reencrypted << " entries re-encrypted";
    }
    cout << ".\n";
    return true;
}

bool startup() {
    return g_master_password == "";
}

