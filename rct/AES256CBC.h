#ifndef AES256CBC_H
#define AES256CBC_H

#include <rct/String.h>
#include <openssl/evp.h>
#include <openssl/aes.h>

class AES256CBC
{
public:
    AES256CBC(const String& key, const unsigned char* salt = 0);
    ~AES256CBC();

    String encrypt(const String& data);
    String decrypt(const String& data);

private:
    bool inited;
    EVP_CIPHER_CTX ectx, dctx;
};

#endif
