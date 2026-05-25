#pragma once

// Project Eve - SFSE_HTTP Reply Queue (Polling-Based)
// ─────────────────────────────────────────────────────────────
// Worker threads put parsed HTTP replies here keyed by event_name.
// Papyrus side polls via SFSE_HTTP.PollReply(eventName) which returns
// the dict_id of the next pending reply (or -1 if none).
//
// Why polling instead of Papyrus mod events: CommonLibSF's SendEvent
// is a 6-parameter monster designed for in-game custom events, not
// straightforward C++→Papyrus dispatch. Papyrus polling via
// RegisterForUpdate(0.1) is simpler and adds only ~100ms latency.

#include <cstdint>
#include <chrono>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>

#include "dictionary.h"
#include "http_client.h"

namespace eve {

struct QueuedReply {
    int status_code = 0;
    DictionaryStore::Id dict_id = DictionaryStore::INVALID;
    std::string url;
    std::chrono::steady_clock::time_point timestamp;
};

class ReplyQueue {
public:
    static ReplyQueue& Instance();

    // Worker-thread side: enqueue a parsed reply for the named event
    void Enqueue(HttpResult result);

    // Main-thread side: pop the oldest reply for this event_name,
    // returns INVALID dict id if none pending.
    // The returned dict has __status and __url injected so Papyrus can read them.
    DictionaryStore::Id PollNext(const std::string& event_name);

    // Diagnostics
    size_t PendingCount();

    // Auto-evict replies older than this many seconds (memory hygiene)
    void EvictStale(std::chrono::seconds max_age = std::chrono::seconds{30});

private:
    ReplyQueue() = default;
    ReplyQueue(const ReplyQueue&) = delete;
    ReplyQueue& operator=(const ReplyQueue&) = delete;

    std::mutex _mutex;
    std::unordered_map<std::string, std::queue<QueuedReply>> _queues;
};

} // namespace eve
