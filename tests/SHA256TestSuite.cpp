#include "SHA256TestSuite.h"

#include <rct/Path.h>
#include <rct/SHA256.h>

#include <fstream>

void SHA256TestSuite::createFile(const std::string &f_filename,
                                 const std::string &f_content)
{
    std::ofstream file(f_filename);
    CPPUNIT_ASSERT(file);

    if(!f_content.empty()) file << f_content;
}

void SHA256TestSuite::someFiles()
{
    createFile("empty.txt", "");

    CPPUNIT_ASSERT(SHA256::hashFile("empty.txt") ==
                   "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    createFile("superSecretPassword.txt", "passw0rd");
    CPPUNIT_ASSERT(SHA256::hashFile("superSecretPassword.txt") ==
                   "8f0e2f76e22b43e2855189877e7dc1e1e7d98c226c95db247cd1d547928334a9");

    createFile("fileWithEmbeddedZeros.txt",
               //                     1          2
               //           12345 678901 234567890 12345
               std::string("some\0zeros\0embedded\0here\n", 25));
    CPPUNIT_ASSERT(SHA256::hashFile("fileWithEmbeddedZeros.txt") ==
                   "c4bf8120487fef13ebc75ab7b5fae7a25a2cd332765ee30df4dc81cccb7bfa0d");
}

void SHA256TestSuite::nonExistingFile()
{
    CPPUNIT_ASSERT(SHA256::hashFile("fileDoesNotExist.txt").isEmpty());
    CPPUNIT_ASSERT(SHA256::hashFile("fileDoesNotExist.txt", SHA256::Raw).isEmpty());
}
