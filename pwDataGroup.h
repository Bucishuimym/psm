#pragma once

#ifndef PWDATAGROUP_H
#define PWDATAGROUP_H

#include <cstring>
#include <iostream>
#include <string>
#include <cstdlib>
#include <windows.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>

#include "pwDataGroup.h"
#include "ListH.h"
#include "json.hpp"

class pwDataGroup{
private:
    std::string account;
    std::string key;
    std::string url;
    std::string email;
    std::string updated_at;
    std::unordered_map<std::string, std::string> custom_com;
    int uid;
public:
    pwDataGroup();
    pwDataGroup(std::string _Username, std::string _key, std::string _url,std::string _email,
        std::string _updated_at, std::unordered_map<std::string, std::string> _custom_com, int _uid);
    ~pwDataGroup();

    std::string GetAccount()const;
    std::string GetKey()const;
    std::string GetUrl()const;
    std::string GetEmail()const;
    std::string GetUpdated()const;
    std::unordered_map<std::string, std::string> GetCustomCom()const;
    int GetUid()const;

    void SetUsername( std::string _Username);
    void SetKey(std::string _key);
    void SetUrl(std::string _url);
    void SetEmail(std::string _email);
    void SetUpdated(std::string _updated_at);
    void SetCustomCom(std::unordered_map<std::string, std::string> _custom_com);
    void SetUid(int _uid);

    nlohmann::json ToJson();
    void FromJson(const nlohmann::json& data);

    friend std::ostream& operator<<(std::ostream& out, const pwDataGroup& data);
    friend bool operator==(const pwDataGroup& pw1, const pwDataGroup& pw2);
};

extern std::vector<pwDataGroup> pw_json;
extern LinkedList<pwDataGroup> pw_data;

#endif // !PWDATAGROUP_H
