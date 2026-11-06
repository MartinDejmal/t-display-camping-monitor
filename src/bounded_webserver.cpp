#include "bounded_webserver.h"
#include <algorithm>
#include <lwip/sockets.h>
#include <errno.h>
#include <new>

namespace {
constexpr size_t MAX_JSON = 4096;
constexpr size_t MAX_MULTIPART = 0x160000 + 4096; // pinned OTA slot plus envelope
bool decimal(const String &s, size_t &value) {
    if (s.isEmpty() || s.length() > 10) return false;
    value = 0;
    for (char c : s) {
        if (c < '0' || c > '9' || value > (MAX_MULTIPART - (c - '0')) / 10) return false;
        value = value * 10 + c - '0';
    }
    return true;
}
bool attribute(const String &header, const char *name, String &value) {
    const String key = String("; ") + name + "=\"";
    const int begin = header.indexOf(key);
    if (begin < 0) return false;
    const int start = begin + key.length(), end = header.indexOf('"', start);
    if (end < 0 || header.indexOf(key, end) >= 0) return false;
    value = header.substring(start, end);
    return true;
}
}
size_t BoundedWebServer::_currentClientWrite(const char *data, size_t size) {
    // A client that stops reading must not hold up the measurement loop. The
    // Arduino WiFiClient writer restarts its retry counter after every byte.
    const uint32_t started = millis();
    size_t done = 0;
    while (done < size && _currentClient && millis() - started < 1500) {
        const int n = ::send(_currentClient.fd(), data + done, size - done, MSG_DONTWAIT);
        if (n > 0) done += n;
        else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) delay(1);
        else break;
    }
    if (done != size) abortClient(); // no successful terminator for partial CSV/JSON
    return done;
}
size_t BoundedWebServer::_currentClientWrite_P(PGM_P data, size_t size) {
    // ESP32 flash is memory mapped; no intermediate RAM copy is necessary.
    return _currentClientWrite(data, size);
}
bool BoundedWebServer::expired() const {
    return millis() - requestStart_ >= totalLimit_ || millis() - lastRead_ >= idleLimit_;
}
bool BoundedWebServer::line(String &out, size_t limit, bool body) {
    out = "";
    while (!expired()) {
        if (body && !remaining_) return false;
        const int c = _currentClient.read();
        if (c < 0) {
            if (!_currentClient.connected()) return false;
            delay(1); continue;
        }
        lastRead_ = millis();
        if (body) --remaining_;
        if (c == '\n') {
            if (!out.endsWith("\r")) return false;
            out.remove(out.length() - 1);
            return true;
        }
        if (c == 0 || out.length() >= limit) return false;
        out += static_cast<char>(c);
    }
    return false;
}
bool BoundedWebServer::bytes(uint8_t *out, size_t count) {
    if (count > remaining_) return false;
    size_t done = 0;
    while (done < count && !expired()) {
        int n = _currentClient.read(out + done, count - done);
        if (n > 0) {
            done += n; remaining_ -= n; lastRead_ = millis();
        } else {
            if (!_currentClient.connected()) return false;
            delay(1);
        }
    }
    return done == count;
}
bool BoundedWebServer::reject(int code, const char *message) {
    _contentLength = CONTENT_LENGTH_NOT_SET;
    _chunked = false;
    sendHeader("Cache-Control", "no-store");
    send(code, "application/json", String("{\"error\":\"") + message + "\"}");
    return false;
}
void BoundedWebServer::uploadEvent(HTTPUploadStatus status) {
    _currentUpload->status = status;
    if (_currentHandler && _currentHandler->canUpload(_currentUri))
        _currentHandler->upload(*this, _currentUri, *_currentUpload);
}
bool BoundedWebServer::multipart(const String &boundary) {
    String part;
    if (!line(part, 128, true) || part != "--" + boundary) return reject(400, "Invalid multipart boundary");
    String disposition, type;
    size_t headerBytes = 0;
    for (unsigned i = 0; ; ++i) {
        if (i == 8 || !line(part, 512, true)) return reject(400, "Invalid multipart headers");
        headerBytes += part.length();
        if (headerBytes > 1024) return reject(413, "Multipart headers too large");
        if (part.isEmpty()) break;
        int colon = part.indexOf(':');
        if (colon < 1) return reject(400, "Invalid multipart header");
        String key = part.substring(0, colon), value = part.substring(colon + 1); value.trim();
        if (key.equalsIgnoreCase("Content-Disposition") && disposition.isEmpty()) disposition = value;
        else if (key.equalsIgnoreCase("Content-Type") && type.isEmpty()) type = value;
        else return reject(400, "Unsupported multipart header");
    }
    String name, filename;
    if (!disposition.startsWith("form-data;") || !attribute(disposition, "name", name) ||
        !attribute(disposition, "filename", filename) || name != "firmware" || filename.length() > 128)
        return reject(400, "A single firmware file is required");
    const String tail = "\r\n--" + boundary + "--\r\n";
    if (remaining_ <= tail.length()) return reject(400, "Empty firmware");
    size_t fileBytes = remaining_ - tail.length();
    if (fileBytes > 0x160000) return reject(413, "Firmware exceeds OTA slot");
    _currentUpload.reset(new HTTPUpload());
    _currentUpload->name = name; _currentUpload->filename = filename; _currentUpload->type = type;
    _currentUpload->totalSize = _currentUpload->currentSize = 0;
    uploadEvent(UPLOAD_FILE_START);
    while (fileBytes && _currentClient.connected()) {
        const size_t n = std::min(fileBytes, static_cast<size_t>(HTTP_UPLOAD_BUFLEN));
        if (!bytes(_currentUpload->buf, n)) break;
        _currentUpload->currentSize = n;
        uploadEvent(UPLOAD_FILE_WRITE);
        _currentUpload->totalSize += n;
        fileBytes -= n;
        yield();
    }
    if (fileBytes || !_currentClient.connected()) { uploadEvent(UPLOAD_FILE_ABORTED); return false; }
    uint8_t closing[128];
    if (tail.length() > sizeof(closing) || !bytes(closing, tail.length()) ||
        memcmp(closing, tail.c_str(), tail.length()) != 0) {
        uploadEvent(UPLOAD_FILE_ABORTED);
        return reject(400, "Incomplete multipart upload");
    }
    _currentUpload->currentSize = 0;
    uploadEvent(UPLOAD_FILE_END);
    return true;
}
bool BoundedWebServer::parse() {
    requestStart_ = lastRead_ = millis(); totalLimit_ = 3000; idleLimit_ = 1500;
    _chunked = false; _currentVersion = 1;
    _contentLength = CONTENT_LENGTH_NOT_SET; _clientContentLength = 0;
    _hostHeader = ""; _currentHandler = nullptr;
    for (int i = 0; i < _headerKeysCount; ++i) _currentHeaders[i].value = "";
    String request;
    if (!line(request, 512)) return reject(400, "Invalid request line");
    const int first = request.indexOf(' '), last = request.lastIndexOf(' ');
    if (first <= 0 || last <= first) return reject(400, "Invalid request line");
    const String method = request.substring(0, first), version = request.substring(last + 1);
    if (version != "HTTP/1.1" && version != "HTTP/1.0") return reject(400, "Unsupported HTTP version");
    _currentVersion = version == "HTTP/1.1" ? 1 : 0;
    if (method != "GET" && method != "POST") return reject(405, "Use GET or POST");
    _currentMethod = method == "GET" ? HTTP_GET : HTTP_POST;
    String target = request.substring(first + 1, last);
    if (!target.startsWith("/")) return reject(400, "Invalid target");
    int query = target.indexOf('?');
    _currentUri = query < 0 ? target : target.substring(0, query);
    _parseArguments(query < 0 ? String("") : target.substring(query + 1));
    String contentType, contentLength; size_t headerBytes = 0;
    bool sawLength = false, sawType = false;
    for (unsigned i = 0; ; ++i) {
        if (i == 32 || !line(request, 1024)) return reject(400, "Invalid HTTP headers");
        headerBytes += request.length();
        if (headerBytes > 8192) return reject(431, "HTTP headers too large");
        if (request.isEmpty()) break;
        const int colon = request.indexOf(':');
        if (colon <= 0) return reject(400, "Invalid HTTP header");
        String key = request.substring(0, colon), value = request.substring(colon + 1); value.trim();
        if (key.equalsIgnoreCase("Content-Length")) {
            if (sawLength || value.isEmpty()) return reject(400, "Invalid or duplicate length");
            contentLength = value; sawLength = true;
        } else if (key.equalsIgnoreCase("Content-Type")) {
            if (sawType || value.isEmpty()) return reject(400, "Invalid or duplicate type");
            contentType = value; sawType = true;
        } else if (key.equalsIgnoreCase("Transfer-Encoding") || key.equalsIgnoreCase("Expect"))
            return reject(400, "Unsupported transfer encoding or expectation");
        if (key.equalsIgnoreCase("Host")) _hostHeader = value;
        _collectHeader(key.c_str(), value.c_str());
    }
    remaining_ = 0;
    if (!contentLength.isEmpty() && !decimal(contentLength, remaining_)) return reject(413, "Invalid or excessive length");
    _clientContentLength = remaining_;
    for (auto *h = _firstHandler; h; h = h->next())
        if (h->canHandle(_currentMethod, _currentUri)) { _currentHandler = h; break; }
    if (_currentMethod == HTTP_GET) return remaining_ ? reject(400, "GET body is unsupported") : true;
    if (_currentUri == "/api/update") {
        const String prefix = "multipart/form-data; boundary=";
        if (!contentType.startsWith(prefix) || !_currentHandler || !_currentHandler->canUpload(_currentUri))
            return reject(400, "Use a multipart firmware file");
        String boundary = contentType.substring(prefix.length());
        if (boundary.startsWith("\"") && boundary.endsWith("\"")) boundary = boundary.substring(1, boundary.length() - 1);
        if (boundary.isEmpty() || boundary.length() > 70) return reject(400, "Invalid boundary");
        for (char c : boundary) if (c < 33 || c > 126 || c == '"') return reject(400, "Invalid boundary");
        totalLimit_ = 180000; idleLimit_ = 10000;
        return multipart(boundary);
    }
    if (remaining_ > MAX_JSON) return reject(413, "JSON body too large");
    const int parameter = contentType.indexOf(';');
    const String mediaType = parameter < 0 ? contentType : contentType.substring(0, parameter);
    if (!contentType.isEmpty() && !mediaType.equalsIgnoreCase("application/json")) return reject(415, "Use application/json");
    const size_t size = remaining_;
    std::unique_ptr<uint8_t[]> body(new (std::nothrow) uint8_t[size + 1]);
    if (!body) return reject(503, "Insufficient memory");
    if (!bytes(body.get(), size)) return reject(408, "Request body timed out");
    if (memchr(body.get(), 0, size)) return reject(400, "Invalid JSON body");
    body[size] = 0;
    _currentArgs[_currentArgCount].key = "plain";
    _currentArgs[_currentArgCount++].value = reinterpret_cast<const char *>(body.get());
    return true;
}
void BoundedWebServer::handleClient() {
    if (_currentStatus == HC_NONE) {
        _currentClient = _server.available();
        if (!_currentClient) return;
        _currentStatus = HC_WAIT_READ; _statusChange = millis();
        _currentClient.setTimeout(1);
    }
    if (_currentClient.connected() && !_currentClient.available() && millis() - _statusChange < 1500) return;
    if (_currentClient.available() && parse()) _handleRequest();
    // Close the owning client, rather than a temporary returned by client().
    _currentClient.stop(); _currentStatus = HC_NONE;
    _currentUpload.reset(); _currentRaw.reset(); _currentHandler = nullptr;
}
