#pragma once

// Project Eve - SFSE_HTTP Papyrus Native Function Registration
// ─────────────────────────────────────────────────────────────
// All native functions exposed to Papyrus scripts via the `SFSE_HTTP`
// global script. Registration happens in kMessage_PostDataLoad.

namespace eve {

// Called from the SFSE message handler when kMessage_PostDataLoad fires.
// Registers all native functions on the Papyrus VM.
//
// Returns true on success, false on failure (VM not available, etc).
bool RegisterPapyrusNatives();

} // namespace eve
