#pragma once
#include "WebServer.h"
#include <cerrno>
constexpr int MSG_DONTWAIT=1;
extern bool httpFakeSendBlocked;
extern uint32_t httpFakeSendDelay;
extern size_t httpFakeSendChunk;
inline int send(int,const void *data,size_t size,int) {
    if(httpFakeSendBlocked){errno=EWOULDBLOCK;return -1;}
    if(!httpSocketPeer || httpSocketPeer->closed)return 0;
    httpFakeMs+=httpFakeSendDelay;size=std::min(size,httpFakeSendChunk);
    httpSocketPeer->output.append(static_cast<const char*>(data),size);return static_cast<int>(size);
}
