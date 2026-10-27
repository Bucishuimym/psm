#include <cstring>
#include <iostream>
#include <string>
#include <cstdlib>
#include <windows.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <conio.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <unordered_set>
#include <map>
#include <random>
#include <stdio.h>
#include <algorithm>

#include "pwDataGroup.h"
#include "ListH.h"
#include "json.hpp"
#include "CLI11.hpp"
#include "psm.hpp"
#include "AesGcm.h"

using namespace std;
using namespace nlohmann;

void LoadData(){
	std::ifstream in(DATA_FILE);
	if (!in.is_open()) {
		cerr << "File failed to open!\n";
		return;
	}

	json json_data;
	in >> json_data;
	in.close();

	pw_json.clear();
	for (const auto& item : json_data) {
		pwDataGroup data;
		data.FromJson(item);
		pw_json.push_back(data);
	}

	pw_data = LinkedList<pwDataGroup>(pw_json.data(), (int)pw_json.size());
}

void SaveData() {
    json json_data = json::array();

    for (int i = 1;i <= pw_data.GetLength();i++) {
        pwDataGroup curr = pw_data.Get(i);
        json_data.push_back(curr.ToJson());
    }

    std::ofstream out(DATA_FILE);
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

	cout << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " ";
}

string CountTime() {
    time_t now = time(nullptr);
    tm local{};
    localtime_s(&local,&now);

    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local);
    return string(buf);
}

std::string getInput(const std::string& prompt) { 
    std::cout << prompt;
    std::string input;
    int ch;
    while (true) {
        ch = _getch();
        if (ch == '\r' || ch == '\n') {
            std::cout << std::endl;
            break;
        }
        if (ch == 8) { // backspace
            if (!input.empty()) {
                input.pop_back();
                std::cout << "\b \b";
            }
        }
        else if (ch >= 32 && ch <= 126) {
            input.push_back((char)ch);
            std::cout << '*';
        }
    }
    return input;
}

void ListDataTime(LinkedList<pwDataGroup>& newData, bool ascending) {
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

void ListDataNoTime(LinkedList<pwDataGroup>& newData, bool ascending) {
    int len = newData.GetLength();
    if (len == 0) {
        std::cout << "No data.\n";
        return;
    }

    std::unordered_set<std::string> custom_keys_set;
    for (int i = 1; i <= len; i++) {
        for (const auto& pair : newData.Get(i).GetCustomCom()) {
            custom_keys_set.insert(pair.first);
        }
    }
    std::vector<std::string> dynamic_headers(custom_keys_set.begin(), custom_keys_set.end());
    std::sort(dynamic_headers.begin(), dynamic_headers.end());

    std::vector<std::string> headers = { "Username", "Url", "Key", "UID" };

    for (const auto& key : dynamic_headers) {
        headers.push_back("[" + key + "]");
    }

    std::vector<size_t> max_widths(headers.size(), 0);

    // 算表头本身的宽度
    for (size_t i = 0; i < headers.size(); i++) {
        max_widths[i] = headers[i].length();
    }

    for (int i = 1; i <= len; i++) {
        pwDataGroup item = newData.Get(i);

        // 基础字段宽度 (注意：UID 的索引变成了 3)
        max_widths[0] = std::max<size_t>(max_widths[0], item.GetAccount().length());
        max_widths[1] = std::max<size_t>(max_widths[1], item.GetUrl().length());
        max_widths[2] = std::max<size_t>(max_widths[2], item.GetKey().length());
        max_widths[3] = std::max<size_t>(max_widths[3], std::to_string(item.GetUid()).length());

        auto custom_map = item.GetCustomCom();
        for (size_t j = 0; j < dynamic_headers.size(); j++) {
            std::string val = "";
            if (custom_map.find(dynamic_headers[j]) != custom_map.end()) {
                val = custom_map[dynamic_headers[j]];
            }
            max_widths[4 + j] = std::max<size_t>(max_widths[4 + j], val.length());
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

        std::cout << std::left
            << std::setw(max_widths[0] + 2) << (item.GetAccount().empty() ? "-" : item.GetAccount())
            << std::setw(max_widths[1] + 2) << (item.GetUrl().empty() ? "-" : item.GetUrl())
            << std::setw(max_widths[2] + 2) << (item.GetKey().empty() ? "-" : item.GetKey())
            << std::setw(max_widths[3] + 2) << item.GetUid(); // UID 变到了这里

        auto custom_map = item.GetCustomCom();
        for (size_t j = 0; j < dynamic_headers.size(); j++) {
            std::string val = "-";
            if (custom_map.find(dynamic_headers[j]) != custom_map.end()) {
                val = custom_map[dynamic_headers[j]];
                if (val.empty()) val = "-";
            }
            std::cout << std::left << std::setw(max_widths[4 + j] + 2) << val;
        }
        std::cout << "\n";
    }
}

void ListDataNoTime(LinkedList<pwDataGroup>& newData, bool ascending) {
    int len = newData.GetLength();
    if (len == 0) {
        std::cout << "No data.\n";
        return;
    }

    std::unordered_set<std::string> custom_keys_set;
    for (int i = 1; i <= len; i++) {
        for (const auto& pair : newData.Get(i).GetCustomCom()) {
            custom_keys_set.insert(pair.first);
        }
    }
    std::vector<std::string> dynamic_headers(custom_keys_set.begin(), custom_keys_set.end());
    std::sort(dynamic_headers.begin(), dynamic_headers.end());

    std::vector<std::string> headers = { "Username", "Url", "Key", "UID" };

    for (const auto& key : dynamic_headers) {
        headers.push_back("[" + key + "]");
    }

    std::vector<size_t> max_widths(headers.size(), 0);

    // 算表头本身的宽度
    for (size_t i = 0; i < headers.size(); i++) {
        max_widths[i] = headers[i].length();
    }

    for (int i = 1; i <= len; i++) {
        pwDataGroup item = newData.Get(i);

        // 基础字段宽度 (注意：UID 的索引变成了 3)
        max_widths[0] = std::max<size_t>(max_widths[0], item.GetAccount().length());
        max_widths[1] = std::max<size_t>(max_widths[1], item.GetUrl().length());
        max_widths[2] = std::max<size_t>(max_widths[2], item.GetKey().length());
        max_widths[3] = std::max<size_t>(max_widths[3], std::to_string(item.GetUid()).length());

        auto custom_map = item.GetCustomCom();
        for (size_t j = 0; j < dynamic_headers.size(); j++) {
            std::string val = "";
            if (custom_map.find(dynamic_headers[j]) != custom_map.end()) {
                val = custom_map[dynamic_headers[j]];
            }
            max_widths[4 + j] = std::max<size_t>(max_widths[4 + j], val.length());
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

        std::cout << std::left
            << std::setw(max_widths[0] + 2) << (item.GetAccount().empty() ? "-" : item.GetAccount())
            << std::setw(max_widths[1] + 2) << (item.GetUrl().empty() ? "-" : item.GetUrl())
            << std::setw(max_widths[2] + 2) << (item.GetKey().empty() ? "-" : item.GetKey())
            << std::setw(max_widths[3] + 2) << item.GetUid(); // UID 变到了这里

        auto custom_map = item.GetCustomCom();
        for (size_t j = 0; j < dynamic_headers.size(); j++) {
            std::string val = "-";
            if (custom_map.find(dynamic_headers[j]) != custom_map.end()) {
                val = custom_map[dynamic_headers[j]];
                if (val.empty()) val = "-";
            }
            std::cout << std::left << std::setw(max_widths[4 + j] + 2) << val;
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
    const std::vector <std::string>& custom_sets) {
    pwDataGroup newData;
    int ID = uidCount();
    string time = CountTime();
    newData.SetUsername(username);
    newData.SetUrl(url);
    newData.SetEmail(email);
    newData.SetUid(ID);
    newData.SetUpdated(time);

    if (!password.empty()) {
        std::string encrypted_base64;
        std::string error_msg;
        bool success = psm::crypto::EncryptToString(
            g_master_password,
            password,
            "psm-pw-v1",
            encrypted_base64,
            error_msg
        );

        if (success) {
            newData.SetKey(encrypted_base64);
        }
        else {
            std::cerr << "Encrpty error.\n\tplease check the startup key and try again.\n";
            return;
        }
    }

    if (!custom_sets.empty())
    {
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
    }

    pw_data.PushBack(newData);
    cout << endl;
}

LinkedList<pwDataGroup> sortLinkedTime(bool ascending) {
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
       
    LinkedList<pwDataGroup> newData(newVec.data(), (int)newVec.size());
    return newData;
}