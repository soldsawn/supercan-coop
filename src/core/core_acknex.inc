// [CORE] Acknex A6/A7/A8 motor API, ENTITY/STRING, animasyon kancalari, SCO6 kukla & interpolasyon
// SuperCan co-op probe: dinput8.dll proxy + engine_frame IAT hook. READ-ONLY probe (no game changes).
// Build: see build.bat (MSVC x86). Drop the DLL as Game\dinput8.dll in a TEST copy only.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

typedef long var;                     // A8 var: 22.10 fixed point
#define VF(x) ((double)(x) / 1024.0)

typedef struct C_LINK { long index; struct C_LINK *next; char *name; } C_LINK;
typedef struct STRING { C_LINK link; char *chars; long length; long flags; } STRING;
// ENTITY prefix from A6 SDK atypes.h; offsets beyond `type`..`roll` UNVERIFIED for A8.
typedef struct ENTITY {
    C_LINK link; char *type;
    var x, y, z, pan, tilt, roll, scale_x, scale_y, scale_z;
    long flags; var frame, nextframe, skin;
} ENTITY;

// ---------- engine API (resolved from acknex.dll) ----------
static ENTITY *(__cdecl *p_ent_next)(ENTITY *);
static void   *(__cdecl *p_engine_getvar)(char *, long *);
static void   *(__cdecl *p_engine_gettaskinfo)(long, void **, void **);
static void   *(__cdecl *p_engine_getscriptinfo)(void *, void **);
static int (__cdecl *p_engine_frame)(void);
static int (__cdecl *real_engine_frame)(void);

static FILE *g_log;
static long g_frame;
static char g_dir[MAX_PATH];

static void logf(const char *fmt, ...) {
    va_list a; if (!g_log) return;
    va_start(a, fmt); vfprintf(g_log, fmt, a); va_end(a);
    fflush(g_log);
}

// Safe readers (SEH) -----------------------------------------------------
static int safe_str(const char *p, char *out, int n) {
    out[0] = 0; if (!p) return 0;
    __try { int i; for (i = 0; i < n - 1 && p[i]; i++) { char c = p[i]; out[i] = (c >= 32 && c < 127) ? c : '?'; } out[i] = 0; return 1; }
    __except (EXCEPTION_EXECUTE_HANDLER) { strcpy(out, "<bad>"); return 0; }
}
static int safe_ent(ENTITY *e, ENTITY *copy) {
    __try { *copy = *e; return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static ENTITY *safe_next(ENTITY *e) {
    __try { return p_ent_next(e); } __except (EXCEPTION_EXECUTE_HANDLER) { return (ENTITY *)-1; }
}
static void *safe_getvar(char *name, long *type) {
    __try { return p_engine_getvar(name, type); } __except (EXCEPTION_EXECUTE_HANDLER) { *type = -999; return 0; }
}
static void *safe_task(long n, void **a, void **b) {
    __try { return p_engine_gettaskinfo(n, a, b); } __except (EXCEPTION_EXECUTE_HANDLER) { *a = *b = (void *)-1; return (void *)-1; }
}
static void *safe_scriptinfo(void *addr, void **o) {
    __try { return p_engine_getscriptinfo(addr, o); } __except (EXCEPTION_EXECUTE_HANDLER) { *o = (void *)-1; return (void *)-1; }
}
static long safe_long(void *p) { __try { return *(long *)p; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0x7badbad; } }

// Dumps ---------------------------------------------------------------------
static void describe_ptr(const char *label, void *p) {
    char s[96]; safe_str((char *)p, s, sizeof s);
    logf("    %s=%p str='%s'\n", label, p, s);
}

static void dump_entities(void) {
    ENTITY *e = 0, c; int n = 0; char nm[64], ty[96];
    logf("== ENTITIES frame %ld\n", g_frame);
    if (!p_ent_next) return;
    for (e = safe_next(0); e && e != (ENTITY *)-1 && n < 20000; e = safe_next(e), n++) {
        if (!safe_ent(e, &c)) { logf("  %p <unreadable>\n", e); continue; }
        safe_str(c.link.name, nm, sizeof nm); safe_str(c.type, ty, sizeof ty);
        logf("  [%d] %p idx=%08lx name='%s' type='%s' pos=(%.1f %.1f %.1f) pan=%.1f flags=%08lx frame=%.2f\n",
             n, e, c.link.index, nm, ty, VF(c.x), VF(c.y), VF(c.z), VF(c.pan), c.flags, VF(c.frame));
    }
    logf("== %d entities\n", n);
}

static void dump_tasks(void) {
    long n; char s1[96], s2[96], s3[96];
    logf("== TASKS frame %ld\n", g_frame);
    if (!p_engine_gettaskinfo) return;
    for (n = 0; n < 4000; n++) {
        void *a = 0, *b = 0, *r = safe_task(n, &a, &b);
        void *so = 0, *sn = 0;
        if (!r && !a && !b) { logf("== end at %ld\n", n); break; }
        safe_str((char *)r, s1, sizeof s1);
        // try to name the code address(es)
        if (p_engine_getscriptinfo) sn = safe_scriptinfo(r, &so);
        safe_str((char *)sn, s2, sizeof s2);
        safe_str((char *)a, s3, sizeof s3);
        logf("  task %ld ret=%p('%s') a=%p('%s') b=%p  scriptinfo(ret)=%p('%s') off=%p\n",
             n, r, s1, a, s3, b, sn, s2, so);
        if (b && b != (void *)-1) {
            ENTITY c; char ty[96], nm[64];
            if (safe_ent((ENTITY *)b, &c)) { safe_str(c.type, ty, sizeof ty); safe_str(c.link.name, nm, sizeof nm);
                logf("      b-as-entity: type='%s' name='%s' pos=(%.1f %.1f %.1f)\n", ty, nm, VF(c.x), VF(c.y), VF(c.z)); }
        }
    }
}

static const char *g_varnames[] = {
    "player", "camera", "me", "you", "level_ent", "total_frames", "time_step", "sky_color",
    "mouse_mode", "video_mode", "video_screen", "fps_max", "freeze_mode", "level_name",
    "connection", "dplay_status", "session_name", "server_name", "player_name", "num_entities",
    "game_state", "oyuncu", "can", "supercan", "karakter", "kamera", "level", "bolum", "puan", "skor",
    "can_ent", "oyuncu_ent", "player_ent", "hero", "ent_player", "p_player", 0 };

static void dump_vars(void) {
    int i; logf("== VARS frame %ld\n", g_frame);
    if (!p_engine_getvar) return;
    for (i = 0; g_varnames[i]; i++) {
        long type = 0; void *p = safe_getvar((char *)g_varnames[i], &type);
        if (!p) { logf("  %-14s -> null (type %ld)\n", g_varnames[i], type); continue; }
        logf("  %-14s -> %p type=%ld value=%08lx (%.3f)\n", g_varnames[i], p, type, safe_long(p), VF(safe_long(p)));
    }
}

static void log_player(void) {
    long type = 0; void *pp; ENTITY *pl, c; char ty[96];
    if (!p_engine_getvar) return;
    pp = safe_getvar("player", &type);
    if (!pp) return;
    pl = (ENTITY *)safe_long(pp);
    if (!pl || (long)pl == 0x7badbad) { logf("f%ld player=NULL\n", g_frame); return; }
    if (!safe_ent(pl, &c)) { logf("f%ld player=%p unreadable\n", g_frame, pl); return; }
    safe_str(c.type, ty, sizeof ty);
    logf("f%ld player=%p '%s' pos=(%.2f %.2f %.2f) ang=(%.1f %.1f %.1f) frame=%.2f next=%.2f\n",
         g_frame, pl, ty, VF(c.x), VF(c.y), VF(c.z), VF(c.pan), VF(c.tilt), VF(c.roll), VF(c.frame), VF(c.nextframe));
}

// ======================= CO-OP: puppet sync (milestone 1) =======================
// Each instance sends its own SuperCan part entities' poses to the peer (UDP), and mirrors the peer's
// parts as passable puppet entities created with ent_create. Config: COOP_LISTEN=port, COOP_PEER=ip:port.
#include <winsock2.h>
#define INVISIBLE_F (1 << 8)
#define PASSABLE_F  (1 << 9)
#define MAX_PARTS 16
#pragma pack(push, 1)
typedef struct { char type[28]; float x, y, z, pan, tilt, roll, sx, sy, sz, frame, nextframe; long flags; long animated; } NETPART;
// Recorded animation call on a local part, replayed on the matching puppet.
#define OP_ANIMATE 1      // ent_animate(ent, scene, a=percent, b=mode)
#define OP_BLENDPOSE 2    // ent_blendpose(ent, a=target, b=source, c=percent)
#define OP_ANIMATEFROM 3  // ent_animatefrom(ent, src(part c), scene, a, b)
#define OP_BONERESETALL 4 // ent_bonereset_all(ent)
#define OP_BONEROTATE 5   // ent_bonerotate(ent, bone name in scene, angle a,b,c)
#define MAX_CALLS 96      // calls accumulate between sends (30 Hz send, 60 fps game)
typedef struct { BYTE op, part; char scene[30]; var a, b, c; var pose; } NETCALL;  // pose = ENTITY.pose at call time
// tms = sender clock (ms) at send; last = index of the first call of the sender's most recent frame
// (calls before it belong to older frames since the previous send).
typedef struct { DWORD magic, seq, tms, last; char level[32]; DWORD count, ncalls; NETPART part[MAX_PARTS]; NETCALL call[MAX_CALLS]; } NETPKT;
#pragma pack(pop)
#define PKT_MAGIC 0x364F4353 /* 'SCO6': compact variable-length wire format + sender clock, see pkt_encode */
#define WIRE_MAX 1200        /* stay under a typical internet MTU: no IP fragmentation */
#define POSE_OFF 548   /* ENTITY.pose byte offset (A6 SDK layout; consistent with material at 552 found at runtime) */

static ENTITY *(__cdecl *p_ent_create)(char *, var *, void *);
static var (__cdecl *p_ent_remove)(ENTITY *);
static var (__cdecl *p_ent_bones)(ENTITY *);

// ---- animation call capture (inline detours on acknex exports) ----
static ENTITY *g_local_ent[MAX_PARTS];  // local part entities (from last tick), index = packet part index
static DWORD g_local_count;
static NETCALL g_rec[MAX_CALLS]; static DWORD g_nrec, g_rec_dropped, g_anim_logs;
static long g_part_animated[MAX_PARTS];
static var (__cdecl *t_ent_animate)(ENTITY *, char *, var, var);
static var (__cdecl *t_ent_blendpose)(ENTITY *, var, var, var);
static var (__cdecl *t_ent_animatefrom)(ENTITY *, ENTITY *, char *, var, var);
static int local_index(ENTITY *e) { DWORD i; for (i = 0; i < g_local_count; i++) if (g_local_ent[i] == e) return (int)i; return -1; }
static int g_in_anim;   // >0 while inside a recorded engine call (ent_animate calls ent_animatefrom internally)
static void wrecord(ENTITY *e, int op, const char *scene, var a, var b, var c, ENTITY *src);   // world (robot) parts, see WORLD SYNC
static void record(ENTITY *e, int op, int part, const char *scene, var a, var b, var c) {
    NETCALL *k; if (g_nrec >= MAX_CALLS) { g_rec_dropped++; return; }
    k = &g_rec[g_nrec++]; memset(k, 0, sizeof *k); k->op = (BYTE)op; k->part = (BYTE)part; k->a = a; k->b = b; k->c = c;
    k->pose = safe_long((BYTE *)e + POSE_OFF);
    if (scene) safe_str(scene, k->scene, sizeof k->scene);
    g_part_animated[part] = 1;
    if (g_anim_logs < 1500 && part == 0) { g_anim_logs++; logf("[anim] op%d part%d pose=%.0f '%s' %.2f %.2f %.2f\n", op, part, VF(k->pose), k->scene, VF(a), VF(b), VF(c)); }
}
static ENTITY *g_mkr_ent[7];
static void str_or_cstr(void *nm, char *out, int n) {   // acknex.dll 0x10004260 ile bit-exact: (hdr & 0xFF700000)==0x02000000 || hdr==-1 -> *(char**)(nm+12)
    long hdr;
    if (!nm) { out[0] = 0; return; }
    hdr = safe_long(nm);
    if (hdr != 0x7badbad && (((hdr & 0xFF700000) == 0x02000000) || hdr == -1 || ((hdr >> 24) & 0xff) == 10)) {
        char *c = 0;
        __try { c = *(char **)((BYTE *)nm + 12); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        safe_str(c, out, n);
    } else {
        safe_str((char *)nm, out, n);
    }
}
static ENTITY *g_boss_govde_ent, *g_boss_dummy_ent;
static char g_boss_anim[24], g_boss_add_anim[24];
static var g_boss_anim_pct, g_boss_anim_mode, g_boss_add_pct, g_boss_add_mode;
static var __cdecl d_ent_animate(ENTITY *e, char *s, var p, var m) {
    var r; int i = g_in_anim ? -1 : local_index(e);
    if (!g_in_anim) {
        char sn[32]; str_or_cstr(s, sn, sizeof sn);
        if (i >= 0) record(e, OP_ANIMATE, i, sn, p, m, 0);
        else {
            wrecord(e, OP_ANIMATE, sn, p, m, 0, 0);
            if (e && e == g_boss_govde_ent && sn[0]) {
                if ((m & (2 << 10)) == 0) {   // temel tam gövde animasyonu (ANM_ADD değil)
                    strncpy(g_boss_anim, sn, sizeof g_boss_anim - 1); g_boss_anim[sizeof g_boss_anim - 1] = 0;
                    g_boss_anim_pct = p; g_boss_anim_mode = m;
                    g_boss_add_anim[0] = 0;
                } else {                      // üst katman (ANM_ADD: kafa/kol nişan veya ateş)
                    strncpy(g_boss_add_anim, sn, sizeof g_boss_add_anim - 1); g_boss_add_anim[sizeof g_boss_add_anim - 1] = 0;
                    g_boss_add_pct = p; g_boss_add_mode = m;
                }
            }
        }
    }
    g_in_anim++; r = t_ent_animate(e, s, p, m); g_in_anim--; return r;
}
static var __cdecl d_ent_blendpose(ENTITY *e, var t, var s, var p) {
    var r; int i = g_in_anim ? -1 : local_index(e); if (i >= 0) record(e, OP_BLENDPOSE, i, 0, t, s, p); else if (!g_in_anim) wrecord(e, OP_BLENDPOSE, 0, t, s, p, 0);
    g_in_anim++; r = t_ent_blendpose(e, t, s, p); g_in_anim--; return r;
}
static var __cdecl d_ent_animatefrom(ENTITY *e, ENTITY *src, char *s, var p, var m) {
    var r; int i = g_in_anim ? -1 : local_index(e);
    if (!g_in_anim) {
        char sn[32]; str_or_cstr(s, sn, sizeof sn);
        if (i >= 0) record(e, OP_ANIMATEFROM, i, sn, p, m, local_index(src));
        else wrecord(e, OP_ANIMATEFROM, sn, p, m, 0, src);
    }
    g_in_anim++; r = t_ent_animatefrom(e, src, s, p, m); g_in_anim--; return r;
}
// Detour: copy `n` prologue bytes (verified position-independent for this acknex build) to a trampoline.
static void *detour(void *target, void *hook, int n, const BYTE *expect) {
    BYTE *t = (BYTE *)target, *tr; DWORD old;
    if (memcmp(t, expect, n)) { logf("[hook] prologue mismatch at %p, not hooking\n", t); return 0; }
    tr = (BYTE *)VirtualAlloc(0, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    memcpy(tr, t, n); tr[n] = 0xE9; *(DWORD *)(tr + n + 1) = (DWORD)(t + n) - (DWORD)(tr + n + 5);
    VirtualProtect(t, n, PAGE_EXECUTE_READWRITE, &old);
    t[0] = 0xE9; *(DWORD *)(t + 1) = (DWORD)hook - (DWORD)(t + 5); memset(t + 5, 0x90, n - 5);
    VirtualProtect(t, n, old, &old); FlushInstructionCache(GetCurrentProcess(), t, n);
    logf("[hook] detoured %p -> %p (tramp %p)\n", t, hook, tr);
    return tr;
}
// sys_getstr detour: find the lite-C code that reads the launcher token and dump it for disassembly.
static void *(__cdecl *t_sys_getstr)(void *, void *, void *);
static void *__cdecl d_sys_getstr(void *a, void *b, void *c) {
    BYTE *ra = (BYTE *)_ReturnAddress(); void *r = t_sys_getstr(a, b, c);
    char sa[96] = "", sb[96] = "", sr[96] = ""; STRING *s;
    __try { s = (STRING *)a; safe_str(s ? s->chars : 0, sa, sizeof sa); } __except (EXCEPTION_EXECUTE_HANDLER) { safe_str((char *)a, sa, sizeof sa); }
    __try { s = (STRING *)b; safe_str(s ? s->chars : 0, sb, sizeof sb); } __except (EXCEPTION_EXECUTE_HANDLER) { safe_str((char *)b, sb, sizeof sb); }
    __try { s = (STRING *)r; safe_str(s ? s->chars : 0, sr, sizeof sr); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    logf("[sys_getstr] ra=%p a=%p('%s') b=%p('%s') c=%p -> %p('%s')\n", ra, a, sa, b, sb, c, r, sr);
    { static int dumped; extern void dump_litec_data(void); dump_litec_data(); if (dumped++) return r; }
    {
        char fn[MAX_PATH]; FILE *f; sprintf(fn, "%sohc_lite_%08lx.bin", g_dir, (DWORD)(ra - 0x1000));
        if ((f = fopen(fn, "wb"))) { __try { fwrite(ra - 0x1000, 1, 0x3000, f); } __except (EXCEPTION_EXECUTE_HANDLER) {} fclose(f); }
        logf("[sys_getstr] dumped lite-C caller region %p..+0x3000\n", ra - 0x1000);
    }
    return r;
}
static DWORD g_cap_ebx;
void dump_litec_data(void) {
    static int done; char fn[MAX_PATH]; FILE *f; BYTE *b = (BYTE *)g_cap_ebx;
    if (done++ || !b) return;
    sprintf(fn, "%slitec_data_ebx_%08lx.bin", g_dir, g_cap_ebx);
    if ((f = fopen(fn, "wb"))) { __try { fwrite(b + 0x288000, 1, 0x28000, f); } __except (EXCEPTION_EXECUTE_HANDLER) {} fclose(f); }
    logf("[sys_getstr] lite-C ebx=%08lx, dumped ebx+0x288000..+0x28000 to %s\n", g_cap_ebx, fn);
}
__declspec(naked) static void stub_sys_getstr(void) { __asm { mov g_cap_ebx, ebx } __asm { jmp d_sys_getstr } }
static void install_early_hooks(HMODULE ack) {
    static const BYTE pg[] = {0x56,0x8b,0x74,0x24,0x10,0x85,0xf6};
    t_sys_getstr = detour(GetProcAddress(ack, "sys_getstr"), stub_sys_getstr, 7, pg);
}
static var (__cdecl *t_ent_bonereset_all)(ENTITY *);
static var (__cdecl *t_ent_bonerotate)(ENTITY *, void *, var *);
static void bone_name(void *nm, char *out, int n) {   // engine accepts STRING* or char*
    long hdr = safe_long(nm);
    if (hdr != 0x7badbad && ((hdr >> 24) & 0xff) == 10) { STRING *st = (STRING *)nm; char *c = 0; __try { c = st->chars; } __except (EXCEPTION_EXECUTE_HANDLER) {} safe_str(c, out, n); }
    else safe_str((char *)nm, out, n);
}
static var __cdecl d_ent_bonereset_all(ENTITY *e) {
    var r; int i = g_in_anim ? -1 : local_index(e); if (i >= 0) record(e, OP_BONERESETALL, i, 0, 0, 0, 0); else if (!g_in_anim) wrecord(e, OP_BONERESETALL, 0, 0, 0, 0, 0);
    g_in_anim++; r = t_ent_bonereset_all(e); g_in_anim--; return r;
}
static var __cdecl d_ent_bonerotate(ENTITY *e, void *bone, var *ang) {
    var r; int i = g_in_anim ? -1 : local_index(e);
    if (i >= 0) { char nm[32]; var a0 = 0, a1 = 0, a2 = 0; bone_name(bone, nm, sizeof nm);
        __try { a0 = ang[0]; a1 = ang[1]; a2 = ang[2]; } __except (EXCEPTION_EXECUTE_HANDLER) {}
        record(e, OP_BONEROTATE, i, nm, a0, a1, a2); }
    else if (!g_in_anim) { char nm[32]; var a0 = 0, a1 = 0, a2 = 0; bone_name(bone, nm, sizeof nm);
        __try { a0 = ang[0]; a1 = ang[1]; a2 = ang[2]; } __except (EXCEPTION_EXECUTE_HANDLER) {}
        wrecord(e, OP_BONEROTATE, nm, a0, a1, a2, 0); }
    g_in_anim++; r = t_ent_bonerotate(e, bone, ang); g_in_anim--; return r;
}
// ---- bone-function spy: which other bone calls does the game make on the player parts? ----
#define NSPY 10
static const char *g_spy_name[NSPY] = {"ent_bonerotate","ent_bonerotate_parent","ent_bonemove","ent_bonescale","ent_bonereset",
    "ent_bonereset_all","ent_bonereset_branch","ent_blendframe","ent_blend","ent_morph"};
static const char *g_spy_pro[NSPY] = {"8b4c240c83ec0c","8b4c240c83ec0c","8b4c240c83ec0c","8b4c240c83ec0c","8b4424086a04",
    "8b4c2404ba01000000","8b4424086a05","568b74240c","8b44240c8b4c2408","8b4424048b4c2408"};
static void *g_spy_tramp[NSPY];
static long g_spy_count[NSPY], g_spy_logged[NSPY];
void __cdecl spy_log(int idx, void *a1) {
    int part = local_index((ENTITY *)a1);
    if (idx == 8) { long t = 0; void *pp = safe_getvar("me", &t); part = pp ? local_index((ENTITY *)safe_long(pp)) : -1; }  // ent_blend uses `me`
    if (part < 0) return;
    g_spy_count[idx]++;
    if (g_spy_logged[idx]++ < 30) logf("[spy] %s on part%d (a1=%p) f%ld\n", g_spy_name[idx], part, a1, g_frame);
}
#define SPY_STUB(N) __declspec(naked) static void spy_stub##N(void) { __asm { push dword ptr [esp+4] } __asm { push N } \
    __asm { call spy_log } __asm { add esp, 8 } __asm { jmp dword ptr [g_spy_tramp + N*4] } }
SPY_STUB(0) SPY_STUB(1) SPY_STUB(2) SPY_STUB(3) SPY_STUB(4) SPY_STUB(5) SPY_STUB(6) SPY_STUB(7) SPY_STUB(8) SPY_STUB(9)
static void *g_spy_stub[NSPY] = {spy_stub0,spy_stub1,spy_stub2,spy_stub3,spy_stub4,spy_stub5,spy_stub6,spy_stub7,spy_stub8,spy_stub9};
static void install_spy_hooks(HMODULE ack) {
    int i, k; BYTE pro[16];
    for (i = 0; i < NSPY; i++) {
        int n = (int)strlen(g_spy_pro[i]) / 2;
        if (i == 0 || i == 5) continue;   // ent_bonerotate / ent_bonereset_all are recorded, not spied
        for (k = 0; k < n; k++) { unsigned v; sscanf(g_spy_pro[i] + 2 * k, "%2x", &v); pro[k] = (BYTE)v; }
        g_spy_tramp[i] = detour(GetProcAddress(ack, g_spy_name[i]), g_spy_stub[i], n, pro);
    }
}
// ---- ent_create detour: log who spawns what (enemies/pickups), and on the co-op client stop its own enemies ----
// Client in a shared level: robots (gbr_box.mdl) are created with NO action, so their AI and parts never start
// (the host's robots are shown as puppets instead). g_world_client_active is set by world_tick.
static ENTITY *(__cdecl *t_ent_create)(char *, var *, void *);
static ENTITY *(__cdecl *t_ent_createlocal)(char *, var *, void *);
static int g_world_client_active; static DWORD g_create_logs, g_create_blocked;
static int is_blocked_spawn(const char *f) {   // NOT tasiyici_box: blocking the drone crashes the level load (its parts are used by the level script); it is hidden instead
    return !_stricmp(f, "gbr_box.mdl") || !_stricmp(f, "zbd_box.mdl") || !_stricmp(f, "ucan_box.mdl");
}
static ENTITY *create_common(ENTITY *(__cdecl *real)(char *, var *, void *), const char *fn, char *file, var *pos, void *action) {
    char f[48] = "", an[48] = ""; ENTITY *r; int block;
    safe_str(file, f, sizeof f);
    block = g_world_client_active && is_blocked_spawn(f);
    if (action && p_engine_getscriptinfo && (g_create_logs < 400 || block)) { void *o = 0; safe_scriptinfo(action, &o); safe_str((char *)o, an, sizeof an); }
    r = real(file, pos, block ? 0 : action);
    if (block) { g_create_blocked++; __try { r->flags |= INVISIBLE_F | PASSABLE_F; } __except (EXCEPTION_EXECUTE_HANDLER) {} }
    if ((g_create_logs < 600 || block) && _strnicmp(f, "mtrl_", 5) && _strnicmp(f, "menu_", 5)) { var x = 0, y = 0, z = 0; g_create_logs++;
        if (pos) __try { x = pos[0]; y = pos[1]; z = pos[2]; } __except (EXCEPTION_EXECUTE_HANDLER) {}
        logf("[create] %s '%s' action=%p '%s' pos=(%.0f %.0f %.0f) -> %p%s\n", fn, f, action, an, VF(x), VF(y), VF(z), r, block ? " BLOCKED (co-op client: host runs enemies)" : ""); }
    return r;
}
static ENTITY *__cdecl d_ent_create(char *file, var *pos, void *action) { return create_common(t_ent_create, "ent_create", file, pos, action); }
static ENTITY *__cdecl d_ent_createlocal(char *file, var *pos, void *action) { return create_common(t_ent_createlocal, "ent_createlocal", file, pos, action); }
static void install_anim_hooks(HMODULE ack) {
    { static const BYTE pc[] = {0x8b,0x54,0x24,0x08,0x85,0xd2};   // ent_create: mov edx,[esp+8]; test edx,edx (next: jnz rel8, not copied)
      t_ent_create = detour(GetProcAddress(ack, "ent_create"), d_ent_create, 6, pc);
      t_ent_createlocal = detour(GetProcAddress(ack, "ent_createlocal"), d_ent_createlocal, 6, pc); }
    static const BYTE pa[] = {0x8b,0x44,0x24,0x10,0x8b,0x4c,0x24,0x0c};       // ent_animate
    static const BYTE pb[] = {0x83,0x7c,0x24,0x10,0x00};                        // ent_blendpose
    static const BYTE pf[] = {0x8b,0x44,0x24,0x0c,0x85,0xc0};                   // ent_animatefrom
    t_ent_animate = detour(GetProcAddress(ack, "ent_animate"), d_ent_animate, 8, pa);
    t_ent_blendpose = detour(GetProcAddress(ack, "ent_blendpose"), d_ent_blendpose, 5, pb);
    t_ent_animatefrom = detour(GetProcAddress(ack, "ent_animatefrom"), d_ent_animatefrom, 6, pf);
    { static const BYTE pr[] = {0x8b,0x4c,0x24,0x04,0xba,0x01,0x00,0x00,0x00}; static const BYTE pt[] = {0x8b,0x4c,0x24,0x0c,0x83,0xec,0x0c};
      t_ent_bonereset_all = detour(GetProcAddress(ack, "ent_bonereset_all"), d_ent_bonereset_all, 9, pr);
      t_ent_bonerotate = detour(GetProcAddress(ack, "ent_bonerotate"), d_ent_bonerotate, 7, pt); }
}

// ---- appearance copy (material/lighting) from receiver's own same-type part ----
static int g_mtl_off = -1;  // byte offset of ENTITY.material, discovered at runtime (A6 SDK says 552)
static void find_material_offset(ENTITY *e) {
    int off;
    for (off = 140; off < 800; off += 4) {
        long p = safe_long((BYTE *)e + off);
        if (p > 0x10000 && p != 0x7badbad && ((safe_long((void *)p) >> 24) & 0xff) == 13) { g_mtl_off = off; break; }
    }
    logf("[look] material offset = %d (SDK A6: 552)\n", g_mtl_off);
}
static void copy_look(ENTITY *dst, ENTITY *src) {
    __try {
        long mask = ~(INVISIBLE_F | PASSABLE_F | 1 | 2 | 16);   // çarpışma/görünürlük dışındaki tüm ışık/gölge/render bayrakları (0x20 vb.)
        dst->flags = (dst->flags & ~mask) | (src->flags & mask);
        memcpy((BYTE *)dst + 64, (BYTE *)src + 64, 96 - 64);                   // skin, ambient, albedo, alpha, lightrange, rgb
        if (g_mtl_off == 552) memcpy((BYTE *)dst + 148, (BYTE *)src + 148, 400); // skill[100] (shader params)
        if (g_mtl_off > 0) *(long *)((BYTE *)dst + g_mtl_off) = *(long *)((BYTE *)src + g_mtl_off);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
static SOCKET g_sock = INVALID_SOCKET;
static struct sockaddr_in g_peer;
static int g_peer_known;                // 0 until configured or learned from the first incoming packet
// LAN discovery while the peer is unknown: about once a second, broadcast our packet to <listen port> on
// 255.255.255.255 and on each local x.y.z.255 (assumes /24). Our own echoes are dropped (local IP + our port).
static u_short g_lport;
static u_long g_my_ip[16]; static int g_n_my_ip;
static int is_self(const struct sockaddr_in *f) {
    int i; if (ntohs(f->sin_port) != g_lport) return 0;
    if ((ntohl(f->sin_addr.s_addr) >> 24) == 127) return 1;
    for (i = 0; i < g_n_my_ip; i++) if (f->sin_addr.s_addr == g_my_ip[i]) return 1;
    return 0;
}
static void discover_send(const char *pkt, int len) {
    struct sockaddr_in b; int i;
    memset(&b, 0, sizeof b); b.sin_family = AF_INET; b.sin_port = htons(g_lport);
    b.sin_addr.s_addr = INADDR_BROADCAST; sendto(g_sock, pkt, len, 0, (struct sockaddr *)&b, sizeof b);
    for (i = 0; i < g_n_my_ip; i++) { b.sin_addr.s_addr = g_my_ip[i] | htonl(0xff); sendto(g_sock, pkt, len, 0, (struct sockaddr *)&b, sizeof b); }
}
static DWORD g_seq, g_rx_count, g_last_rx_seq, g_last_rx_tick;
static NETPKT g_remote;                 // latest received
static int g_have_remote;
static NETCALL g_pend[256]; static DWORD g_npend;   // received animation calls awaiting replay

// ---- wire format 'SCO5' (little-endian, no padding) ----
// u32 magic, u32 seq, u32 tms, u8 last (first call of the newest frame, after trimming)
// u8 nstr, nstr x (u8 len, chars)   string table: [0] = level name, then part types / scene names
// u8 count,  count x (u8 type_str, f32 x y z pan tilt roll sx sy sz frame nextframe, u32 flags, u8 animated)   50 bytes
// u8 ncalls, ncalls x (u8 op, u8 part, u8 scene_str (255 = none), u8 pose, i32 a b c)                           16 bytes
// Every packet carries the full state (no deltas), so a lost packet costs nothing but its own update.
typedef struct { BYTE *p, *end; int bad; } WBUF;
static void w_put(WBUF *w, const void *v, int n) { if (w->p + n > w->end) { w->bad = 1; return; } memcpy(w->p, v, n); w->p += n; }
static void w_get(WBUF *w, void *v, int n) { if (w->p + n > w->end) { w->bad = 1; memset(v, 0, n); return; } memcpy(v, w->p, n); w->p += n; }
static int str_index(char tab[][32], int *n, const char *s) {
    int i; for (i = 0; i < *n; i++) if (!strcmp(tab[i], s)) return i;
    if (*n >= 250) return 255; strncpy(tab[*n], s, 31); tab[*n][31] = 0; return (*n)++;
}
static DWORD g_tx_trunc;   // calls left out because the packet was full
static int pkt_encode(const NETPKT *k, BYTE *buf) {
    char tab[251][32]; int nt = 0, i; BYTE ti[MAX_PARTS], ci[MAX_CALLS]; DWORD nc = k->ncalls; WBUF w; BYTE b;
    str_index(tab, &nt, k->level);
    for (i = 0; i < (int)k->count; i++) ti[i] = (BYTE)str_index(tab, &nt, k->part[i].type);
    for (i = 0; i < (int)nc; i++) ci[i] = k->call[i].scene[0] ? (BYTE)str_index(tab, &nt, k->call[i].scene) : 255;
    for (;;) {   // drop the OLDEST calls until it fits (the newest frame's calls describe the current pose)
        int size = 4 + 4 + 4 + 1 + 1 + 1 + (int)k->count * 50 + 1 + (int)nc * 16;
        for (i = 0; i < nt; i++) size += 1 + (int)strlen(tab[i]);
        if (size <= WIRE_MAX || !nc) break;
        nc--; g_tx_trunc++;
    }
    w.p = buf; w.end = buf + WIRE_MAX; w.bad = 0;
    w_put(&w, &k->magic, 4); w_put(&w, &k->seq, 4); w_put(&w, &k->tms, 4);
    { DWORD dropped = k->ncalls - nc; b = (BYTE)(k->last > dropped ? k->last - dropped : 0); w_put(&w, &b, 1); }
    b = (BYTE)nt; w_put(&w, &b, 1);
    for (i = 0; i < nt; i++) { b = (BYTE)strlen(tab[i]); w_put(&w, &b, 1); w_put(&w, tab[i], b); }
    b = (BYTE)k->count; w_put(&w, &b, 1);
    for (i = 0; i < (int)k->count; i++) { const NETPART *p = &k->part[i]; BYTE an = (BYTE)(p->animated != 0);
        w_put(&w, &ti[i], 1); w_put(&w, &p->x, 11 * 4); w_put(&w, &p->flags, 4); w_put(&w, &an, 1); }
    b = (BYTE)nc; w_put(&w, &b, 1);
    for (i = (int)(k->ncalls - nc); i < (int)k->ncalls; i++) { const NETCALL *c = &k->call[i]; BYTE pose = (BYTE)(c->pose >> 10);
        w_put(&w, &c->op, 1); w_put(&w, &c->part, 1); w_put(&w, &ci[i], 1); w_put(&w, &pose, 1);
        w_put(&w, &c->a, 4); w_put(&w, &c->b, 4); w_put(&w, &c->c, 4); }
    return w.bad ? 0 : (int)(w.p - buf);
}
static int pkt_decode(const BYTE *buf, int len, NETPKT *k) {
    char tab[256][32]; int nt, i; BYTE b; WBUF w;
    w.p = (BYTE *)buf; w.end = (BYTE *)buf + len; w.bad = 0; memset(k, 0, sizeof *k);
    w_get(&w, &k->magic, 4); if (k->magic != PKT_MAGIC) return 0;
    w_get(&w, &k->seq, 4); w_get(&w, &k->tms, 4); w_get(&w, &b, 1); k->last = b;
    w_get(&w, &b, 1); nt = b;
    for (i = 0; i < nt; i++) { w_get(&w, &b, 1); if (b > 31) return 0; w_get(&w, tab[i], b); tab[i][b] = 0; }
    if (!nt || w.bad) return 0;
    strcpy(k->level, tab[0]);
    w_get(&w, &b, 1); k->count = b; if (k->count > MAX_PARTS) return 0;
    for (i = 0; i < (int)k->count; i++) { NETPART *p = &k->part[i]; BYTE t, an;
        w_get(&w, &t, 1); if (t >= nt) return 0; strncpy(p->type, tab[t], sizeof p->type - 1);
        w_get(&w, &p->x, 11 * 4); w_get(&w, &p->flags, 4); w_get(&w, &an, 1); p->animated = an; }
    w_get(&w, &b, 1); k->ncalls = b; if (k->ncalls > MAX_CALLS) return 0;
    for (i = 0; i < (int)k->ncalls; i++) { NETCALL *c = &k->call[i]; BYTE s, pose;
        w_get(&w, &c->op, 1); w_get(&w, &c->part, 1); w_get(&w, &s, 1); w_get(&w, &pose, 1);
        w_get(&w, &c->a, 4); w_get(&w, &c->b, 4); w_get(&w, &c->c, 4);
        c->pose = (var)pose << 10; if (s != 255) { if (s >= nt) return 0; strncpy(c->scene, tab[s], sizeof c->scene - 1); } }
    if (k->last > k->ncalls) return 0;
    return !w.bad && w.p == w.end;
}
// send pacing: COOP_RATE / rate= Hz (default 30; 0 = every frame)
static double g_send_interval = 1.0 / 30, g_next_send, g_next_discover;
static double now_s(void) { static LARGE_INTEGER f; LARGE_INTEGER t; if (!f.QuadPart) QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t); return (double)t.QuadPart / f.QuadPart; }
static DWORD g_tx_count, g_tx_bytes, g_tx_max;
static DWORD g_frame_mark;   // g_nrec at the end of the previous tick = first call of the current frame

// ---- remote smoothing: render the peer ~interp_ms in the past, between two received snapshots ----
// Packets come at 30 Hz with relay jitter, the game draws at 60 fps. Each received packet is kept with its sender
// time; every frame the puppet is placed at render time rt = now - clock_offset - delay, interpolated between the
// snapshots around rt. The newest frame's animation calls of the earlier snapshot are re-run every frame with their
// numbers (anim percent, bone angles, blend percent) interpolated toward the later snapshot; every call of a
// snapshot is also run once when rt passes it, so one-off calls (pose slot fills) are never skipped.
static ENTITY *g_puppet[MAX_PARTS]; static int g_puppet_posed[MAX_PARTS];   /* tentative; defined with the puppet state below */
#define NSNAP 32
typedef struct { double t; NETPKT k; int done; } SNAP;
static SNAP g_snap[NSNAP]; static int g_snap_head, g_nsnap;
static double g_clk_off, g_interp_delay = 0.1; static int g_clk_init;
// adaptive delay: grows while the puppet has to hold (no newer snapshot), shrinks slowly back to interp_ms
static double g_interp_min = 0.08, g_interp_max = 0.25; static int g_interp_adapt = 1;
static double g_rt, g_rt_now, g_last_rt, g_interp_settle;   // smoothed render time (never jumps back), wall time of the last pick
static DWORD g_interp_hold, g_interp_frames, g_clk_resets;   // stats: frames with no later snapshot (held), total
static DWORD g_snap_late_used, g_snap_late_old;              // reordered packets: inserted in time order / too late
#define SNAPI(i) (&g_snap[(g_snap_head + (i)) % NSNAP])
static void snap_push(const NETPKT *k) {
    double t = k->tms / 1000.0, o = now_s() - t; SNAP *s;
    if (!g_clk_init || o < g_clk_off - 0.0005 || o - g_clk_off > 1.5) {   // min filter; big jump (>1.5s) = new clock/peer
        if (g_clk_init && o - g_clk_off > 1.5) g_clk_resets++;
        g_clk_off = o; g_clk_init = 1;
    } else g_clk_off += (o - g_clk_off) * 0.004;                          // slow drift upward (latency grew / clock skew)
    if (g_nsnap && (strcmp(SNAPI(g_nsnap - 1)->k.level, k->level) || t < SNAPI(g_nsnap - 1)->t - 5.0)) {   // new level / restart
        g_nsnap = 0; g_interp_settle = now_s() + 3.0;                     // loading stalls are not jitter: restart the delay
        if (g_interp_adapt && g_interp_delay > 0) g_interp_delay = g_interp_min;
    }
    if (g_nsnap && t <= SNAPI(g_nsnap - 1)->t) {   // late (reordered): insert in time order unless already rendered past
        int j, m;
        if (t <= g_last_rt) { g_snap_late_old++; return; }
        for (j = g_nsnap - 1; j >= 0 && SNAPI(j)->t > t; j--) {}
        if (j >= 0 && SNAPI(j)->t == t) return;   // duplicate
        if (g_nsnap == NSNAP) { if (j < 0) return; g_snap_head = (g_snap_head + 1) % NSNAP; g_nsnap--; j--; }
        for (m = g_nsnap; m > j + 1; m--) *SNAPI(m) = *SNAPI(m - 1);
        g_nsnap++; s = SNAPI(j + 1); s->t = t; s->k = *k; s->done = 0; g_snap_late_used++;
        return;
    }
    if (g_nsnap == NSNAP) { g_snap_head = (g_snap_head + 1) % NSNAP; g_nsnap--; }
    s = SNAPI(g_nsnap++); s->t = t; s->k = *k; s->done = 0;
}
#define ABSF(x) ((x) < 0 ? -(x) : (x))
static float lerpf(float a, float b, float f) { return a + (b - a) * f; }
static float lerp_ang(float a, float b, float f) { float d = b - a; while (d > 180) d -= 360; while (d < -180) d += 360;   /* no math.h: it clashes with our logf */ return a + d * f; }
static var lerp_ang_v(var a, var b, float f) { return (var)(lerp_ang((float)VF(a), (float)VF(b), f) * 1024.0f); }
static var lerp_pct_v(var a, var b, float f, int wrap) {   // anim percent 0..100, cyclic animations wrap 100 -> 0
    float x = (float)VF(a), y = (float)VF(b), v;
    if (wrap && y < x - 50) y += 100;
    v = lerpf(x, y, f); if (wrap && v >= 100) v -= 100;
    return (var)(v * 1024.0f);
}
static void replay_on(ENTITY *q, ENTITY *src, int *posed, const NETCALL *a);
static void replay_call(const NETCALL *a) {
    ENTITY *q = (a->part < MAX_PARTS) ? g_puppet[a->part] : 0;
    if (!q) return;
    replay_on(q, (a->op == OP_ANIMATEFROM && (DWORD)a->c < MAX_PARTS) ? g_puppet[a->c] : 0, &g_puppet_posed[a->part], a);
}
static void replay_on(ENTITY *q, ENTITY *src, int *posed, const NETCALL *a) {
    __try {
        // The original filled its extra pose slots (2..4) before the puppet existed; blending from an
        // unfilled slot collapses the skeleton. Seed every slot once with the first animation we see.
        if (a->op == OP_ANIMATE && t_ent_animate && !*posed) {
            int s; for (s = 4; s >= 1; s--) { *(var *)((BYTE *)q + POSE_OFF) = s << 10; t_ent_animate(q, (char *)a->scene, a->a, a->b); }
            *posed = 1;
            logf("[coop] seeded pose slots 1..4 of puppet %p with '%s'\n", q, a->scene);
        }
        *(var *)((BYTE *)q + POSE_OFF) = a->pose;   // animate/blend into the same pose slot as the original
        if (a->op == OP_ANIMATE && t_ent_animate) t_ent_animate(q, (char *)a->scene, a->a, a->b);
        else if (a->op == OP_BLENDPOSE && t_ent_blendpose) t_ent_blendpose(q, a->a, a->b, a->c);
        else if (a->op == OP_BONERESETALL && t_ent_bonereset_all) t_ent_bonereset_all(q);
        else if (a->op == OP_BONEROTATE && t_ent_bonerotate) { var ang[3]; ang[0] = a->a; ang[1] = a->b; ang[2] = a->c; t_ent_bonerotate(q, (void *)a->scene, ang); }
        else if (a->op == OP_ANIMATEFROM && t_ent_animatefrom && src)
            t_ent_animatefrom(q, src, (char *)a->scene, a->a, a->b);
    } __except (EXCEPTION_EXECUTE_HANDLER) { logf("[coop] replay fault op%d part%d on %p\n", a->op, a->part, q); }
}
// Which snapshots surround render time: *pa, *pb (may be equal) and fraction f. 0 if there is nothing yet.
static int snap_pick(SNAP **pa, SNAP **pb, float *f) {
    double rt, now, dt, target; int i, held;
    if (!g_nsnap) return 0;
    now = now_s(); dt = g_rt_now ? now - g_rt_now : 0; g_rt_now = now; if (dt < 0 || dt > 0.25) dt = 0;
    if (g_interp_delay > 0) {
        // render time runs with the wall clock and is pulled gently toward the target, so delay changes never jump
        target = now - g_clk_off - g_interp_delay;
        if (!g_rt || ABSF(target - g_rt) > 0.25) g_rt = target; else g_rt += dt + (target - g_rt) * 0.05;
        rt = g_rt;
    } else rt = 1e300;
    g_last_rt = rt;
    for (i = 0; i < g_nsnap && SNAPI(i)->t <= rt; i++) {}
    g_interp_frames++;
    held = i == g_nsnap && g_interp_delay > 0;
    if (g_interp_adapt && g_interp_delay > 0 && now >= g_interp_settle) {   // ~1% held frames at equilibrium (+0.5 s per held s, -5 ms per s)
        static double hold_t;   // length of the current hold; long ones are stalls (loading, frozen window), not jitter
        hold_t = held ? hold_t + dt : 0;
        if (held) { if (hold_t < 0.3) { g_interp_delay += dt * 0.5; if (g_interp_delay > g_interp_max) g_interp_delay = g_interp_max; } }
        else { g_interp_delay -= dt * 0.005; if (g_interp_delay < g_interp_min) g_interp_delay = g_interp_min; }
    }
    if (i == 0) { *pa = *pb = SNAPI(0); *f = 0; return 1; }                 // render time before everything we have
    if (i == g_nsnap) { *pa = *pb = SNAPI(g_nsnap - 1); *f = 1; if (held) g_interp_hold++; return 1; }   // no newer one yet: hold
    *pa = SNAPI(i - 1); *pb = SNAPI(i);
    *f = (float)((rt - (*pa)->t) / ((*pb)->t - (*pa)->t)); if (*f < 0) *f = 0; if (*f > 1) *f = 1;
    return 1;
}
// Interpolated value of remote part i (falls back to the newest packet when the pair doesn't match).
static NETPART part_at(int i, const SNAP *a, const SNAP *b, float f) {
    const NETPART *pa = (DWORD)i < a->k.count ? &a->k.part[i] : 0, *pb = (DWORD)i < b->k.count ? &b->k.part[i] : 0;
    NETPART r = g_remote.part[i];
    if (!pa || !pb || strcmp(pa->type, r.type) || strcmp(pb->type, r.type)) return r;
    r = f < 0.5f ? *pa : *pb;   // flags, frame of non-animated parts: nearest
    if (ABSF(pb->x - pa->x) + ABSF(pb->y - pa->y) + ABSF(pb->z - pa->z) < 400) {   // else teleport: no sliding
        r.x = lerpf(pa->x, pb->x, f); r.y = lerpf(pa->y, pb->y, f); r.z = lerpf(pa->z, pb->z, f);
    }
    r.pan = lerp_ang(pa->pan, pb->pan, f); r.tilt = lerp_ang(pa->tilt, pb->tilt, f); r.roll = lerp_ang(pa->roll, pb->roll, f);
    r.sx = lerpf(pa->sx, pb->sx, f); r.sy = lerpf(pa->sy, pb->sy, f); r.sz = lerpf(pa->sz, pb->sz, f);
    return r;
}
// Run the animation side for this frame: one-off replays of passed snapshots, then a's newest frame interpolated to b.
static void anim_at(SNAP *a, SNAP *b, float f) {
    int i; DWORD k, na, nb; const NETCALL *ca, *cb; int match;
    for (i = 0; i < g_nsnap; i++) { SNAP *s = SNAPI(i);
        if (s->done || (s != a && s->t > a->t)) continue;
        for (k = 0; k < s->k.ncalls; k++) replay_call(&s->k.call[k]);
        s->done = 1; }
    na = a->k.ncalls - a->k.last; nb = b->k.ncalls - b->k.last;
    ca = &a->k.call[a->k.last]; cb = &b->k.call[b->k.last];
    match = a != b && na == nb;
    for (k = 0; match && k < na; k++) match = ca[k].op == cb[k].op && ca[k].part == cb[k].part && !strcmp(ca[k].scene, cb[k].scene);
    for (k = 0; k < na; k++) {
        NETCALL c = ca[k];
        if (match) switch (c.op) {
            case OP_ANIMATE: case OP_ANIMATEFROM: c.a = lerp_pct_v(ca[k].a, cb[k].a, f, 1); break;
            case OP_BONEROTATE: c.a = lerp_ang_v(ca[k].a, cb[k].a, f); c.b = lerp_ang_v(ca[k].b, cb[k].b, f); c.c = lerp_ang_v(ca[k].c, cb[k].c, f); break;
            case OP_BLENDPOSE: c.c = lerp_pct_v(ca[k].c, cb[k].c, f, 0); break;
        }
        replay_call(&c);
    }
    for (i = 0; i < MAX_PARTS; i++) if (g_puppet[i]) __try { *(var *)((BYTE *)g_puppet[i] + POSE_OFF) = 1 << 10; } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
static ENTITY *g_puppet[MAX_PARTS];     // puppet entity for remote part i
static int g_puppet_posed[MAX_PARTS];   // pose slots seeded?
static int g_puppet_looked[MAX_PARTS];  // görünüm (material/skill) kopyalandı mı?
static char g_puppet_type[MAX_PARTS][28];

