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

void LoadData();                                                               //加载数据
void SaveData();                                                               //保存数据
void GetTime();                                                                //返回系统时间
std::string getInput(const std::string& prompt);                               //隐式输入
void ListData();                                                               //打印数据矩阵

#endif // !PSM_HPP

