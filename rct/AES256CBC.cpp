#include "AES256CBC.h"

#ifdef OS_Darwin
#include <CommonCrypto/CommonCryptor.h>
#else
#include <openssl/evp.h>
#include <openssl/aes.h>
#endif

#include "rct/Log.h"
#include "SHA256.h"

#if defined OS_Darwin && defined OPENSSL_VERSION_NUMBER && OPENSSL_VERSION_NUMBER < 0x10100000L
static EVP_CIPHER_CTX *EVP_CIPHER_CTX_new(void)
{
   EVP_CIPHER_CTX *ctx = OPENSSL_malloc(sizeof *ctx);
   if (ctx)
      EVP_CIPHER_CTX_init(ctx);
   return ctx;
}
static void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX *ctx)
{
   if (ctx) {
       EVP_CIPHER_CTX_cleanup(ctx);
      OPENSSL_free(ctx);
   }
}
#endif

class AES256CBCPrivate
{
public:
    AES256CBCPrivate() : inited(false) { }
    ~AES256CBCPrivate();

    bool inited;
    unsigned char iv[32];
#ifdef OS_Darwin
    CCCryptorRef ectx, dctx;
#else
    EVP_CIPHER_CTX *ectx, *dctx;
#endif
};

AES256CBCPrivate::~AES256CBCPrivate()
{
    if (!inited)
        return;
#ifdef OS_Darwin
    CCCryptorRelease(ectx);
    CCCryptorRelease(dctx);
#else
    EVP_CIPHER_CTX_free(ectx);
    EVP_CIPHER_CTX_free(dctx);
#endif
}

static void deriveKey(const String& key, unsigned char* outkey,
                      unsigned char* outiv, int rounds,
                      const unsigned char* salt)
{
    String preHash = key, currentHash, hash;
    if (salt) // we're assuming that salt is at least 8 bytes
        preHash += String(reinterpret_cast<const char*>(salt), 8);
    currentHash = SHA256::hash(preHash, SHA256::Raw);
    for (int i = 1; i < rounds; ++i)
        currentHash = SHA256::hash(currentHash, SHA256::Raw);
    hash += currentHash;
    while (hash.size() < 64) { // 32 byte key and 32 byte iv
        preHash = currentHash + key;
        if (salt)
            preHash += String(reinterpret_cast<const char*>(salt), 8);
        currentHash = SHA256::hash(preHash, SHA256::Raw);
        for (int i = 1; i < rounds; ++i)
            currentHash = SHA256::hash(currentHash, SHA256::Raw);
        hash += currentHash;
    }
    memcpy(outkey, hash.constData(), 32);
    memcpy(outiv, hash.constData() + 32, 32);
}

AES256CBC::AES256CBC(const String& key, const unsigned char* salt)
    : priv(new AES256CBCPrivate)
{
    unsigned char outkey[32];

    deriveKey(key, outkey, priv->iv, 100, salt);
#ifdef OS_Darwin
    CCCryptorCreate(kCCEncrypt, kCCAlgorithmAES128, kCCOptionPKCS7Padding,
                    outkey, kCCKeySizeAES256, priv->iv, &priv->ectx);
    CCCryptorCreate(kCCDecrypt, kCCAlgorithmAES128, kCCOptionPKCS7Padding,
                    outkey, kCCKeySizeAES256, priv->iv, &priv->dctx);
#else
    priv->ectx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(priv->ectx, EVP_aes_256_cbc(), nullptr, outkey, priv->iv);
    priv->dctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(priv->dctx, EVP_aes_256_cbc(), nullptr, outkey, priv->iv);
#endif

    priv->inited = true;
}

AES256CBC::~AES256CBC()
{
    delete priv;
}

String AES256CBC::encrypt(const String& data)
{
    if (!priv->inited)
        return String();
#ifdef OS_Darwin
    size_t flen;
    String out(data.size() + kCCBlockSizeAES128, '\0');
    CCCryptorUpdate(priv->ectx, data.constData(), data.size(), out.data(), out.size(), &flen);
    CCCryptorFinal(priv->ectx, out.data(), flen, &flen);
    CCCryptorReset(priv->ectx, priv->iv);
    out.resize(flen);
#else
    int elen = data.size() + AES_BLOCK_SIZE, flen;
    String out(elen, '\0');
    EVP_EncryptInit_ex(priv->ectx, nullptr, nullptr, nullptr, nullptr);
    EVP_EncryptUpdate(priv->ectx, reinterpret_cast<unsigned char*>(out.data()), &elen,
                      reinterpret_cast<const unsigned char*>(data.constData()), data.size());
    EVP_EncryptFinal_ex(priv->ectx, reinterpret_cast<unsigned char*>(out.data()) + elen, &flen);
    out.resize(elen + flen);
#endif
    return out;
}

String AES256CBC::decrypt(const String& data)
{
    if (!priv->inited)
        return String();
#ifdef OS_Darwin
    size_t flen;
    String out(data.size() + kCCBlockSizeAES128, '\0');
    CCCryptorUpdate(priv->dctx, data.constData(), data.size(), out.data(), out.size(), &flen);
    CCCryptorFinal(priv->dctx, out.data(), flen, &flen);
    CCCryptorReset(priv->dctx, priv->iv);
    out.resize(flen);
#else
    int dlen = data.size(), flen;
    String out(dlen + AES_BLOCK_SIZE, '\0');
    EVP_DecryptInit_ex(priv->dctx, nullptr, nullptr, nullptr, nullptr);
    EVP_DecryptUpdate(priv->dctx, reinterpret_cast<unsigned char*>(out.data()), &dlen,
                      reinterpret_cast<const unsigned char*>(data.constData()), data.size());
    EVP_DecryptFinal_ex(priv->dctx, reinterpret_cast<unsigned char*>(out.data()) + dlen, &flen);
    out.resize(dlen + flen);
#endif
    return out;
}
