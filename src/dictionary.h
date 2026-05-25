#pragma once

// Project Eve - SFSE_HTTP Dictionary Store
// ─────────────────────────────────────────────────────────────
// Thread-safe nlohmann::json-backed dictionary store with reference counting.
// Papyrus scripts allocate IDs via CreateDictionary, fill them via SetX,
// pass them to SendHttpPost (plugin takes an internal reference), and call
// ReleaseDictionary to release their own reference.

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace eve {

class DictionaryStore {
public:
    using Id = int32_t;
    static constexpr Id INVALID = -1;

    static DictionaryStore& Instance();

    Id Create();
    void Retain(Id id);
    void Release(Id id);

    // Setters
    void SetString(Id id, const std::string& key, const std::string& value);
    void SetInt(Id id, const std::string& key, int32_t value);
    void SetFloat(Id id, const std::string& key, float value);
    void SetBool(Id id, const std::string& key, bool value);
    void SetNested(Id id, const std::string& key, Id nestedId);
    void SetStringArray(Id id, const std::string& key, const std::vector<std::string>& values);
    void SetIntArray(Id id, const std::string& key, const std::vector<int32_t>& values);
    void SetFloatArray(Id id, const std::string& key, const std::vector<float>& values);

    // Getters
    std::string GetString(Id id, const std::string& key);
    int32_t GetInt(Id id, const std::string& key);
    float GetFloat(Id id, const std::string& key);
    bool GetBool(Id id, const std::string& key);
    Id GetNested(Id id, const std::string& key);
    std::vector<std::string> GetStringArray(Id id, const std::string& key);
    bool HasKey(Id id, const std::string& key);
    std::vector<std::string> GetKeys(Id id);

    // Serialization
    std::string SerializeToJson(Id id);
    Id CreateFromJson(const std::string& json_str);

    // Diagnostics
    size_t Size();

private:
    DictionaryStore() = default;
    DictionaryStore(const DictionaryStore&) = delete;
    DictionaryStore& operator=(const DictionaryStore&) = delete;

    struct Entry {
        nlohmann::json data;
        int refcount = 1;
    };

    std::mutex _mutex;
    std::unordered_map<Id, Entry> _entries;
    Id _next_id = 1;

    // Soft cap with LRU eviction guard
    static constexpr size_t MAX_ENTRIES = 10000;
};

} // namespace eve
