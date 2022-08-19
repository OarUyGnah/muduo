#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include <map>

using namespace muduo;
using namespace muduo::net;

typedef std::map<string, string> UserMap;
UserMap users;

string getUser(const string &user)
{
  string result = "No such user";
  UserMap::iterator it = users.find(user);
  if (it != users.end())
  {
    result = it->second;
  }
  return result;
}

void onMessage(const TcpConnectionPtr &conn,
               Buffer *buf,
               Timestamp receiveTime)
{
  const char *crlf = buf->findCRLF();
  if (crlf)
  {
    string user(buf->peek(), crlf);
    conn->send(getUser(user) + "\r\n");
    //跳过\r\n到下一行
    buf->retrieveUntil(crlf + 2);
    conn->shutdown();
  }
}

//监听1079,在telnet localhost 1079 输入oar
//服务端返回Happy and well

int main()
{
  users["oar"] = "Happy and well";
  EventLoop loop;
  TcpServer server(&loop, InetAddress(1079), "Finger");
  server.setMessageCallback(onMessage);
  server.start();
  loop.loop();
}
