#include "AesGcm.h"

#include <climits>
#include <cstring>
#include <memory>
#include <utility>

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

// 库目录由项目配置为 third_party/openssl/lib/VC/x64/MD
#pragma comment(lib, "libcrypto.lib")

namespace psm::crypto
{
namespace
{
constexpr unsigned char kMagic[6] = { 'P', 'S', 'M', 'G', 'C', 'M' };
constexpr unsigned char kVersion = 1;
constexpr unsigned char kModePassword = 1;
constexpr unsigned char kModeRawKey = 2;
constexpr std::uint32_t kMaxIterations = 10000000;

constexpr std::size_t kBaseHeaderSize = sizeof(kMagic) + 2; // magic + version + mode

struct CipherCtxDeleter
{
    void operator()(EVP_CIPHER_CTX* ctx) const noexcept
    {
        EVP_CIPHER_CTX_free(ctx);
    }
};

using CipherCtx = std::unique_ptr<EVP_CIPHER_CTX, CipherCtxDeleter>;

// 析构时擦除临时密钥，避免口令派生出的密钥长时间留在堆上。
class ScopedCleanse
{
public:
    ScopedCleanse(unsigned char* data, std::size_t size) noexcept
        : data_(data), size_(size)
    {
    }

    ~ScopedCleanse()
    {
        if (data_ != nullptr && size_ != 0)
            OPENSSL_cleanse(data_, size_);
    }

    ScopedCleanse(const ScopedCleanse&) = delete;
    ScopedCleanse& operator=(const ScopedCleanse&) = delete;

private:
    unsigned char* data_;
    std::size_t size_;
};

std::string OpensslError(const char* what)
{
    const unsigned long code = ERR_get_error();
    if (code == 0)
        return std::string(what) + " 失败";

    char text[256] = {};
    ERR_error_string_n(code, text, sizeof(text));
    return std::string(what) + " 失败: " + text;
}

bool FillRandom(unsigned char* data, std::size_t size)
{
    if (size > static_cast<std::size_t>(INT_MAX))
        return false;
    return RAND_bytes(data, static_cast<int>(size)) == 1;
}

void AppendU32(Bytes& out, std::uint32_t value)
{
    out.push_back(static_cast<unsigned char>((value >> 24) & 0xFF));
    out.push_back(static_cast<unsigned char>((value >> 16) & 0xFF));
    out.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
    out.push_back(static_cast<unsigned char>(value & 0xFF));
}

std::uint32_t ReadU32(const unsigned char* data)
{
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

struct ParsedBlob
{
    unsigned char mode = 0;
    Bytes salt;
    std::uint32_t iterations = 0;
    const unsigned char* nonce = nullptr;
    const unsigned char* tag = nullptr;
    const unsigned char* ciphertext = nullptr;
    std::size_t ciphertextSize = 0;
    std::size_t headerSize = 0; // magic 到 nonce 结束，不含 tag
};

bool ParseBlob(const Bytes& blob, ParsedBlob& parsed, std::string& error)
{
    if (blob.size() < kBaseHeaderSize + kNonceSize + kTagSize)
    {
        error = "密文长度不足";
        return false;
    }

    if (std::memcmp(blob.data(), kMagic, sizeof(kMagic)) != 0)
    {
        error = "密文格式不正确（magic 不匹配）";
        return false;
    }

    if (blob[6] != kVersion)
    {
        error = "不支持的密文版本";
        return false;
    }

    parsed.mode = blob[7];
    std::size_t offset = kBaseHeaderSize;

    if (parsed.mode == kModePassword)
    {
        if (blob.size() < kBaseHeaderSize + kSaltSize + 4 + kNonceSize + kTagSize)
        {
            error = "密文长度不足";
            return false;
        }

        parsed.salt.assign(blob.begin() + static_cast<std::ptrdiff_t>(kBaseHeaderSize),
                           blob.begin() + static_cast<std::ptrdiff_t>(kBaseHeaderSize + kSaltSize));
        offset = kBaseHeaderSize + kSaltSize;
        parsed.iterations = ReadU32(blob.data() + offset);
        offset += 4;

        if (parsed.iterations == 0 || parsed.iterations > kMaxIterations)
        {
            error = "密文中的迭代次数不合法";
            return false;
        }
    }
    else if (parsed.mode != kModeRawKey)
    {
        error = "不支持的密文模式";
        return false;
    }

    parsed.headerSize = offset + kNonceSize;
    if (blob.size() < parsed.headerSize + kTagSize)
    {
        error = "密文长度不足";
        return false;
    }

    parsed.nonce = blob.data() + offset;
    parsed.tag = blob.data() + parsed.headerSize;
    parsed.ciphertext = parsed.tag + kTagSize;
    parsed.ciphertextSize = blob.size() - parsed.headerSize - kTagSize;
    return true;
}

bool AeadEncrypt(const Bytes& key,
                 const unsigned char* nonce,
                 const Bytes& aad,
                 const Bytes& plaintext,
                 unsigned char* tag,
                 Bytes& ciphertext,
                 std::string& error)
{
    if (plaintext.size() > static_cast<std::size_t>(INT_MAX) ||
        aad.size() > static_cast<std::size_t>(INT_MAX))
    {
        error = "明文或附加数据过长";
        return false;
    }

    CipherCtx ctx(EVP_CIPHER_CTX_new());
    if (!ctx)
    {
        error = OpensslError("EVP_CIPHER_CTX_new");
        return false;
    }

    if (EVP_EncryptInit_ex2(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
    {
        error = OpensslError("EVP_EncryptInit_ex2");
        return false;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN,
                            static_cast<int>(kNonceSize), nullptr) != 1)
    {
        error = OpensslError("EVP_CTRL_GCM_SET_IVLEN");
        return false;
    }

    if (EVP_EncryptInit_ex2(ctx.get(), nullptr, key.data(), nonce, nullptr) != 1)
    {
        error = OpensslError("EVP_EncryptInit_ex2(key/nonce)");
        return false;
    }

    int len = 0;
    if (!aad.empty())
    {
        if (EVP_EncryptUpdate(ctx.get(), nullptr, &len, aad.data(),
                              static_cast<int>(aad.size())) != 1)
        {
            error = OpensslError("EVP_EncryptUpdate(aad)");
            return false;
        }
    }

    ciphertext.assign(plaintext.size() + EVP_MAX_BLOCK_LENGTH, 0);

    int outLen = 0;
    if (!plaintext.empty())
    {
        if (EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &outLen, plaintext.data(),
                              static_cast<int>(plaintext.size())) != 1)
        {
            error = OpensslError("EVP_EncryptUpdate");
            return false;
        }
    }

    int finalLen = 0;
    if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + outLen, &finalLen) != 1)
    {
        error = OpensslError("EVP_EncryptFinal_ex");
        return false;
    }
    outLen += finalLen;
    ciphertext.resize(static_cast<std::size_t>(outLen));

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG,
                            static_cast<int>(kTagSize), tag) != 1)
    {
        error = OpensslError("EVP_CTRL_GCM_GET_TAG");
        return false;
    }

    return true;
}

bool AeadDecrypt(const Bytes& key,
                 const unsigned char* nonce,
                 const unsigned char* tag,
                 const Bytes& aad,
                 const unsigned char* ciphertext,
                 std::size_t ciphertextSize,
                 Bytes& plaintext,
                 std::string& error)
{
    if (ciphertextSize > static_cast<std::size_t>(INT_MAX) ||
        aad.size() > static_cast<std::size_t>(INT_MAX))
    {
        error = "密文或附加数据过长";
        return false;
    }

    CipherCtx ctx(EVP_CIPHER_CTX_new());
    if (!ctx)
    {
        error = OpensslError("EVP_CIPHER_CTX_new");
        return false;
    }

    if (EVP_DecryptInit_ex2(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
    {
        error = OpensslError("EVP_DecryptInit_ex2");
        return false;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN,
                            static_cast<int>(kNonceSize), nullptr) != 1)
    {
        error = OpensslError("EVP_CTRL_GCM_SET_IVLEN");
        return false;
    }

    if (EVP_DecryptInit_ex2(ctx.get(), nullptr, key.data(), nonce, nullptr) != 1)
    {
        error = OpensslError("EVP_DecryptInit_ex2(key/nonce)");
        return false;
    }

    int len = 0;
    if (!aad.empty())
    {
        if (EVP_DecryptUpdate(ctx.get(), nullptr, &len, aad.data(),
                              static_cast<int>(aad.size())) != 1)
        {
            error = OpensslError("EVP_DecryptUpdate(aad)");
            return false;
        }
    }

    plaintext.assign(ciphertextSize + EVP_MAX_BLOCK_LENGTH, 0);

    int outLen = 0;
    if (ciphertextSize != 0)
    {
        if (EVP_DecryptUpdate(ctx.get(), plaintext.data(), &outLen, ciphertext,
                              static_cast<int>(ciphertextSize)) != 1)
        {
            error = OpensslError("EVP_DecryptUpdate");
            plaintext.clear();
            return false;
        }
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG,
                            static_cast<int>(kTagSize),
                            const_cast<unsigned char*>(tag)) != 1)
    {
        error = OpensslError("EVP_CTRL_GCM_SET_TAG");
        plaintext.clear();
        return false;
    }

    int finalLen = 0;
    if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + outLen, &finalLen) != 1)
    {
        plaintext.clear();
        error = "解密失败：口令错误或数据已被篡改";
        return false;
    }
    outLen += finalLen;
    plaintext.resize(static_cast<std::size_t>(outLen));
    return true;
}

bool Seal(const Bytes& key,
          unsigned char mode,
          const Bytes& salt,
          std::uint32_t iterations,
          const Bytes& plaintext,
          const Bytes& userAad,
          Bytes& blob,
          std::string& error)
{
    if (mode == kModePassword)
    {
        if (salt.size() != kSaltSize)
        {
            error = "salt 必须为 16 字节";
            return false;
        }
    }
    else if (mode != kModeRawKey)
    {
        error = "不支持的加密模式";
        return false;
    }

    Bytes nonce(kNonceSize);
    if (!FillRandom(nonce.data(), nonce.size()))
    {
        error = OpensslError("RAND_bytes");
        return false;
    }

    Bytes header;
    header.reserve(kBaseHeaderSize + kSaltSize + 4 + kNonceSize);
    header.insert(header.end(), kMagic, kMagic + sizeof(kMagic));
    header.push_back(kVersion);
    header.push_back(mode);

    if (mode == kModePassword)
    {
        header.insert(header.end(), salt.begin(), salt.end());
        AppendU32(header, iterations);
    }

    header.insert(header.end(), nonce.begin(), nonce.end());

    // 头部 + 调用方附加数据一起参与认证。
    Bytes aad = header;
    aad.insert(aad.end(), userAad.begin(), userAad.end());

    Bytes ciphertext;
    unsigned char tag[kTagSize] = {};
    if (!AeadEncrypt(key, nonce.data(), aad, plaintext, tag, ciphertext, error))
        return false;

    blob.clear();
    blob.reserve(header.size() + kTagSize + ciphertext.size());
    blob.insert(blob.end(), header.begin(), header.end());
    blob.insert(blob.end(), tag, tag + kTagSize);
    blob.insert(blob.end(), ciphertext.begin(), ciphertext.end());
    return true;
}

bool Open(const Bytes& key,
          const Bytes& blob,
          const Bytes& userAad,
          Bytes& plaintext,
          std::string& error)
{
    ParsedBlob parsed;
    if (!ParseBlob(blob, parsed, error))
        return false;

    Bytes aad(blob.begin(), blob.begin() + static_cast<std::ptrdiff_t>(parsed.headerSize));
    aad.insert(aad.end(), userAad.begin(), userAad.end());

    return AeadDecrypt(key, parsed.nonce, parsed.tag, aad,
                       parsed.ciphertext, parsed.ciphertextSize, plaintext, error);
}
} // namespace

bool DeriveKey(const std::string& password,
               const Bytes& salt,
               std::uint32_t iterations,
               Bytes& key,
               std::string& error)
{
    error.clear();

    if (salt.size() != kSaltSize)
    {
        error = "salt 必须为 16 字节";
        return false;
    }

    if (iterations == 0 || iterations > kMaxIterations)
    {
        error = "迭代次数不合法";
        return false;
    }

    if (password.size() > static_cast<std::size_t>(INT_MAX))
    {
        error = "口令过长";
        return false;
    }

    Bytes derived(kKeySize);
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                          salt.data(), static_cast<int>(salt.size()),
                          static_cast<int>(iterations), EVP_sha256(),
                          static_cast<int>(kKeySize), derived.data()) != 1)
    {
        error = OpensslError("PKCS5_PBKDF2_HMAC");
        if (!derived.empty())
            OPENSSL_cleanse(derived.data(), derived.size());
        return false;
    }

    key.swap(derived);
    if (!derived.empty())
        OPENSSL_cleanse(derived.data(), derived.size());
    return true;
}

bool Encrypt(const Bytes& key,
             const Bytes& plaintext,
             const Bytes& aad,
             Bytes& blob,
             std::string& error)
{
    error.clear();
    blob.clear();

    if (key.size() != kKeySize)
    {
        error = "AES-256 需要 32 字节密钥";
        return false;
    }

    return Seal(key, kModeRawKey, Bytes{}, 0, plaintext, aad, blob, error);
}

bool Decrypt(const Bytes& key,
             const Bytes& blob,
             const Bytes& aad,
             Bytes& plaintext,
             std::string& error)
{
    error.clear();
    plaintext.clear();

    if (key.size() != kKeySize)
    {
        error = "AES-256 需要 32 字节密钥";
        return false;
    }

    return Open(key, blob, aad, plaintext, error);
}

bool EncryptWithPassword(const std::string& password,
                         const Bytes& plaintext,
                         const Bytes& aad,
                         Bytes& blob,
                         std::string& error)
{
    error.clear();
    blob.clear();

    Bytes salt(kSaltSize);
    if (!FillRandom(salt.data(), salt.size()))
    {
        error = OpensslError("RAND_bytes");
        return false;
    }

    Bytes key;
    if (!DeriveKey(password, salt, kDefaultIterations, key, error))
        return false;

    ScopedCleanse keyGuard(key.data(), key.size());
    return Seal(key, kModePassword, salt, kDefaultIterations, plaintext, aad, blob, error);
}

bool DecryptWithPassword(const std::string& password,
                         const Bytes& blob,
                         const Bytes& aad,
                         Bytes& plaintext,
                         std::string& error)
{
    error.clear();
    plaintext.clear();

    ParsedBlob parsed;
    if (!ParseBlob(blob, parsed, error))
        return false;

    if (parsed.mode != kModePassword)
    {
        error = "该密文不是口令模式加密的";
        return false;
    }

    Bytes key;
    if (!DeriveKey(password, parsed.salt, parsed.iterations, key, error))
        return false;

    ScopedCleanse keyGuard(key.data(), key.size());
    return Open(key, blob, aad, plaintext, error);
}

std::string Base64Encode(const Bytes& data)
{
    if (data.empty())
        return {};

    if (data.size() > static_cast<std::size_t>(INT_MAX))
        return {};

    const std::size_t encodedSize = 4 * ((data.size() + 2) / 3);
    std::string out(encodedSize + 1, '\0');

    const int written = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(out.data()),
                                        data.data(), static_cast<int>(data.size()));
    if (written < 0)
        return {};

    out.resize(static_cast<std::size_t>(written));
    return out;
}

bool Base64Decode(const std::string& text, Bytes& data, std::string& error)
{
    error.clear();

    if (text.size() % 4 != 0)
    {
        error = "Base64 长度不是 4 的倍数";
        return false;
    }

    if (text.size() > static_cast<std::size_t>(INT_MAX))
    {
        error = "Base64 文本过长";
        return false;
    }

    Bytes out(3 * (text.size() / 4));
    if (!text.empty())
    {
        const int written = EVP_DecodeBlock(out.data(),
                                            reinterpret_cast<const unsigned char*>(text.data()),
                                            static_cast<int>(text.size()));
        if (written < 0)
        {
            error = "Base64 解码失败";
            return false;
        }

        std::size_t padding = 0;
        if (text.back() == '=')
            padding = 1;
        if (text.size() >= 2 && text[text.size() - 2] == '=')
            padding = 2;

        if (static_cast<std::size_t>(written) < padding)
        {
            error = "Base64 填充不合法";
            return false;
        }

        out.resize(static_cast<std::size_t>(written) - padding);
    }

    data.swap(out);
    return true;
}

bool EncryptToString(const std::string& password,
                     const std::string& plaintext,
                     const std::string& aad,
                     std::string& outBase64,
                     std::string& error)
{
    error.clear();
    outBase64.clear();

    const Bytes plaintextBytes(plaintext.begin(), plaintext.end());
    const Bytes aadBytes(aad.begin(), aad.end());
    Bytes blob;

    if (!EncryptWithPassword(password, plaintextBytes, aadBytes, blob, error))
        return false;

    outBase64 = Base64Encode(blob);
    if (outBase64.empty())
    {
        error = "Base64 编码失败";
        return false;
    }

    return true;
}

bool DecryptFromString(const std::string& password,
                       const std::string& inBase64,
                       const std::string& aad,
                       std::string& plaintext,
                       std::string& error)
{
    error.clear();
    plaintext.clear();

    Bytes blob;
    if (!Base64Decode(inBase64, blob, error))
        return false;

    const Bytes aadBytes(aad.begin(), aad.end());
    Bytes plaintextBytes;

    if (!DecryptWithPassword(password, blob, aadBytes, plaintextBytes, error))
        return false;

    plaintext.assign(plaintextBytes.begin(), plaintextBytes.end());
    return true;
}
} // namespace psm::crypto
