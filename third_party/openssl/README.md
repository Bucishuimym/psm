# OpenSSL 4.0.3 (Windows x64)

本目录是项目自带的 OpenSSL 运行/链接文件，只包含 AES-256-GCM 需要的 `libcrypto`，
不包含 `libssl`（项目没有使用 TLS）。

## 版本与构建

- 版本：OpenSSL 4.0.3
- 源码标识：`openssl-4.0.3-0-gaf1775b60d`（见 `version.txt`）
- 平台：Windows x64
- 构建目标：`VC-WIN64A`
- 运行库：`/MD`（Release CRT）
- 来源：FireDaemon 的 OpenSSL 4.0.3 预编译包
  <https://download.firedaemon.com/FireDaemon-OpenSSL/openssl-4.0.3.zip>
- 官方发布页：<https://github.com/openssl/openssl/releases/tag/openssl-4.0.3>

## 包含的文件

```text
include/openssl/                  OpenSSL 公共头文件
lib/VC/x64/MD/libcrypto.lib       MSVC x64 导入库
bin/libcrypto-4-x64.dll           x64 运行库
LICENSE.txt                       Apache License 2.0
version.txt                       源码版本标识
```

没有包含：`libssl.lib`、`libssl-4-x64.dll`、`*.pdb`、`openssl.exe`、
`fips.dll`、`legacy.dll`。这些文件当前项目都用不到。

## 校验和（SHA-256）

```text
199656dbdfe8d2b9dd0db8abd44fedba8e61bfcb98b712f2ba63c5d99e858c8f  bin/libcrypto-4-x64.dll
3bf902dafc2711ef12f7a4b63c527b0eaa9c8c1b669b3c6856f2c08dd9c979f9  lib/VC/x64/MD/libcrypto.lib
```

## 构建时的使用方式

- 头文件目录：`third_party/openssl/include`
- 导入库目录：`third_party/openssl/lib/VC/x64/MD`
- 链接方式：`AesGcm.cpp` 中的 `#pragma comment(lib, "libcrypto.lib")`，
  构建系统需要把上面的导入库目录加入库搜索路径
- 运行时：把 `bin/libcrypto-4-x64.dll` 复制到可执行文件所在目录

## 升级步骤

1. 用相同的构建目标（`VC-WIN64A`、x64、`/MD`）重新构建 OpenSSL，或取得新的预编译包。
2. 替换 `include/openssl/`、`lib/VC/x64/MD/libcrypto.lib` 和 `bin/libcrypto-4-x64.dll`。
3. 更新本文件中的版本、来源和 SHA-256。
4. 重新构建并运行 `psm.exe --crypto-selftest`，确认加解密、篡改检测和错误口令检测都通过。

## 许可证

OpenSSL 4.x 使用 Apache License 2.0，完整文本见同目录的 `LICENSE.txt`。
