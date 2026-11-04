#pragma once
#include <WebServer.h>

// The pinned Arduino server parses multipart uploads byte by byte and can wait
// indefinitely on an idle peer. Keep its routing/authentication/response APIs,
// but bound requests and stream our single firmware file in blocks.
class BoundedWebServer : public WebServer {
public:
    using WebServer::WebServer;
    void handleClient() override;
    void abortClient() { _currentClient.stop(); }
protected:
    size_t _currentClientWrite(const char *data, size_t size) override;
    size_t _currentClientWrite_P(PGM_P data, size_t size) override;
private:
    uint32_t requestStart_ = 0, lastRead_ = 0, totalLimit_ = 3000, idleLimit_ = 1500;
    size_t remaining_ = 0;
    bool expired() const;
    bool line(String &out, size_t limit, bool body = false);
    bool bytes(uint8_t *out, size_t count);
    bool parse();
    bool multipart(const String &boundary);
    bool reject(int code, const char *message);
    void uploadEvent(HTTPUploadStatus status);
};
