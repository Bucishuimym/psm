#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <cmath>
#include <windows.h>
#include <unordered_map>
#include <fstream>

#include "AesGcm.h"
#include "ListH.h"
#include "CLI11.hpp"
#include "json.hpp"
#include "pwDataGroup.h"
#include "psm.hpp"
#include "CommandBox.h"
#include "McpServer.h"
#include "termcolor.hpp"

using namespace std;
using namespace nlohmann;

std::vector<pwDataGroup> pw_json;
linked::LinkedList<pwDataGroup> pw_data(nullptr, 0);

namespace
{
bool RunCryptoSelfTest()
{
	using namespace psm::crypto;

	const std::string password = "psm-selftest-password";
	const std::string aad = "psm-aes-256-gcm-selftest";
	const std::string plaintext = "hello AES-256-GCM 密码管理器";

	std::string blob;
	std::string restored;
	std::string error;

	if (!EncryptToString(password, plaintext, aad, blob, error)) {
		std::cerr << "[FAIL] 加密失败: " << error << std::endl;
		return false;
	}

	if (!DecryptFromString(password, blob, aad, restored, error)) {
		std::cerr << "[FAIL] 解密失败: " << error << std::endl;
		return false;
	}

	if (restored != plaintext) {
		std::cerr << "[FAIL] 解密结果与原文不一致" << std::endl;
		return false;
	}

	// 篡改密文最后一个字节，GCM 的认证标签应该校验失败
	Bytes rawBlob;
	if (!Base64Decode(blob, rawBlob, error) || rawBlob.empty()) {
		std::cerr << "[FAIL] Base64 解码失败: " << error << std::endl;
		return false;
	}

	rawBlob.back() ^= 0x01;
	Bytes tampered;
	if (DecryptWithPassword(password, rawBlob, Bytes(aad.begin(), aad.end()), tampered, error)) {
		std::cerr << "[FAIL] 篡改后的密文竟然解密成功了" << std::endl;
		return false;
	}

	// 错误口令必须失败
	if (DecryptFromString("wrong-password", blob, aad, restored, error)) {
		std::cerr << "[FAIL] 错误口令竟然解密成功了" << std::endl;
		return false;
	}

	// 原始密钥接口
	const Bytes key(kKeySize, 0x5A);
	const Bytes plainBytes(plaintext.begin(), plaintext.end());
	Bytes rawKeyBlob;
	Bytes rawPlain;

	if (!Encrypt(key, plainBytes, Bytes{}, rawKeyBlob, error) ||
		!Decrypt(key, rawKeyBlob, Bytes{}, rawPlain, error)) {
		std::cerr << "[FAIL] 原始密钥接口失败: " << error << std::endl;
		return false;
	}

	if (std::string(rawPlain.begin(), rawPlain.end()) != plaintext) {
		std::cerr << "[FAIL] 原始密钥接口解密结果不一致" << std::endl;
		return false;
	}

	std::cout << "[PASS] AES-256-GCM 加解密、篡改检测、错误口令检测、原始密钥接口"
	          << std::endl;
	return true;
}
} // namespace

namespace {

std::string GetEnvUtf8(const wchar_t* name) {
	const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
	if (size == 0) {
		return std::string();
	}

	std::wstring value(size, L'\0');
	const DWORD written = GetEnvironmentVariableW(name, value.data(), size);
	if (written == 0 || written >= size) {
		return std::string();
	}
	value.resize(written);

	const int bytes = WideCharToMultiByte(
		CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
		nullptr, 0, nullptr, nullptr);
	if (bytes <= 0) {
		return std::string();
	}

	std::string result(bytes, '\0');
	WideCharToMultiByte(
		CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
		result.data(), bytes, nullptr, nullptr);
	return result;
}

} // namespace

int main(int argc, char** argv) {

	std::string dataFile = DATA_FILE;
	char* buffer = nullptr;
	size_t size = 0;

	if (_dupenv_s(&buffer, &size, "PSM_DATA_FILE") == 0 && buffer != nullptr) {
		dataFile = buffer;
		free(buffer);
		buffer = nullptr;
	}
	else if (_dupenv_s(&buffer, &size, "PW_DATA_FILE") == 0 && buffer != nullptr) {
		dataFile = buffer;
		free(buffer);
		buffer = nullptr;
	}

	g_data_file = dataFile;
	ReadMasterpsm();

	const std::string envPassword = GetEnvUtf8(L"PSM_MASTER_PASSWORD");
	if (!envPassword.empty()) {
		ApplyMasterPasswordOverride(envPassword);
	}

	const bool dataLoaded = LoadData(pw_data, dataFile);
	CLI::App app{ "Password mananger CLI" };
	app.require_subcommand(1);
	app.set_version_flag("--version", "psm v1.0.0");
	int exitCode = 0;

	auto requireData = [&]() -> bool {
		if (!dataLoaded) {
			cerr << termcolor::red
				<< "fatal: data file could not be parsed; refusing to continue.\n"
				<< termcolor::reset;
			exitCode = 1;
			return false;
		}
		return true;
	};

	auto requireMaster = [&]() -> bool {
		if (!g_master_error.empty()) {
			cerr << termcolor::red << "fatal: " << g_master_error << "\n"
				<< termcolor::reset;
			exitCode = 1;
			return false;
		}
		if (startup()) {
			cerr << termcolor::red << "fatal: master password not found.\n";
			cerr << "you can try the command: \n";
			cerr << "        psm init\n" << termcolor::reset;
			exitCode = 1;
			return false;
		}
		return true;
	};

	ListBox opts;
	auto* list = app.add_subcommand("list", "List the data.");
	list->alias("ls");
	list->add_flag("-T", opts.show_T, "Print time");
	list->add_flag("-r,--reverse", opts.show_reverse, "Print in reverse order.");
	list->add_flag("-p,--pass", opts.show_pass, "Print plaintext.");
	list->add_flag("-j,--json", opts.output_json, "Print json.");
	list->add_option("-s,--sort", opts.show_sort, "Sort <name>");
	list->callback([&] {
		if (!requireData() || !requireMaster()) {
			return;
		}
		ListCommand(opts,pw_data);
		});

	std::string url,name,key,email;
	bool op = false;
	std::vector<std::string> custom_sets;
	auto* add = app.add_subcommand("add", "add data.");
	add->add_flag("-O,--op", op, "set protection variable.");
	add->add_option("-u,--username", name, "Enter username");
	add->add_option("--url", url, "Enter url")->required();
	add->add_option("-k,--key", key, "Enter password.");
	add->add_option("-e,--email", email, "Enter email");
	add->add_option("-s,--set", custom_sets, "Custom parameters: key=value")
		->expected(-1);
	add->callback([&] {
		if (!requireData() || !requireMaster()) {
			return;
		}
		addData(name, url, key, email, custom_sets,op);
		});

	RemoveBox opts_rm;
	auto* remove = app.add_subcommand("rm", "remove data.");
	remove->add_option("-u,--id",opts_rm.rm_uid,"remove <id> data.");
	remove->add_flag("-i", opts_rm.rm_info, "Interactive deletion.");
	remove->add_flag("-f", opts_rm.rm_force, "remove protection variable.");
	remove->add_flag("-v", opts_rm.rm_verbose, "show deletion process.");
	remove->add_flag("-d", opts_rm.rm_emptyd, "password to remove empty data.");
	remove->callback([&] {
		if (!requireData() || !requireMaster()) {
			return;
		}
		RemoveCommand(opts_rm, pw_data);
		});


	auto* mcp = app.add_subcommand("mcp", "Run as an MCP stdio server.");
	mcp->callback([&] {
		if (!requireData() || !requireMaster()) {
			return;
		}
		psm::mcp::RunMcpServer(dataFile);
		});

	auto* init = app.add_subcommand("init", "Init the master password.");
	init->callback([&] {
		if (!requireData()) {
			return;
		}
		if (!initMasterpsm()) {
			exitCode = 1;
		}
		});

	bool cryptoSelfTest = false;
	app.add_flag("--crypto-selftest", cryptoSelfTest, "Run AES-256-GCM Testing");

	CLI11_PARSE(app, argc, argv);

	if (cryptoSelfTest) {
		return RunCryptoSelfTest() ? 0 : 1;
	}
	if (dataLoaded) {
		SaveData(pw_data, dataFile);
	}
	return exitCode;
}