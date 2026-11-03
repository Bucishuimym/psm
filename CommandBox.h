#pragma once
#ifndef COMMANDBOX_H
#define COMMANDBOX_H

#include "ListH.h"
#include "pwDataGroup.h"
#include <iostream>

struct ListBox {
	bool show_T = false;                                                       //输出时间
	std::string show_sort = "";                                                //自定义排序（time）
	bool show_reverse = false;                                                 //倒序输出
	bool show_pass = false;                                                    //明文输出
	bool output_json = false;                                                  //输出json
};
void ListCommand(const ListBox& opts,linked::LinkedList<pwDataGroup>& newData, bool ascending = true);                                                            //List指令盒子操作函数

struct RemoveBox {
	int rm_uid = 0;                                                            //删除为<uid>的数据
	bool rm_info = false;                                                      //交互式删除
	bool rm_force = false;                                                     //删除op数据
	bool rm_verbose = false;                                                   //显示详细的删除数据
	bool rm_emptyd = false;                                                    //删除没有密码的空数据
};
void RemoveCommand(const RemoveBox& opts, linked::LinkedList<pwDataGroup>& pwdata);    //Remove指令盒子操作函数
#endif // !COMMANDBOX_H
