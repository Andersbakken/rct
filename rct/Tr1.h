#ifndef Tr1_h
#define Tr1_h

#ifdef __GXX_EXPERIMENTAL_CXX0X__
#include <memory>
#include <unordered_set>
using std::shared_ptr;
using std::static_pointer_cast;
using std::weak_ptr;
using std::enable_shared_from_this;
using std::unordered_set;
#else
#include <tr1/memory>
#include <tr1/unordered_set>
using std::tr1::shared_ptr;
using std::tr1::static_pointer_cast;
using std::tr1::weak_ptr;
using std::tr1::enable_shared_from_this;
using std::tr1::unordered_set;
#endif // __GXX_EXPERIMENTAL_CXX0X__

#endif // Tr1_h
