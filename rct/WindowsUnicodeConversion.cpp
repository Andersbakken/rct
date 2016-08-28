#ifdef _WIN32

#include "WindowsUnicodeConversion.h"

#include <Windows.h>
#include <iostream>

Utf8To16::Utf8To16(const char *f_utf8String)
{
    const std::size_t inSize = ::strlen(f_utf8String);


    // find out how many utf16 characters we need
    const int requiredU16Chars =
        MultiByteToWideChar(CP_UTF8, 0,
                            f_utf8String, inSize,
                            nullptr, 0);

    m_str.resize(requiredU16Chars);

    // finally, do the conversion
    MultiByteToWideChar(CP_UTF8, 0,
                        f_utf8String, inSize,
                        &m_str[0], m_str.size());
}

Utf16To8::Utf16To8(const wchar_t *f_utf16String)
{
    // get number of required bytes in output
    const int numBytes = WideCharToMultiByte(CP_UTF8, 0,
                                             f_utf16String, -1,
                                             nullptr, 0,
                                             nullptr, nullptr);
    m_str.resize(numBytes);

    // do the conversion
    WideCharToMultiByte(CP_UTF8, 0,
                        f_utf16String, -1,
                        &m_str[0], m_str.size(),
                        nullptr, nullptr);
}

#endif  /* _WIN32 */
