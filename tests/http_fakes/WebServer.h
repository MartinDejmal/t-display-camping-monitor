#pragma once
#include "Arduino.h"
#include <memory>
#include <vector>
#include <functional>

enum HTTPUploadStatus {UPLOAD_FILE_START,UPLOAD_FILE_WRITE,UPLOAD_FILE_END,UPLOAD_FILE_ABORTED};
enum HTTPClientStatus {HC_NONE,HC_WAIT_READ,HC_WAIT_CLOSE};
enum HTTPMethod {HTTP_GET,HTTP_POST};
constexpr size_t HTTP_UPLOAD_BUFLEN=1436,CONTENT_LENGTH_NOT_SET=static_cast<size_t>(-2);
struct HTTPUpload {HTTPUploadStatus status;String name,filename,type;size_t totalSize,currentSize;uint8_t buf[HTTP_UPLOAD_BUFLEN];};
struct Peer {std::string input,output;size_t pos=0;uint32_t byteDelay=0;bool closed=false;};
extern std::shared_ptr<Peer> httpPending;
extern std::shared_ptr<Peer> httpSocketPeer;
class WiFiClient {
    std::shared_ptr<Peer> peer_;
public:
    WiFiClient()=default;
    explicit WiFiClient(std::shared_ptr<Peer> peer):peer_(peer){httpSocketPeer=peer;}
    int fd() const {return peer_?99:-1;}
    explicit operator bool() const {return static_cast<bool>(peer_);}
    bool connected() const {return peer_ && !peer_->closed;}
    int available() const {return peer_ ? peer_->input.size()-peer_->pos : 0;}
    void setTimeout(int){}
    int read(){uint8_t c;return read(&c,1)==1?c:-1;}
    int read(uint8_t *out,size_t count){
        if(!peer_ || peer_->pos==peer_->input.size())return -1;
        size_t n=std::min(count,peer_->input.size()-peer_->pos);
        memcpy(out,peer_->input.data()+peer_->pos,n);peer_->pos+=n;
        httpFakeMs+=peer_->byteDelay;return static_cast<int>(n);
    }
    void stop(){if(peer_)peer_->closed=true;peer_.reset();}
    void output(const String &s){if(peer_)peer_->output+=s.c_str();}
};
class WiFiServer {public:WiFiClient available(){auto p=httpPending;httpPending.reset();return WiFiClient(p);}};
class WebServer;
class RequestHandler {
public:
    std::function<bool(HTTPMethod,String)> match;
    std::function<void(WebServer&)> action;
    std::function<void(HTTPUpload&)> data;
    RequestHandler *next_=nullptr;
    RequestHandler *next(){return next_;}
    bool canHandle(HTTPMethod m,String uri){return match && match(m,uri);}
    bool canUpload(String){return static_cast<bool>(data);}
    void upload(WebServer&,String,HTTPUpload &u){data(u);}
};
class WebServer {
public:
    explicit WebServer(int){}
    virtual ~WebServer(){delete[] _currentArgs;}
    virtual void handleClient(){}
    void install(RequestHandler *h){_firstHandler=h;}
    String uri(){return _currentUri;}
    String arg(const char *key){for(int i=0;i<_currentArgCount;++i)if(_currentArgs[i].key==key)return _currentArgs[i].value;return "";}
    void sendHeader(const char*,const char*){}
    void send(int code,const char*,const String &body){_currentClient.output(String(std::to_string(code))+" "+body);}
protected:
    virtual size_t _currentClientWrite(const char*,size_t){return 0;}
    virtual size_t _currentClientWrite_P(PGM_P,size_t){return 0;}
    struct RequestArgument{String key,value;};
    WiFiServer _server;
    WiFiClient _currentClient;
    HTTPClientStatus _currentStatus=HC_NONE;
    HTTPMethod _currentMethod=HTTP_GET;
    unsigned long _statusChange=0;
    uint8_t _currentVersion=0;
    bool _chunked=false;
    size_t _contentLength=CONTENT_LENGTH_NOT_SET;
    int _clientContentLength=0,_headerKeysCount=0,_currentArgCount=0;
    String _hostHeader,_currentUri;
    RequestArgument *_currentHeaders=nullptr,*_currentArgs=nullptr;
    RequestHandler *_currentHandler=nullptr,*_firstHandler=nullptr;
    std::unique_ptr<HTTPUpload> _currentUpload;
    std::unique_ptr<int> _currentRaw;
    bool _collectHeader(const char*,const char*){return true;}
    void _parseArguments(const String&){delete[] _currentArgs;_currentArgs=new RequestArgument[1];_currentArgCount=0;}
    void _handleRequest(){if(_currentHandler && _currentHandler->action)_currentHandler->action(*this);}
};
