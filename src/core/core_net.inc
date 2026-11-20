// [CORE] Ag simulasyonu, Steam Spacewar P2P + Lobi/Davet (F8), UDP/LAN kesif, FPS siniri

// ---- network simulation for testing on one PC: sim_lag / sim_jitter (ms), sim_loss (%) applied to received packets ----
static void rx_accept(const NETPKT *k) {
    if (k->seq <= g_last_rx_seq && g_last_rx_seq - k->seq < 1000) { snap_push(k); return; }   // reordered: still useful for smoothing
    g_last_rx_seq = k->seq; g_remote = *k; g_have_remote = 1; g_rx_count++; g_last_rx_tick = GetTickCount();
    snap_push(k);
    if (g_rx_count == 1) logf("[coop] first packet from peer: level='%s' parts=%lu\n", k->level, k->count);
}
#define SIMQ 128
typedef struct { double rel; NETPKT k; } SIMP;
static SIMP *g_simq; static int g_simn, g_sim_lag, g_sim_jit, g_sim_loss; static DWORD g_sim_dropped, g_sim_seed = 12345;
static int sim_rand(int n) { g_sim_seed = g_sim_seed * 1103515245u + 12345u; return n > 0 ? (int)((g_sim_seed >> 8) % (DWORD)n) : 0; }
static int sim_rx(const NETPKT *k) {   // 1 = consumed (queued/dropped), 0 = deliver now
    if (!g_sim_lag && !g_sim_jit && !g_sim_loss) return 0;
    if (g_sim_loss && sim_rand(100) < g_sim_loss) { g_sim_dropped++; return 1; }
    if (!g_simq) g_simq = (SIMP *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(SIMP) * SIMQ);
    if (!g_simq || g_simn == SIMQ) return 1;
    g_simq[g_simn].k = *k; g_simq[g_simn].rel = now_s() + (g_sim_lag + sim_rand(g_sim_jit + 1)) / 1000.0; g_simn++;
    return 1;
}
static void sim_release(void) {   // deliver due packets in release-time order (jitter can reorder: rx_accept drops the late ones)
    for (;;) { int i, b = -1; double t = now_s();
        for (i = 0; i < g_simn; i++) if (g_simq[i].rel <= t && (b < 0 || g_simq[i].rel < g_simq[b].rel)) b = i;
        if (b < 0) break;
        rx_accept(&g_simq[b].k); g_simq[b] = g_simq[--g_simn]; }
}
static int is_player_part(const char *ty) {
    if (_strnicmp(ty, "SUPERCAN_", 9)) return 0;
    if (!_strnicmp(ty, "SUPERCAN_BOX", 12) || !_strnicmp(ty, "SUPERCAN_RUZ", 12) || !_strnicmp(ty, "SUPERCAN_TUNELBOX", 17)) return 0;
    return 1;
}
static int is_puppet(ENTITY *e) { int i; for (i = 0; i < MAX_PARTS; i++) if (g_puppet[i] == e) return 1; return 0; }

// Config: environment variable first, else [coop] section of Game\coop.ini (needed when the launcher starts the game).
static int cfg(const char *env, const char *key, char *buf, int n) {
    char ini[MAX_PATH];
    if (GetEnvironmentVariableA(env, buf, n)) return 1;
    sprintf(ini, "%scoop.ini", g_dir);
    return GetPrivateProfileStringA("coop", key, "", buf, n, ini) > 0;
}

// ---- Steam transport (net=steam): Spacewar AppID 480, ISteamNetworkingMessages through Valve's relay ----
// Flat C API loaded at runtime from Game\steam_api.dll (Steamworks.NET Standalone 2025.164.1, x86; see the
// coop-anything playbook networking-steam-spacewar.md). No lobbies: while the peer is unknown, our packet goes about
// once a second to every Steam friend currently running AppID 480; the first friend whose valid packet arrives
// becomes the peer. Session requests are accepted from friends only (or from steam_peer= if configured).
// Initialised lazily on the game thread (first coop_tick), never from DllMain. Falls back to UDP on failure.
typedef unsigned __int64 u64;
typedef struct { int user; int callback; BYTE *param; int size; } ST_CBMSG;   // CallbackMsg_t (32-bit)
#pragma pack(push, 8)
typedef struct { u64 call; int callback; unsigned param_size; } ST_APICALL_DONE;             // 703
typedef struct { int result; u64 lobby; } ST_LOBBY_CREATED;                                  // 513
typedef struct { u64 lobby; unsigned perms; BYTE locked; unsigned response; } ST_LOBBY_ENTER;// 504
typedef struct { u64 lobby; u64 user_changed; u64 making_change; unsigned state; } ST_LOBBY_UPD; // 506
typedef struct { u64 lobby; u64 friend_id; } ST_LOBBY_JOIN_REQ;                              // 333
typedef struct { u64 friend_id; char connect[256]; } ST_RP_JOIN_REQ;                         // 337
#pragma pack(pop)
static int g_net_steam;          // 1 = steam requested; becomes 2 once initialised, 0 after a failed init (UDP)
static HMODULE g_st_dll; static int g_st_pipe; static void *g_st_user, *g_st_friends, *g_st_msgs, *g_st_mm;
static u64 g_st_me, g_st_peer, g_st_cfg_peer, g_st_lobby, g_st_cfg_lobby;
static int g_st_lobby_creating, g_st_lobby_is_owner, g_st_lobby_members;
static double g_st_next_scan; static int g_st_ident_off;
typedef struct { u64 id; char name[32]; int in_480; int state; } ST_FRIEND;
#define ST_MAX_UI_FRIENDS 8
static ST_FRIEND g_st_ui_friends[ST_MAX_UI_FRIENDS];
static int g_st_ui_nfriend, g_st_ui_open;
static char g_st_cands[256] = "\x01";   // last logged candidate list (sentinel: log the first scan too)
static int (__cdecl *st_InitFlat)(char *);
static int (__cdecl *st_GetHSteamPipe)(void);
static void (__cdecl *st_MD_Init)(void);
static void (__cdecl *st_MD_RunFrame)(int);
static BYTE (__cdecl *st_MD_GetNext)(int, ST_CBMSG *);
static void (__cdecl *st_MD_Free)(int);
static BYTE (__cdecl *st_MD_GetAPICallResult)(int, u64, void *, int, int, BYTE *);
static void *(__cdecl *st_User)(void);
static u64 (__cdecl *st_GetSteamID)(void *);
static void *(__cdecl *st_Friends)(void);
static int (__cdecl *st_GetFriendCount)(void *, int);
static u64 (__cdecl *st_GetFriendByIndex)(void *, int, int);
static BYTE (__cdecl *st_GetFriendGamePlayed)(void *, u64, void *);
static const char *(__cdecl *st_GetFriendPersonaName)(void *, u64);
static int (__cdecl *st_GetFriendRelationship)(void *, u64);
static int (__cdecl *st_GetFriendPersonaState)(void *, u64);
static const char *(__cdecl *st_GetPersonaName)(void *);
static BYTE (__cdecl *st_SetRichPresence)(void *, const char *, const char *);
static void (__cdecl *st_ClearRichPresence)(void *);
static void *(__cdecl *st_Matchmaking)(void);
static u64 (__cdecl *st_CreateLobby)(void *, int, int);
static u64 (__cdecl *st_JoinLobby)(void *, u64);
static void (__cdecl *st_LeaveLobby)(void *, u64);
static BYTE (__cdecl *st_SetLobbyData)(void *, u64, const char *, const char *);
static BYTE (__cdecl *st_SetLobbyJoinable)(void *, u64, BYTE);
static u64 (__cdecl *st_GetLobbyOwner)(void *, u64);
static int (__cdecl *st_GetNumLobbyMembers)(void *, u64);
static u64 (__cdecl *st_GetLobbyMemberByIndex)(void *, u64, int);
static BYTE (__cdecl *st_InviteUserToLobby)(void *, u64, u64);
static void *(__cdecl *st_Msgs)(void);
static int (__cdecl *st_Send)(void *, const void *, const void *, unsigned, int, int);
static int (__cdecl *st_Recv)(void *, int, void **, int);
static BYTE (__cdecl *st_Accept)(void *, const void *);
static BYTE (__cdecl *st_Close)(void *, const void *);
static void (__cdecl *st_IdSet)(void *, u64);
static u64 (__cdecl *st_IdGet)(const void *);
static void (__cdecl *st_MsgRelease)(void *);
static void *(__cdecl *st_Utils)(void);
static void (__cdecl *st_InitRelay)(void *);
static void hud_toast(const char *msg, double sec);
static void hud_ascii_copy(const char *src, char *dst, int cap);
#define ST_SEND_UNRELIABLE_NONAGLE (1 | 32)   // NoNagle (1) | AutoRestartBrokenSession (32)
static const char *st_name(u64 id) { const char *n = (st_GetFriendPersonaName && id) ? st_GetFriendPersonaName(g_st_friends, id) : 0; return n ? n : "?"; }

static void steam_lobby_refresh_friends(void) {
    int i, n, flags = 4 /* k_EFriendFlagImmediate */;
    g_st_ui_nfriend = 0;
    if (!g_st_friends || !st_GetFriendCount || !st_GetFriendByIndex) return;
    n = st_GetFriendCount(g_st_friends, flags);
    // Önce Spacewar (480) oynayan arkadaşları, sonra diğer çevrimiçi arkadaşları listele
    for (int pass = 0; pass < 2 && g_st_ui_nfriend < ST_MAX_UI_FRIENDS; pass++) {
        for (i = 0; i < n && g_st_ui_nfriend < ST_MAX_UI_FRIENDS; i++) {
            u64 id = st_GetFriendByIndex(g_st_friends, i, flags);
            int st = st_GetFriendPersonaState ? st_GetFriendPersonaState(g_st_friends, id) : 1;
            BYTE gi[64]; int in480 = 0;
            if (st == 0) continue;   // çevrimdışı
            memset(gi, 0, sizeof gi);
            if (st_GetFriendGamePlayed && st_GetFriendGamePlayed(g_st_friends, id, gi) && (*(u64 *)gi & 0xFFFFFF) == 480) in480 = 1;
            if ((pass == 0 && !in480) || (pass == 1 && in480)) continue;
            g_st_ui_friends[g_st_ui_nfriend].id = id;
            g_st_ui_friends[g_st_ui_nfriend].in_480 = in480;
            g_st_ui_friends[g_st_ui_nfriend].state = st;
            hud_ascii_copy(st_name(id), g_st_ui_friends[g_st_ui_nfriend].name, sizeof g_st_ui_friends[0].name);
            g_st_ui_nfriend++;
        }
    }
}
static void steam_lobby_sync_peer(void) {
    int i, n;
    if (!g_st_mm || !g_st_lobby || !st_GetNumLobbyMembers || !st_GetLobbyMemberByIndex) return;
    n = st_GetNumLobbyMembers(g_st_mm, g_st_lobby);
    g_st_lobby_members = n;
    if (st_GetLobbyOwner) g_st_lobby_is_owner = (st_GetLobbyOwner(g_st_mm, g_st_lobby) == g_st_me);
    for (i = 0; i < n; i++) {
        u64 mid = st_GetLobbyMemberByIndex(g_st_mm, g_st_lobby, i);
        if (mid && mid != g_st_me) {
            if (g_st_peer != mid) {
                g_st_peer = mid;
                logf("[steam-lobby] locked peer from lobby %I64u -> %I64u '%s' (members=%d)\n", g_st_lobby, mid, st_name(mid), n);
                if (st_Accept && g_st_msgs) {
                    BYTE ident[160]; memset(ident, 0, sizeof ident); st_IdSet(ident, mid);
                    st_Accept(g_st_msgs, ident);
                }
            }
            return;
        }
    }
}
static void steam_lobby_create(void) {
    if (!g_st_mm || !st_CreateLobby || g_st_lobby_creating) return;
    if (g_st_lobby && st_LeaveLobby) { st_LeaveLobby(g_st_mm, g_st_lobby); g_st_lobby = 0; g_st_lobby_members = 0; }
    g_st_lobby_creating = 1;
    st_CreateLobby(g_st_mm, 1 /* k_ELobbyTypeFriendsOnly */, 2);
    logf("[steam-lobby] CreateLobby(FriendsOnly, 2) requested\n");
}
static void steam_lobby_join(u64 lobby_id) {
    if (!g_st_mm || !st_JoinLobby || !lobby_id) return;
    if (g_st_lobby == lobby_id) return;
    if (g_st_lobby && st_LeaveLobby) { st_LeaveLobby(g_st_mm, g_st_lobby); g_st_lobby = 0; g_st_lobby_members = 0; }
    st_JoinLobby(g_st_mm, lobby_id);
    logf("[steam-lobby] JoinLobby(%I64u) requested\n", lobby_id);
}
static void steam_lobby_leave(void) {
    if (!g_st_mm || !g_st_lobby) return;
    if (st_LeaveLobby) st_LeaveLobby(g_st_mm, g_st_lobby);
    if (st_ClearRichPresence && g_st_friends) st_ClearRichPresence(g_st_friends);
    logf("[steam-lobby] Left lobby %I64u\n", g_st_lobby);
    g_st_lobby = 0; g_st_lobby_creating = 0; g_st_lobby_is_owner = 0; g_st_lobby_members = 0;
    if (!g_st_cfg_peer) g_st_peer = 0;
    hud_toast("STEAM LOBISINDEN AYRILDIN", 2.5);
}
static void steam_lobby_invite(int idx) {
    char msg[96];
    if (idx < 0 || idx >= g_st_ui_nfriend || !g_st_mm || !st_InviteUserToLobby) return;
    if (!g_st_lobby) { steam_lobby_create(); hud_toast("LOBI KURULUYOR, 1 SN SONRA TEKRAR BAS", 2.5); return; }
    if (st_InviteUserToLobby(g_st_mm, g_st_lobby, g_st_ui_friends[idx].id)) {
        sprintf(msg, "DAVET GONDERILDI: %.20s", g_st_ui_friends[idx].name);
        hud_toast(msg, 3.0);
        logf("[steam-lobby] InviteUserToLobby(lobby=%I64u, friend=%I64u '%s') OK\n",
             g_st_lobby, g_st_ui_friends[idx].id, g_st_ui_friends[idx].name);
    } else {
        hud_toast("DAVET GONDERILEMEDI", 2.5);
    }
}
static void steam_lobby_copy_id(void) {
    char s[64]; HGLOBAL hg;
    if (!g_st_lobby) { hud_toast("ONCE LOBI KUR", 2.0); return; }
    sprintf(s, "%I64u", g_st_lobby);
    if (OpenClipboard(0)) {
        EmptyClipboard();
        hg = GlobalAlloc(GMEM_MOVEABLE, strlen(s) + 1);
        if (hg) { memcpy(GlobalLock(hg), s, strlen(s) + 1); GlobalUnlock(hg); SetClipboardData(CF_TEXT, hg); }
        CloseClipboard();
        hud_toast("LOBI ID PANOYA KOPYALANDI!", 2.5);
    }
}
static void steam_parse_connect_str(const char *s) {
    const char *p; u64 lid;
    if (!s || !s[0]) return;
    p = strstr(s, "+connect_lobby");
    if (!p) p = strstr(s, "connect_lobby");
    if (p) {
        while (*p && (*p < '0' || *p > '9')) p++;
        lid = _strtoui64(p, 0, 10);
        if (lid) { logf("[steam-lobby] parsed lobby id %I64u from '%s'\n", lid, s); steam_lobby_join(lid); }
    }
}
static int steam_init(void) {
    char p[MAX_PATH], err[1024] = ""; int r; FILE *f;
    sprintf(p, "%ssteam_appid.txt", g_dir);
    if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES && (f = fopen(p, "w"))) { fputs("480", f); fclose(f); }
    SetEnvironmentVariableA("SteamAppId", "480"); SetEnvironmentVariableA("SteamGameId", "480");   // cwd-independent
    sprintf(p, "%ssteam_api.dll", g_dir);
    if (!(g_st_dll = LoadLibraryA(p))) { logf("[steam] %s could not be loaded (%lu)\n", p, GetLastError()); return 0; }
#define ST_GET(v, name) if (!(*(FARPROC *)&v = GetProcAddress(g_st_dll, name))) { logf("[steam] missing export %s\n", name); return 0; }
#define ST_OPT(v, name) (*(FARPROC *)&v = GetProcAddress(g_st_dll, name))
    ST_GET(st_InitFlat, "SteamAPI_InitFlat") ST_GET(st_GetHSteamPipe, "SteamAPI_GetHSteamPipe")
    ST_GET(st_MD_Init, "SteamAPI_ManualDispatch_Init") ST_GET(st_MD_RunFrame, "SteamAPI_ManualDispatch_RunFrame")
    ST_GET(st_MD_GetNext, "SteamAPI_ManualDispatch_GetNextCallback") ST_GET(st_MD_Free, "SteamAPI_ManualDispatch_FreeLastCallback")
    ST_OPT(st_MD_GetAPICallResult, "SteamAPI_ManualDispatch_GetAPICallResult");
    ST_GET(st_User, "SteamAPI_SteamUser_v023") ST_GET(st_GetSteamID, "SteamAPI_ISteamUser_GetSteamID")
    ST_GET(st_Friends, "SteamAPI_SteamFriends_v018") ST_GET(st_GetFriendCount, "SteamAPI_ISteamFriends_GetFriendCount")
    ST_GET(st_GetFriendByIndex, "SteamAPI_ISteamFriends_GetFriendByIndex") ST_GET(st_GetFriendGamePlayed, "SteamAPI_ISteamFriends_GetFriendGamePlayed")
    ST_GET(st_GetFriendPersonaName, "SteamAPI_ISteamFriends_GetFriendPersonaName") ST_GET(st_GetFriendRelationship, "SteamAPI_ISteamFriends_GetFriendRelationship")
    ST_OPT(st_GetFriendPersonaState, "SteamAPI_ISteamFriends_GetFriendPersonaState");
    ST_GET(st_GetPersonaName, "SteamAPI_ISteamFriends_GetPersonaName")
    ST_OPT(st_SetRichPresence, "SteamAPI_ISteamFriends_SetRichPresence");
    ST_OPT(st_ClearRichPresence, "SteamAPI_ISteamFriends_ClearRichPresence");
    ST_OPT(st_Matchmaking, "SteamAPI_SteamMatchmaking_v009");
    ST_OPT(st_CreateLobby, "SteamAPI_ISteamMatchmaking_CreateLobby");
    ST_OPT(st_JoinLobby, "SteamAPI_ISteamMatchmaking_JoinLobby");
    ST_OPT(st_LeaveLobby, "SteamAPI_ISteamMatchmaking_LeaveLobby");
    ST_OPT(st_SetLobbyData, "SteamAPI_ISteamMatchmaking_SetLobbyData");
    ST_OPT(st_SetLobbyJoinable, "SteamAPI_ISteamMatchmaking_SetLobbyJoinable");
    ST_OPT(st_GetLobbyOwner, "SteamAPI_ISteamMatchmaking_GetLobbyOwner");
    ST_OPT(st_GetNumLobbyMembers, "SteamAPI_ISteamMatchmaking_GetNumLobbyMembers");
    ST_OPT(st_GetLobbyMemberByIndex, "SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex");
    ST_OPT(st_InviteUserToLobby, "SteamAPI_ISteamMatchmaking_InviteUserToLobby");
    ST_GET(st_Msgs, "SteamAPI_SteamNetworkingMessages_SteamAPI_v002") ST_GET(st_Send, "SteamAPI_ISteamNetworkingMessages_SendMessageToUser")
    ST_GET(st_Recv, "SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel") ST_GET(st_Accept, "SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser")
    ST_OPT(st_Close, "SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser");
    ST_GET(st_IdSet, "SteamAPI_SteamNetworkingIdentity_SetSteamID64") ST_GET(st_IdGet, "SteamAPI_SteamNetworkingIdentity_GetSteamID64")
    ST_GET(st_MsgRelease, "SteamAPI_SteamNetworkingMessage_t_Release")
    ST_GET(st_Utils, "SteamAPI_SteamNetworkingUtils_SteamAPI_v004") ST_GET(st_InitRelay, "SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess")
#undef ST_GET
#undef ST_OPT
    r = st_InitFlat(err);
    if (r) { logf("[steam] SteamAPI_InitFlat failed %d (%s)%s\n", r, err, r == 2 ? " - Steam is not running" : ""); return 0; }
    st_MD_Init(); g_st_pipe = st_GetHSteamPipe();
    g_st_user = st_User(); g_st_friends = st_Friends(); g_st_msgs = st_Msgs();
    g_st_mm = st_Matchmaking ? st_Matchmaking() : 0;
    if (!g_st_user || !g_st_friends || !g_st_msgs) { logf("[steam] interface missing user=%p friends=%p msgs=%p\n", g_st_user, g_st_friends, g_st_msgs); return 0; }
    { void *u = st_Utils(); if (u) st_InitRelay(u); }
    g_st_me = st_GetSteamID(g_st_user);
    logf("[steam] ready: me=%I64u '%s', pipe=%d, lobby_api=%s%s\n", g_st_me, st_GetPersonaName(g_st_friends), g_st_pipe,
         g_st_mm ? "ON(F8)" : "off",
         g_st_cfg_peer ? "" : ", peer = lobby member or first friend running AppID 480");
    if (g_st_cfg_peer) { g_st_peer = g_st_cfg_peer; logf("[steam] configured peer %I64u '%s'\n", g_st_peer, st_name(g_st_peer)); }
    if (g_st_cfg_lobby) steam_lobby_join(g_st_cfg_lobby);
    else steam_parse_connect_str(GetCommandLineA());
    return 1;
}
static int st_send_to(u64 id, const void *buf, int len) {
    BYTE ident[160]; int r; static int fails;
    memset(ident, 0, sizeof ident); st_IdSet(ident, id);
    r = st_Send(g_st_msgs, ident, buf, (unsigned)len, ST_SEND_UNRELIABLE_NONAGLE, 0);
    if (r == 35 && st_Close) {
        // k_EResultConnectFailed (35): bozuk oturumu kapat ve yeniden dene
        st_Close(g_st_msgs, ident);
        r = st_Send(g_st_msgs, ident, buf, (unsigned)len, ST_SEND_UNRELIABLE_NONAGLE, 0);
    }
    if (r != 1) {
        fails++;
        if (fails <= 20 || (fails % 300) == 0) logf("[steam] SendMessageToUser(%I64u) -> EResult %d (fails=%d)\n", id, r, fails);
    }
    return r == 1;
}
static void steam_send(const void *buf, int len) {
    int i, n, flags = 4 /* k_EFriendFlagImmediate */; char cands[256] = ""; double t = now_s();
    if (g_st_peer) { st_send_to(g_st_peer, buf, len); return; }
    if (g_st_lobby) return;   // Lobi kuruluysa daveti kabul edip lobiye gireni bekle (davetsiz otomatik tarama yapma)
    if (t < g_st_next_scan) return;
    g_st_next_scan = t + 1.0;
    n = st_GetFriendCount(g_st_friends, flags);
    for (i = 0; i < n; i++) {
        u64 id = st_GetFriendByIndex(g_st_friends, i, flags); BYTE gi[64];   // FriendGameInfo_t: u64 gameID @0 (appid = low 24 bits)
        memset(gi, 0, sizeof gi);
        if (!st_GetFriendGamePlayed(g_st_friends, id, gi) || (*(u64 *)gi & 0xFFFFFF) != 480) continue;
        st_send_to(id, buf, len);
        if (strlen(cands) < 200) { strcat(cands, st_name(id)); strcat(cands, " "); }
    }
    if (strcmp(cands, g_st_cands)) { strcpy(g_st_cands, cands); logf("[steam] friends running AppID 480 (of %d): %s\n", n, cands[0] ? cands : "(none)"); }
}
static void steam_handle_cb(int cb, BYTE *param) {
    if (!param) return;
    if (cb == 1251) {          // SteamNetworkingMessagesSessionRequest_t { identity @0 }
        u64 id = st_IdGet(param); int rel = st_GetFriendRelationship(g_st_friends, id);
        int ok = g_st_cfg_peer ? (id == g_st_cfg_peer) : (g_st_lobby ? (g_st_peer == id) : (rel == 3 /* friend */));
        logf("[steam] session request from %I64u '%s' (relationship %d, lobby=%I64u) -> %s\n", id, st_name(id), rel, g_st_lobby, ok ? "accepted" : "ignored");
        if (ok) st_Accept(g_st_msgs, param);
    } else if (cb == 1252) {   // SessionFailed: identity @0 of the connection info
        u64 id = st_IdGet(param);
        logf("[steam] session failed with %I64u '%s' -> closing broken session\n", id, st_name(id));
        if (st_Close && id) {
            BYTE ident[160]; memset(ident, 0, sizeof ident); st_IdSet(ident, id);
            st_Close(g_st_msgs, ident);
        }
    } else if (cb == 513) {    // LobbyCreated_t (idempotent: real Steam delivers both 703 call result and 513 broadcast)
        ST_LOBBY_CREATED *lc = (ST_LOBBY_CREATED *)param;
        g_st_lobby_creating = 0;
        if (lc->result == 1 && lc->lobby) {
            if (g_st_lobby != lc->lobby) {
                char rp[64];
                g_st_lobby = lc->lobby; g_st_lobby_is_owner = 1; g_st_lobby_members = 1;
                if (st_SetLobbyData) st_SetLobbyData(g_st_mm, g_st_lobby, "game", "supercan1");
                if (st_SetLobbyJoinable) st_SetLobbyJoinable(g_st_mm, g_st_lobby, 1);
                sprintf(rp, "+connect_lobby %I64u", g_st_lobby);
                if (st_SetRichPresence && g_st_friends) {
                    st_SetRichPresence(g_st_friends, "connect", rp);
                    st_SetRichPresence(g_st_friends, "status", "SuperCan Co-op Lobisi");
                }
                logf("[steam-lobby] LobbyCreated OK: id=%I64u\n", g_st_lobby);
                hud_toast("STEAM LOBI KURULDU! 1-8 ILE DAVET ET", 3.5);
                steam_lobby_refresh_friends();
            }
        } else {
            logf("[steam-lobby] LobbyCreated failed result=%d\n", lc->result);
            hud_toast("STEAM LOBI KURULAMADI", 2.5);
        }
    } else if (cb == 504) {    // LobbyEnter_t
        ST_LOBBY_ENTER *le = (ST_LOBBY_ENTER *)param;
        if (le->response == 1 && le->lobby) {
            char rp[64];
            g_st_lobby = le->lobby; g_st_lobby_creating = 0;
            sprintf(rp, "+connect_lobby %I64u", g_st_lobby);
            if (st_SetRichPresence && g_st_friends) st_SetRichPresence(g_st_friends, "connect", rp);
            steam_lobby_sync_peer();
            logf("[steam-lobby] LobbyEnter OK: id=%I64u members=%d owner=%d\n", g_st_lobby, g_st_lobby_members, g_st_lobby_is_owner);
            if (!g_st_lobby_is_owner) hud_toast("STEAM LOBISINE KATILINDI!", 3.0);
        } else {
            logf("[steam-lobby] LobbyEnter failed response=%u\n", le->response);
            hud_toast("LOBIYE KATILINAMADI", 2.5);
        }
    } else if (cb == 506) {    // LobbyChatUpdate_t
        ST_LOBBY_UPD *lu = (ST_LOBBY_UPD *)param;
        if (lu->lobby == g_st_lobby) {
            logf("[steam-lobby] LobbyChatUpdate user=%I64u '%s' state=0x%x\n", lu->user_changed, st_name(lu->user_changed), lu->state);
            if (lu->state & 0x01 /* entered */) {
                steam_lobby_sync_peer();
                if (lu->user_changed != g_st_me) {
                    char msg[96]; sprintf(msg, "LOBIYE KATILDI: %.20s", st_name(lu->user_changed));
                    hud_toast(msg, 3.0);
                }
            } else if (lu->state & 0x1E /* left/disconnected/kicked/banned */) {
                if (lu->user_changed == g_st_peer && !g_st_cfg_peer) g_st_peer = 0;
                steam_lobby_sync_peer();
                if (lu->user_changed != g_st_me) hud_toast("ARKADAS LOBIDEN AYRILDI", 3.0);
            }
        }
    } else if (cb == 333) {    // GameLobbyJoinRequested_t (arkadaş Steam davetini kabul etti)
        ST_LOBBY_JOIN_REQ *jr = (ST_LOBBY_JOIN_REQ *)param;
        logf("[steam-lobby] GameLobbyJoinRequested lobby=%I64u from=%I64u '%s'\n", jr->lobby, jr->friend_id, st_name(jr->friend_id));
        if (jr->lobby) { hud_toast("DAVET KABUL EDILDI, BAGLANIYOR...", 3.0); steam_lobby_join(jr->lobby); }
    } else if (cb == 337) {    // GameRichPresenceJoinRequested_t (Steam arkadaş listesinden "Oyuna Katıl")
        ST_RP_JOIN_REQ *rj = (ST_RP_JOIN_REQ *)param;
        logf("[steam-lobby] GameRichPresenceJoinRequested from=%I64u '%s' connect='%s'\n", rj->friend_id, st_name(rj->friend_id), rj->connect);
        steam_parse_connect_str(rj->connect);
    }
}
static void steam_callbacks(void) {
    ST_CBMSG m;
    st_MD_RunFrame(g_st_pipe);
    while (st_MD_GetNext(g_st_pipe, &m)) {
        if (m.callback == 703 && m.param && st_MD_GetAPICallResult) {
            ST_APICALL_DONE *done = (ST_APICALL_DONE *)m.param;
            if (done->param_size > 0 && done->param_size <= 512) {
                BYTE rbuf[512]; BYTE failed = 0;
                if (st_MD_GetAPICallResult(g_st_pipe, done->call, rbuf, (int)done->param_size, done->callback, &failed) && !failed)
                    steam_handle_cb(done->callback, rbuf);
            }
        } else {
            steam_handle_cb(m.callback, m.param);
        }
        st_MD_Free(g_st_pipe);
    }
}
// One message from channel 0 into buf: length, 0 = skip, -1 = none left. Sender goes to *from.
static int steam_recv(BYTE *buf, int cap, u64 *from) {
    void *msg = 0; int len = 0; BYTE *mp;
    if (st_Recv(g_st_msgs, 0, &msg, 1) <= 0 || !msg) return -1;
    mp = (BYTE *)msg;
    __try {
        BYTE *data = *(BYTE **)mp; int size = *(int *)(mp + 4); u64 a = st_IdGet(mp + 12), b = st_IdGet(mp + 16);
        if (!g_st_ident_off) { g_st_ident_off = a ? 12 : (b ? 16 : 0); logf("[steam] message sender identity at offset %d (12:%I64u 16:%I64u)\n", g_st_ident_off, a, b); }
        *from = g_st_ident_off == 16 ? b : (a ? a : b);
        if (size > 0 && size <= cap && data) { memcpy(buf, data, size); len = size; }
    } __except (EXCEPTION_EXECUTE_HANDLER) { len = 0; }
    st_MsgRelease(msg);
    return len;
}

static DWORD g_run_id, g_peer_run_id;
static int g_ssn_pending;
static void net_on_peer_run_id(DWORD new_rid, DWORD old_rid);

static void coop_init(void) {
    char buf[64], ip[32]; int lport = 0, pport = 0; WSADATA w; u_long nb = 1; struct sockaddr_in a;
    { LARGE_INTEGER qpc; QueryPerformanceCounter(&qpc);
      g_run_id = GetTickCount() ^ (GetCurrentProcessId() * 2654435761u) ^ qpc.LowPart ^ (qpc.HighPart << 16);
      if (!g_run_id) g_run_id = 1;
      logf("[coop] session run_id=%08lx\n", g_run_id); }
    if (cfg("COOP_RATE", "rate", buf, sizeof buf)) { int hz = atoi(buf); g_send_interval = hz > 0 ? 1.0 / hz : 0; }
    if (cfg("COOP_SIM_LAG", "sim_lag", buf, sizeof buf)) g_sim_lag = atoi(buf);
    if (cfg("COOP_SIM_JITTER", "sim_jitter", buf, sizeof buf)) g_sim_jit = atoi(buf);
    if (cfg("COOP_SIM_LOSS", "sim_loss", buf, sizeof buf)) g_sim_loss = atoi(buf);
    if (g_sim_lag || g_sim_jit || g_sim_loss) logf("[coop] SIMULATED NETWORK: lag %d ms + jitter 0..%d ms, loss %d%%\n", g_sim_lag, g_sim_jit, g_sim_loss);
    if (cfg("COOP_INTERP_MS", "interp_ms", buf, sizeof buf)) { int ms = atoi(buf); g_interp_delay = g_interp_min = ms > 0 ? ms / 1000.0 : 0; }
    if (cfg("COOP_INTERP_MAX", "interp_max", buf, sizeof buf)) g_interp_max = atoi(buf) / 1000.0;
    if (g_interp_max < g_interp_min) g_interp_max = g_interp_min;
    if (cfg("COOP_INTERP_ADAPT", "interp_adapt", buf, sizeof buf) && buf[0] == '0') g_interp_adapt = 0;
    logf("[coop] send interval %.1f ms, interp %.0f..%.0f ms%s, wire max %d B, proto SCO6\n", g_send_interval * 1000, g_interp_min * 1000, g_interp_max * 1000, g_interp_adapt ? " adaptive" : "", WIRE_MAX);
    // net=steam: Steam relay (UDP below stays as the fallback if Steam can't start); net=udp or nothing: LAN UDP
    if (cfg("COOP_NET", "net", buf, sizeof buf) && !_stricmp(buf, "steam")) g_net_steam = 1;
    if (cfg("COOP_STEAM_PEER", "steam_peer", buf, sizeof buf)) g_st_cfg_peer = _strtoui64(buf, 0, 10);
    if (cfg("COOP_STEAM_LOBBY", "steam_lobby", buf, sizeof buf)) g_st_cfg_lobby = _strtoui64(buf, 0, 10);
    logf("[coop] transport %s\n", g_net_steam ? "steam (started on the first frame)" : "udp");
    if (!cfg("COOP_LISTEN", "listen", buf, sizeof buf)) { logf("[coop] no UDP (no listen port in env or coop.ini)\n"); return; }
    lport = atoi(buf);
    // peer=ip:port, or empty/"auto": wait and answer whoever sends a valid packet first (only one side needs the other's IP)
    if (!cfg("COOP_PEER", "peer", buf, sizeof buf) || sscanf(buf, "%31[^:]:%d", ip, &pport) != 2) { strcpy(ip, "auto"); pport = 0; }
    WSAStartup(MAKEWORD(2, 2), &w);
    g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ioctlsocket(g_sock, FIONBIO, &nb);
    memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_port = htons((u_short)lport); a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(g_sock, (struct sockaddr *)&a, sizeof a)) { logf("[coop] bind %d failed %d\n", lport, WSAGetLastError()); closesocket(g_sock); g_sock = INVALID_SOCKET; return; }
    memset(&g_peer, 0, sizeof g_peer); g_peer.sin_family = AF_INET; g_peer.sin_port = htons((u_short)pport); g_peer.sin_addr.s_addr = inet_addr(ip);
    g_peer_known = pport > 0 && g_peer.sin_addr.s_addr != INADDR_NONE;
    g_lport = (u_short)lport;
    { BOOL on = TRUE; char hn[256]; struct hostent *he; setsockopt(g_sock, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof on);
      if (!gethostname(hn, sizeof hn) && (he = gethostbyname(hn)) && he->h_addrtype == AF_INET)
          for (; he->h_addr_list[g_n_my_ip] && g_n_my_ip < 16; g_n_my_ip++) {
              memcpy(&g_my_ip[g_n_my_ip], he->h_addr_list[g_n_my_ip], 4);
              logf("[coop] local ip %s\n", inet_ntoa(*(struct in_addr *)&g_my_ip[g_n_my_ip])); } }
    logf("[coop] udp listening %d, peer %s:%d%s\n", lport, ip, pport, g_peer_known ? "" : " (unknown: LAN broadcast discovery)");
}

static int safe_create(char *file, var *pos, ENTITY **out) {
    __try { *out = p_ent_create(file, pos, 0); return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { *out = 0; return 0; }
}

static void current_level(char *out, int n) {
    long t = 0; void *pp = safe_getvar("level_name", &t); STRING *s = pp ? (STRING *)safe_long(pp) : 0;
    out[0] = 0; if (s && (long)s != 0x7badbad) { char *c = 0; __try { c = s->chars; } __except (EXCEPTION_EXECUTE_HANDLER) { c = 0; } safe_str(c, out, n); }
}


// Engine variables (type 19 from engine_getvar): the returned slot holds a pointer to the real var in acknex.dll.
static var *engine_var(const char *name) {
    long t = 0; void *p = safe_getvar((char *)name, &t); long q;
    if (!p || t != 19) return 0;
    q = safe_long(p); if (q < 0x10000 || q == 0x7badbad) return 0;
    return (var *)q;
}
// fps_max=N (coop.ini / COOP_FPS_MAX): 0 = oyunun orijinal sınırını koru (varsayılan), N > 0 = FPS sınırı koy.
static int g_fps_max = -1;
static void fps_tick(void) {
    static int logged; var *fm, *fl;
    if (g_fps_max < 0) { char b[16]; g_fps_max = cfg("COOP_FPS_MAX", "fps_max", b, sizeof b) ? atoi(b) : 0; }
    { static double t0; static long f0; double t = now_s();   // measured frame rate, every 5 s
      if (!t0) { t0 = t; f0 = g_frame; } else if (t - t0 >= 5.0) { logf("[fps] measured %.1f fps\n", (g_frame - f0) / (t - t0)); t0 = t; f0 = g_frame; } }
    if (g_frame % 30) return;
    fm = engine_var("fps_max"); fl = engine_var("fps_lock");
    if (!logged && fm) { logged = 1; __try { logf("[fps] fps_max=%.1f fps_lock=%.1f var@%p, override %d\n", VF(*fm), fl ? VF(*fl) : -1.0, fm, g_fps_max); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
    if (fm && g_fps_max > 0) __try { if (*fm != (var)(g_fps_max << 10)) *fm = (var)(g_fps_max << 10); } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
