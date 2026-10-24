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

#include "pwDataGroup.h"
#include "ListH.h"
#include "json.hpp"
#include "CLI11.hpp"
#include "psm.hpp"

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
		data.FromJson(json_data);
		pw_json.push_back(data);
	}

	pw_data = LinkedList<pwDataGroup>(pw_json.data(), (int)pw_json.size());
}

void SaveData() {
    json json_data = json::array();

    for (int i = 1;i <  pw_data.GetLength();i++) {
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

void GetTime() {                                                               //输出系统时间
	auto now = std::chrono::system_clock::now();
	std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm,&time);

	cout << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " ";
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

void ListData() {
    int len = pw_data.GetLength();
    if (len == 0) {
        std::cout << "当前密码库为空！\n";
        return;
    }

    std::unordered_set<std::string> custom_keys_set;
    for (int i = 1; i <= len; i++) {
        pwDataGroup item = pw_data.Get(i);
        for (const auto& pair : item.GetCustomCom()) {
            custom_keys_set.insert(pair.first);
        }
    }

    std::vector<std::string> dynamic_headers(custom_keys_set.begin(), custom_keys_set.end());
    std::vector<std::string> headers = { "Username", "Url", "Key", "Update_Time", "UID" };
    // 把动态键追加到表头后面
    for (const auto& key : dynamic_headers) {
        headers.push_back("[" + key + "]");
    }

    std::vector<size_t> max_widths(headers.size(), 0);

    for (size_t i = 0; i < headers.size(); i++) {
        max_widths[i] = headers[i].length();
    }

    for (int i = 1; i <= len; i++) {
        pwDataGroup item = pw_data.Get(i);

        max_widths[0] = std::max<size_t>(max_widths[0], item.GetAccount().length());
        max_widths[1] = std::max<size_t>(max_widths[1], item.GetUrl().length());
        max_widths[2] = std::max<size_t>(max_widths[2], item.GetKey().length());
        max_widths[3] = std::max<size_t>(max_widths[3], item.GetUpdated().length());
        max_widths[4] = std::max<size_t>(max_widths[4], std::to_string(item.GetUid()).length());

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

   //data
    for (int i = 1; i <= len; i++) {
        pwDataGroup item = pw_data.Get(i);

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
            std::cout << std::setw(max_widths[5 + j] + 2) << val;
        }
        std::cout << "\n";
    }
}