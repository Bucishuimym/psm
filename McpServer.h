#pragma once
#ifndef MCPSERVER_H
#define MCPSERVER_H

#include <string>

#include "json.hpp"

namespace psm::mcp {

void RunMcpServer(const std::string& dataFile);

nlohmann::json ToolsList();                                                    //工具清单
nlohmann::json CallTool(const nlohmann::json& params);                         //分发工具调用
nlohmann::json ToolList(const nlohmann::json& args);                           //列出条目
nlohmann::json ToolAdd(const nlohmann::json& args);                            //新增条目
nlohmann::json ToolGet(const nlohmann::json& args);                            //查询条目
nlohmann::json ToolRemove(const nlohmann::json& args);                         //删除条目
void RunMcpServer(const std::string& dataFile);                                //MCP循环

} // namespace psm::mcp

#endif // !MCPSERVER_H
