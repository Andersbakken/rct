#ifndef Flags_h
#define Flags_h

#include <rct/String.h>

template <typename T>
class Flags
{
public:
    Flags(T t) : mValue(t) {}
    Flags() : mValue(static_cast<T>(0)) {}
    Flags(const Flags<T> &other) : mValue(other.mValue) {}
    Flags<T> &operator=(Flags<T> other) { mValue = other.mValue; return *this; }

    Flags<T> operator|(Flags<T> other) const { return static_cast<T>(static_cast<unsigned long long>(mValue) | other.mValue); }
    Flags<T> operator&(Flags<T> other) const { return static_cast<T>(static_cast<unsigned long long>(mValue) & other.mValue); }
    Flags<T> operator^(Flags<T> other) const { return static_cast<T>(static_cast<unsigned long long>(mValue) ^ other.mValue); }
    Flags<T> operator+(Flags<T> other) const { return static_cast<T>(static_cast<unsigned long long>(mValue) + other.mValue); }
    Flags<T> operator-(Flags<T> other) const { return static_cast<T>(static_cast<unsigned long long>(mValue) - other.mValue); }
    Flags<T> operator*(Flags<T> other) const { return static_cast<T>(static_cast<unsigned long long>(mValue) * other.mValue); }
    Flags<T> operator%(Flags<T> other) const { return static_cast<T>(static_cast<unsigned long long>(mValue) % other.mValue); }
    Flags<T> operator/(Flags<T> other) const { return static_cast<T>(static_cast<unsigned long long>(mValue) / other.mValue); }

    Flags<T> operator|(T other) const { return static_cast<T>(static_cast<unsigned long long>(mValue) | other); }
    Flags<T> operator&(T other) const { return static_cast<T>(static_cast<unsigned long long>(mValue) & other); }
    Flags<T> operator^(T other) const { return static_cast<T>(static_cast<unsigned long long>(mValue) ^ other); }
    Flags<T> operator+(T other) const { return static_cast<T>(static_cast<unsigned long long>(mValue) + other); }
    Flags<T> operator-(T other) const { return static_cast<T>(static_cast<unsigned long long>(mValue) - other); }
    Flags<T> operator*(T other) const { return static_cast<T>(static_cast<unsigned long long>(mValue) * other); }
    Flags<T> operator%(T other) const { return static_cast<T>(static_cast<unsigned long long>(mValue) % other); }
    Flags<T> operator/(T other) const { return static_cast<T>(static_cast<unsigned long long>(mValue) / other); }

    Flags<T> operator|=(Flags<T> other) { mValue = static_cast<T>(static_cast<unsigned long long>(mValue) | other.mValue); return *this; }
    Flags<T> operator&=(Flags<T> other) { mValue = static_cast<T>(static_cast<unsigned long long>(mValue) & other.mValue); return *this; }
    Flags<T> operator^=(Flags<T> other) { mValue = static_cast<T>(static_cast<unsigned long long>(mValue) ^ other.mValue); return *this; }
    Flags<T> operator+=(Flags<T> other) { mValue = static_cast<T>(static_cast<unsigned long long>(mValue) + other.mValue); return *this; }
    Flags<T> operator-=(Flags<T> other) { mValue = static_cast<T>(static_cast<unsigned long long>(mValue) - other.mValue); return *this; }
    Flags<T> operator*=(Flags<T> other) { mValue = static_cast<T>(static_cast<unsigned long long>(mValue) * other.mValue); return *this; }
    Flags<T> operator%=(Flags<T> other) { mValue = static_cast<T>(static_cast<unsigned long long>(mValue) % other.mValue); return *this; }
    Flags<T> operator/=(Flags<T> other) { mValue = static_cast<T>(static_cast<unsigned long long>(mValue) / other.mValue); return *this; }

    Flags<T> operator|=(T other) { mValue = static_cast<T>(static_cast<unsigned long long>(mValue) | other); return *this; }
    Flags<T> operator&=(T other) { mValue = static_cast<T>(static_cast<unsigned long long>(mValue) & other); return *this; }
    Flags<T> operator^=(T other) { mValue = static_cast<T>(static_cast<unsigned long long>(mValue) ^ other); return *this; }
    Flags<T> operator+=(T other) { mValue = static_cast<T>(static_cast<unsigned long long>(mValue) + other); return *this; }
    Flags<T> operator-=(T other) { mValue = static_cast<T>(static_cast<unsigned long long>(mValue) - other); return *this; }
    Flags<T> operator*=(T other) { mValue = static_cast<T>(static_cast<unsigned long long>(mValue) * other); return *this; }
    Flags<T> operator%=(T other) { mValue = static_cast<T>(static_cast<unsigned long long>(mValue) % other); return *this; }
    Flags<T> operator/=(T other) { mValue = static_cast<T>(static_cast<unsigned long long>(mValue) / other); return *this; }

    Flags<T> operator~() const { return static_cast<T>(~static_cast<unsigned long long>(mValue)); }
    bool operator!() const { return !mValue; }
    operator T() const { return mValue; }

    bool test(T flag) const { return mValue & flag; }
    Flags<T> test(Flags<T> flags) const { return mValue & flags; }
    void set(T flag, bool on = true)
    {
        if (on) {
            operator|=(flag);
        } else {
            Flags<T> t = ~flag;
            operator&=(t);
        }
    }
    void set(Flags<T> flags, bool on = true)
    {
        if (on) {
            operator|=(flags);
        } else {
            operator&=(~flags);
        }
    }

    bool operator==(Flags<T> other) const { return mValue == other.mValue; }
    bool operator!=(Flags<T> other) const { return mValue != other.mValue; }
    bool operator<(Flags<T> other) const { return mValue < other.mValue; }
    bool operator>(Flags<T> other) const { return mValue > other.mValue; }
    bool operator<=(Flags<T> other) const { return mValue <= other.mValue; }
    bool operator>=(Flags<T> other) const { return mValue >= other.mValue; }

    bool operator==(T other) const { return mValue == other; }
    bool operator!=(T other) const { return mValue != other; }
    bool operator<(T other) const { return mValue < other; }
    bool operator>(T other) const { return mValue > other; }
    bool operator<=(T other) const { return mValue <= other; }
    bool operator>=(T other) const { return mValue >= other; }

    String toString() const { return String::format<16>("0x%x", cast<unsigned long long>()); }
    template <typename Type> Type cast() const { return static_cast<Type>(mValue); }
private:
    T mValue;
};

#define RCT_FLAGS_OPERATORS(T)                            \
    inline Flags<T> operator|(T l, T r) { return Flags<T>(l) | r; } \
    inline Flags<T> operator&(T l, T r) { return Flags<T>(l) & r; } \
    inline Flags<T> operator^(T l, T r) { return Flags<T>(l) ^ r; } \
    inline Flags<T> operator+(T l, T r) { return Flags<T>(l) + r; } \
    inline Flags<T> operator-(T l, T r) { return Flags<T>(l) - r; } \
    inline Flags<T> operator*(T l, T r) { return Flags<T>(l) * r; } \
    inline Flags<T> operator%(T l, T r) { return Flags<T>(l) % r; } \
    inline Flags<T> operator/(T l, T r) { return Flags<T>(l) / r; } \
    inline Flags<T> operator~(T t) { return ~Flags<T>(t); }

#define RCT_FLAGS(T)                                                    \
    RCT_FLAGS_OPERATORS(T)                                              \
    inline Serializer &operator<<(Serializer &s, Flags<T> f)            \
    {                                                                   \
        const T t = f.cast<T>();                                        \
        s.write(reinterpret_cast<const unsigned char*>(&t), sizeof(T)); \
        return s;                                                       \
    }                                                                   \
    inline Deserializer &operator>>(Deserializer &s, Flags<T> &f)       \
    {                                                                   \
        union                                                           \
        {                                                               \
            char buf[sizeof(T)];                                        \
            T val;                                                      \
        } aliased;                                                      \
        s.read(aliased.buf, sizeof(T));                                 \
        f = aliased.val;                                                \
        return s;                                                       \
    }
#endif

