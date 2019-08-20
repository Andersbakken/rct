#ifndef AES256CBC_H
#define AES256CBC_H

#include <rct/String.h>

class AES256CBCPrivate;

class AES256CBC
{
public:
    AES256CBC(const String &key, const unsigned char *salt = nullptr);
    ~AES256CBC();

    String encrypt(const String &data);
    String decrypt(const String &data);

private:
    AES256CBCPrivate *priv;
};

#endif
