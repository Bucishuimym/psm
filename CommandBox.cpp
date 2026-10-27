#include <iostream>
#include <string>
#include <cstring>
#include <iomanip>
#include <algorithm>
#include <unordered_set>
#include <vector>

#include "json.hpp"
#include "CommandBox.h"
#include "ListH.h"
#include "pwDataGroup.h"
#include "AesGcm.h"
#include "psm.hpp"

using namespace std;
using namespace nlohmann;

void ListCommand(const ListBox& opts, LinkedList<pwDataGroup>& pwData, bool ascending) {
    int len = pwData.GetLength();

    if (len == 0) {
        cerr << "no data.\n";
        return;
    }

    auto get_display_key = [&](const pwDataGroup& item) -> string {
        if (!opts.show_pass) {
            return item.GetKey();          // 不解密
        }
        string plain, err2;
        psm::crypto::DecryptFromString(g_master_password, item.GetKey(), "psm-pw-v1", plain, err2);
        if (!err2.empty()) {
            return item.GetKey();          // 解密失败回退密文
        }
        return plain;
        };

    if (opts.output_json) {
        json j_arr = json::array();
        for (int i = 1; i <= len; i++) {
            j_arr.push_back(pwData.Get(i).ToJson());
        }

        std::cout << j_arr.dump(4) << endl;
        return;
    }

    //收集并排序
    vector<pwDataGroup> sorted_data;
    sorted_data.reserve(len);
    for (int i = 1; i <= len; i++) {
        sorted_data.push_back(pwData.Get(i));
    }


    if (opts.show_sort == "time") {
        std::sort(sorted_data.begin(), sorted_data.end(),
            [ascending](const pwDataGroup& a, const pwDataGroup& b) {
                string timeA = a.GetUpdated();
                string timeB = b.GetUpdated();
                return ascending ? (timeA < timeB) : (timeA > timeB);
            });
    }
    if (opts.show_reverse) {
        reverse(sorted_data.begin(), sorted_data.end());
    }

    //收集定义列
    unordered_set<std::string> custom_key_set;
    for (const auto& item : sorted_data) {
        for (const auto& pair : item.GetCustomCom()) {
            custom_key_set.insert(pair.first);
        }
    }
    vector<string> custom_keys(custom_key_set.begin(), custom_key_set.end());
    std::sort(custom_keys.begin(), custom_keys.end());

    //表头顺序
    vector<string> headers;
    headers.push_back("account");
    headers.push_back("url");
    headers.push_back("email");
    headers.push_back("key");
    for (const auto& key : custom_keys) {
        headers.push_back("[" + key + "]");
    }
    if (opts.show_T) {
        headers.push_back("uptime");
    }
    headers.push_back("ID");

    // 列索引
    const size_t CUSTOM_START = 4;                                  // 自定义列起始
    const size_t UPTIME_IDX = CUSTOM_START + custom_keys.size();  // 若 show_T 则有效
    const size_t ID_IDX = UPTIME_IDX + (opts.show_T ? 1 : 0);

    vector<string> display_keys;
    display_keys.reserve(sorted_data.size());
    for (const auto& item : sorted_data) {
        display_keys.push_back(get_display_key(item));
    }

    //宽度计算
    vector<size_t> max_widths(headers.size(), 0);
    for (size_t i = 0; i < headers.size(); i++) {
        max_widths[i] = headers[i].length();
    }

    for (size_t i = 0; i < sorted_data.size(); i++) {
        const auto& item = sorted_data[i];

        // account
        max_widths[0] = max<size_t>(max_widths[0], item.GetAccount().length());
        // url
        max_widths[1] = max<size_t>(max_widths[1], item.GetUrl().length());

        //email
        max_widths[2] = max<size_t>(max_widths[2], item.GetEmail().length());

        // key
        max_widths[3] = max<size_t>(max_widths[3], display_keys[i].length());

        // 自定义列
        auto custom_map = item.GetCustomCom();
        for (size_t j = 0; j < custom_keys.size(); j++) {
            string val;
            auto it = custom_map.find(custom_keys[j]);
            if (it != custom_map.end()) val = it->second;
            max_widths[CUSTOM_START + j] = max<size_t>(max_widths[CUSTOM_START + j], val.length());
        }

        // uptime
        if (opts.show_T) {
            max_widths[UPTIME_IDX] = max<size_t>(max_widths[UPTIME_IDX], item.GetUpdated().length());
        }

        // ID
        max_widths[ID_IDX] = max<size_t>(max_widths[ID_IDX], to_string(item.GetUid()).length());
    }

    // 打印表头
    std::cout << left;
    for (size_t i = 0; i < headers.size(); i++) {
        std::cout << setw(max_widths[i] + 2) << headers[i];
    }
    std::cout << "\n";

    /*size_t total_len = 0;
    for (size_t w : max_widths) total_len += (w + 2);
    std::cout << string(total_len, '-') << endl;*/

    //打印数据
    for (size_t i = 0; i < sorted_data.size(); i++) {
        const auto& item = sorted_data[i];

        // account
        std::cout << setw(max_widths[0] + 2)
            << (item.GetAccount().empty() ? "-" : item.GetAccount());
      
        // url
        std::cout << setw(max_widths[1] + 2)
            << (item.GetUrl().empty() ? "-" : item.GetUrl());

        //email
        std::cout << setw(max_widths[2] + 2)
            << (item.GetEmail().empty() ? "-" : item.GetEmail());

        // key
        std::cout << setw(max_widths[3] + 2) << display_keys[i];

        // 自定义列
        auto custom_map = item.GetCustomCom();
        for (size_t j = 0; j < custom_keys.size(); j++) {
            string val = "-";
            auto it = custom_map.find(custom_keys[j]);
            if (it != custom_map.end() && !it->second.empty()) {
                val = it->second;
            }
            std::cout << setw(max_widths[CUSTOM_START + j] + 2) << val;
        }
        // uptime
        if (opts.show_T) {
            std::cout << setw(max_widths[UPTIME_IDX] + 2)
                << (item.GetUpdated().empty() ? "-" : item.GetUpdated());
        }

        // ID
        std::cout << setw(max_widths[ID_IDX] + 2) << item.GetUid();

        std::cout << endl;   // 每行换行
    }
}