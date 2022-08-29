// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADLOCAL_H
#define MUDUO_BASE_THREADLOCAL_H

#include "muduo/base/Mutex.h"
#include "muduo/base/noncopyable.h"

#include <pthread.h>

namespace muduo
{
/*
   在多线程程序中，所有线程共享进程中的变量。
   现在有一全局变量，所有线程都可以使用它，改变它的值。
   而如果每个线程希望能单独拥有它，那么就需要使用线程存储了。
   表面上看起来这是一个全局变量，所有线程都可以使用它，而它的值在每一个线程中又是单独存储的。这就是线程存储的意义。
*/
template<typename T>
class ThreadLocal : noncopyable
{
 public:
  ThreadLocal()
  {
    // 初始化并设置清理函数
    // 当每个线程结束时，系统将调用这个函数来释放绑定在这个键上的内存块。
    MCHECK(pthread_key_create(&pkey_, &ThreadLocal::destructor));
  }

  ~ThreadLocal()
  {
    MCHECK(pthread_key_delete(pkey_));
  }

  T& value()
  {
    T* perThreadValue = static_cast<T*>(pthread_getspecific(pkey_));
    if (!perThreadValue)
    {
      T* newObj = new T();
      MCHECK(pthread_setspecific(pkey_, newObj));
      perThreadValue = newObj;
    }
    return *perThreadValue;
  }

 private:
  // 参数x为pthread_setspecific的那块内存，进行delete
  static void destructor(void *x)
  {
    T* obj = static_cast<T*>(x);
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    T_must_be_complete_type dummy; (void) dummy;
    delete obj;
  }

 private:
  pthread_key_t pkey_;
};

}  // namespace muduo

#endif  // MUDUO_BASE_THREADLOCAL_H
