#include "http_client.h"

#include <cpr/cpr.h>

namespace eve {

HttpClient& HttpClient::Instance() {
    static HttpClient instance;
    return instance;
}

void HttpClient::Start(size_t worker_count) {
    _stop = false;
    for (size_t i = 0; i < worker_count; ++i) {
        _workers.emplace_back([this] { WorkerLoop(); });
    }
}

void HttpClient::Stop() {
    {
        std::lock_guard lock(_queue_mutex);
        _stop = true;
    }
    _queue_cv.notify_all();
    for (auto& t : _workers) {
        if (t.joinable()) t.join();
    }
    _workers.clear();
}

void HttpClient::PostJson(const std::string& url,
                          const std::string& json_body,
                          const std::string& event_name,
                          HttpCallback cb) {
    {
        std::lock_guard lock(_queue_mutex);
        _queue.push(Request{ Request::Method::POST, url, json_body, event_name, std::move(cb) });
    }
    _queue_cv.notify_one();
}

void HttpClient::Get(const std::string& url,
                     const std::string& event_name,
                     HttpCallback cb) {
    {
        std::lock_guard lock(_queue_mutex);
        _queue.push(Request{ Request::Method::GET, url, "", event_name, std::move(cb) });
    }
    _queue_cv.notify_one();
}

void HttpClient::SetGlobalHeader(const std::string& name, const std::string& value) {
    std::lock_guard lock(_headers_mutex);
    for (auto& [n, v] : _global_headers) {
        if (n == name) {
            v = value;
            return;
        }
    }
    _global_headers.emplace_back(name, value);
}

void HttpClient::WorkerLoop() {
    while (true) {
        Request req;
        {
            std::unique_lock lock(_queue_mutex);
            _queue_cv.wait(lock, [this] { return _stop || !_queue.empty(); });
            if (_stop && _queue.empty()) return;
            req = std::move(_queue.front());
            _queue.pop();
        }
        Execute(req);
    }
}

void HttpClient::Execute(const Request& req) {
    HttpResult result;
    result.url = req.url;
    result.event_name = req.event_name;

    cpr::Header headers;
    {
        std::lock_guard lock(_headers_mutex);
        for (const auto& [n, v] : _global_headers) headers[n] = v;
    }
    if (req.method == Request::Method::POST) {
        headers["Content-Type"] = "application/json";
    }

    cpr::Timeout timeout{ _default_timeout_ms.load() };

    try {
        cpr::Response resp;
        if (req.method == Request::Method::POST) {
            resp = cpr::Post(cpr::Url{ req.url },
                             cpr::Body{ req.body },
                             headers,
                             timeout);
        } else {
            resp = cpr::Get(cpr::Url{ req.url }, headers, timeout);
        }

        if (resp.error.code != cpr::ErrorCode::OK) {
            result.status_code = 0;  // network error
        } else {
            result.status_code = static_cast<int>(resp.status_code);
            result.body = resp.text;
        }
    } catch (...) {
        result.status_code = 0;
    }

    if (req.callback) {
        try {
            req.callback(std::move(result));
        } catch (...) {
            // Callback threw - swallow; we don't want a worker thread death
        }
    }
}

} // namespace eve
