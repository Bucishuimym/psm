# psm

一个 Windows 平台上的命令行密码管理器。支持 AES-256-GCM 加密、启动密钥、删除回收站与审计日志，并可以作为 MCP（Model Context Protocol）stdio 服务端被 AI 客户端调用。

当前版本：`0.0.1`

## 功能特性

- 新增 / 更新密码条目：按 `url + username` 自动匹配，存在则更新，否则新增
- 列表展示：时间列、倒序、按时间排序、JSON 输出、明文密码
- 受保护条目：新增时用 `-O` 标记，删除时必须显式 `-f`
- 删除：按 uid 删除、批量删除无密码条目、逐条确认、详细输出
- 删除记录写入 JSON 回收站 `remove.json` 和 txt 审计日志 `log.txt`
- 启动密钥初始化 / 更换：自动用旧密码解密、新密码重新加密已有条目
- 启动密钥文件使用 Windows DPAPI 保护，并带 AES-GCM verifier 校验错误密码
- MCP stdio 服务端：`psm_list` / `psm_add` / `psm_get` / `psm_remove`

## 环境要求

- Windows 10 / 11 x64
- Visual Studio 2022 或更新版本（项目当前使用 `v145` 工具集；旧版本需在项目属性里调整 `PlatformToolset`）
- Windows SDK
- 已 vendored 的依赖：
  - `CLI11.hpp`
  - `json.hpp`（nlohmann/json）
  - `termcolor.hpp`
  - `third_party/openssl`（OpenSSL 4.0.3 libcrypto）

## 构建

```bash
git clone https://github.com/Bucishuimym/psm.git
```

命令行构建：

```bat
msbuild psm.vcxproj /p:Configuration=Debug   /p:Platform=x64 /m
msbuild psm.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

产物：

```text
x64/Debug/psm.exe      + libcrypto-4-x64.dll
x64/Release/psm.exe    + libcrypto-4-x64.dll
```

`libcrypto-4-x64.dll` 由项目里的 PostBuild 事件自动复制到输出目录。

加密自测：

```bat
psm --crypto-selftest list
```

> 当前 CLI 配置了 `require_subcommand(1)`，所以 `--crypto-selftest` 需要跟一个子命令。

## 快速开始

```bat
psm init
psm add --url https://github.com -u alice -k "my-password" -e alice@example.com
:: -O 设置OP参数
psm add --url https://bank.example -u bob -k "bank-password" -O
::查看
psm list
psm ls -p -T
psm list -j
psm list -s time -r
::删除
psm rm -u 12345 -v
psm rm -u 12345 -f
psm rm -d -i
:: 作为 MCP 服务端运行
psm mcp
```

## 命令说明

### `psm list` / `psm ls`

| 参数 | 说明 |
|---|---|
| `-T` | 输出 `updated_at` 列 |
| `-r, --reverse` | 倒序输出 |
| `-p, --pass` | 解密并显示明文密码 |
| `-j, --json` | 输出 JSON 数组 |
| `-s, --sort <name>` | 排序，目前支持 `time` |

### `psm add`

| 参数 | 说明 |
|---|---|
| `--url <url>` | 必填，条目网址 |
| `-u, --username <name>` | 用户名 |
| `-k, --key <password>` | 明文密码，写入前加密 |
| `-e, --email <email>` | 邮箱 |
| `-s, --set key=value` | 自定义字段，可重复 |
| `-O, --op` | 标记为受保护条目 |

同名 `url + username` 已存在时会更新该条目；否则新增。

### `psm rm`

| 参数 | 说明 |
|---|---|
| `-u, --id <uid>` | 按 uid 删除 |
| `-d` | 批量删除没有密码的条目 |
| `-i` | 删除前逐条 `y/N` 确认 |
| `-f` | 允许删除受保护条目（`op == true`） |
| `-v` | 打印被删除条目的详情 |

### `psm init`

- 交互输入两次新主密码，空密码或两次不一致会失败。
- 如果已有数据，会先用旧密码解密所有 `key`，再用新密码重新加密。
- 主密码文件写入数据文件所在目录的 `masterpsm.txt`。
- 支持管道输入，便于自动化：
  ```bat
  echo new-password> pwd.txt
  echo new-password>> pwd.txt
  type pwd.txt | psm init
  del pwd.txt
  ```

### `psm mcp`

以 MCP stdio 服务端模式运行，详见下文。

### `psm --version`

输出 `psm 0.0.1`。

命令行选项：

```bat
psm [--version] [--crypto-selftest] <list|add|rm|init|mcp> ...
```

## 启动密钥

优先级：

1. 环境变量 `PSM_MASTER_PASSWORD`（UTF-8）
2. 数据文件同目录的 `masterpsm.txt`

`masterpsm.txt` 格式：

```text
#psm-master-v2
<Base64(DPAPI(主密码))>
<Base64(AES-GCM verifier)>
```

- 密钥由 Windows DPAPI 保护，绑定当前 Windows 用户，复制到其他用户/机器无法解密。
- verifier 用于检测错误密码：环境变量与文件不一致、或文件被篡改时会直接报错并退出。
- 旧版明文 `masterpsm.txt` 会在读取时自动升级为新格式。
- 没有 `masterpsm.txt` 且没有环境变量时，除 `init` 外的命令会提示先运行 `psm init`。

## 数据文件

| 文件 | 说明 | 位置 |
|---|---|---|
| `data.json` | 主数据，密码以 AES-256-GCM 密文保存 | `PSM_DATA_FILE` / `PW_DATA_FILE` 指定，默认当前目录 |
| `masterpsm.txt` | DPAPI 保护的主密码 + verifier | 数据文件同目录 |
| `remove.json` | 被删除条目的 JSON 回收站 | 当前工作目录 |
| `log.txt` | 被删除条目的 txt 审计日志 | 当前工作目录 |

`log.txt` 为制表符分隔，首行 `#uid op account url email updated_at key custom`，字段内的 `\t`、`\n`、`\r`、`\\` 以及自定义字段中的 `;`、`=` 会转义。

环境变量：

```bat
set PSM_DATA_FILE=K:\C++\psm\data.json
set PSM_MASTER_PASSWORD=your-master-password
psm list
```

## MCP 服务端

`psm mcp` 使用 stdio 传输：

- stdin / stdout 承载换行分隔的 JSON-RPC 2.0 消息
- stdout 只输出协议消息，日志全部走 stderr
- 支持协议版本：`2024-11-05`、`2025-03-26`、`2025-06-18`（默认 `2025-06-18`）
- 生命周期：`initialize` → `notifications/initialized` → `tools/list` / `tools/call`
- 未收到 `notifications/initialized` 前调用工具会返回 `-32002 Server not initialized`

工具列表：

| 工具 | 参数 | 说明 |
|---|---|---|
| `psm_list` | `with_password: bool` | 列出条目，默认不返回明文密码 |
| `psm_add` | `url`（必填）、`username`、`password`、`email`、`custom`（对象）、`op` | 新增或更新条目 |
| `psm_get` | `uid`（必填）、`with_password: bool` | 按 uid 查询 |
| `psm_remove` | `uid`、`empty_only`、`force` | 删除条目 |

客户端配置示例（Claude Desktop 等使用 `mcpServers`；ZCode 使用 `mcp.servers`，字段相同）：

```json
{
  "mcpServers": {
    "psm": {
      "type": "stdio",
      "command": "K:\\C++\\psm\\x64\\Release\\psm.exe",
      "args": ["mcp"],
      "env": {
        "PSM_MASTER_PASSWORD": "your-master-password",
        "PSM_DATA_FILE": "K:\\C++\\psm\\data.json"
      }
    }
  }
}
```

> 配置了 `PSM_MASTER_PASSWORD` 时，它必须与 `masterpsm.txt` 里的 verifier 匹配，否则服务端会直接退出。

## 加密说明

- 对称加密：AES-256-GCM
- 密钥派生：PBKDF2-HMAC-SHA256，600000 次迭代，每条记录随机 16 字节 salt
- 每条记录随机 12 字节 nonce，16 字节认证标签
- AAD：`psm-pw-v1`
- 密文与参数以 Base64 字符串写入 `data.json`
- 主密码 verifier 使用独立 AAD：`psm-verifier-v1`

## 安全提示

- `psm add -k <password>` 会把明文密码暴露在命令行历史/进程列表中，共享机器上请谨慎使用。
- `psm list -p`、MCP 的 `with_password=true` 会把明文交给终端或 MCP 客户端（包括模型）。
- 环境变量对同一用户下的其他进程可见。
- `data.json`、`masterpsm.txt`、`remove.json`、`log.txt` 已在 `.gitignore` 中排除，请勿提交。
- DPAPI 保护的主密码文件不能跨 Windows 用户或机器使用。

## 项目结构

```text
Main.cpp             CLI 入口
psm.cpp / psm.hpp    数据读写、主密码、txt 日志、addData
pwDataGroup.cpp/.h   密码条目模型与 JSON 序列化
ListH.h              泛型链表
AesGcm.cpp / .h      AES-256-GCM + PBKDF2 实现
CommandBox.cpp/.h    list / rm 命令实现
McpServer.cpp/.h     MCP stdio 服务端（namespace psm::mcp）
third_party/openssl  vendored OpenSSL
CLI11.hpp / json.hpp / termcolor.hpp
```

## 打包发布

最小发布包：

```text
psm-0.0.1-win64/
├── psm.exe
├── libcrypto-4-x64.dll
└── README.md
```

## 外部依赖库源码地址
1.[OpenSSL](https://github.com/openssl/openssl)\
2.[CLI11](https://github.com/CLIUtils/CLI11)\
3.[nlohmann/json](https://github.com/nlohmann/json)\
4.[termcolor](https://github.com/ikalnytskyi/termcolor/blob/master/include/termcolor/termcolor.hpp)
