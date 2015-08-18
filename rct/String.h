#ifndef String_h
#define String_h

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <errno.h>
#include <string>
#include <stdarg.h>
#include <time.h>
#include <rct/List.h>
#include <strings.h>

class String
{
public:
    enum CaseSensitivity
    {
        CaseSensitive,
        CaseInsensitive
    };
    String(const char *data = 0, int len = -1)
    {
        if (data) {
            if (len == -1)
                len = strlen(data);
            mString = std::string(data, len);
        }
    }
    String(const char *start, const char *end)
    {
        if (start) {
            mString = std::string(start, end);
        }
    }
    String(int len, char fillChar)
        : mString(len, fillChar)
    {}

    String(const String &ba)
        : mString(ba.mString)
    {}

    String(String &&ba)
        : mString(std::move(ba.mString))
    {
    }

    String(const std::string &str)
        : mString(str)
    {}

    String &operator=(const String &other)
    {
        mString = other.mString;
        return *this;
    }

    void assign(const char *data, int len = -1)
    {
        if (data || !len) {
            if (len == -1)
                len = strlen(data);
            mString.assign(data, len);
        } else {
            clear();
        }
    }

    int lastIndexOf(char ch, int from = -1, CaseSensitivity cs = CaseSensitive) const
    {
        if (cs == CaseSensitive)
            return mString.rfind(ch, from == -1 ? std::string::npos : size_t(from));
        const char *data = mString.c_str();
        if (from == -1)
            from = mString.size() - 1;
        ch = tolower(ch);
        while (from >= 0) {
            if (tolower(data[from]) == ch)
                return from;
            --from;
        }
        return -1;
    }

    int indexOf(char ch, int from = 0, CaseSensitivity cs = CaseSensitive) const
    {
        if (cs == CaseSensitive)
            return mString.find(ch, from);
        const char *data = mString.c_str();
        ch = tolower(ch);
        const int size = mString.size();
        while (from < size) {
            if (tolower(data[from]) == ch)
                return from;
            ++from;
        }
        return -1;
    }

    bool contains(const String &other, CaseSensitivity cs = CaseSensitive) const
    {
        return indexOf(other, 0, cs) != -1;
    }

    bool contains(char ch, CaseSensitivity cs = CaseSensitive) const
    {
        return indexOf(ch, 0, cs) != -1;
    }

    int chomp(const String &chars)
    {
        int idx = size() - 1;
        while (idx > 0) {
            if (chars.contains(at(idx - 1))) {
                --idx;
            } else {
                break;
            }
        }
        const int ret = size() - idx - 1;
        if (ret)
            resize(idx);
        return ret;
    }

    int chomp(char ch)
    {
        return chomp(String(&ch, 1));
    }

    int lastIndexOf(const String &ba, int from = -1, CaseSensitivity cs = CaseSensitive) const
    {
        if (ba.isEmpty())
            return -1;
        if (ba.size() == 1)
            return lastIndexOf(ba.first(), from, cs);
        if (cs == CaseSensitive)
            return mString.rfind(ba.mString, from == -1 ? std::string::npos : size_t(from));
        if (from == -1)
            from = mString.size() - 1;
        const String lowered = ba.toLower();
        const int needleSize = lowered.size();
        int matched = 0;
        while (from >= 0) {
            if (lowered.at(needleSize - matched - 1) != tolower(at(from))) {
                matched = 0;
            } else if (++matched == needleSize) {
                return from;
            }

            --from;
        }
        return -1;
    }

    int indexOf(const String &ba, int from = 0, CaseSensitivity cs = CaseSensitive) const
    {
        if (ba.isEmpty())
            return -1;
        if (ba.size() == 1)
            return indexOf(ba.first(), from, cs);
        if (cs == CaseSensitive)
            return mString.find(ba.mString, from);

        const String lowered = ba.toLower();
        const int count = size();
        int matched = 0;

        for (int i=from; i<count; ++i) {
            if (lowered.at(matched) != tolower(at(i))) {
                matched = 0;
            } else if (++matched == lowered.size()) {
                return i - matched + 1;
            }
        }
        return -1;
    }

    char first() const
    {
        return at(0);
    }

    char &first()
    {
        return operator[](0);
    }

    char last() const
    {
        assert(!isEmpty());
        return at(size() - 1);
    }

    char &last()
    {
        assert(!isEmpty());
        return operator[](size() - 1);
    }


    String toLower() const
    {
        std::string ret = mString;
        std::transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
        return ret;
    }

    String toUpper() const
    {
        std::string ret = mString;
        std::transform(ret.begin(), ret.end(), ret.begin(), ::toupper);
        return ret;
    }

    String trimmed(const String &trim = " \f\n\r\t\v") const
    {
        const int start = mString.find_first_not_of(trim);
        if (start == static_cast<int>(std::string::npos))
            return String();

        const int end = mString.find_last_not_of(trim);
        assert(end != static_cast<int>(std::string::npos));
        return mid(start, end - start + 1);
    }

    enum Pad {
        Beginning,
        End
    };
    String padded(Pad pad, int size, char fillChar = ' ', bool truncate = false) const
    {
        const int l = length();
        if (l == size) {
            return *this;
        } else if (l > size) {
            if (!truncate)
                return *this;
            if (pad == Beginning) {
                return right(size);
            } else {
                return left(size);
            }
        } else {
            String ret = *this;
            if (pad == Beginning) {
                ret.prepend(String(size - l, fillChar));
            } else {
                ret.append(String(size - l, fillChar));
            }
            return ret;
        }
    }

    char *data()
    {
        return &mString[0];
    }

    void clear()
    {
        mString.clear();
    }
    const char *data() const
    {
        return mString.data();
    }
    bool isEmpty() const
    {
        return mString.empty();
    }

    char at(int i) const
    {
        return mString.at(i);
    }

    char& operator[](int i)
    {
        return mString.operator[](i);
    }

    const char& operator[](int i) const
    {
        return mString.operator[](i);
    }

    const char *constData() const
    {
        return mString.data();
    }

    const char *nullTerminated() const
    {
        return mString.c_str();
    }

    int size() const
    {
        return mString.size();
    }

    int length() const
    {
        return size();
    }

    void truncate(int size)
    {
        if (mString.size() > static_cast<size_t>(size))
            mString.resize(size);
    }

    void chop(int s)
    {
        mString.resize(size() - s);
    }

    void resize(int size)
    {
        mString.resize(size);
    }

    void reserve(int size)
    {
        mString.reserve(size);
    }

    void prepend(const String &other)
    {
        mString.insert(0, other);
    }

    void prepend(char ch)
    {
        mString.insert(0, &ch, 1);
    }

    void insert(int pos, const String &text)
    {
        mString.insert(pos, text.constData(), text.size());
    }

    void insert(int pos, const char *str, int len = -1)
    {
        if (str) {
            if (len == -1)
                len = strlen(str);
            mString.insert(pos, str, len);
        }
    }

    void insert(int pos, char ch)
    {
        mString.insert(pos, &ch, 1);
    }

    void append(char ch)
    {
        mString += ch;
    }

    void append(const String &ba)
    {
        mString.append(ba);
    }

    String compress() const;
    String uncompress() const { return uncompress(constData(), size()); }
    static String uncompress(const char *data, int size);

    void append(const char *str, int len = -1)
    {
        if (len == -1)
            len = strlen(str);
        if (len > 0)
            mString.append(str, len);
    }

    void remove(int idx, int count)
    {
        mString.erase(idx, count);
    }

    String &operator+=(char ch)
    {
        mString += ch;
        return *this;
    }

    String &operator+=(const char *cstr)
    {
        if (cstr)
            mString += cstr;
        return *this;
    }

    String &operator+=(const String &other)
    {
        mString += other.mString;
        return *this;
    }

    int compare(const String &other, CaseSensitivity cs = CaseSensitive) const
    {
        if (cs == CaseSensitive)
            return mString.compare(other.mString);
        return strcasecmp(mString.c_str(), other.mString.c_str());
    }

    bool operator==(const String &other) const
    {
        return mString == other.mString;
    }

    bool operator==(const char *other) const
    {
        return other && !mString.compare(other);
    }

    bool operator!=(const String &other) const
    {
        return mString != other.mString;
    }

    bool operator!=(const char *other) const
    {
        return !other || mString.compare(other);
    }

    bool operator<(const String &other) const
    {
        return mString < other.mString;
    }

    bool operator>(const String &other) const
    {
        return mString > other.mString;
    }

    bool endsWith(char ch, CaseSensitivity c = CaseSensitive) const
    {
        const int s = mString.size();
        if (s) {
            return (c == CaseInsensitive
                    ? tolower(at(s - 1)) == tolower(ch)
                    : at(s - 1) == ch);
        }
        return false;
    }

    bool startsWith(char ch, CaseSensitivity c = CaseSensitive) const
    {
        if (!isEmpty()) {
            return (c == CaseInsensitive
                    ? tolower(at(0)) == tolower(ch)
                    : at(0) == ch);
        }
        return false;
    }

    bool endsWith(const String &str, CaseSensitivity cs = CaseSensitive) const
    {
        return endsWith(str.constData(), str.size(), cs);
    }

    bool endsWith(const char *str, int len = -1, CaseSensitivity cs = CaseSensitive) const
    {
        if (len == -1)
            len = strlen(str);
        const int s = mString.size();
        if (s >= len) {
            return (cs == CaseInsensitive ? !strncasecmp(str, constData() + s - len, len) : !strncmp(str, constData() + s - len, len));
        }
        return false;
    }


    bool startsWith(const String &str, CaseSensitivity cs = CaseSensitive) const
    {
        return startsWith(str.constData(), str.size(), cs);
    }

    bool startsWith(const char *str, int len = -1, CaseSensitivity cs = CaseSensitive) const
    {
        const int s = mString.size();
        if (len == -1)
            len = strlen(str);
        if (s >= len) {
            return (cs == CaseInsensitive ? !strncasecmp(str, constData(), len) : !strncmp(str, constData(), len));
        }
        return false;
    }

    void replace(int idx, int len, const String &with)
    {
        mString.replace(idx, len, with.mString);
    }

    void replace(const String &from, const String &to)
    {
        int idx = 0;
        while (true) {
            idx = indexOf(from, idx);
            if (idx == -1)
                break;
            replace(idx, from.size(), to);
            idx += to.size();
        }
    }

    int replace(char from, char to)
    {
        int count = 0;
        for (int i=size() - 1; i>=0; --i) {
            char &ch = operator[](i);
            if (ch == from) {
                ch = to;
                ++count;
            }
        }
        return count;
    }

    String mid(int from, int l = -1) const
    {
        if (l == -1)
            l = size() - from;
        if (from == 0 && l == size())
            return *this;
        return mString.substr(from, l);
    }

    String left(int l) const
    {
        return mString.substr(0, l);
    }

    String right(int l) const
    {
        return mString.substr(size() - l, l);
    }

    operator std::string() const
    {
        return mString;
    }

    std::string& ref()
    {
        return mString;
    }

    const std::string& ref() const
    {
        return mString;
    }

    enum SplitFlag {
        NoSplitFlag = 0x0,
        SkipEmpty = 0x1,
        KeepSeparators = 0x2
    };
    List<String> split(char ch, unsigned int flags = NoSplitFlag) const
    {
        List<String> ret;
        int last = 0;
        const int add = flags & KeepSeparators ? 1 : 0;
        while (1) {
            const int next = indexOf(ch, last);
            if (next == -1)
                break;
            if (next > last || !(flags & SkipEmpty))
                ret.append(mid(last, next - last + add));
            last = next + 1;
        }
        if (last < size() || !(flags & SkipEmpty))
            ret.append(mid(last));
        return ret;
    }

    List<String> split(const String &split, unsigned int flags = NoSplitFlag) const
    {
        List<String> ret;
        int last = 0;
        while (1) {
            const int next = indexOf(split, last);
            if (next == -1)
                break;
            if (next > last || !(flags & SkipEmpty))
                ret.append(mid(last, next - last));
            last = next + split.size();
        }
        if (last < size() || !(flags & SkipEmpty))
            ret.append(mid(last));
        return ret;
    }

    uint64_t toULongLong(bool *ok = 0, int base = 10) const
    {
        errno = 0;
        char *end = 0;
        const uint64_t ret = ::strtoull(constData(), &end, base);
        if (ok)
            *ok = !errno && !*end;
        return ret;
    }
    int64_t toLongLong(bool *ok = 0, int base = 10) const
    {
        errno = 0;
        char *end = 0;
        const int64_t ret = ::strtoll(constData(), &end, base);
        if (ok)
            *ok = !errno && !*end;
        return ret;
    }
    uint32_t toULong(bool *ok = 0, int base = 10) const
    {
        errno = 0;
        char *end = 0;
        const uint32_t ret = ::strtoul(constData(), &end, base);
        if (ok)
            *ok = !errno && !*end;
        return ret;
    }
    int32_t toLong(bool *ok = 0, int base = 10) const
    {
        errno = 0;
        char *end = 0;
        const int32_t ret = ::strtol(constData(), &end, base);
        if (ok)
            *ok = !errno && !*end;
        return ret;
    }

    enum TimeFormat {
        DateTime,
        Time,
        Date
    };

    static String formatTime(time_t t, TimeFormat fmt = DateTime)
    {
        const char *format = 0;
        switch (fmt) {
        case DateTime:
            format = "%Y-%m-%d %H:%M:%S";
            break;
        case Date:
            format = "%Y-%m-%d";
            break;
        case Time:
            format = "%H:%M:%S";
            break;
        }

        char buf[32];
        tm tm;
        localtime_r(&t, &tm);
        const int w = strftime(buf, sizeof(buf), format, &tm);
        return String(buf, w);
    }

    String toHex() const { return toHex(*this); }
    static String toHex(const String &hex) { return toHex(hex.constData(), hex.size()); }
    static String toHex(const void *data, int len);

    static String number(int8_t num, int base = 10) { return String::number(static_cast<int64_t>(num), base); }
    static String number(uint8_t num, int base = 10) { return String::number(static_cast<int64_t>(num), base); }
    static String number(int16_t num, int base = 10) { return String::number(static_cast<int64_t>(num), base); }
    static String number(uint16_t num, int base = 10) { return String::number(static_cast<int64_t>(num), base); }
    static String number(int32_t num, int base = 10) { return String::number(static_cast<int64_t>(num), base); }
    static String number(uint32_t num, int base = 10) { return String::number(static_cast<int64_t>(num), base); }
    static String number(int64_t num, int base = 10)
    {
        const char *format = 0;
        switch (base) {
        case 10: format = "%lld"; break;
        case 16: format = "0x%llx"; break;
        case 8: format = "%llo"; break;
        case 1: {
            String ret;
            while (num) {
                ret.append(num & 1 ? '1' : '0');
                num >>= 1;
            }
            return ret; }
        default:
            assert(0);
            return String();
        }
        char buf[32];
        const int w = ::snprintf(buf, sizeof(buf), format, num);
        return String(buf, w);
    }

    static String number(uint64_t num, int base = 10)
    {
        const char *format = 0;
        switch (base) {
        case 10: format = "%llu"; break;
        case 16: format = "0x%llx"; break;
        case 8: format = "%llo"; break;
        case 1: {
            String ret;
            while (num) {
                ret.append(num & 1 ? '1' : '0');
                num >>= 1;
            }
            return ret; }
        default:
            assert(0);
            return String();
        }
        char buf[32];
        const int w = ::snprintf(buf, sizeof(buf), format, num);
        return String(buf, w);
    }

    static String number(double num, int prec = 2)
    {
        char format[32];
        snprintf(format, sizeof(format), "%%.%df", prec);
        char buf[32];
        const int w = ::snprintf(buf, sizeof(buf), format, num);
        return String(buf, w);
    }

    static String join(const List<String> &list, char ch)
    {
        return String::join(list, String(&ch, 1));
    }

    static String join(const List<String> &list, const String &sep)
    {
        String ret;
        const int sepSize = sep.size();
        int size = std::max(0, list.size() - 1) * sepSize;
        const int count = list.size();
        for (int i=0; i<count; ++i)
            size += list.at(i).size();
        ret.reserve(size);
        for (int i=0; i<count; ++i) {
            const String &b = list.at(i);
            ret.append(b);
            if (sepSize && i + 1 < list.size())
                ret.append(sep);
        }
        return ret;
    }
    template <int StaticBufSize = 4096>
    static String format(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        const String ret = String::format<StaticBufSize>(format, args);
        va_end(args);
        return ret;
    }

    template <int StaticBufSize = 4096>
    static String format(const char *format, va_list args)
    {
        va_list copy;
        va_copy(copy, args);

        char buffer[StaticBufSize];
        const int size = ::vsnprintf(buffer, StaticBufSize, format, args);
        assert(size >= 0);
        String ret;
        if (size < StaticBufSize) {
            ret.assign(buffer, size);
        } else {
            ret.resize(size);
            ::vsnprintf(&ret[0], size+1, format, copy);
        }
        va_end(copy);
        return ret;
    }
private:
    std::string mString;
};

inline bool operator==(const char *l, const String &r)
{
    return r.operator==(l);
}

inline bool operator!=(const char *l, const String &r)
{
    return r.operator!=(l);
}

inline const String operator+(const String &l, const char *r)
{
    String ret = l;
    ret += r;
    return ret;
}

inline const String operator+(const char *l, const String &r)
{
    String ret = l;
    ret += r;
    return ret;
}

inline const String operator+(const String &l, char ch)
{
    String ret = l;
    ret += ch;
    return ret;
}

inline const String operator+(char l, const String &r)
{
    String ret;
    ret.reserve(r.size() + 1);
    ret += l;
    ret += r;
    return ret;
}


inline const String operator+(const String &l, const String &r)
{
    String ret = l;
    ret += r;
    return ret;
}

namespace std
{
template <> struct hash<String> : public unary_function<String, size_t>
{
    size_t operator()(const String& value) const
    {
        std::hash<std::string> h;
        return h(value);
    }
};
}


#endif
