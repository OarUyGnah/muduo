// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/AsyncLogging.h"
#include "muduo/base/LogFile.h"
#include "muduo/base/Timestamp.h"

#include <stdio.h>

using namespace muduo;

AsyncLogging::AsyncLogging(const string& basename,
                           off_t rollSize,
                           int flushInterval)
  : flushInterval_(flushInterval),
    running_(false),
    basename_(basename),
    rollSize_(rollSize),
    thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
    latch_(1),
    mutex_(),
    cond_(mutex_),
    currentBuffer_(new Buffer),
    nextBuffer_(new Buffer),
    buffers_()
{
  currentBuffer_->bzero();
  nextBuffer_->bzero();
  buffers_.reserve(16);
}

/*
  typedef muduo::detail::FixedBuffer<muduo::detail::kLargeBuffer> Buffer;
  typedef std::vector<std::unique_ptr<Buffer>> BufferVector;
  typedef BufferVector::value_type BufferPtr;
  其中缓冲区默认都为FixedBuffer<4000*1000>，其内部有一个char data_[4000*1000]的缓冲区
  通过currentBuffer(unique_ptr)调用FixedBuffer::append,append底层调用memcpy向缓冲区填充数据
*/
void AsyncLogging::append(const char* logline, int len)
{
  muduo::MutexLockGuard lock(mutex_);
  // currentBuffer_能够写入当前的logline，则直接append
  if (currentBuffer_->avail() > len)
  {
    currentBuffer_->append(logline, len);
  }
  else
  {
    // 不够写则将currentBuffer_放入指针vector中
    buffers_.push_back(std::move(currentBuffer_));
    // 有nextBuffer_则移动赋值给currentBuffer_
    if (nextBuffer_)
    {
      currentBuffer_ = std::move(nextBuffer_);
    }
    else
    {
      currentBuffer_.reset(new Buffer); // Rarely happens
    }
    currentBuffer_->append(logline, len);
    cond_.notify();
  }
}
/*
    buffers_对应threadFunc内部的buffersToWrite，二者每次循环进行swap，buffersToWrite负责写到Logfile中
    currentBuffer_、nextBuffer_对应newBuffer1、newBuffer2，当前二者被移动push_back由后二者swap替代上
    threadFunc内部主要就是进行buffer buffervector的更换和写入logfile，并从中获取

*/
void AsyncLogging::threadFunc()
{
  assert(running_ == true);
  latch_.countDown();
  LogFile output(basename_, rollSize_, false);
  // 两块备用buffer，缓冲区写满后用其替代
  BufferPtr newBuffer1(new Buffer);
  BufferPtr newBuffer2(new Buffer);
  newBuffer1->bzero();
  newBuffer2->bzero();
  // 存放buffer unique_ptr的vector，用于和buffers_交换
  BufferVector buffersToWrite;
  buffersToWrite.reserve(16);
  while (running_)
  {
    // 每轮都要确定buffer为空且buffervector空
    assert(newBuffer1 && newBuffer1->length() == 0);
    assert(newBuffer2 && newBuffer2->length() == 0);
    assert(buffersToWrite.empty());

    {
      muduo::MutexLockGuard lock(mutex_);
      if (buffers_.empty())  // unusual usage!
      {
        //有时间的等待，过了时间若没有也继续
        cond_.waitForSeconds(flushInterval_);
      }
      //缓冲区放入vector中
      buffers_.push_back(std::move(currentBuffer_));
      //替换新缓冲区
      currentBuffer_ = std::move(newBuffer1);
      //swap内部只是交换指针
      buffersToWrite.swap(buffers_);
      //如果nextbuffer也用了，则替换
      if (!nextBuffer_)
      {
        nextBuffer_ = std::move(newBuffer2);
      }
    }

    assert(!buffersToWrite.empty());
    //如果vector中要写的buffer超过25个，则只留前两个，剩余的丢弃
    if (buffersToWrite.size() > 25)
    {
      char buf[256];
      snprintf(buf, sizeof buf, "Dropped log messages at %s, %zd larger buffers\n",
               Timestamp::now().toFormattedString().c_str(),
               buffersToWrite.size()-2);
      fputs(buf, stderr);
      output.append(buf, static_cast<int>(strlen(buf)));
      buffersToWrite.erase(buffersToWrite.begin()+2, buffersToWrite.end());
    }
    //遍历并append
    for (const auto& buffer : buffersToWrite)
    {
      // FIXME: use unbuffered stdio FILE ? or use ::writev ?
      output.append(buffer->data(), buffer->length());
    }
    
    if (buffersToWrite.size() > 2)
    {
      // drop non-bzero-ed buffers, avoid trashing
      buffersToWrite.resize(2);
    }
    //如果newBuffer1和newBuffer2被currentBuffer_和nextBuffer_进行swap了，则从buffersToWrite取一块新的，并将其pop_back
    if (!newBuffer1)
    {
      assert(!buffersToWrite.empty());
      newBuffer1 = std::move(buffersToWrite.back());
      buffersToWrite.pop_back();
      newBuffer1->reset();
    }
    
    if (!newBuffer2)
    {
      assert(!buffersToWrite.empty());
      newBuffer2 = std::move(buffersToWrite.back());
      buffersToWrite.pop_back();
      newBuffer2->reset();
    }
    
    buffersToWrite.clear();
    output.flush();
  } //end while
  
  //Logfile::flush -> AppendFile::flush -> fflush()
  output.flush();
}

