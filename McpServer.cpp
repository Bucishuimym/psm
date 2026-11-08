#include "McpServer.h"

#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "AesGcm.h"
#include "json.hpp"
#include "ListH.h"
#include "psm.hpp"
#include "pwDataGroup.h"

namespace psm::mcp {

using nlohmann::json;
using namespace std;
using namespace linked;

json ToolsList();
json CallTool(const json& params);

namespace {

	constexpr const char* kServerVersion = "0.0.1";
	constexpr const char* kDefaultProtocolVersion = "2025-06-18";

	string g_dataFile = DATA_FILE;
	bool g_initialized = false;                                                //握手状态

	class stdoutTostderr {                                                     //stdout重定向
	private:
		streambuf* old_;
	public:
		stdoutTostderr() :old_(cout.rdbuf(cerr.rdbuf())) {}                    //保存并重定向
		~stdoutTostderr() { cout.rdbuf(old_); }                                //恢复stdout
	};

    void Send(const json& message);                                            //消息输出
    void SendResult(const json& id, const json& result);                       //结果消息
    void SendError(const json& id, int code, const string& message);           //报错消息
    json TextResult(const string& text);                                       //文本结果
    json ToolError(const string& text);                                        //工具错误
    string LogPath();                                                          //日志文件路径拼接
    json EntryToJson(const pwDataGroup& data, bool withPassWord);              //条目转json
    bool HasId(const json& message);                                           //判断是否请求
    void HandleMessage(const json& message);                                   //处理消息

	void Send(const json& message) {                                          
		cout << message.dump() << '\n' << flush;
	}

	void SendResult(const json& id, const json& result) {                      
		Send(json{ {"jsonrpc","2.0"},{"id",id},{"result",result} });           //基础消息
	}

	void SendError(const json& id, int code, const string& message) {          
		Send(json{ {"jsonrpc","2.0"},
			{"id",id},
			{"error",json{{"code",code},{"message",message}}}});
	}

	json TextResult(const string& text) {                                      
		return json{ {"content",json::array({json{{"type","text"},{"text",text}}})}};
	}

	json ToolError(const string& text) {                                       
		json result = TextResult(text);
		result["isError"] = true;

		return result;
	}

	string LogPath() {       
		filesystem::path p(g_dataFile);
		filesystem::path dir =
			p.has_parent_path() ? p.parent_path() : filesystem::path(".");

		return (dir / LOG_FILE).string();
	}

	json EntryToJson(const pwDataGroup& data, bool withPassWord) {            
		json item;
		item["username"] = data.GetAccount();
		item["url"] = data.GetUrl();
		item["email"] = data.GetEmail();
		item["updated_at"] = data.GetUpdated();
		item["custom"] = data.GetCustomCom();
		item["op"] = data.GetOp();
		item["uid"] = data.GetUid();

		if (withPassWord && !data.GetKey().empty()) {
			string plain;
			string err;
			if (psm::crypto::DecryptFromString(
				g_master_password,
				data.GetKey(),
				"psm-pw-v1",
				plain,
				err
			)) {
				item["password"] = plain;
			}
			else {
				item["password_error"] = err;
			}
		}

		return item;
	}

	bool HasId(const json& message) {                                        
		return message.contains("id") && !message["id"].is_null();
	}

	void HandleMessage(const json& message) {     
		if (!message.is_object()) {
			SendError(nullptr, -32600, "Invalid Request");
			return;
		}

		if (!message.contains("jsonrpc") || !message["jsonrpc"].is_string() ||
			message["jsonrpc"].get<std::string>() != "2.0") {
			if (HasId(message)) {
				SendError(message["id"], -32600,
					"Invalid Request: jsonrpc must be \"2.0\"");
			}
			return;
		}

		if (!message.contains("method") || !message["method"].is_string()) {
			if (HasId(message)) {
				SendError(message["id"], -32600,
					"Invalid Request: method must be a string");
			}
			return;
		}

		const std::string method = message["method"].get<std::string>();
		const bool hasId = HasId(message);

		json params = json::object();
		if (message.contains("params") && message["params"].is_object()) {
			params = message["params"];
		}

		if (method == "initialize") {
			if (!message.contains("params") || !message["params"].is_object()) {
				if (hasId) {
					SendError(message["id"], -32602,
						"Invalid params: params must be an object");
				}
				return;
			}

			const json& rawParams = message["params"];
			if (!rawParams.contains("protocolVersion") ||
				!rawParams["protocolVersion"].is_string()) {
				if (hasId) {
					SendError(message["id"], -32602,
						"Invalid params: protocolVersion must be a string");
				}
				return;
			}

			const std::string requested =
				rawParams["protocolVersion"].get<std::string>();

			static const std::set<std::string> supported = {
				"2024-11-05", "2025-03-26", "2025-06-18"
			};
			const std::string version =
				supported.count(requested) ? requested : kDefaultProtocolVersion;

			json result = {
				{"protocolVersion", version},
				{"capabilities", {{"tools", {{"listChanged", false}}}}},
				{"serverInfo", {{"name", "psm"}, {"version", kServerVersion}}}
			};
			if (hasId) {
				g_initialized = false;   //等待 notifications/initialized
				SendResult(message["id"], result);
			}
			return;
		}

		if (method == "notifications/initialized") {
			g_initialized = true;
			return;
		}

		if (method == "notifications/cancelled") {
			return;
		}

		if (!hasId) {
			return; //其他通知不回复
		}

		if (method == "ping") {
			SendResult(message["id"], json::object());
		}
		else if (method == "tools/list" || method == "tools/call") {
			if (!g_initialized) {
				SendError(message["id"], -32002, "Server not initialized");
				return;
			}

			if (method == "tools/list") {
				SendResult(message["id"], ToolsList());
			}
			else {
				SendResult(message["id"], CallTool(params));
			}
		}
		else {
			SendError(message["id"], -32601, "Method not found: " + method);
		}
	}

} // namespace

json ToolsList() {                                                         //工具清单
    static const json tools = json::parse(R"JSON(
    {
      "tools": [
        {
          "name": "psm_list",
          "description": "List password entries. Passwords are hidden unless with_password=true.",
          "inputSchema": {
            "type": "object",
            "properties": {
              "with_password": {"type": "boolean", "default": false}
            }
          }
        },
        {
          "name": "psm_add",
          "description": "Add an entry, or update the entry matched by url + username.",
          "inputSchema": {
            "type": "object",
            "properties": {
              "url": {"type": "string"},
              "username": {"type": "string"},
              "password": {"type": "string"},
              "email": {"type": "string"},
              "custom": {"type": "object", "additionalProperties": {"type": "string"}},
              "op": {"type": "boolean", "default": false}
            },
            "required": ["url"]
          }
        },
        {
          "name": "psm_get",
          "description": "Get one entry by uid. Password is hidden unless with_password=true.",
          "inputSchema": {
            "type": "object",
            "properties": {
              "uid": {"type": "integer"},
              "with_password": {"type": "boolean", "default": false}
            },
            "required": ["uid"]
          }
        },
        {
          "name": "psm_remove",
          "description": "Remove entries by uid, or all entries without a password when empty_only=true.",
          "inputSchema": {
            "type": "object",
            "properties": {
              "uid": {"type": "integer"},
              "empty_only": {"type": "boolean", "default": false},
              "force": {"type": "boolean", "default": false}
            }
          }
        }
      ]
    }
    )JSON");
    return tools;
}

json ToolList(const json& args) {       
    const bool withPassword = args.value("with_password", false);
    json entries = json::array();
    for (int i = 1; i <= pw_data.GetLength(); i++) {
        entries.push_back(EntryToJson(pw_data.GetRef(i), withPassword));
    }
    return TextResult(entries.dump(2));
}

json ToolAdd(const json& args) {  
    if (!args.contains("url") || !args["url"].is_string()) {
        return ToolError("url is required");
    }

    const std::string url = args["url"].get<std::string>();
    const std::string username = args.value("username", std::string(""));
    const std::string password = args.value("password", std::string(""));
    const std::string email = args.value("email", std::string(""));
    const bool op = args.value("op", false);

    std::vector<std::string> custom;
    if (args.contains("custom") && args["custom"].is_object()) {
        for (auto it = args["custom"].begin(); it != args["custom"].end(); ++it) {
            if (!it.value().is_string()) {
                return ToolError("custom values must be strings");
            }
            custom.push_back(it.key() + "=" + it.value().get<std::string>());
        }
    }

    {
        stdoutTostderr guard;
        addData(username, url, password, email, custom, op);
    }
    SaveData(pw_data, g_dataFile);

    for (int i = 1; i <= pw_data.GetLength(); i++) {
        const pwDataGroup& data = pw_data.GetRef(i);
        if (data.GetUrl() == url && data.GetAccount() == username) {
            return TextResult(EntryToJson(data, false).dump(2));
        }
    }
    return TextResult("ok");
}

json ToolGet(const json& args) {  
    if (!args.contains("uid") || !args["uid"].is_number_integer()) {
        return ToolError("uid is required");
    }

    const int uid = args["uid"].get<int>();
    const bool withPassword = args.value("with_password", false);

    for (int i = 1; i <= pw_data.GetLength(); i++) {
        const pwDataGroup& data = pw_data.GetRef(i);
        if (data.GetUid() == uid) {
            return TextResult(EntryToJson(data, withPassword).dump(2));
        }
    }
    return ToolError("no data with id " + std::to_string(uid));
}

json ToolRemove(const json& args) {                                      
    const bool emptyOnly = args.value("empty_only", false);
    const bool force = args.value("force", false);
    const bool hasUid = args.contains("uid") && args["uid"].is_number_integer();

    if (!hasUid && !emptyOnly) {
        return ToolError("uid or empty_only is required");
    }
    if (pw_data.GetLength() == 0) {
        return ToolError("no data");
    }

    const int uid = hasUid ? args["uid"].get<int>() : 0;

    std::vector<int> targets;
    for (int i = 1; i <= pw_data.GetLength(); i++) {
        const pwDataGroup& data = pw_data.GetRef(i);
        if (hasUid) {
            if (data.GetUid() == uid) {
                targets.push_back(i);
            }
        }
        else if (data.GetKey().empty()) {
            targets.push_back(i);
        }
    }

    if (targets.empty()) {
        return ToolError(hasUid ? "no data with id " + std::to_string(uid)
            : "no data without password");
    }

    LinkedList<pwDataGroup> removed;
    int skipped = 0;

    for (auto it = targets.rbegin(); it != targets.rend(); ++it) {
        const int idx = *it;
        const pwDataGroup& data = pw_data.GetRef(idx);
        if (data.GetOp() && !force) {
            skipped++;
            continue;
        }
        removed.PushBack(pw_data.Delete(idx));
    }

    if (removed.GetLength() == 0) {
        return ToolError("all matched records are protected; use force=true");
    }

    SaveData(pw_data, g_dataFile);
    AddData(removed, LogPath());

    json result;
    result["removed"] = removed.GetLength();
    result["skipped_protected"] = skipped;
    return TextResult(result.dump(2));
}

json CallTool(const json& params) {         
    const std::string name = params.value("name", std::string(""));
    json args = json::object();
    if (params.contains("arguments") && params["arguments"].is_object()) {
        args = params["arguments"];
    }

    if (name == "psm_list") return ToolList(args);
    if (name == "psm_add") return ToolAdd(args);
    if (name == "psm_get") return ToolGet(args);
    if (name == "psm_remove") return ToolRemove(args);
    return ToolError("unknown tool: " + name);
}

void RunMcpServer(const std::string& dataFile) {    
    g_dataFile = dataFile;
    g_initialized = false;

    std::cerr << "[psm-mcp] ready, data=" << g_dataFile
        << ", log=" << LogPath() << std::endl;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();      //兼容 CRLF
        }
        if (line.empty()) {
            continue;
        }

        json message;
        try {
            message = json::parse(line);
        }
        catch (const std::exception& e) {
            SendError(nullptr, -32700, std::string("Parse error: ") + e.what());
            continue;
        }

        try {
            HandleMessage(message);
        }
        catch (const std::exception& e) {
            json id = nullptr;
            if (message.is_object() && HasId(message)) {
                id = message["id"];
            }
            SendError(id, -32603, std::string("Internal error: ") + e.what());
        }
    }
}

} // namespace psm::mcp
