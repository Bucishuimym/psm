#pragma once

#ifndef PSM_HPP
#define PSM_HPP

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
#include "CLI11.hpp"

const std::string DATA_FILE = "data.json";
std::string g_master_password = "02230ma";                                     //psm启动密钥

void LoadData();                                                               //加载数据
void SaveData();                                                               //保存数据
void GetTime();                                                                //返回系统时间
void ListDataTime(LinkedList<pwDataGroup>& newData, bool ascending = true);    //打印数据矩阵
void ListDataNoTime(LinkedList<pwDataGroup>& newData, bool ascending = true);  //打印但没时间
std::string getInput(const std::string& prompt);                               //隐式输入
std::string CountTime();                                                       //return一个时间
int uidCount();                                                                //为数据生成一个ID
int randomInt(int max);                                                        //区间随机数
LinkedList<pwDataGroup> sortLinkedTime(bool ascending = true);                 //链表转数组排序
void addData(const std::string& username,
			 const std::string& url,
			 const std::string& password,
			 const std::string& email,
			 const std::vector <std::string>&custom_sets
			);                                                                 //添加一条passwordData

#endif // !PSM_HPP

