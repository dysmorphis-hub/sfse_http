#include "dictionary.h"

namespace eve {

DictionaryStore& DictionaryStore::Instance() {
    static DictionaryStore instance;
    return instance;
}

DictionaryStore::Id DictionaryStore::Create() {
    std::lock_guard lock(_mutex);
    if (_entries.size() >= MAX_ENTRIES) {
        // Soft cap reached - leak-guard: drop the lowest-refcount entry.
        // In practice this should never fire if scripts call ReleaseDictionary.
        Id victim = INVALID;
        int min_rc = INT32_MAX;
        for (auto& [id, entry] : _entries) {
            if (entry.refcount < min_rc) {
                min_rc = entry.refcount;
                victim = id;
            }
        }
        if (victim != INVALID) _entries.erase(victim);
    }
    Id id = _next_id++;
    _entries[id] = Entry{ nlohmann::json::object(), 1 };
    return id;
}

void DictionaryStore::Retain(Id id) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it != _entries.end()) it->second.refcount++;
}

void DictionaryStore::Release(Id id) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it == _entries.end()) return;
    if (--it->second.refcount <= 0) {
        _entries.erase(it);
    }
}

void DictionaryStore::SetString(Id id, const std::string& key, const std::string& value) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it != _entries.end()) it->second.data[key] = value;
}

void DictionaryStore::SetInt(Id id, const std::string& key, int32_t value) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it != _entries.end()) it->second.data[key] = value;
}

void DictionaryStore::SetFloat(Id id, const std::string& key, float value) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it != _entries.end()) it->second.data[key] = value;
}

void DictionaryStore::SetBool(Id id, const std::string& key, bool value) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it != _entries.end()) it->second.data[key] = value;
}

void DictionaryStore::SetNested(Id id, const std::string& key, Id nestedId) {
    // Copy the nested dict's data inline (not a reference). This means
    // changes to the nested dict after SetNested are NOT reflected in the
    // parent - matches scripting expectations of value semantics.
    std::lock_guard lock(_mutex);
    auto parent = _entries.find(id);
    auto nested = _entries.find(nestedId);
    if (parent != _entries.end() && nested != _entries.end()) {
        parent->second.data[key] = nested->second.data;
    }
}

void DictionaryStore::SetStringArray(Id id, const std::string& key, const std::vector<std::string>& values) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it != _entries.end()) it->second.data[key] = values;
}

void DictionaryStore::SetIntArray(Id id, const std::string& key, const std::vector<int32_t>& values) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it != _entries.end()) it->second.data[key] = values;
}

void DictionaryStore::SetFloatArray(Id id, const std::string& key, const std::vector<float>& values) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it != _entries.end()) it->second.data[key] = values;
}

std::string DictionaryStore::GetString(Id id, const std::string& key) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it == _entries.end()) return "";
    auto val = it->second.data.find(key);
    if (val == it->second.data.end() || !val->is_string()) return "";
    return val->get<std::string>();
}

int32_t DictionaryStore::GetInt(Id id, const std::string& key) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it == _entries.end()) return 0;
    auto val = it->second.data.find(key);
    if (val == it->second.data.end() || !val->is_number_integer()) return 0;
    return val->get<int32_t>();
}

float DictionaryStore::GetFloat(Id id, const std::string& key) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it == _entries.end()) return 0.0f;
    auto val = it->second.data.find(key);
    if (val == it->second.data.end() || !val->is_number()) return 0.0f;
    return val->get<float>();
}

bool DictionaryStore::GetBool(Id id, const std::string& key) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it == _entries.end()) return false;
    auto val = it->second.data.find(key);
    if (val == it->second.data.end() || !val->is_boolean()) return false;
    return val->get<bool>();
}

DictionaryStore::Id DictionaryStore::GetNested(Id id, const std::string& key) {
    nlohmann::json nested_data;
    {
        std::lock_guard lock(_mutex);
        auto it = _entries.find(id);
        if (it == _entries.end()) return INVALID;
        auto val = it->second.data.find(key);
        if (val == it->second.data.end() || !val->is_object()) return INVALID;
        nested_data = *val;
    }
    // Allocate a new dict containing the nested data, return its ID
    Id new_id = Create();
    {
        std::lock_guard lock(_mutex);
        _entries[new_id].data = std::move(nested_data);
    }
    return new_id;
}

std::vector<std::string> DictionaryStore::GetStringArray(Id id, const std::string& key) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it == _entries.end()) return {};
    auto val = it->second.data.find(key);
    if (val == it->second.data.end() || !val->is_array()) return {};
    std::vector<std::string> result;
    for (const auto& e : *val) {
        if (e.is_string()) result.push_back(e.get<std::string>());
    }
    return result;
}

bool DictionaryStore::HasKey(Id id, const std::string& key) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it == _entries.end()) return false;
    return it->second.data.contains(key);
}

std::vector<std::string> DictionaryStore::GetKeys(Id id) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it == _entries.end()) return {};
    std::vector<std::string> keys;
    for (auto& [k, _v] : it->second.data.items()) keys.push_back(k);
    return keys;
}

std::string DictionaryStore::SerializeToJson(Id id) {
    std::lock_guard lock(_mutex);
    auto it = _entries.find(id);
    if (it == _entries.end()) return "{}";
    return it->second.data.dump();
}

DictionaryStore::Id DictionaryStore::CreateFromJson(const std::string& json_str) {
    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(json_str);
    } catch (const nlohmann::json::parse_error&) {
        return INVALID;
    }
    if (!parsed.is_object()) return INVALID;
    Id id = Create();
    {
        std::lock_guard lock(_mutex);
        _entries[id].data = std::move(parsed);
    }
    return id;
}

size_t DictionaryStore::Size() {
    std::lock_guard lock(_mutex);
    return _entries.size();
}

} // namespace eve
