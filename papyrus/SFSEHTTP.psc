Scriptname SFSEHTTP Native Hidden

; Project Eve - SFSE_HTTP Papyrus API
; ────────────────────────────────────────────────────────────────────
; Declaration script for the native functions registered by sfse_http.dll.
; All implementations live in C++; this file is just the type signature
; bridge so Papyrus can call them.
;
; The plugin registers these in kMessage_PostDataLoad via
; CommonLibSF::RE::BSScript::IVirtualMachine::BindNativeMethod.
;
; Threading model:
;   - All functions return on the calling thread (game thread). Never block.
;   - HTTP requests dispatch to a worker pool. Reply fires a mod event on
;     the game thread once the response is back.
;   - Dictionary store is mutex-protected, safe to call concurrently.
; ────────────────────────────────────────────────────────────────────


; ── Dictionary construction ────────────────────────────────────────

int Function CreateDictionary() global native

Function ReleaseDictionary(int dictId) global native

Function SetString(int dictId, string iniKey, string value) global native
Function SetInt(int dictId, string iniKey, int value) global native
Function SetFloat(int dictId, string iniKey, float value) global native
Function SetBool(int dictId, string iniKey, bool value) global native
Function SetNestedDictionary(int dictId, string iniKey, int nestedId) global native

Function SetStringArray(int dictId, string iniKey, string[] values) global native
Function SetIntArray(int dictId, string iniKey, int[] values) global native
Function SetFloatArray(int dictId, string iniKey, float[] values) global native


; ── Dictionary reading (used on reply) ─────────────────────────────

string  Function GetString(int dictId, string iniKey) global native
int     Function GetInt(int dictId, string iniKey) global native
float   Function GetFloat(int dictId, string iniKey) global native
bool    Function GetBool(int dictId, string iniKey) global native
int     Function GetNestedDictionary(int dictId, string iniKey) global native
string[] Function GetStringArray(int dictId, string iniKey) global native
bool    Function HasKey(int dictId, string iniKey) global native
string[] Function GetKeys(int dictId) global native


; ── HTTP requests ──────────────────────────────────────────────────
; Fire-and-forget. The reply mod event fires on the game thread with
; (statusCode, replyDictId, url). Caller must Release the reply dict.

Function SendHttpPost(string url, int requestDictId, string replyEventName) global native
Function SendHttpGet(string url, string replyEventName) global native

Function SetDefaultTimeout(int ms) global native
Function SetGlobalHeader(string headerName, string headerValue) global native


; ── Reply polling ──────────────────────────────────────────────
; Pop the next pending reply for the named event. Returns the dict id
; of the reply (which has __status and __url keys injected plus the
; parsed JSON body fields), or -1 if no reply is pending.
;
; The caller must ReleaseDictionary(replyDict) after reading it.

int Function PollReply(string eventName) global native


; ── Object placement ───────────────────────────────────────────────
; Places any form at the specified reference's location using the
; engine's console command code path. This handles GBFM (ship blueprint)
; forms that Papyrus's built-in PlaceAtMe silently rejects (returns None).
;
; Works for any form type that the console "placeatme" command accepts.
; Returns the newly created ObjectReference, or None on failure.

ObjectReference Function PlaceFormAtRef(ObjectReference akRef, Form akForm) global native


; ── Health / diagnostics ───────────────────────────────────────────

bool Function IsReady() global native
string Function GetVersion() global native
