#pragma once

#ifndef PSM_AES_GCM_H
#define PSM_AES_GCM_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/*
 * AES-256-GCM 封装（OpenSSL 4.x，libcrypto）
 *
 * 两种用法：
 *   1) 原始密钥：key 必须是 32 字节，调用方自己保存密钥；每次加密自动生成随机 nonce。
 *   2) 口令模式：用 PBKDF2-HMAC-SHA256 从口令派生密钥，每次加密自动生成随机 salt 和 nonce，
 *      加密结果里带 salt / 迭代次数，解密只需要同一个口令。
 *
 * 密文二进制格式（所有整数为大端）：
 *   口令模式： "PSMGCM" | ver(1) | mode=1 | salt(16) | iter(4) | nonce(12) | tag(16) | ciphertext
 *   原始密钥： "PSMGCM" | ver(1) | mode=2 | nonce(12) | tag(16) | ciphertext
 *
 * tag 之前的所有头部字节会连同调用方传入的 aad 一起作为 GCM 的附加认证数据，
 * 因此 salt / 迭代次数 / nonce 被篡改时解密同样会失败。
 *
 * 典型用法（密码管理器的某个密码字段）：
 *
 *   std::string blob, err;
 *   if (!psm::crypto::EncryptToString(masterPassword, item.GetKey(), "psm-pw-v1", blob, err)) {
 *       std::cerr << err << std::endl;
 *   }
 *   item.SetKey(blob);                    // 数据库里存 Base64 字符串
 *
 *   std::string plain, err2;
 *   if (!psm::crypto::DecryptFromString(masterPassword, item.GetKey(), "psm-pw-v1", plain, err2)) {
 *       std::cerr << err2 << std::endl;   // 口令错误或数据被篡改
 *   }
 *
 * aad 可以传固定上下文（如 "psm-pw-v1"）或稳定不变的记录标识；
 * 解密时必须传入完全相同的值。用户名、网址这类可修改字段不适合做 aad，
 * 否则改一次字段就解不开了。
 */
namespace psm::crypto
{
using Bytes = std::vector<unsigned char>;

inline constexpr std::size_t kKeySize = 32;      // AES-256
inline constexpr std::size_t kNonceSize = 12;    // GCM 推荐的 96 位 nonce
inline constexpr std::size_t kTagSize = 16;      // 128 位认证标签
inline constexpr std::size_t kSaltSize = 16;
inline constexpr std::uint32_t kDefaultIterations = 600000; // PBKDF2-HMAC-SHA256

// 从口令派生 32 字节密钥。salt 必须为 16 字节。
bool DeriveKey(const std::string& password,
               const Bytes& salt,
               std::uint32_t iterations,
               Bytes& key,
               std::string& error);

// 原始密钥接口。key 必须正好 32 字节。aad 可以为空。
bool Encrypt(const Bytes& key,
             const Bytes& plaintext,
             const Bytes& aad,
             Bytes& blob,
             std::string& error);

bool Decrypt(const Bytes& key,
             const Bytes& blob,
             const Bytes& aad,
             Bytes& plaintext,
             std::string& error);

// 口令接口。salt 和 nonce 每次随机生成，并随密文一起保存。
bool EncryptWithPassword(const std::string& password,
                         const Bytes& plaintext,
                         const Bytes& aad,
                         Bytes& blob,
                         std::string& error);

bool DecryptWithPassword(const std::string& password,
                         const Bytes& blob,
                         const Bytes& aad,
                         Bytes& plaintext,
                         std::string& error);

// 方便把密文存进 std::string / 文本文件。
std::string Base64Encode(const Bytes& data);
bool Base64Decode(const std::string& text, Bytes& data, std::string& error);

bool EncryptToString(const std::string& password,
                     const std::string& plaintext,
                     const std::string& aad,
                     std::string& outBase64,
                     std::string& error);

bool DecryptFromString(const std::string& password,
                       const std::string& inBase64,
                       const std::string& aad,
                       std::string& plaintext,
                       std::string& error);
} // namespace psm::crypto

#endif // PSM_AES_GCM_H
