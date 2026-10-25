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

void ListCommand(const ListBox& opts, LinkedList<pwDataGroup>& pwData, bool ascending = true) {
	int len = pw_data.GetLength();

	if (len == 0) {
		cerr << "no data.\n";
		return;
	}

	if (opts.output_json) {
		json j_arr = json::array();
		for (int i = 1;i <= len;i++) {
			j_arr.push_back(pwData.Get(i).ToJson());
		}
		cout << j_arr.dump(4) << endl;
		return;
	}

	vector<pwDataGroup> sorted_data;
	for (int i = 1;i <= len;i++) {	sorted_data.push_back(pwData.Get(i));}
	if (opts.show_time) {
		std::sort(sorted_data.begin(), sorted_data.end(), [ascending](const pwDataGroup& a, const pwDataGroup& b) {
			string timeA = a.GetUpdated();
			string timeB = b.GetUpdated();

			if (ascending) {
				return timeA < timeB;//升序
			}
			else {
				return timeA > timeB;//降序
			}
			});
	}
	if (opts.show_reverse) {
		reverse(sorted_data.begin(), sorted_data.end());
	}

	//存表头
	unordered_set<std::string>custom_key_set;
	for (const auto& item : sorted_data) {
		for (const auto& pair : item.GetCustomCom()) {
			custom_key_set.insert(pair.first);
		}
	}
	vector<string> head(custom_key_set.begin(), custom_key_set.end());
	sort(head.begin(), head.end());

	vector<string> headers = { "account","url","key" };
	if (opts.show_T) {headers.push_back("uptime");}
	headers.push_back("ID");
	for (const auto& key : head) {headers.push_back("[" + key + "]");}

	//宽度计算
	vector<size_t> max_widths(headers.size(), 0);
	for (size_t i = 0;i <headers.size();i++) {
		max_widths[i] = headers[i].length();
	}
	for (const auto& item : sorted_data) {
			max_widths[0] = max<size_t>(max_widths[0], item.GetAccount().length());
			max_widths[1] = max<size_t>(max_widths[1], item.GetUrl().length());

		string plain,err2;
		string pass_str;
		psm::crypto::DecryptFromString(g_master_password, item.GetKey(), "psm-pw-v1", plain, err2);
		pass_str = opts.show_pass ? plain : item.GetKey();
			max_widths[2] = max<size_t>(max_widths[2], pass_str.length());

		int id_idx;
		int custom_key_idx = 3;
		auto custom_map = item.GetCustomCom();
		for (int j = 0;j < head.size();j++) {
			string val = "";
			if (custom_map.count(head[j])) val = custom_map[head[j]];
			max_widths[custom_key_idx + j] = max<size_t>(max_widths[custom_key_idx + j], val.length());
			id_idx = custom_key_idx + j + 1;
		}
		if (opts.show_T) {
			max_widths[id_idx] = max<size_t>(max_widths[id_idx], item.GetUpdated().length());
			id_idx++;
		}
			max_widths[id_idx] = max<size_t>(max_widths[id_idx], to_string(item.GetUid()).length());
			cout << endl;
	}

	//打印数据
	for (size_t i = 0;i < headers.size();i++) {
		cout << left << setw(max_widths[i] + 2) << headers[i];
	}
	cout << "\n";

	size_t total_len = 0;
	for (size_t w : max_widths)total_len += (w + 2);
	cout << string(total_len, '-') << endl;

	for (const auto& item : sorted_data) {
		string plain, err2;
		string pass_str;
		psm::crypto::DecryptFromString(g_master_password, item.GetKey(), "psm-pw-v1", plain, err2);
		if (opts.show_pass) {
			pass_str = plain;
		}
		pass_str = item.GetKey();
		std::cout << std::left
			<< std::setw(max_widths[0] + 2) << (item.GetAccount().empty() ? "-" : item.GetAccount())
			<< std::setw(max_widths[1] + 2) << (item.GetUrl().empty() ? "-" : item.GetUrl())
			<< std::setw(max_widths[2] + 2) << pass_str;

		int custom_set_idx = 3;
		int id_idx;
		auto custom_map = item.GetCustomCom();
		for (size_t j = 0;j < head.size();j++) {
			string val = "-";
			if (custom_map.count(head[j])) {
				val = custom_map[head[j]];
				if (val.empty())
					val = "-";
			}
			cout << setw(max_widths[custom_set_idx + j] + 2) << val;
			id_idx = custom_set_idx + j;
		}
		if (opts.show_T) {
			cout << setw(max_widths[id_idx] + 2) << (item.GetUpdated().empty() ? "-": item.GetUpdated());
			id_idx++;
		}
		cout << setw(max_widths[id_idx] + 2) << item.GetUid();
	}
	cout << endl;
}