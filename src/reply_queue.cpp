#include "reply_queue.h"

namespace eve {

ReplyQueue& ReplyQueue::Instance() {
    static ReplyQueue instance;
    return instance;
}

void ReplyQueue::Enqueue(HttpResult result) {
    // Parse body into a dict; inject __status and __url so Papyrus can
    // recover them via the same dict that holds the JSON response.
    DictionaryStore::Id dict_id = DictionaryStore::INVALID;
    if (!result.body.empty()) {
        dict_id = DictionaryStore::Instance().CreateFromJson(result.body);
    }
    // If body wasn't valid JSON we still create an empty dict so Papyrus
    // gets a non-INVALID id - status code carries the failure reason.
    if (dict_id == DictionaryStore::INVALID) {
        dict_id = DictionaryStore::Instance().Create();
    }
    DictionaryStore::Instance().SetInt(dict_id, "__status", result.status_code);
    DictionaryStore::Instance().SetString(dict_id, "__url", result.url);

    QueuedReply q;
    q.status_code = result.status_code;
    q.dict_id = dict_id;
    q.url = std::move(result.url);
    q.timestamp = std::chrono::steady_clock::now();

    std::lock_guard lock(_mutex);
    _queues[result.event_name].push(std::move(q));
}

DictionaryStore::Id ReplyQueue::PollNext(const std::string& event_name) {
    std::lock_guard lock(_mutex);
    auto it = _queues.find(event_name);
    if (it == _queues.end() || it->second.empty()) {
        return DictionaryStore::INVALID;
    }
    auto reply = std::move(it->second.front());
    it->second.pop();
    return reply.dict_id;
}

size_t ReplyQueue::PendingCount() {
    std::lock_guard lock(_mutex);
    size_t total = 0;
    for (const auto& [_name, q] : _queues) total += q.size();
    return total;
}

void ReplyQueue::EvictStale(std::chrono::seconds max_age) {
    const auto cutoff = std::chrono::steady_clock::now() - max_age;
    std::lock_guard lock(_mutex);
    for (auto& [_name, queue] : _queues) {
        // Pop stale entries from the front (FIFO order preserves age ordering)
        while (!queue.empty() && queue.front().timestamp < cutoff) {
            DictionaryStore::Instance().Release(queue.front().dict_id);
            queue.pop();
        }
    }
}

} // namespace eve
