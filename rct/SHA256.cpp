#include "SHA256.h"

#include <stdio.h>
#ifdef OS_Darwin
#  include "CommonCrypto/CommonDigest.h"
#  define SHA256_Update        CC_SHA256_Update
#  define SHA256_Init          CC_SHA256_Init
#  define SHA256_Final         CC_SHA256_Final
#  define SHA256_CTX           CC_SHA256_CTX
#  define SHA256_DIGEST_LENGTH CC_SHA256_DIGEST_LENGTH
#else
#  include <openssl/sha.h>
#endif

#include "rct/Path.h"
#include "rct/MemoryMappedFile.h"

class SHA256Private
{
public:
    SHA256_CTX ctx;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    bool finalized;
};

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
    if (priv->finalized)
        priv->finalized = false;
    SHA256_Update(&priv->ctx, data, size);
}

void SHA256::update(const String &data)
{
    if (data.isEmpty())
        return;
    if (priv->finalized)
        priv->finalized = false;
    SHA256_Update(&priv->ctx, data.constData(), data.size());
}

void SHA256::reset()
{
    priv->finalized = false;
    SHA256_Init(&priv->ctx);
}

static const char* const hexLookup = "0123456789abcdef";

static inline String hashToHex(SHA256Private* priv)
{
    String out(SHA256_DIGEST_LENGTH * 2, '\0');
    const unsigned char* get = priv->hash;
    char* put = out.data();
    const char* const end = out.data() + out.size();
    for (; put != end; ++get) {
        *(put++) = hexLookup[(*get >> 4) & 0xf];
        *(put++) = hexLookup[*get & 0xf];
    }
    return out;
}

String SHA256::hash(MapType type) const
{
    if (!priv->finalized) {
        SHA256_Final(priv->hash, &priv->ctx);
        SHA256_Init(&priv->ctx);
        priv->finalized = true;
    }
    if (type == Hex)
        return hashToHex(priv);
    return String(reinterpret_cast<char*>(priv->hash), SHA256_DIGEST_LENGTH);
}

String SHA256::hash(const String& data, MapType type)
{
    return SHA256::hash(data.constData(), data.size(), type);
}

String SHA256::hash(const char* data, unsigned int size, MapType type)
{
    SHA256Private priv;
    SHA256_Init(&priv.ctx);
    SHA256_Update(&priv.ctx, data, size);
    SHA256_Final(priv.hash, &priv.ctx);
    if (type == Hex)
        return hashToHex(&priv);
    return String(reinterpret_cast<char*>(priv.hash), SHA256_DIGEST_LENGTH);
}

String SHA256::hashFile(const Path& file, MapType type)
{
    MemoryMappedFile mmf(file);
    if(!mmf.isOpen()) return String();
    return hash(static_cast<const char*>(mmf.filePtr()), mmf.size(), type);
}
