#ifndef APPLY_H
#define APPLY_H

#include <tuple>
#include <type_traits>
#include <utility>

// magic courtesy of http://stackoverflow.com/questions/687490/how-do-i-expand-a-tuple-into-variadic-template-functions-arguments
template<size_t N>
struct Apply {
    template<typename F, typename T, typename... A>
    static inline auto apply(F && f, T && t, A &&... a)
        -> decltype(Apply<N-1>::apply(
                                      ::std::forward<F>(f), ::std::forward<T>(t),
                                      ::std::get<N-1>(::std::forward<T>(t)), ::std::forward<A>(a)...
                                      ))
    {
        return Apply<N-1>::apply(::std::forward<F>(f), ::std::forward<T>(t),
                                 ::std::get<N-1>(::std::forward<T>(t)), ::std::forward<A>(a)...
                                 );
    }
};

template<>
struct Apply<0> {
    template<typename F, typename T, typename... A>
    static inline auto apply(F && f, T &&, A &&... a)
        -> decltype(::std::forward<F>(f)(::std::forward<A>(a)...))
    {
        return ::std::forward<F>(f)(::std::forward<A>(a)...);
    }
};

template<typename F, typename T>
inline auto apply(F && f, T && t)
    -> decltype(Apply< ::std::tuple_size<
                    typename ::std::decay<T>::type
                    >::value>::apply(::std::forward<F>(f), ::std::forward<T>(t)))
{
    return Apply< ::std::tuple_size<
        typename ::std::decay<T>::type
        >::value>::apply(::std::forward<F>(f), ::std::forward<T>(t));
}


template<size_t N>
struct ApplyMove {
    template<typename F, typename T, typename... A>
    static inline auto applyMove(F && f, T && t, A &&... a)
        -> decltype(ApplyMove<N-1>::applyMove(
                                              ::std::forward<F>(f), ::std::forward <T>(t),
                                              ::std::get<N-1>(::std::forward<T>(t)), ::std::forward<A>(a)...
                                              ))
    {
        return ApplyMove<N-1>::applyMove(::std::forward<F>(f), ::std::forward<T>(t),
                                         ::std::get<N-1>(::std::forward<T>(t)), ::std::forward<A>(a)...
                                         );
    }
};

template<>
struct ApplyMove<0> {
    template<typename F, typename T, typename... A>
    static inline auto applyMove(F && f, T &&, A &&... a)
        -> decltype(::std::forward<F>(f)(::std::forward<A>(a)...))
    {
        return ::std::forward<F>(f)(::std::forward<A>(a)...);
    }
};

template<typename F, typename T>
inline auto applyMove(F && f, T && t)
    -> decltype(ApplyMove< ::std::tuple_size<
                    typename ::std::decay<T>::type
                    >::value>::applyMove(::std::forward<F>(f), ::std::move<T>(t)))
{
    return ApplyMove< ::std::tuple_size<
        typename ::std::decay<T>::type
        >::value>::applyMove(::std::forward<F>(f), ::std::move<T>(t));
}

#endif
