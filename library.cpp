// Project Eve - SFSE_HTTP Plugin Entry Point
// ──────────────────────────────────────────────────────────────────
// Exports SFSE plugin metadata + load handler.
// On PostDataLoad: starts HTTP worker pool, registers Papyrus natives.
// Replies are queued in ReplyQueue; Papyrus polls via SFSE_HTTP.PollReply.

#include "../sfse/sfse/PluginAPI.h"
#include "../sfse/sfse_common/sfse_version.h"

#include "src/dictionary.h"
#include "src/http_client.h"
#include "src/reply_queue.h"
#include "src/papyrus_api.h"

// ── Globals captured at plugin load ──

static PluginHandle g_plugin_handle = kPluginHandle_Invalid;
static SFSEMessagingInterface* g_messaging = nullptr;
static SFSETaskInterface* g_task = nullptr;

// ── Periodic maintenance task: evict stale replies once per second ──

class MaintenanceTask : public SFSETaskInterface::ITaskDelegate {
public:
    void Run() override {
        eve::ReplyQueue::Instance().EvictStale();
    }
    void Destroy() override {
        delete this;
    }
};

// ── SFSE messaging callback ──

static void OnSfseMessage(SFSEMessagingInterface::Message* msg) {
    if (!msg) return;

    switch (msg->type) {
        case SFSEMessagingInterface::kMessage_PostLoad:
        case SFSEMessagingInterface::kMessage_PostPostLoad:
            break;

        case SFSEMessagingInterface::kMessage_PostDataLoad: {
            // VM is up - register Papyrus natives now
            eve::RegisterPapyrusNatives();

            // Start the HTTP worker pool (4 threads)
            eve::HttpClient::Instance().Start(4);

            // Schedule maintenance task (stale-reply eviction every frame)
            if (g_task) {
                g_task->AddTaskPermanent(new MaintenanceTask());
            }
            break;
        }

        case SFSEMessagingInterface::kMessage_PostPostDataLoad:
        case SFSEMessagingInterface::kMessage_PreSaveGame:
        case SFSEMessagingInterface::kMessage_PostSaveGame:
        case SFSEMessagingInterface::kMessage_PreLoadGame:
        case SFSEMessagingInterface::kMessage_PostLoadGame:
        default:
            break;
    }
}

// ── SFSE Plugin Metadata Export ──

extern "C" {

__declspec(dllexport) SFSEPluginVersionData SFSEPlugin_Version =
{
    SFSEPluginVersionData::kVersion,

    1,                                  // plugin version
    "SFSE_HTTP",                        // name (shown in SFSE log)
    "Bojan Kustura",                    // author

    SFSEPluginVersionData::kAddressIndependence_AddressLibrary,
    SFSEPluginVersionData::kStructureIndependence_NoStructs,

    { CURRENT_RELEASE_RUNTIME, 0 },     // compatible Starfield versions

    0, 0, 0,                            // SFSE version requirement / reserved
};

__declspec(dllexport) bool SFSEPlugin_Load(const SFSEInterface* sfse)
{
    g_plugin_handle = sfse->GetPluginHandle();

    g_messaging = static_cast<SFSEMessagingInterface*>(
        sfse->QueryInterface(kInterface_Messaging));
    if (g_messaging) {
        g_messaging->RegisterListener(g_plugin_handle, "SFSE", OnSfseMessage);
    }

    g_task = static_cast<SFSETaskInterface*>(
        sfse->QueryInterface(kInterface_Task));

    return true;
}

} // extern "C"
