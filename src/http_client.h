#pragma once

// Project Eve - SFSE_HTTP Async HTTP Client
// ─────────────────────────────────────────────────────────────
// Worker pool of N threads. Each runs blocking cpr::Post/Get and reports
// the result via a callback on the same worker thread. The callback is
// expected to push to a main-thread queue (see mod_event.h) before
// touching game state.

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace eve {

struct HttpResult {
    int status_code = 0;     // 0 = network/timeout error
    std::string body;        // response body (may be empty)
    std::string url;         // echo of the URL for correlation
    std::string event_name;  // Papyrus mod event to fire with this result
};

using HttpCallback = std::function<void(HttpResult)>;

class HttpClient {
public:
    static HttpClient& Instance();

    void Start(size_t worker_count = 4);
    void Stop();

    void PostJson(const std::string& url,
                  const std::string& json_body,
                  const std::string& event_name,
                  HttpCallback cb);

    void Get(const std::string& url,
             const std::string& event_name,
             HttpCallback cb);

    void SetDefaultTimeoutMs(int ms) { _default_timeout_ms = ms; }
    void SetGlobalHeader(const std::string& name, const std::string& value);

private:
    HttpClient() = default;
    ~HttpClient() { Stop(); }
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    struct Request {
        enum class Method { POST, GET } method;
        std::string url;
        std::string body;       // for POST
        std::string event_name;
        HttpCallback callback;
    };

    void WorkerLoop();
    void Execute(const Request& req);

    std::atomic<int> _default_timeout_ms = 5000;
    std::vector<std::pair<std::string, std::string>> _global_headers;
    std::mutex _headers_mutex;

    std::vector<std::thread> _workers;
    std::queue<Request> _queue;
    std::mutex _queue_mutex;
    std::condition_variable _queue_cv;
    std::atomic<bool> _stop = false;
};

} // namespace eve
