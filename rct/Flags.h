#ifndef Flags_h
#define Flags_h

#include "String.h"

enum FlagsInitializer { NullFlags };

#if !defined(NDEBUG)
template <typename T>
class Flags
{
    static void safeBool(Flags<T> *) {}
public:
    typedef void (*SafeBool)(Flags<T> *);
    Flags(FlagsInitializer) { mValue = 0; }
    Flags() { mValue = 0; }
    Flags(T t) { mValue = t; }
    template <typename F> explicit Flags(F f) { mValue = 0; *this |= f; }
    Flags(const Flags<T> &other) { mValue = other.mValue; }
    Flags(Flags &&other) noexcept : mValue(other.mValue) { other.mValue = 0; }

    inline unsigned long long value() const { return mValue; }

    static Flags<T> construct(int64_t value) { Flags<T> ret; ret.mValue = value; return ret; }
    Flags<T> &operator=(Flags<T> &&other) { mValue = other.mValue; other.mValue = 0; return *this; }
    Flags<T> &operator=(const Flags<T> &other) { mValue = other.mValue; return *this; }
    Flags<T> &operator=(T t) { mValue = t; return *this; }
    template <typename F> Flags &operator=(F f)
    {
        clear();
        *this |= f;
        return *this;
    }

#define ADD_OPERATOR(OP)                                                \
    Flags<T> operator OP(Flags<T> other) const                          \
    {                                                                   \
        return Flags<T>::construct(static_cast<unsigned long long>(mValue) OP static_cast<unsigned long long>(other.mValue)); \
    }                                                                   \
    Flags<T> &operator OP ## =(Flags<T> other)                          \
    {                                                                   \
        mValue = static_cast<unsigned long long>(mValue) OP static_cast<unsigned long long>(other.mValue); \
        return *this;                                                   \
    }


    ADD_OPERATOR(|)
    ADD_OPERATOR(&)
    ADD_OPERATOR(^)
    ADD_OPERATOR(+)
    ADD_OPERATOR(-)
    ADD_OPERATOR(*)
    ADD_OPERATOR(%)
    ADD_OPERATOR(/)
    Flags<T> operator~() const { return construct(~+mValue); }

    void clear() { mValue = 0; }
    bool operator!() const { return !mValue; }
    operator SafeBool() const { return mValue ? &safeBool : nullptr; }

    template <typename F> bool test(F flag) const { return *this & flag; }
    Flags<T> test(Flags<T> flags) const { return construct(mValue & flags); }
    template <typename F> void set(F flag, bool on = true)
    {
        if (on) {
            *this |= flag;
        } else {
            *this &= ~flag;
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

    bool operator==(T other) const { return mValue == static_cast<int64_t>(other); }
    bool operator!=(T other) const { return mValue != static_cast<int64_t>(other); }
    bool operator<(T other) const { return mValue < static_cast<int64_t>(other); }
    bool operator>(T other) const { return mValue > static_cast<int64_t>(other); }
    bool operator<=(T other) const { return mValue <= static_cast<int64_t>(other); }
    bool operator>=(T other) const { return mValue >= static_cast<int64_t>(other); }

    String toString() const
    {
        char buf[16];
        const int w = snprintf(buf, sizeof(buf), "0x%llx", cast<unsigned long long>());
        return String(buf, w);
    }
    T cast() const { return cast<T>(); }
    template <typename Type> Type cast() const { return static_cast<Type>(mValue); }
private:
    union {
        int64_t mValue;
        T mT;
    };
};

#define RCT_FLAGS_OPERATOR(OP, L, R, ORIG)      \
    inline ORIG operator OP(L l, R r)           \
    {                                           \
        return static_cast<ORIG>(+l OP +r);     \
    }


#define RCT_FLAGS_OPERATORS(L, R, ORIG)         \
    RCT_FLAGS_OPERATOR(|, L, R, ORIG)           \
    RCT_FLAGS_OPERATOR(&, L, R, ORIG)           \
    RCT_FLAGS_OPERATOR(^, L, R, ORIG)           \
    RCT_FLAGS_OPERATOR(+, L, R, ORIG)           \
    RCT_FLAGS_OPERATOR(-, L, R, ORIG)           \
    RCT_FLAGS_OPERATOR(*, L, R, ORIG)           \
    RCT_FLAGS_OPERATOR(%, L, R, ORIG)           \
    RCT_FLAGS_OPERATOR(/, L, R, ORIG)

#define RCT_FLAGS_MEMBER_OPERATOR(OP, T, ORIG)                          \
    inline Flags<ORIG> operator OP ## =(Flags<ORIG> &f, T l) \
    {                                                                   \
        f OP ## = Flags<ORIG>(static_cast<ORIG>(l));           \
        return f;                                                       \
    }                                                                   \
        inline Flags<ORIG> operator OP(Flags<ORIG> f, T l) \
        {                                                               \
            return Flags<ORIG>(static_cast<ORIG>(f.cast<unsigned long long>() OP +l)); \
        }

#define RCT_FLAGS_MEMBER_OPERATORS(T, ORIG)     \
    RCT_FLAGS_MEMBER_OPERATOR(|, T, ORIG)       \
    RCT_FLAGS_MEMBER_OPERATOR(&, T, ORIG)       \
    RCT_FLAGS_MEMBER_OPERATOR(^, T, ORIG)       \
    RCT_FLAGS_MEMBER_OPERATOR(+, T, ORIG)       \
    RCT_FLAGS_MEMBER_OPERATOR(-, T, ORIG)       \
    RCT_FLAGS_MEMBER_OPERATOR(*, T, ORIG)       \
    RCT_FLAGS_MEMBER_OPERATOR(%, T, ORIG)       \
    RCT_FLAGS_MEMBER_OPERATOR(/, T, ORIG)

#define RCT_FLAGS_BOOLEAN_OPERATOR(OP, ORIG, SUB)           \
    inline bool operator OP (Flags<ORIG> l, SUB r) \
    {                                                       \
        return l.cast<SUB>() OP r;                          \
    }                                                       \
    inline bool operator OP (SUB r, Flags<ORIG> l) \
    {                                                       \
        return r OP l.cast<SUB>();                          \
    }

#define RCT_FLAGS_BOOLEAN_OPERATORS(ORIG, SUB)  \
    RCT_FLAGS_BOOLEAN_OPERATOR(==, ORIG, SUB)   \
    RCT_FLAGS_BOOLEAN_OPERATOR(!=, ORIG, SUB)   \
    RCT_FLAGS_BOOLEAN_OPERATOR(<, ORIG, SUB)    \
    RCT_FLAGS_BOOLEAN_OPERATOR(>, ORIG, SUB)    \
    RCT_FLAGS_BOOLEAN_OPERATOR(<=, ORIG, SUB)   \
    RCT_FLAGS_BOOLEAN_OPERATOR(>=, ORIG, SUB)

#define RCT_FLAGS(T)                                        \
    RCT_FLAGS_OPERATORS(T, T, T)                            \
    RCT_FLAGS_MEMBER_OPERATORS(T, T)                        \
    inline Flags<T> operator~(T t) { return Flags<T>::construct(~+t); }

#define RCT_FLAG_FRIEND(OP, TYPE)                                       \
    friend TYPE operator OP (TYPE, TYPE);                               \
    friend Flags<TYPE> operator OP ## =(Flags<TYPE> &, TYPE); \
    friend Flags<TYPE> operator OP(Flags<TYPE>, TYPE)

#define RCT_FLAGS_FRIEND(TYPE)                  \
    RCT_FLAG_FRIEND(|, TYPE);                   \
    RCT_FLAG_FRIEND(&, TYPE);                   \
    RCT_FLAG_FRIEND(^, TYPE);                   \
    RCT_FLAG_FRIEND(+, TYPE);                   \
    RCT_FLAG_FRIEND(-, TYPE);                   \
    RCT_FLAG_FRIEND(*, TYPE);                   \
    RCT_FLAG_FRIEND(%, TYPE);                   \
    RCT_FLAG_FRIEND(/, TYPE);                   \
    friend Flags<TYPE> operator~(TYPE)

#define RCT_SUBFLAGS(ORIGINAL, SUB)                                     \
    RCT_FLAGS_OPERATORS(ORIGINAL, SUB, ORIGINAL)                        \
    RCT_FLAGS_OPERATORS(SUB, ORIGINAL, ORIGINAL)                        \
    RCT_FLAGS_OPERATORS(SUB, SUB, ORIGINAL)                             \
    RCT_FLAGS_MEMBER_OPERATORS(SUB, ORIGINAL)                           \
    RCT_FLAGS_BOOLEAN_OPERATORS(ORIGINAL, SUB)                          \
    inline Flags<ORIGINAL> operator~(SUB t) { return Flags<ORIGINAL>::construct(~+t); }

#else
template <typename T>
class Flags
{
public:
    Flags(unsigned long long val = 0)
        : mValue(val)
    {}

    inline unsigned long long value() const { return mValue; }

    inline void clear() { mValue = 0; }
    static Flags<T> construct(uint64_t value) { Flags<T> ret; ret.mValue = value; return ret; }

    operator unsigned long long &() { return mValue; }
    operator unsigned long long() const { return mValue; }
    T cast() const { return cast<T>(); }
    template <typename Type> Type cast() const { return static_cast<Type>(mValue); }

    String toString() const
    {
        char buf[16];
        const int w = snprintf(buf, sizeof(buf), "0x%llx", cast<unsigned long long>());
        return String(buf, w);
    }
    void set(unsigned long long flag, bool on = true)
    {
        if (on) {
            mValue |= flag;
        } else {
            mValue &= ~flag;
        }
    }
    unsigned int test(unsigned long long flag) const { return mValue & flag; }
private:
    unsigned long long mValue;
};

#define RCT_FLAGS(T) //
#define RCT_FLAGS_FRIEND(T) //
#define RCT_SUBFLAGS(A, B) //

#endif
#endif

