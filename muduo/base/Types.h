#ifndef MUDUO_BASE_TYPES_H
#define MUDUO_BASE_TYPES_H

#include <stdint.h>
#include <string.h>  // memset
#include <string>

#ifndef NDEBUG
#include <assert.h>
#endif

///
/// The most common stuffs.
///
namespace muduo
{

using std::string;

inline void memZero(void* p, size_t n)
{
  memset(p, 0, n);
}
  
// 使用implicit_cast 作为static_cast 或const_cast 的安全版本
// 用于在类型层次结构中向上转换
template<typename To, typename From>
inline To implicit_cast(From const &f)
{
  return f;
}

// 下行转换，使用高效的static_cast,先用dynamic_cast确定能否下行转换,能的话再用static_cast
template<typename To, typename From>     // use like this: down_cast<T*>(foo);
inline To down_cast(From* f)                     // so we only accept pointers
{
  // Ensures that To is a sub-type of From *.  This test is here only
  // for compile-time type checking, and has no overhead in an
  // optimized build at run-time, as it will be optimized away
  // completely.
  if (false)
  {
    implicit_cast<From*, To>(0);
  }

  // assert依赖一个NDEBUG的宏，如果NDEBUG已定义，assert啥也不干，如果没有定义，assert才会发挥作用
  // dynamic_cast失败返回NULL
#if !defined(NDEBUG) && !defined(GOOGLE_PROTOBUF_NO_RTTI)
  assert(f == NULL || dynamic_cast<To>(f) != NULL);  // RTTI: debug mode only!
#endif
  return static_cast<To>(f);
}

}  // namespace muduo

#endif  // MUDUO_BASE_TYPES_H
