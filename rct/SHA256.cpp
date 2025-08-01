#include "SHA256.h"

#include <stdio.h>
#ifdef OS_Darwin
#include "CommonCrypto/CommonDigest.h"
#define SHA256_Update CC_SHA256_Update
#define SHA256_Init CC_SHA256_Init
#define SHA256_Final CC_SHA256_Final
#define SHA256_CTX CC_SHA256_CTX
#define SHA256_DIGEST_LENGTH CC_SHA256_DIGEST_LENGTH
#else
#include <openssl/evp.h>
#include <openssl/sha.h>
#endif

#include "rct/MemoryMappedFile.h"
#include "rct/Path.h"

#ifndef OS_Darwin // Keep using CommonCrypto as-is on macOS
class SHA256Private
{
public:
    EVP_MD_CTX *ctx;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    bool finalized;

    SHA256Private()
        : ctx(EVP_MD_CTX_new()), finalized(false)
    {
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    }

    ~SHA256Private()
    {
        EVP_MD_CTX_free(ctx);
    }

    void reset()
    {
        finalized = false;
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    }

    void update(const void *data, size_t size)
    {
        if (finalized)
            finalized = false;
        EVP_DigestUpdate(ctx, data, size);
    }

    void finalize()
    {
        if (!finalized) {
            EVP_DigestFinal_ex(ctx, hash, nullptr);
            finalized = true;
        }
    }
};
#else
// Keep existing SHA256Private definition for macOS using CommonCrypto
class SHA256Private
{
public:
    SHA256_CTX ctx;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    bool finalized;
};
#endif

SHA256::SHA256()
    : priv(new SHA256Private)
{
    reset();
}

SHA256::~SHA256()
{
    delete priv;
}

void SHA256::update(const char *data, unsigned int size)
{
    if (!size)
        return;
#ifndef OS_Darwin
    priv->update(data, size);
#else
    if (priv->finalized)
        priv->finalized = false;
    SHA256_Update(&priv->ctx, data, size);
#endif
}

void SHA256::update(const String &data)
{
    if (data.empty())
        return;
    update(data.c_str(), data.size());
}

void SHA256::reset()
{
#ifndef OS_Darwin
    priv->reset();
#else
    priv->finalized = false;
    SHA256_Init(&priv->ctx);
#endif
}

static const char *const hexLookup = "0123456789abcdef";

static inline String hashToHex(SHA256Private *priv)
{
    String out(SHA256_DIGEST_LENGTH * 2, '\0');
    const unsigned char *get = priv->hash;
    char *put                = out.data();
    const char *const end    = out.data() + out.size();
    for (; put != end; ++get) {
        *(put++) = hexLookup[(*get >> 4) & 0xf];
        *(put++) = hexLookup[*get & 0xf];
    }
    return out;
}

String SHA256::hash(MapType type) const
{
#ifndef OS_Darwin
    const_cast<SHA256Private *>(priv)->finalize();
#else
    if (!priv->finalized) {
        SHA256_Final(priv->hash, &priv->ctx);
        SHA256_Init(&priv->ctx);
        priv->finalized = true;
    }
#endif
    if (type == Hex)
        return hashToHex(priv);
    return String(reinterpret_cast<char *>(priv->hash), SHA256_DIGEST_LENGTH);
}

String SHA256::hash(const String &data, MapType type)
{
    return SHA256::hash(data.c_str(), data.size(), type);
}

String SHA256::hash(const char *data, unsigned int size, MapType type)
{
#ifndef OS_Darwin
    SHA256Private priv;
    priv.update(data, size);
    priv.finalize();
#else
    SHA256Private priv;
    SHA256_Init(&priv.ctx);
    SHA256_Update(&priv.ctx, data, size);
    SHA256_Final(priv.hash, &priv.ctx);
#endif
    if (type == Hex)
        return hashToHex(&priv);
    return String(reinterpret_cast<char *>(priv.hash), SHA256_DIGEST_LENGTH);
}

String SHA256::hashFile(const Path &file, MapType type)
{
    MemoryMappedFile mmf(file);
    if (!mmf.isOpen())
        return String();
    return hash(static_cast<const char *>(mmf.filePtr()), mmf.size(), type);
}
