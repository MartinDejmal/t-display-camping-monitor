#include "bounded_webserver.h"
#include <cassert>
#include <iostream>
#include <random>
uint32_t httpFakeMs=0;
std::shared_ptr<Peer> httpPending;
std::shared_ptr<Peer> httpSocketPeer;
bool httpFakeSendBlocked=false;
uint32_t httpFakeSendDelay=0;
size_t httpFakeSendChunk=1000000;
class WriteProbe : public BoundedWebServer {
public:
    WriteProbe():BoundedWebServer(80){}
    void peer(std::shared_ptr<Peer> p){_currentClient=WiFiClient(p);}
    size_t write(const char *p,size_t n){return _currentClientWrite(p,n);}
};

struct Fixture {
    BoundedWebServer server{80};
    RequestHandler handler;
    std::vector<uint8_t> file;
    std::vector<HTTPUploadStatus> events;
    unsigned calls=0;
    Fixture(){
        handler.match=[](HTTPMethod,String){return true;};
        handler.action=[&](WebServer &s){++calls;s.send(200,"application/json",s.arg("plain"));};
        handler.data=[&](HTTPUpload &u){events.push_back(u.status);if(u.status==UPLOAD_FILE_WRITE)file.insert(file.end(),u.buf,u.buf+u.currentSize);};
        server.install(&handler);
    }
    std::shared_ptr<Peer> run(const std::string &request,uint32_t delay=0){
        auto p=std::make_shared<Peer>();p->input=request;p->byteDelay=delay;httpPending=p;
        server.handleClient();return p;
    }
};
std::string post(const std::string &path,const std::string &body,const std::string &type="application/json"){
    return "POST "+path+" HTTP/1.1\r\nHost: logger\r\nContent-Length: "+std::to_string(body.size())+"\r\nContent-Type: "+type+"\r\n\r\n"+body;
}
std::string form(const std::string &data){
    return "--test\r\nContent-Disposition: form-data; name=\"firmware\"; filename=\"firmware.bin\"\r\nContent-Type: application/octet-stream\r\n\r\n"+data+"\r\n--test--\r\n";
}
int main(){
    WriteProbe writer;auto peer=std::make_shared<Peer>();writer.peer(peer);
    assert(writer.write("test",4)==4 && peer->output=="test");
    httpFakeSendBlocked=true;auto writeStart=httpFakeMs;
    assert(writer.write("test",4)==0 && peer->closed && httpFakeMs-writeStart==1500);
    httpFakeSendBlocked=false;
    peer=std::make_shared<Peer>();writer.peer(peer);httpFakeSendDelay=100;httpFakeSendChunk=1;writeStart=httpFakeMs;
    assert(writer.write(std::string(100,'a').c_str(),100)==15 && peer->closed && httpFakeMs-writeStart==1500);
    httpFakeSendDelay=0;httpFakeSendChunk=1000000;
    Fixture f;
    assert(f.run("GET /api/status HTTP/1.1\r\nHost: logger\r\n\r\n")->output=="200 ");
    assert(f.run(post("/api/config","{\"brightness\":42}"))->output=="200 {\"brightness\":42}");
    auto before=f.calls;
    for(const auto &request:std::vector<std::string>{
        "GET / HTTP/2\r\n\r\n","GET http://other/ HTTP/1.1\r\n\r\n",
        "POST /api/config HTTP/1.1\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\n{}",
        "POST /api/config HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n",
        "POST /api/config HTTP/1.1\r\nContent-Length: 4294967295\r\n\r\n",
        post("/api/config",std::string(4097,'a')),
        post("/api/update","{}"), // must not dereference an absent HTTPUpload
        post("/api/config","x","multipart/form-data; boundary=test"),
        "GET / HTTP/1.1\r\nX: "+std::string(1025,'x')+"\r\n\r\n",
        post("/api/config",std::string("a\0b",3))
    }){
        auto p=f.run(request);assert(p->output.substr(0,1)=="4");assert(f.calls==before);
    }
    // These peers deliberately stay connected after sending an incomplete request.
    auto t=httpFakeMs;
    f.run("POST /api/config HTTP/1.1\r\nContent-Type: application/json\r\nContent-Length: 10\r\n\r\n{");
    assert(httpFakeMs-t<=3000 && f.calls==before);
    t=httpFakeMs;
    f.run("GET / HTTP/1.1\r\nX: unfinished");
    assert(httpFakeMs-t<=3000 && f.calls==before);
    // A trickling peer cannot extend the total request budget indefinitely.
    t=httpFakeMs;f.run("GET / HTTP/1.1\r\nX: "+std::string(600,'a')+"\r\n\r\n",10);
    assert(httpFakeMs-t<=3010 && f.calls==before);
    // Wrap-safe timeout arithmetic.
    httpFakeMs=UINT32_MAX-500;t=httpFakeMs;
    f.run("GET / HTTP/1.1\r\nX: unfinished");assert(httpFakeMs-t<=3000);
    std::string binary(20000,'x');binary[0]=static_cast<char>(0xe9);binary[500]=0;
    binary.replace(1000,8,"\r\n--test"); // boundary-like bytes inside firmware are data
    auto p=f.run(post("/api/update",form(binary),"multipart/form-data; boundary=test"));
    assert(p->output=="200 " && f.file==std::vector<uint8_t>(binary.begin(),binary.end()));
    assert(f.events.front()==UPLOAD_FILE_START && f.events.back()==UPLOAD_FILE_END);
    assert(f.events.size()>3);
    f.events.clear();f.file.clear();
    auto partial=post("/api/update",form(binary),"multipart/form-data; boundary=test");partial.resize(partial.size()-1000);
    t=httpFakeMs;f.run(partial);assert(httpFakeMs-t<=10000 && f.events.back()==UPLOAD_FILE_ABORTED);
    // Invalid closing boundary aborts instead of completing the image.
    auto bad=form(binary);bad.back()='x';f.events.clear();f.run(post("/api/update",bad,"multipart/form-data; boundary=test"));
    assert(f.events.back()==UPLOAD_FILE_ABORTED);
    // Text-only multipart forms are outside the endpoint contract and terminate.
    assert(f.run(post("/api/update","--test\r\nContent-Disposition: form-data; name=\"x\"\r\n\r\nunfinished","multipart/form-data; boundary=test"))->output.substr(0,3)=="400");
    // The same server recovers after malformed/stalled connections.
    assert(f.run("GET / HTTP/1.0\r\n\r\n")->output=="200 ");
    std::mt19937 random(42);
    for(int i=0;i<200;++i){std::string r;for(int j=0,n=random()%800;j<n;++j)r+=static_cast<char>(random()%128);f.run(r);}
    std::cout<<"HTTP: actual bounded parser handles binary streaming, stalled/trickling peers, malformed/oversized requests, aborts and recovery\n";
}
