#pragma once
#ifndef COMMANDBOX_H
#define COMMANDBOX_H

struct ListBox {
	bool show_T = false;                                                       //输出时间
	bool show_time = false;                                                    //按时间排序
	bool show_reverse = false;                                                 //倒序输出
	bool show_pass = false;                                                    //明文输出
	bool output_json = false;                                                  //输出json
};
void ListCommand(const ListBox& opts,LinkedList<pwDataGroup>& newData, bool ascending = true);                                                            //List指令盒子操作函数

#endif // !COMMANDBOX_H
