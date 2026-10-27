#include <cstring>
#include <iostream>
#include <string>
#include <cstdlib>
#include <windows.h>
#include <fstream>
#include <sstream>
#include <vector>

#include "pwDataGroup.h"
#include "ListH.h"

pwDataGroup::pwDataGroup() :account(""), key(""), url(""),email(""),updated_at(""), custom_com(0), uid(0) {}
pwDataGroup::pwDataGroup(std::string _Username, std::string _key, std::string _url,std::string _email,
	std::string _updated_at, std::unordered_map<std::string, std::string> _custom_com, int _uid) :
	account(_Username), key(_key), url(_url),email(_email), updated_at(_updated_at), custom_com(_custom_com), uid(_uid) { }
pwDataGroup::~pwDataGroup() = default;

std::string pwDataGroup::GetAccount()const { return account; }
std::string pwDataGroup::GetKey()const { return key; }
std::string pwDataGroup::GetUrl()const { return url; }
std::string pwDataGroup::GetEmail()const { return email; }
std::string pwDataGroup::GetUpdated()const { return updated_at; }
const std::unordered_map<std::string, std::string>& pwDataGroup::GetCustomCom()const { return custom_com; }
int pwDataGroup::GetUid()const { return uid; }

void pwDataGroup::SetUsername(std::string _Username) { account = std::move(_Username); }
void pwDataGroup::SetKey(std::string _key) { key = std::move(_key); }
void pwDataGroup::SetUrl(std::string _url) { url = std::move(_url); }
void pwDataGroup::SetEmail(std::string _email) { email = std::move(_email); }
void pwDataGroup::SetUpdated(std::string _updated_at) { updated_at = std::move(_updated_at); }
void pwDataGroup::SetCustomCom(std::unordered_map<std::string, std::string> _custom_com) { custom_com = std::move(_custom_com); }
void pwDataGroup::SetUid(int _uid) { uid = std::move(_uid); }
void pwDataGroup::addCustomField(const std::string& key, const std::string& value) { custom_com[key] = value; }

nlohmann::json pwDataGroup::ToJson(){
	nlohmann::json data;
	data["username"] = account;
	data["key"] = key;
	data["url"] = url;
	data["email"] = email;
	data["updated_at"] = updated_at;
	data["custom_com"] = custom_com;
	data["uid"] = uid;
	return data;
}
void pwDataGroup::FromJson(const nlohmann::json& data) {
	account = data.value("username", "");
	key = data.value("key", "");
	url = data.value("url", "");
	email = data.value("email", "");
	updated_at = data.value("updated_at", "");
	uid = data.value("uid", 000);
	if (data.contains("custom_com")) {
		custom_com = data["custom_com"].get<std::unordered_map<std::string, std::string>>(); //模板类类型转换函数
	}
}
std::ostream& operator<<(std::ostream& out, const pwDataGroup& data) {
	std::vector<std::pair<std::string,std::string>> fields = {
		{"Account",data.account},
		{"Url",data.url},
		{"Email",data.email}
	};

	for (const auto& field : fields) {
		if (!field.second.empty()) {
			 out << field.second << "\t";
		}
	}

	if (!data.key.empty()) {
		out << data.key << "\t";
	}

	if (!data.custom_com.empty()) {
		for (const auto& pair : data.custom_com) {
			if (!pair.second.empty()) {
				out << pair.second << "\t";
			}
		}
	}

	if (!data.updated_at.empty()) {
		out << data.updated_at << "\t";
	}

	if (data.uid != 0) {
		out << data.uid << "\t";
	}
	out << std::endl;
	return out;
}

bool operator==(const pwDataGroup& pw1, const pwDataGroup& pw2) {
	if (pw1.url == pw2.url) {
		return true;
	}
	return false;
}

pwDataGroup& pwDataGroup::operator=(const pwDataGroup& item) {
	if (this != &item) {
		account = item.account;
		key = item.key;
		url = item.url;
		email = item.email;
		updated_at = item.updated_at;
		custom_com = item.custom_com;
		uid = item.uid;
	}
	return *this;
}