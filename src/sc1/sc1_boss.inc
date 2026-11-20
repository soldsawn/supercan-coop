// [SC1] D.E.V.E. Boss Fight (SB10): tek otoriteli ayna, 4 pil, QTE, lazer, son ara sahne

static void hud_toast(const char *msg, double sec);
// Deve / Hangar Boss — Tek Otoriteli (Host-Driven) Mimari ('SB10')
// Host: doğal AIF_DEVE_* AI + 4 pil + temel/ADD animasyon kaydı -> 30 Hz BOSSPKT ('SB10')
// Client: AIF_DEVE_sevin/ziplageri engelli (2. Deve imkânsız!), 4 pil nesne_pil_loop doğal, Deve modeli/kutusu/animasyonu/işareti host aynası, QTE iki yönlü.
#define BOSS_MAGIC 0x30314253 /* 'SB10' */
typedef struct {
    DWORD magic, seq;
    var deve_hp, pil_hp[4], pil_dead;
    var deve_x, deve_y, deve_z, deve_pan;
    var govde_x, govde_y, govde_z;
    var deve_anim_pct, deve_anim_mode;
    var deve_add_pct, deve_add_mode;
    var isaret_x, isaret_y, isaret_pan;
    var lsr_sap1_y, lsr_sap2_y;
    long kapi_flags, govde_flags, lsr1_flags, lsr2_flags, alt_blok_flags, alt_lsr_flags;
    long isaret_flags, lsr_isin1_flags;
    BYTE has_deve, has_kapi, from_host, combo_busy;
    BYTE has_govde, has_lsr1, has_lsr2, sub_lvl;
    BYTE has_alt_blok, has_alt_lsr, hangar_stage, deve_phase2;
    BYTE has_isaret, has_lsr_sap1, has_lsr_sap2, pad2;
    DWORD laser_seq;
    char deve_anim[24];
    char deve_add_anim[24];
} BOSSPKT;
static DWORD g_boss_seq, g_boss_rx_seq, g_boss_laser_seq, g_boss_rx_laser_seq;
static var g_boss_last_deve = -1, g_boss_last_pil[4] = {-1, -1, -1, -1};
static long g_boss_last_kapi = -1, g_boss_last_govde = -1, g_boss_last_lsr1 = -1, g_boss_last_lsr2 = -1;
static long g_boss_last_alt_blok = -1, g_boss_last_alt_lsr = -1, g_boss_last_stage = -1, g_boss_last_phase2 = -1;
static int g_boss_tx_left, g_boss_logged, g_boss_last_combo, g_boss_last_laser, g_boss_peer_combo;
static int g_boss_pils_triggered, g_boss_panel_opened, g_boss_panel_closed, g_boss_have_rx, g_boss_qte_armed, g_boss_end_started;
static int g_boss_have_prev;
static double g_boss_next_pose_tx, g_boss_combo_toast_t, g_boss_prev_t, g_boss_rx_t;
static BOSSPKT g_boss_pkt, g_boss_rx_pkt, g_boss_prev_pkt;
static char g_boss_level[64];

// Client'ta bağımsız Deve AI döngüsünü (AIF_DEVE_sevin / AIF_DEVE_ziplageri) engelle;
// Host'ta da client QTE (AIF_DEVE_combo_kap) yaparken AIF_DEVE_ziplageri'yi beklet (g_boss_host_deferred_zgeri = 1) ki Deve client'ın altından kaçmasın!
static volatile int g_boss_client_block, g_boss_host_block_zgeri, g_boss_host_deferred_zgeri;
static DWORD g_orig_deve_sevin_cont, g_orig_deve_ziplageri_cont;
static __declspec(naked) void stub_deve_sevin(void) { __asm {
    cmp dword ptr [g_boss_client_block], 0
    jne skip_sevin
    push ebp
    push ebx
    push esi
    push edi
    push dword ptr [g_orig_deve_sevin_cont]
    jmp dword ptr [g_orig_deve_sevin_cont]
skip_sevin:
    ret
} }
static __declspec(naked) void stub_deve_ziplageri(void) { __asm {
    cmp dword ptr [g_boss_client_block], 0
    jne skip_zgeri
    cmp dword ptr [g_boss_host_block_zgeri], 0
    jne defer_zgeri
    push ebp
    push ebx
    push esi
    push edi
    push dword ptr [g_orig_deve_ziplageri_cont]
    jmp dword ptr [g_orig_deve_ziplageri_cont]
defer_zgeri:
    mov dword ptr [g_boss_host_deferred_zgeri], 1
skip_zgeri:
    ret
} }
static void boss_ai_hooks_install(void) {
    static int done;
    static void *(__cdecl *p_gs)(char *);
    static const BYTE pro[9] = {0x55, 0x53, 0x56, 0x57, 0xE8, 0, 0, 0, 0};
    const char *names[2] = { "AIF_DEVE_sevin", "AIF_DEVE_ziplageri" };
    void *stubs[2] = { (void *)stub_deve_sevin, (void *)stub_deve_ziplageri };
    DWORD *conts[2] = { &g_orig_deve_sevin_cont, &g_orig_deve_ziplageri_cont };
    int i;
    if (done) return;
    done = 1;
    if (!p_gs) p_gs = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
    if (!p_gs) return;
    for (i = 0; i < 2; i++) {
        BYTE *a = 0; DWORD old; int ok = 0;
        __try { a = (BYTE *)p_gs((char *)names[i]); } __except (EXCEPTION_EXECUTE_HANDLER) { a = 0; }
        if (!a) continue;
        __try { ok = !memcmp(a, pro, 9); } __except (EXCEPTION_EXECUTE_HANDLER) { ok = 0; }
        if (!ok) { logf("[boss] %s prologue mismatch at %p, not hooking\n", names[i], a); continue; }
        *conts[i] = (DWORD)(a + 9);
        if (VirtualProtect(a, 9, PAGE_EXECUTE_READWRITE, &old)) {
            a[0] = 0xE9;
            *(DWORD *)(a + 1) = (DWORD)stubs[i] - (DWORD)(a + 5);
            memset(a + 5, 0x90, 4);
            VirtualProtect(a, 9, old, &old);
            FlushInstructionCache(GetCurrentProcess(), a, 9);
            logf("[boss] hooked %s at %p (client AI block ready)\n", names[i], a);
        }
    }
}

static int boss_proc_active(const char *fn_name) {
    static void *(__cdecl *p_gs)(char *);
    static var (__cdecl *p_ps)(void *);
    void *fn = 0; var r = 0;
    if (!p_gs) p_gs = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
    if (!p_ps) p_ps = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "proc_status");
    if (!p_gs || !p_ps) return 0;
    __try { fn = p_gs((char *)fn_name); if (fn) r = p_ps(fn); } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return r != 0;
}

static void boss_call_script_me(const char *fn_name, long a1, long a2, ENTITY *me_ent) {
    static void *(__cdecl *p_gs)(char *);
    static void *(__cdecl *p_gv)(char *, long *);
    void *fn = 0;
    if (!p_gs) p_gs = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
    if (!p_gv) p_gv = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getvar");
    if (!p_callscript4) p_callscript4 = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_callscript4");
    if (!p_gs || !p_callscript4) return;
    __try { fn = p_gs((char *)fn_name); } __except (EXCEPTION_EXECUTE_HANDLER) { fn = 0; }
    if (!fn) return;
    if (me_ent && p_gv) {
        long t = 0;
        ENTITY **me_ptr = (ENTITY **)p_gv("me", &t);
        if (me_ptr) {
            ENTITY *old_me = 0;
            __try { old_me = *me_ptr; *me_ptr = me_ent; p_callscript4((long)fn, a1, a2, 0, 0); *me_ptr = old_me; } __except (EXCEPTION_EXECUTE_HANDLER) {}
            return;
        }
    }
    __try { p_callscript4((long)fn, a1, a2, 0, 0); } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
static void boss_call_script(const char *fn_name, long a1, long a2) { boss_call_script_me(fn_name, a1, a2, 0); }

static var boss_lerp_var(var a, var b, double f) {
    return (var)((double)a + (double)(b - a) * f);
}
static var boss_lerp_ang(var a, var b, double f) {
    double d = (double)(b - a);
    while (d > (double)(180 << 10)) d -= (double)(360 << 10);
    while (d < -(double)(180 << 10)) d += (double)(360 << 10);
    return (var)((double)a + d * f);
}
static var boss_lerp_pct(const char *na, var pa, const char *nb, var pb, var mode_b, double f) {
    if (!na[0] || strcmp(na, nb) != 0) return pb;
    if (pb >= pa && (pb - pa) < (50 << 10)) return boss_lerp_var(pa, pb, f);
    if ((mode_b & (1 << 10)) && pa > (65 << 10) && pb < (35 << 10)) {
        double d = (double)((pb + (100 << 10)) - pa);
        double v = (double)pa + d * f;
        if (v >= (double)(100 << 10)) v -= (double)(100 << 10);
        return (var)v;
    }
    return pb;
}

// Client'ta Deve kutusu, modeli, animasyonu, QTE işareti ve üst lazer rayını her kare host durumuna yumuşakça (interpole) aynalar (QTE yapmıyorken)
static void boss_client_apply_mirror(DWORD ebx) {
    const BOSSPKT *p = &g_boss_rx_pkt;
    const BOSSPKT *q = &g_boss_prev_pkt;
    int interp = 0; double f = 1.0;
    if (!g_boss_have_rx || !p->from_host || is_host()) return;
    if (g_boss_last_combo) return;   // client QTE (AIF_DEVE_combo_kap) oynuyorken kamerasına/modeline dokunma
    {   long cur_st = 0;
        __try { cur_st = G_HANGAR_STAGE(ebx) >> 10; } __except (EXCEPTION_EXECUTE_HANDLER) { cur_st = 0; }
        // Boss öldükten sonra (stage >= 3) ölü Deve'yi host'un eski "Bekle" karesiyle ayağa kaldırma!
        if (p->hangar_stage >= 3 || cur_st >= 3) return;
    }
    if (g_boss_have_prev && q->from_host) {
        double dt = g_boss_rx_t - g_boss_prev_t;
        if (dt > 0.005 && dt < 0.35) {
            double span = dt < 0.02 ? 0.02 : (dt > 0.15 ? 0.15 : dt);
            f = (now_s() - g_boss_rx_t) / span;
            if (f < 0.0) f = 0.0; else if (f > 1.0) f = 1.0;
            interp = 1;
        }
    }
    if (p->has_deve) {
        ENTITY *de = 0, dc;
        __try { de = G_DEVE_BOX(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { de = 0; }
        if (de && safe_ent(de, &dc)) {
            int ok = interp && q->has_deve;
            var dx = ok ? boss_lerp_var(q->deve_x, p->deve_x, f) : p->deve_x;
            var dy = ok ? boss_lerp_var(q->deve_y, p->deve_y, f) : p->deve_y;
            var dz = ok ? boss_lerp_var(q->deve_z, p->deve_z, f) : p->deve_z;
            var dp = ok ? boss_lerp_ang(q->deve_pan, p->deve_pan, f) : p->deve_pan;
            __try {
                de->x = dx; de->y = dy; de->z = dz;
                G_DEVE_PAN(ebx) = dp;
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
    if (p->has_govde) {
        ENTITY *ge = 0, gc;
        __try { ge = G_DEVE_GOVDE(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { ge = 0; }
        if (ge && safe_ent(ge, &gc)) {
            long want = (gc.flags & ~INVISIBLE_F) | (p->govde_flags & INVISIBLE_F);
            int ok = interp && q->has_govde && ((q->govde_flags & INVISIBLE_F) == 0);
            var gx = ok ? boss_lerp_var(q->govde_x, p->govde_x, f) : p->govde_x;
            var gy = ok ? boss_lerp_var(q->govde_y, p->govde_y, f) : p->govde_y;
            var gz = ok ? boss_lerp_var(q->govde_z, p->govde_z, f) : p->govde_z;
            var gp = (interp && q->has_deve) ? boss_lerp_ang(q->deve_pan, p->deve_pan, f) : p->deve_pan;
            var apct = ok ? boss_lerp_pct(q->deve_anim, q->deve_anim_pct, p->deve_anim, p->deve_anim_pct, p->deve_anim_mode, f) : p->deve_anim_pct;
            var addpct = ok ? boss_lerp_pct(q->deve_add_anim, q->deve_add_pct, p->deve_add_anim, p->deve_add_pct, p->deve_add_mode, f) : p->deve_add_pct;
            __try {
                if (want != gc.flags) ge->flags = want;
                if ((want & INVISIBLE_F) == 0) {
                    ge->x = gx; ge->y = gy; ge->z = gz;
                    ge->pan = gp; ge->tilt = 0; ge->roll = 0;
                    if (p->deve_anim[0] && t_ent_animate) {
                        g_in_anim++;
                        // Önce temel tam gövde animasyonu (ANM_ADD olmadan iskeleti sıfırdan kur), sonra varsa ANM_ADD katmanı!
                        t_ent_animate(ge, (char *)p->deve_anim, apct, p->deve_anim_mode & ~(2 << 10));
                        if (p->deve_add_anim[0])
                            t_ent_animate(ge, (char *)p->deve_add_anim, addpct, p->deve_add_mode);
                        g_in_anim--;
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
    if (p->has_isaret) {
        ENTITY *ie = 0, ic;
        __try { ie = G_DEVE_ISARET(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { ie = 0; }
        if (ie && safe_ent(ie, &ic)) {
            long want = (ic.flags & ~INVISIBLE_F) | (p->isaret_flags & INVISIBLE_F);
            __try {
                if (want != ic.flags) ie->flags = want;
                if ((want & INVISIBLE_F) == 0) {
                    ie->x = p->isaret_x; ie->y = p->isaret_y; ie->pan = p->isaret_pan;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
    if (p->has_lsr_sap2) {
        ENTITY *s2 = 0, s2c;
        var s2y = (interp && q->has_lsr_sap2) ? boss_lerp_var(q->lsr_sap2_y, p->lsr_sap2_y, f) : p->lsr_sap2_y;
        __try { s2 = G_LSR_SAP2(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { s2 = 0; }
        if (s2 && safe_ent(s2, &s2c)) __try { s2->y = s2y; } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    // Client'ta DEVE_laser_atak aktifken görsel (G_LSR_SAP1/ISIN1) ile yerel hasar taraması aynı döngüde yürüsün;
    // aktif değilken host'un ray konumunu ve görünürlüğünü birebir aynala (görünmez lazer hasarını önler).
    if (p->has_lsr_sap1 && !boss_proc_active("DEVE_laser_atak")) {
        ENTITY *s1 = 0, *i1 = 0, sc, ic;
        var sy = (interp && q->has_lsr_sap1) ? boss_lerp_var(q->lsr_sap1_y, p->lsr_sap1_y, f) : p->lsr_sap1_y;
        __try { s1 = G_LSR_SAP1(ebx); i1 = G_LSR_ISIN1(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        if (s1 && safe_ent(s1, &sc)) __try { s1->y = sy; } __except (EXCEPTION_EXECUTE_HANDLER) {}
        if (i1 && safe_ent(i1, &ic)) {
            long want = (ic.flags & ~INVISIBLE_F) | (p->lsr_isin1_flags & INVISIBLE_F);
            __try { i1->y = sy; if (want != ic.flags) i1->flags = want; } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
}

static void boss_rx(const BYTE *buf, int len) {
    static void (__cdecl *p_hull)(ENTITY *, var);
    BOSSPKT p; DWORD ebx = ebx_base(); int i;
    if (len < (int)sizeof(BOSSPKT) || !ebx || _stricmp(g_boss_level, "tunel.wmb")) return;
    memcpy(&p, buf, sizeof p);
    if (p.magic != BOSS_MAGIC) return;
    if (p.seq && p.seq <= g_boss_rx_seq && g_boss_rx_seq - p.seq < 1000) return;
    g_boss_rx_seq = p.seq;
    if (p.from_host && !is_host()) {
        double now = now_s();
        if (g_boss_have_rx && g_boss_rx_pkt.from_host) {
            g_boss_prev_pkt = g_boss_rx_pkt;
            g_boss_prev_t = g_boss_rx_t;
            g_boss_have_prev = 1;
        }
        g_boss_rx_pkt = p;
        g_boss_rx_t = now;
        g_boss_have_rx = 1;
    }
    // Client QTE yaparken host'ta Deve'nin animasyonunu client'tan aynalamak için paketi sakla
    if (!p.from_host && is_host() && p.combo_busy) { g_boss_rx_pkt = p; g_boss_have_rx = 1; }
    // QTE / boss üstüne çıkma kilidi
    if (p.combo_busy && !g_boss_peer_combo) {
        double now = now_s();
        if (now >= g_boss_combo_toast_t) { hud_toast("ARKADAS BOSSUN USTUNDE!", 2.5); g_boss_combo_toast_t = now + 3.0; }
        logf("[boss] peer entered QTE combo (AIF_DEVE_combo_kap) -> locking local QTE\n");
    }
    // Boss ölümü (hangar_stage >= 3) İKİ YÖNLÜ: client bossu öldürünce host'ta da kesin ölsün ve asla dirilmesin!
    if (p.hangar_stage >= 3) {
        long cur_st = 0;
        __try { cur_st = G_HANGAR_STAGE(ebx) >> 10; } __except (EXCEPTION_EXECUTE_HANDLER) { cur_st = 0; }
        if (cur_st < 3) {
            __try {
                ENTITY *de = G_DEVE_BOX(ebx), *ie = G_DEVE_ISARET(ebx), *ge = G_DEVE_GOVDE(ebx), gc;
                G_HANGAR_STAGE(ebx) = 3 << 10;
                G_DEVE_PHASE2(ebx) = 1 << 10;
                G_DEVE_HP(ebx) = 1 << 10;
                if (de) de->flags |= PASSABLE_F;
                if (ie) ie->flags |= INVISIBLE_F;
                if (ge && safe_ent(ge, &gc) && t_ent_animate) {
                    g_in_anim++;
                    t_ent_animate(ge, "olum", 100 << 10, 0);
                    g_in_anim--;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
            g_boss_host_deferred_zgeri = 0;
            g_boss_last_stage = 3;
            if (!g_boss_panel_closed) {
                g_boss_panel_closed = 1;
                boss_call_script("EP_deve_panel_kapa", 0, 0);
                if (is_host()) boss_call_script("AIF_DEVE_combo_bns", 0, 0);
            }
            if (!is_host() && !g_boss_end_started && !boss_proc_active("nesne_lsr_ustengel")) {
                g_boss_end_started = 1;
                boss_call_script("nesne_lsr_ustengel", 0, 0);
                logf("[boss] started nesne_lsr_ustengel on client for ending cutscene (y > 650)\n");
            }
            logf("[boss] rx boss DEFEATED (stage=3) -> synced death & closed panel!\n");
        }
    }
    if (!p.combo_busy && g_boss_peer_combo && is_host()) {
        g_boss_host_block_zgeri = 0;
        if (g_boss_host_deferred_zgeri) {
            long cur_st = 0;
            g_boss_host_deferred_zgeri = 0;
            __try { cur_st = G_HANGAR_STAGE(ebx) >> 10; } __except (EXCEPTION_EXECUTE_HANDLER) { cur_st = 0; }
            if (cur_st < 3 && p.hangar_stage < 3) {
                // me = g_boss_dummy_ent (FISH.MDL) ile devam ettir ki Faz 2 zıplamaları da en yakın oyuncuyu (client dahil!) hedeflesin!
                boss_call_script_me("AIF_DEVE_ziplageri", 0, 0, g_boss_dummy_ent);
                logf("[boss] client finished QTE -> resumed deferred AIF_DEVE_ziplageri on host (me=%p)\n", g_boss_dummy_ent);
            }
        }
    }
    g_boss_peer_combo = p.combo_busy ? 1 : 0;
    if (is_host()) g_boss_host_block_zgeri = g_boss_peer_combo;
    // 4 pil canı: henüz başlamamış pil (0.0 && pil_dead==0) asla 160.0'ı sıfırlamasın!
    for (i = 0; i < 4; i++) {
        ENTITY *pe = 0, pc;
        if (p.pil_hp[i] == 0 && p.pil_dead <= 0) continue;
        __try { pe = G_PIL_BOX(ebx, i); } __except (EXCEPTION_EXECUTE_HANDLER) { pe = 0; }
        if (!pe || !safe_ent(pe, &pc)) continue;
        __try {
            var *sk = (var *)((BYTE *)pe + 148);
            var cur = sk[35];
            if (cur > 0 && p.pil_hp[i] < cur) {
                if (p.pil_hp[i] <= 0) {
                    sk[35] = 0;
                    sk[34] = -102;
                    sk[36] = 0;
                    sk[38] += (1 << 10);   // nesne_pil_loop (decomp.c:170418) s39 değişmeden patlatmayı atlar — ANINDA patlat!
                } else {
                    sk[35] = p.pil_hp[i];
                    sk[34] = p.pil_hp[i];
                }
                g_boss_last_pil[i] = p.pil_hp[i];
                logf("[boss] rx pil[%d] hp %.1f -> %.1f\n", i, VF(cur), VF(p.pil_hp[i]));
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    // Deve Boss gerçek canı (G_DEVE_HP = g_07fa467d) ve 2. faz (G_DEVE_PHASE2 = g_07d5a271)
    // DİKKAT: Yerel oyuncu QTE yapıyorken (g_boss_last_combo == 1) karşıdan gelen eski HP asla yerel 1->201 yükselişini geri ezmesin!
    if (p.has_deve && p.deve_hp > 0 && !g_boss_last_combo) {
        __try {
            var cur_hp = G_DEVE_HP(ebx);
            long cur_p2 = G_DEVE_PHASE2(ebx) >> 10;
            if (p.deve_phase2 > cur_p2) {
                G_DEVE_PHASE2(ebx) = (var)((long)p.deve_phase2 << 10);
                G_DEVE_HP(ebx) = p.deve_hp;
                g_boss_last_deve = p.deve_hp;
                g_boss_last_phase2 = p.deve_phase2;
                logf("[boss] rx deve phase2 %ld -> %d, G_DEVE_HP -> %.1f\n", cur_p2, (int)p.deve_phase2, VF(p.deve_hp));
            } else if (p.combo_busy && p.deve_hp != cur_hp) {
                // Karşı taraf aktif QTE yapıyorsa onun otoriteli HP'sini (1 -> 201 dahil) birebir al!
                G_DEVE_HP(ebx) = p.deve_hp;
                g_boss_last_deve = p.deve_hp;
                logf("[boss] rx deve QTE G_DEVE_HP %.1f -> %.1f\n", VF(cur_hp), VF(p.deve_hp));
            } else if (p.deve_phase2 == cur_p2 && cur_hp > (1 << 10) && p.deve_hp < cur_hp) {
                G_DEVE_HP(ebx) = p.deve_hp;
                g_boss_last_deve = p.deve_hp;
                logf("[boss] rx deve G_DEVE_HP %.1f -> %.1f\n", VF(cur_hp), VF(p.deve_hp));
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    // Host otoriteli Deve lazer saldırısı (DEVE_laser_atak)
    if (p.from_host && !is_host() && p.laser_seq && p.laser_seq != g_boss_rx_laser_seq) {
        g_boss_rx_laser_seq = p.laser_seq;
        if (!boss_proc_active("DEVE_laser_atak")) {
            // Başlatmadan hemen önce lazer raylarını host'un gerçek başlangıç konumuna hizala (arenanın sonunda takılı kalmasın!)
            __try {
                ENTITY *s1 = G_LSR_SAP1(ebx), *s2 = G_LSR_SAP2(ebx), *i1 = G_LSR_ISIN1(ebx), ec;
                if (p.has_lsr_sap2 && s2 && safe_ent(s2, &ec)) s2->y = p.lsr_sap2_y;
                if (p.has_lsr_sap1 && s1 && safe_ent(s1, &ec)) s1->y = p.lsr_sap1_y;
                if (p.has_lsr_sap1 && i1 && safe_ent(i1, &ec)) i1->y = p.lsr_sap1_y;
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
            boss_call_script("DEVE_laser_atak", 0, 0);
            logf("[boss] rx laser_seq=%lu -> started DEVE_laser_atak on client (sap1_y=%.1f sap2_y=%.1f)\n",
                 p.laser_seq, VF(p.lsr_sap1_y), VF(p.lsr_sap2_y));
        }
    }
    // Hangar kapı bloğu + giriş/merdiven lazerleri + stage + 4 pil başlatma (client'ta host'a göre eşitle)
    if (p.from_host && !is_host()) {
        if (!p_hull) p_hull = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "c_updatehull");
        if (p.has_kapi) {
            ENTITY *ke = 0, kc;
            __try { ke = G_DEVE_KAPI(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { ke = 0; }
            if (ke && safe_ent(ke, &kc)) {
                long want = (kc.flags & ~(INVISIBLE_F | PASSABLE_F)) | (p.kapi_flags & (INVISIBLE_F | PASSABLE_F));
                if (want != kc.flags) {
                    __try { ke->flags = want; if (p_hull) p_hull(ke, 0); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    logf("[boss] rx hangar kapi flags=%08lx -> %08lx\n", kc.flags, want);
                }
            }
        }
        if (p.has_lsr1) {
            ENTITY *le = 0, lc;
            __try { le = G_HANGAR_LSR1(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { le = 0; }
            if (le && safe_ent(le, &lc)) {
                long want = (lc.flags & ~(INVISIBLE_F | PASSABLE_F)) | (p.lsr1_flags & (INVISIBLE_F | PASSABLE_F));
                if (want != lc.flags) {
                    __try { le->flags = want; if (p_hull) p_hull(le, 0); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    logf("[boss] rx hangar lsr1 flags=%08lx -> %08lx\n", lc.flags, want);
                }
            }
        }
        if (p.has_lsr2) {
            ENTITY *le = 0, lc;
            __try { le = G_HANGAR_LSR2(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { le = 0; }
            if (le && safe_ent(le, &lc)) {
                long want = (lc.flags & ~(INVISIBLE_F | PASSABLE_F)) | (p.lsr2_flags & (INVISIBLE_F | PASSABLE_F));
                if (want != lc.flags) {
                    __try { le->flags = want; if (p_hull) p_hull(le, 0); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    logf("[boss] rx hangar lsr2 flags=%08lx -> %08lx\n", lc.flags, want);
                }
            }
        }
        if (p.has_alt_blok) {
            static var (__cdecl *pX_settype)(ENTITY *, var, var);
            ENTITY *ae = 0, ac;
            __try { ae = G_ALT_BLOK(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { ae = 0; }
            if (ae && safe_ent(ae, &ac)) {
                long want = (ac.flags & ~(INVISIBLE_F | PASSABLE_F)) | (p.alt_blok_flags & (INVISIBLE_F | PASSABLE_F));
                if (want != ac.flags) {
                    if (!pX_settype) pX_settype = (void *)GetProcAddress(GetModuleHandleA("ackphysX.dll"), "pXent_settype");
                    __try {
                        ae->flags = want;
                        if (p_hull) p_hull(ae, 0);
                        if ((want & PASSABLE_F) && pX_settype) pX_settype(ae, 0, 0);
                    } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    logf("[boss] rx hangar alt_blok flags=%08lx -> %08lx\n", ac.flags, want);
                }
            }
        }
        if (p.has_alt_lsr) {
            ENTITY *ae = 0, ac;
            __try { ae = G_ALT_LSR(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { ae = 0; }
            if (ae && safe_ent(ae, &ac)) {
                long want = (ac.flags & ~(INVISIBLE_F | PASSABLE_F)) | (p.alt_lsr_flags & (INVISIBLE_F | PASSABLE_F));
                if (want != ac.flags) {
                    __try { ae->flags = want; if (p_hull) p_hull(ae, 0); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    logf("[boss] rx hangar alt_lsr flags=%08lx -> %08lx\n", ac.flags, want);
                }
            }
        }
        // Giriş lazeri hasar bayrağı (0=hasarlı, 1=lazer kapalı, 2=boss aktif, 3=boss öldü)
        if (p.hangar_stage > 0) {
            long cur_st = 0;
            __try { cur_st = G_HANGAR_STAGE(ebx) >> 10; } __except (EXCEPTION_EXECUTE_HANDLER) { cur_st = p.hangar_stage; }
            if (p.hangar_stage > cur_st) {
                __try { G_HANGAR_STAGE(ebx) = (var)((long)p.hangar_stage << 10); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                logf("[boss] rx hangar_stage %ld -> %d\n", cur_st, (int)p.hangar_stage);
                if (p.hangar_stage >= 3 && !g_boss_panel_closed) {
                    g_boss_panel_closed = 1;
                    boss_call_script("EP_deve_panel_kapa", 0, 0);
                    logf("[boss] boss defeated (stage=3) -> closed D.E.V.E. panel on client\n");
                }
            }
        }
        // Host'ta 4 pil başladıysa (pil_hp[0] > 0 && stage >= 2) ama client'ta henüz başlamadıysa:
        // AIF_Hangar_Load (sinematik + 2. deve riski!) yerine YALNIZCA 4 pil için nesne_pil_loop başlat!
        if (!g_boss_pils_triggered && p.pil_hp[0] > 0 && p.hangar_stage >= 2) {
            var local_p0 = 0;
            ENTITY *pe0 = 0, pc0;
            __try { pe0 = G_PIL_BOX(ebx, 0); } __except (EXCEPTION_EXECUTE_HANDLER) { pe0 = 0; }
            if (pe0 && safe_ent(pe0, &pc0)) local_p0 = sk_get(pe0, 36);
            if (local_p0 <= 0 && !boss_proc_active("nesne_pil_loop")) {
                g_boss_pils_triggered = 1;
                __try {
                    ENTITY *de = G_DEVE_BOX(ebx);
                    var *lp = G_LIGHT_POS(ebx);
                    G_SUB_LVL(ebx) = 9 << 10;
                    G_DEVE_HP(ebx) = 500 << 10;
                    G_PIL_DEAD(ebx) = 0;
                    G_HANGAR_STAGE(ebx) = 2 << 10;
                    // AIF_Hangar_Start (decomp.c:184959): Hangar karakter dinamik ışık konumu ve menzili
                    lp[0] = -506 << 10; lp[1] = -1840 << 10; lp[2] = 1019 << 10;
                    G_LIGHT_RNG(ebx) = 300 << 10;
                    if (de) *(DWORD *)((BYTE *)de + 96) |= 256u;
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
                for (i = 0; i < 4; i++) {
                    ENTITY *pbx = 0, *pcm = 0;
                    __try { pbx = G_PIL_BOX(ebx, i); pcm = G_PIL_CAM(ebx, i); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    if (pbx && pcm) boss_call_script("nesne_pil_loop", (long)pbx, (long)pcm);
                }
                logf("[boss] host started 4 pils (p0=%.0f) -> started 4x nesne_pil_loop on client (no duplicate Deve AI)\n", VF(p.pil_hp[0]));
            } else if (local_p0 > 0) {
                g_boss_pils_triggered = 1;
            }
        }
        // 4 pil kırılınca client'ta D.E.V.E. can barını aç (AIF_DEVE_sevin client'ta engelli olduğu için biz açıyoruz)
        if (!g_boss_panel_opened && p.pil_dead >= (4 << 10)) {
            g_boss_panel_opened = 1;
            __try { G_PIL_DEAD(ebx) = 4 << 10; } __except (EXCEPTION_EXECUTE_HANDLER) {}
            boss_call_script("EP_deve_panel_ac", 0, 0);
            logf("[boss] all 4 pils dead -> opened D.E.V.E. boss panel on client\n");
        }
        boss_client_apply_mirror(ebx);
    }
}

static void boss_tick(const char *level, int in_level, int peer_active) {
    DWORD ebx = ebx_base(); int i, chg = 0, have_any = 0, has_deve = 0, has_kapi = 0, has_govde = 0, has_lsr1 = 0, has_lsr2 = 0, has_alt_blok = 0, has_alt_lsr = 0;
    int has_isaret = 0, has_lsr_sap1 = 0, has_lsr_sap2 = 0;
    int local_combo = 0, local_laser = 0;
    var cur_deve = -1, cur_pil[4] = {-1, -1, -1, -1}, cur_dead = 0;
    var dx = 0, dy = 0, dz = 0, dpan = 0, gx = 0, gy = 0, gz = 0;
    var ix = 0, iy = 0, ipan = 0, sap1_y = 0, sap2_y = 0;
    long kflags = 0, gflags = 0, l1flags = 0, l2flags = 0, abflags = 0, alflags = 0, iflags = 0, i1flags = 0;
    long sub_lvl = 0, hstage = 0, phase2 = 0;
    double t = now_s();
    if (strcmp(g_boss_level, level)) {
        strncpy(g_boss_level, level, sizeof g_boss_level - 1); g_boss_level[sizeof g_boss_level - 1] = 0;
        g_boss_last_deve = -1; g_boss_last_kapi = -1; g_boss_last_govde = -1; g_boss_last_lsr1 = -1; g_boss_last_lsr2 = -1;
        g_boss_last_alt_blok = -1; g_boss_last_alt_lsr = -1; g_boss_last_stage = -1; g_boss_last_phase2 = -1;
        for (i = 0; i < 4; i++) g_boss_last_pil[i] = -1;
        g_boss_tx_left = 0; g_boss_seq = g_boss_rx_seq = 0; g_boss_logged = 0; g_boss_next_pose_tx = 0;
        g_boss_laser_seq = g_boss_rx_laser_seq = 0;
        g_boss_last_combo = g_boss_last_laser = g_boss_peer_combo = 0;
        g_boss_host_block_zgeri = g_boss_host_deferred_zgeri = 0;
        g_boss_pils_triggered = g_boss_panel_opened = g_boss_panel_closed = g_boss_have_rx = g_boss_have_prev = g_boss_qte_armed = g_boss_end_started = 0;
        g_boss_prev_t = g_boss_rx_t = 0;
        g_boss_govde_ent = 0; g_boss_dummy_ent = 0; g_boss_anim[0] = 0; g_boss_add_anim[0] = 0;
    }
    g_boss_client_block = (in_level && peer_active && !is_host() && !_stricmp(level, "tunel.wmb")) ? 1 : 0;
    if (!in_level || !peer_active || _stricmp(level, "tunel.wmb")) g_boss_host_block_zgeri = g_boss_host_deferred_zgeri = 0;
    if (!in_level || !peer_active || !ebx || _stricmp(level, "tunel.wmb")) return;
    __try { sub_lvl = G_SUB_LVL(ebx) >> 10; hstage = G_HANGAR_STAGE(ebx) >> 10; phase2 = G_DEVE_PHASE2(ebx) >> 10; } __except (EXCEPTION_EXECUTE_HANDLER) { sub_lvl = 0; hstage = 0; phase2 = 0; }
    if ((is_host() || hstage >= 3) && hstage >= 0 && hstage <= 4) {
        if (g_boss_last_stage < 0) g_boss_last_stage = hstage;
        else if (hstage != g_boss_last_stage) { g_boss_last_stage = hstage; chg = 1; }
    }
    if (phase2 >= 0 && phase2 <= 2) {
        if (g_boss_last_phase2 < 0) g_boss_last_phase2 = phase2;
        else if (phase2 > g_boss_last_phase2) { g_boss_last_phase2 = phase2; chg = 1; }
    }
    for (i = 0; i < 4; i++) {
        ENTITY *pe = 0, pc;
        __try { pe = G_PIL_BOX(ebx, i); } __except (EXCEPTION_EXECUTE_HANDLER) { pe = 0; }
        if (pe && safe_ent(pe, &pc)) {
            have_any = 1;
            cur_pil[i] = sk_get(pe, 36);
            if (g_boss_last_pil[i] < 0) g_boss_last_pil[i] = cur_pil[i];
            else if (cur_pil[i] != g_boss_last_pil[i]) {
                // Hem 0 -> 160 başlangıcını hem her hasar düşüşünü anında yakala!
                if (cur_pil[i] < g_boss_last_pil[i] || (g_boss_last_pil[i] <= 0 && cur_pil[i] > 0)) chg = 1;
                g_boss_last_pil[i] = cur_pil[i];
            }
        }
    }
    {   ENTITY *de = 0, dc;
        __try { de = G_DEVE_BOX(ebx); cur_dead = G_PIL_DEAD(ebx); cur_deve = G_DEVE_HP(ebx); dpan = G_DEVE_PAN(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { de = 0; }
        if (de && safe_ent(de, &dc)) {
            have_any = 1; has_deve = 1;
            dx = dc.x; dy = dc.y; dz = dc.z;
            if (cur_deve > 0) {
                if (g_boss_last_deve <= 0) g_boss_last_deve = cur_deve;
                else if (cur_deve != g_boss_last_deve) {
                    if (cur_deve < g_boss_last_deve || cur_deve > g_boss_last_deve + (50 << 10)) chg = 1;
                    g_boss_last_deve = cur_deve;
                }
            }
            if (g_boss_peer_combo) {
                __try {
                    *(DWORD *)((BYTE *)de + 96) &= ~256u;
                    ((var *)((BYTE *)de + 148))[34] = ((var *)((BYTE *)de + 148))[35];
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
    }
    {   ENTITY *ke = 0, kc;
        __try { ke = G_DEVE_KAPI(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { ke = 0; }
        if (ke && safe_ent(ke, &kc)) {
            has_kapi = 1; kflags = kc.flags & (INVISIBLE_F | PASSABLE_F);
            if (g_boss_last_kapi < 0) g_boss_last_kapi = kflags;
            else if (kflags != g_boss_last_kapi && is_host()) { g_boss_last_kapi = kflags; chg = 1; }
        }
    }
    {   ENTITY *ge = 0, *l1 = 0, *l2 = 0, *ab = 0, *al = 0, *ie = 0, *s1 = 0, *s2 = 0, *i1 = 0, ec;
        __try {
            ge = G_DEVE_GOVDE(ebx); l1 = G_HANGAR_LSR1(ebx); l2 = G_HANGAR_LSR2(ebx);
            ab = G_ALT_BLOK(ebx); al = G_ALT_LSR(ebx); ie = G_DEVE_ISARET(ebx);
            s1 = G_LSR_SAP1(ebx); s2 = G_LSR_SAP2(ebx); i1 = G_LSR_ISIN1(ebx);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        if (ge && safe_ent(ge, &ec)) {
            if (is_host() || g_boss_last_combo) g_boss_govde_ent = ge;
            has_govde = 1; gflags = ec.flags & INVISIBLE_F;
            gx = ec.x; gy = ec.y; gz = ec.z;
            if (g_boss_last_govde < 0) g_boss_last_govde = gflags;
            else if (gflags != g_boss_last_govde && is_host()) { g_boss_last_govde = gflags; chg = 1; }
        }
        if (l1 && safe_ent(l1, &ec)) {
            has_lsr1 = 1; l1flags = ec.flags & (INVISIBLE_F | PASSABLE_F);
            if (g_boss_last_lsr1 < 0) g_boss_last_lsr1 = l1flags;
            else if (l1flags != g_boss_last_lsr1 && is_host()) { g_boss_last_lsr1 = l1flags; chg = 1; }
        }
        if (l2 && safe_ent(l2, &ec)) {
            has_lsr2 = 1; l2flags = ec.flags & (INVISIBLE_F | PASSABLE_F);
            if (g_boss_last_lsr2 < 0) g_boss_last_lsr2 = l2flags;
            else if (l2flags != g_boss_last_lsr2 && is_host()) { g_boss_last_lsr2 = l2flags; chg = 1; }
        }
        if (ab && safe_ent(ab, &ec)) {
            has_alt_blok = 1; abflags = ec.flags & (INVISIBLE_F | PASSABLE_F);
            if (g_boss_last_alt_blok < 0) g_boss_last_alt_blok = abflags;
            else if (abflags != g_boss_last_alt_blok && is_host()) { g_boss_last_alt_blok = abflags; chg = 1; }
        }
        if (al && safe_ent(al, &ec)) {
            has_alt_lsr = 1; alflags = ec.flags & (INVISIBLE_F | PASSABLE_F);
            if (g_boss_last_alt_lsr < 0) g_boss_last_alt_lsr = alflags;
            else if (alflags != g_boss_last_alt_lsr && is_host()) { g_boss_last_alt_lsr = alflags; chg = 1; }
        }
        if (ie && safe_ent(ie, &ec)) {
            has_isaret = 1; iflags = ec.flags & INVISIBLE_F;
            ix = ec.x; iy = ec.y; ipan = ec.pan;
        }
        if (s1 && safe_ent(s1, &ec)) {
            has_lsr_sap1 = 1; sap1_y = ec.y;
            if (i1 && safe_ent(i1, &ec)) i1flags = ec.flags & INVISIBLE_F;
        }
        if (s2 && safe_ent(s2, &ec)) {
            has_lsr_sap2 = 1; sap2_y = ec.y;
        }
    }
    if (have_any) {
        local_combo = boss_proc_active("AIF_DEVE_combo_kap");
        if (local_combo != g_boss_last_combo) { g_boss_last_combo = local_combo; chg = 1; }
        if (is_host()) {
            local_laser = boss_proc_active("DEVE_laser_atak");
            if (local_laser && !g_boss_last_laser) { g_boss_laser_seq++; chg = 1; }
            g_boss_last_laser = local_laser;
            // Client QTE yaparken host'ta Deve yerinden kaçmasın ve client'ın gönderdiği sallanma ("galgima") animasyonunu oynatsın!
            if (g_boss_peer_combo && g_boss_have_rx && !g_boss_rx_pkt.from_host && g_boss_rx_pkt.combo_busy) {
                ENTITY *ge = 0, gc;
                __try { ge = G_DEVE_GOVDE(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { ge = 0; }
                if (ge && safe_ent(ge, &gc) && g_boss_rx_pkt.deve_anim[0] && t_ent_animate) {
                    __try {
                        g_in_anim++;
                        t_ent_animate(ge, (char *)g_boss_rx_pkt.deve_anim, g_boss_rx_pkt.deve_anim_pct, g_boss_rx_pkt.deve_anim_mode & ~(2 << 10));
                        g_in_anim--;
                    } __except (EXCEPTION_EXECUTE_HANDLER) {}
                }
            }
        } else {
            // Client: bağımsız lazer saldırısı tetiklenmesin (yalnız host p.laser_seq ile başlatır)
            __try { G_LSR_TIMER(ebx) = 0; } __except (EXCEPTION_EXECUTE_HANDLER) {}
            // Client son vuruşu kendisi yaptıysa da çıkış kapısı (y > 650) son ara sahnesi için nesne_lsr_ustengel'i başlat!
            if (hstage >= 3 && !g_boss_end_started && !boss_proc_active("nesne_lsr_ustengel")) {
                g_boss_end_started = 1;
                boss_call_script("nesne_lsr_ustengel", 0, 0);
                logf("[boss] local stage=3 on client -> started nesne_lsr_ustengel for ending cutscene (y > 650)\n");
            }
            // Client: her kare host'un Deve konumu + açısı + animasyonunu pürüzsüz uygula!
            boss_client_apply_mirror(ebx);
            // Client Faz 2 QTE: host'ta G_DEVE_ISARET görünürken ve host QTE'de değilken client yaklaşınca S simgesini göster ve QTE'ye izin ver!
            {   int show_s = 0;
                if (g_boss_have_rx && g_boss_rx_pkt.has_isaret && (g_boss_rx_pkt.isaret_flags & INVISIBLE_F) == 0 && !g_boss_peer_combo && !local_combo) {
                    ENTITY *pb = player_box(), *de = 0, pc, dc;
                    __try { de = G_DEVE_BOX(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { de = 0; }
                    if (pb && de && safe_ent(pb, &pc) && safe_ent(de, &dc)) {
                        double ddx = VF(pc.x) - VF(dc.x), ddy = VF(pc.y) - VF(dc.y);
                        double d2 = ddx * ddx + ddy * ddy;
                        HMODULE ack = GetModuleHandleA("acknex.dll");
                        var key_qte = 0;
                        if (ack) __try { key_qte = *(var *)((BYTE *)ack + 0x225864); } __except (EXCEPTION_EXECUTE_HANDLER) { key_qte = 0; }
                        if (d2 > 60.0 * 60.0 && d2 < 185.0 * 185.0) {
                            show_s = 1;
                            if (key_qte == 0) g_boss_qte_armed = 1;
                            else if (g_boss_qte_armed) {
                                g_boss_qte_armed = 0; show_s = 0;
                                boss_call_script("AIF_DEVE_combo_kap", 0, 0);
                                logf("[boss] client entered QTE combo (AIF_DEVE_combo_kap)!\n");
                            }
                        } else {
                            g_boss_qte_armed = 0;
                        }
                    }
                } else {
                    g_boss_qte_armed = 0;
                }
                // Oyunun kendi S tuş simgesi paneli (0x7d5062d = ebx + 0x0105e8, PANEL.flags bit 14 = 16384 VISIBLE)
                __try {
                    DWORD *pf = (DWORD *)((BYTE *)ebx + 0x0105e8 + 28);
                    if (show_s) *pf |= 16384u;
                    else        *pf &= ~16384u;
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
    }
    if (have_any && !g_boss_logged) {
        g_boss_logged = 1;
        logf("[boss] hangar/deve active on '%s': deve_hp=%.1f pils=(%.1f,%.1f,%.1f,%.1f) dead=%.0f kapi=%08lx\n",
             level, VF(cur_deve), VF(cur_pil[0]), VF(cur_pil[1]), VF(cur_pil[2]), VF(cur_pil[3]), VF(cur_dead), kflags);
    }
    // Host 30 Hz Deve durumu + animasyonu gönderir; client QTE yaparken 30 Hz animasyon, normalde yalnız değişiklik gönderir
    if (chg || ((is_host() || local_combo) && has_deve && t >= g_boss_next_pose_tx)) {
        memset(&g_boss_pkt, 0, sizeof g_boss_pkt);
        g_boss_pkt.magic = BOSS_MAGIC;
        g_boss_pkt.seq = ++g_boss_seq;
        g_boss_pkt.deve_hp = cur_deve;
        for (i = 0; i < 4; i++) g_boss_pkt.pil_hp[i] = cur_pil[i];
        g_boss_pkt.pil_dead = cur_dead;
        g_boss_pkt.deve_x = dx; g_boss_pkt.deve_y = dy; g_boss_pkt.deve_z = dz; g_boss_pkt.deve_pan = dpan;
        g_boss_pkt.govde_x = gx; g_boss_pkt.govde_y = gy; g_boss_pkt.govde_z = gz;
        g_boss_pkt.deve_anim_pct = g_boss_anim_pct; g_boss_pkt.deve_anim_mode = g_boss_anim_mode;
        g_boss_pkt.deve_add_pct = g_boss_add_pct; g_boss_pkt.deve_add_mode = g_boss_add_mode;
        strncpy(g_boss_pkt.deve_anim, g_boss_anim, sizeof g_boss_pkt.deve_anim - 1);
        strncpy(g_boss_pkt.deve_add_anim, g_boss_add_anim, sizeof g_boss_pkt.deve_add_anim - 1);
        g_boss_pkt.isaret_x = ix; g_boss_pkt.isaret_y = iy; g_boss_pkt.isaret_pan = ipan; g_boss_pkt.isaret_flags = iflags;
        g_boss_pkt.lsr_sap1_y = sap1_y; g_boss_pkt.lsr_sap2_y = sap2_y; g_boss_pkt.lsr_isin1_flags = i1flags;
        g_boss_pkt.kapi_flags = kflags; g_boss_pkt.govde_flags = gflags;
        g_boss_pkt.lsr1_flags = l1flags; g_boss_pkt.lsr2_flags = l2flags;
        g_boss_pkt.alt_blok_flags = abflags; g_boss_pkt.alt_lsr_flags = alflags;
        g_boss_pkt.has_deve = (BYTE)has_deve; g_boss_pkt.has_kapi = (BYTE)has_kapi;
        g_boss_pkt.from_host = (BYTE)(is_host() != 0); g_boss_pkt.combo_busy = (BYTE)local_combo;
        g_boss_pkt.has_govde = (BYTE)has_govde; g_boss_pkt.has_lsr1 = (BYTE)has_lsr1;
        g_boss_pkt.has_lsr2 = (BYTE)has_lsr2; g_boss_pkt.sub_lvl = (BYTE)(sub_lvl & 0xFF);
        g_boss_pkt.has_alt_blok = (BYTE)has_alt_blok; g_boss_pkt.has_alt_lsr = (BYTE)has_alt_lsr;
        g_boss_pkt.hangar_stage = (BYTE)(hstage > 0 && hstage <= 4 ? hstage : 0);
        g_boss_pkt.deve_phase2 = (BYTE)(phase2 > 0 && phase2 <= 2 ? phase2 : 0);
        g_boss_pkt.has_isaret = (BYTE)has_isaret; g_boss_pkt.has_lsr_sap1 = (BYTE)has_lsr_sap1; g_boss_pkt.has_lsr_sap2 = (BYTE)has_lsr_sap2;
        g_boss_pkt.laser_seq = g_boss_laser_seq;
        if (chg) {
            g_boss_tx_left = 3;
            logf("[boss] state change -> sending deve=%.1f p2=%ld pils=(%.1f,%.1f,%.1f,%.1f) dead=%.0f st=%ld combo=%d lsr=%lu\n",
                 VF(cur_deve), phase2, VF(cur_pil[0]), VF(cur_pil[1]), VF(cur_pil[2]), VF(cur_pil[3]), VF(cur_dead), hstage, local_combo, g_boss_laser_seq);
        } else {
            net_send_peer((BYTE *)&g_boss_pkt, sizeof(BOSSPKT));
        }
        g_boss_next_pose_tx = t + 0.033;
    }
    if (g_boss_tx_left > 0) {
        net_send_peer((BYTE *)&g_boss_pkt, sizeof(BOSSPKT));
        g_boss_tx_left--;
    }
}

static UINT g_bb_w, g_bb_h;
static HWND g_hwnd;
