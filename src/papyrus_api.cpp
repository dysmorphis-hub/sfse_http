#include "papyrus_api.h"

// CommonLibSF umbrella header pulls in PCH + REL/REX + RE IDs + BSFixedString.
// MUST come before our project headers to ensure the std::format / concept
// machinery is set up correctly.
#include "SFSE/SFSE.h"
#include "RE/M/MemoryManager.h"  // RE::malloc/free + SF_HEAP_REDEFINE_NEW macro
#include "RE/A/Array.h"          // Full RE::BSScript::Array definition (not forward decl)
#include "RE/B/BSScriptUtil.h"
#include "RE/V/VirtualMachine.h"

// PlaceFormAtRef dependencies
#include "RE/T/TESObjectREFR.h"
#include "RE/T/TESObjectCELL.h"
#include "RE/T/TESForm.h"
#include "RE/P/PlayerCharacter.h"

#include "dictionary.h"
#include "http_client.h"
#include "reply_queue.h"

#include <optional>
#include <variant>
#include <set>
#include <format>

namespace eve {

namespace {

// Papyrus script class name (matches SFSEHTTP.psc).
// NOTE: Starfield's Papyrus 4.7 rejects underscores in script names, so the
// class is "SFSEHTTP" not "SFSE_HTTP" despite the project being SFSE_HTTP.
constexpr const char* SCRIPT_CLASS = "SFSEHTTP";

// In this CommonLibSF, static-function natives use std::monostate as the
// "self" placeholder (the historical StaticFunctionTag* is now monostate).

using Tag = std::monostate;

// ── Native function implementations ────────────────────────────────

std::int32_t Native_CreateDictionary(Tag) {
    return DictionaryStore::Instance().Create();
}

void Native_ReleaseDictionary(Tag, std::int32_t dictId) {
    DictionaryStore::Instance().Release(dictId);
}

void Native_SetString(Tag, std::int32_t dictId, RE::BSFixedString key, RE::BSFixedString value) {
    DictionaryStore::Instance().SetString(dictId,
                                          std::string(key.c_str()),
                                          std::string(value.c_str()));
}

void Native_SetInt(Tag, std::int32_t dictId, RE::BSFixedString key, std::int32_t value) {
    DictionaryStore::Instance().SetInt(dictId, std::string(key.c_str()), value);
}

void Native_SetFloat(Tag, std::int32_t dictId, RE::BSFixedString key, float value) {
    DictionaryStore::Instance().SetFloat(dictId, std::string(key.c_str()), value);
}

void Native_SetBool(Tag, std::int32_t dictId, RE::BSFixedString key, bool value) {
    DictionaryStore::Instance().SetBool(dictId, std::string(key.c_str()), value);
}

void Native_SetNestedDictionary(Tag, std::int32_t dictId, RE::BSFixedString key, std::int32_t nestedId) {
    DictionaryStore::Instance().SetNested(dictId, std::string(key.c_str()), nestedId);
}

void Native_SetStringArray(Tag, std::int32_t dictId, RE::BSFixedString key,
                            std::vector<RE::BSFixedString> values) {
    std::vector<std::string> v;
    v.reserve(values.size());
    for (const auto& s : values) v.emplace_back(s.c_str());
    DictionaryStore::Instance().SetStringArray(dictId, std::string(key.c_str()), v);
}

void Native_SetIntArray(Tag, std::int32_t dictId, RE::BSFixedString key,
                         std::vector<std::int32_t> values) {
    DictionaryStore::Instance().SetIntArray(dictId, std::string(key.c_str()), values);
}

void Native_SetFloatArray(Tag, std::int32_t dictId, RE::BSFixedString key,
                           std::vector<float> values) {
    DictionaryStore::Instance().SetFloatArray(dictId, std::string(key.c_str()), values);
}

RE::BSFixedString Native_GetString(Tag, std::int32_t dictId, RE::BSFixedString key) {
    auto s = DictionaryStore::Instance().GetString(dictId, std::string(key.c_str()));
    return RE::BSFixedString(s.c_str());
}

std::int32_t Native_GetInt(Tag, std::int32_t dictId, RE::BSFixedString key) {
    return DictionaryStore::Instance().GetInt(dictId, std::string(key.c_str()));
}

float Native_GetFloat(Tag, std::int32_t dictId, RE::BSFixedString key) {
    return DictionaryStore::Instance().GetFloat(dictId, std::string(key.c_str()));
}

bool Native_GetBool(Tag, std::int32_t dictId, RE::BSFixedString key) {
    return DictionaryStore::Instance().GetBool(dictId, std::string(key.c_str()));
}

std::int32_t Native_GetNestedDictionary(Tag, std::int32_t dictId, RE::BSFixedString key) {
    return DictionaryStore::Instance().GetNested(dictId, std::string(key.c_str()));
}

std::vector<RE::BSFixedString> Native_GetStringArray(Tag, std::int32_t dictId, RE::BSFixedString key) {
    auto strings = DictionaryStore::Instance().GetStringArray(dictId, std::string(key.c_str()));
    std::vector<RE::BSFixedString> out;
    out.reserve(strings.size());
    for (auto& s : strings) out.emplace_back(s.c_str());
    return out;
}

bool Native_HasKey(Tag, std::int32_t dictId, RE::BSFixedString key) {
    return DictionaryStore::Instance().HasKey(dictId, std::string(key.c_str()));
}

std::vector<RE::BSFixedString> Native_GetKeys(Tag, std::int32_t dictId) {
    auto keys = DictionaryStore::Instance().GetKeys(dictId);
    std::vector<RE::BSFixedString> out;
    out.reserve(keys.size());
    for (auto& k : keys) out.emplace_back(k.c_str());
    return out;
}

void Native_SendHttpPost(Tag, RE::BSFixedString url, std::int32_t requestDictId,
                          RE::BSFixedString replyEventName) {
    DictionaryStore::Instance().Retain(requestDictId);
    auto json_body = DictionaryStore::Instance().SerializeToJson(requestDictId);
    auto event_name = std::string(replyEventName.c_str());
    auto target_url = std::string(url.c_str());

    HttpClient::Instance().PostJson(
        target_url,
        json_body,
        event_name,
        [requestDictId](HttpResult result) {
            DictionaryStore::Instance().Release(requestDictId);
            ReplyQueue::Instance().Enqueue(std::move(result));
        });
}

void Native_SendHttpGet(Tag, RE::BSFixedString url, RE::BSFixedString replyEventName) {
    auto event_name = std::string(replyEventName.c_str());
    auto target_url = std::string(url.c_str());

    HttpClient::Instance().Get(
        target_url,
        event_name,
        [](HttpResult result) {
            ReplyQueue::Instance().Enqueue(std::move(result));
        });
}

void Native_SetDefaultTimeout(Tag, std::int32_t ms) {
    HttpClient::Instance().SetDefaultTimeoutMs(ms);
}

void Native_SetGlobalHeader(Tag, RE::BSFixedString name, RE::BSFixedString value) {
    HttpClient::Instance().SetGlobalHeader(std::string(name.c_str()),
                                            std::string(value.c_str()));
}

std::int32_t Native_PollReply(Tag, RE::BSFixedString eventName) {
    return ReplyQueue::Instance().PollNext(std::string(eventName.c_str()));
}

bool Native_IsReady(Tag) {
    return true;
}

RE::BSFixedString Native_GetVersion(Tag) {
    return RE::BSFixedString("1.0.0");
}

// ── Engine functions for console command execution ────────────────
// These are internal engine functions discovered via reverse engineering.
// BGSScaleFormManager singleton + ExecuteCommand replicates what the
// console UI does when you type a command and press Enter.
//
// Source: Console Command Runner SFSE plugin (Bobbyclue)
// IDs verified against Address Library.

namespace EngineConsole {

void* GetScaleFormManager() {
    static REL::Relocation<void**> singleton{ REL::ID(879512) };
    return *singleton;
}

void ExecuteCommand(void* a_scaleFormMgr, const char* a_command) {
    using func_t = void(*)(void*, const char*);
    static REL::Relocation<func_t> func{ REL::ID(166307) };
    func(a_scaleFormMgr, a_command);
}

} // namespace EngineConsole

// ── PlaceFormAtRef ────────────────────────────────────────────────
// Places any form at a reference's location using the console command
// code path. This handles GBFM (ship blueprint) forms that Papyrus's
// built-in PlaceAtMe silently rejects.
//
// Strategy:
//   1. Snapshot all reference FormIDs in the target ref's cell
//   2. Execute "player.placeatme <formID>" via the engine's console
//      command executor (same code path as typing in the console)
//   3. Diff the cell's references to find the newly created one
//   4. Return it to Papyrus
//
// The console command executor runs synchronously on the game thread,
// and Papyrus native functions also execute on the game thread, so the
// reference will exist in the cell by the time we scan for it.

RE::TESObjectREFR* Native_PlaceFormAtRef(Tag,
                                          RE::TESObjectREFR* akRef,
                                          RE::TESForm* akForm) {
    if (!akRef || !akForm) {
        return nullptr;
    }

    // Get the cell the target ref lives in
    auto* cell = akRef->parentCell;
    if (!cell) {
        return nullptr;
    }

    // 1. Snapshot existing reference FormIDs in the cell
    std::set<RE::TESFormID> beforeIDs;
    cell->ForEachReference([&](const RE::NiPointer<RE::TESObjectREFR>& ref) {
        if (ref) {
            beforeIDs.insert(ref->GetFormID());
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });

    // 2. Build and execute the console command
    //    Format: <refID>.placeatme <baseFormID> 1
    //    We target the specific reference so the object spawns at its location.
    RE::TESFormID refID = akRef->GetFormID();
    RE::TESFormID formID = akForm->GetFormID();

    // Console expects hex FormIDs without 0x prefix
    auto cmd = std::format("{:X}.placeatme {:X} 1", refID, formID);

    auto* sfMgr = EngineConsole::GetScaleFormManager();
    if (!sfMgr) {
        return nullptr;
    }

    EngineConsole::ExecuteCommand(sfMgr, cmd.c_str());

    // 3. Scan the cell for a new reference that wasn't in the snapshot
    RE::TESObjectREFR* newRef = nullptr;
    cell->ForEachReference([&](const RE::NiPointer<RE::TESObjectREFR>& ref) {
        if (ref && beforeIDs.find(ref->GetFormID()) == beforeIDs.end()) {
            // This reference didn't exist before the command
            newRef = ref.get();
            return RE::BSContainer::ForEachResult::kStop;
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });

    return newRef;
}

} // anonymous namespace

// ── Registration entry point ───────────────────────────────────────

bool RegisterPapyrusNatives() {
    auto* vmImpl = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vmImpl) return false;

    // C++ name-hiding: Internal::VirtualMachine overrides the 1-arg virtual
    // BindNativeMethod(IFunction*) which hides the 5-arg template inherited
    // from IVirtualMachine. Cast to base to access the template.
    auto* vm = static_cast<RE::BSScript::IVirtualMachine*>(vmImpl);

    // BindNativeMethod template: (object, function, fn, taskletCallable, isLatent)
    // taskletCallable: std::optional<bool> — pass true so scripts may call
    // these from any context (tasklets included).
    const std::optional<bool> tasklet = true;
    const bool not_latent = false;

    vm->BindNativeMethod(SCRIPT_CLASS, "CreateDictionary", Native_CreateDictionary, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "ReleaseDictionary", Native_ReleaseDictionary, tasklet, not_latent);

    vm->BindNativeMethod(SCRIPT_CLASS, "SetString", Native_SetString, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "SetInt", Native_SetInt, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "SetFloat", Native_SetFloat, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "SetBool", Native_SetBool, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "SetNestedDictionary", Native_SetNestedDictionary, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "SetStringArray", Native_SetStringArray, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "SetIntArray", Native_SetIntArray, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "SetFloatArray", Native_SetFloatArray, tasklet, not_latent);

    vm->BindNativeMethod(SCRIPT_CLASS, "GetString", Native_GetString, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "GetInt", Native_GetInt, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "GetFloat", Native_GetFloat, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "GetBool", Native_GetBool, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "GetNestedDictionary", Native_GetNestedDictionary, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "GetStringArray", Native_GetStringArray, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "HasKey", Native_HasKey, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "GetKeys", Native_GetKeys, tasklet, not_latent);

    vm->BindNativeMethod(SCRIPT_CLASS, "SendHttpPost", Native_SendHttpPost, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "SendHttpGet", Native_SendHttpGet, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "SetDefaultTimeout", Native_SetDefaultTimeout, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "SetGlobalHeader", Native_SetGlobalHeader, tasklet, not_latent);

    vm->BindNativeMethod(SCRIPT_CLASS, "PollReply", Native_PollReply, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "IsReady", Native_IsReady, tasklet, not_latent);
    vm->BindNativeMethod(SCRIPT_CLASS, "GetVersion", Native_GetVersion, tasklet, not_latent);

    // Object placement (handles GBFM/ship forms that Papyrus PlaceAtMe rejects)
    // DISABLED: crashes on 1.16.242 — REL::ID needs verification. Uncomment after fix.
    // vm->BindNativeMethod(SCRIPT_CLASS, "PlaceFormAtRef", Native_PlaceFormAtRef, tasklet, not_latent);

    return true;
}

} // namespace eve
