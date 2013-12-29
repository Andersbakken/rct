#include "AES256CBC.h"
#include <rct/Log.h>

AES256CBC::AES256CBC(const String& key, const unsigned char* salt)
    : inited(false)
{
    unsigned char outkey[32], outiv[32];

    const int ret = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), salt,
                                   reinterpret_cast<const unsigned char*>(key.constData()),
                                   key.size(), 5, outkey, outiv);
    if (ret != 32) {
        error("Key size is %d bits - should be 256 bits", ret);
        return;
    }

    EVP_CIPHER_CTX_init(&ectx);
    EVP_EncryptInit_ex(&ectx, EVP_aes_256_cbc(), NULL, outkey, outiv);
    EVP_CIPHER_CTX_init(&dctx);
    EVP_DecryptInit_ex(&dctx, EVP_aes_256_cbc(), NULL, outkey, outiv);

    inited = true;
}

AES256CBC::~AES256CBC()
{
    if (!inited)
        return;
    EVP_CIPHER_CTX_cleanup(&ectx);
    EVP_CIPHER_CTX_cleanup(&dctx);
}

String AES256CBC::encrypt(const String& data)
{
    if (!inited)
        return String();
    int elen = data.size() + AES_BLOCK_SIZE, flen;
    String out(elen, '\0');
    EVP_EncryptInit_ex(&ectx, NULL, NULL, NULL, NULL);
    EVP_EncryptUpdate(&ectx, reinterpret_cast<unsigned char*>(out.data()), &elen,
                      reinterpret_cast<const unsigned char*>(data.constData()), data.size());
    EVP_EncryptFinal_ex(&ectx, reinterpret_cast<unsigned char*>(out.data()) + elen, &flen);
    out.resize(elen + flen);
    return out;
}

String AES256CBC::decrypt(const String& data)
{
    if (!inited)
        return String();
    int dlen = data.size(), flen;
    String out(dlen + AES_BLOCK_SIZE, '\0');
    EVP_DecryptInit_ex(&dctx, NULL, NULL, NULL, NULL);
    EVP_DecryptUpdate(&dctx, reinterpret_cast<unsigned char*>(out.data()), &dlen,
                      reinterpret_cast<const unsigned char*>(data.constData()), data.size());
    EVP_DecryptFinal_ex(&dctx, reinterpret_cast<unsigned char*>(out.data()) + dlen, &flen);
    out.resize(dlen + flen);
    return out;
}
