#ifndef WINDOWSUNICODECONVERSION_H
#define WINDOWSUNICODECONVERSION_H
#ifdef _WIN32

#include <string>

class Utf8To16
{
public:
    Utf8To16(const char *utf8String);

    operator const wchar_t      *  () const {return m_str.c_str();}
    operator const std::wstring &  () const {return m_str;}

    const std::wstring &asWstring() const {return m_str;}
    const wchar_t      *asWchar_t() const {return m_str.c_str();}

private:
    std::wstring m_str;
};

class Utf16To8
{
public:
    Utf16To8(const wchar_t *utf16String);

    operator const char        * () const {return m_str.c_str();}
    operator const std::string & () const {return m_str;}

    const std::string &asStdString() const {return m_str;}
    const char *asCString() const {return m_str.c_str();}
private:
    std::string m_str;
};

#endif /* _WIN32 */
#endif /* WINDOWSUNICODECONVERSION_H */
