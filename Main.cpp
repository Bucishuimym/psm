#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <cmath>
#include <windows.h>
#include <unordered_map>

#include "AesGcm.h"
#include "ListH.h"
#include "CLI11.hpp"
#include "json.hpp"
#include "pwDataGroup.h"
#include "psm.hpp"
#include "CommandBox.h"

using namespace std;
using namespace nlohmann;

std::vector<pwDataGroup> pw_json;
LinkedList<pwDataGroup> pw_data(nullptr, 0);

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

int main(int argc, char** argv) {
	LoadData();
	CLI::App app{ "Password mananger CLI" };
	app.require_subcommand(1);
	app.set_version_flag("--version", "psm 0.0.1");
	int exitCode = 0;

	ListBox opts;
	auto* list = app.add_subcommand("list", "List the data.");
	list->alias("ls");
	list->add_flag("-T", opts.show_T, "Print time");
	list->add_flag("-r,--reverse", opts.show_reverse, "Print in reverse order.");
	list->add_flag("-p,--pass", opts.show_pass, "Print plaintext.");
	list->add_flag("-j,--json", opts.output_json, "Print json.");
	list->add_option("-s,--sort", opts.show_sort, "Sort <name>");
	list->callback([&] {
		ListCommand(opts,pw_data);
		});

	std::string url,name,key,email;
	std::vector<std::string> custom_sets;
	auto* add = app.add_subcommand("add", "add data.");
	add->add_option("-u,--username", name, "Enter username");
	add->add_option("--url", url, "Enter url")->required();
	add->add_option("-k,--key", key, "Enter password.");
	add->add_option("-e,--email", email, "Enter email");
	add->add_option("-s,--set", custom_sets, "Custom parameters: key=value")
		->expected(-1);
	add->callback([&] {
		addData(name, url, key, email, custom_sets);
		});

	bool cryptoSelfTest = false;
	app.add_flag("--crypto-selftest", cryptoSelfTest, "运行 AES-256-GCM 自测");

	CLI11_PARSE(app, argc, argv);

	if (cryptoSelfTest) {
		return RunCryptoSelfTest() ? 0 : 1;
	}
	SaveData();
	return 0;
}