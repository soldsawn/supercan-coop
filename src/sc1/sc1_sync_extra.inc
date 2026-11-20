// [SC1] Esya (SIT3), puan/elmas (SST2), snapshot (SSN1), ses/muzik/ara sahne (SND1/SMU1/SCT1), world_tick

// Çevre çöpleri (cop_model_fizik, s23=1) + Elmaslar (bonus_model_init, s23=3) senkronu ('SIT3')
// Oyunun kendi kayıtlı eşya tablosu G_ITM_TBL(ebx, idx) (g_07faa41d[0..199], idx = entity.skill[5]>>10) kullanılır:
// 1->0 düşüşü %100 gerçek toplanmadır (LOD/INVISIBLE/koordinat kayması sıfır; meyveler s23!=1,3 olduğu için bağımsız kalır).
#define ITEM_MAGIC 0x33544953 /* 'SIT3' */
#define MAX_SYNC_ITEMS 200
#define ITM_Q_MAX 32
typedef struct { DWORD magic, seq; short idx, kind; } ITEMPKT;
static ENTITY *g_itm_ent[MAX_SYNC_ITEMS];
static int g_itm_kind[MAX_SYNC_ITEMS], g_itm_prev[MAX_SYNC_ITEMS];
static long g_itm_seen[MAX_SYNC_ITEMS];
static int g_itm_logged;
static char g_itm_level[64];
static DWORD g_itm_tx_seq, g_itm_rx_seen[32]; static int g_itm_rx_head;
static ITEMPKT g_itm_q[ITM_Q_MAX]; static int g_itm_left[ITM_Q_MAX]; static DWORD g_itm_due[ITM_Q_MAX];

static void itm_collect_ent(ENTITY *e, const char *ty) {
    __try {
        var s23 = *(var *)((BYTE *)e + 148 + 22 * 4);
        int kind = 0; long idx;
        if (is_trash_type(ty)) return;
        if (s23 == (3 << 10)) kind = 3; // elmas/bonus (bonus_model_init)
        else if (s23 == (1 << 10) && *(var *)((BYTE *)e + 148 + 6 * 4) == (5 << 10) && *(var *)((BYTE *)e + 148 + 3 * 4) == (3 << 10)) kind = 1; // cop_model_fizik
        else return;
        idx = (*(var *)((BYTE *)e + 148 + 5 * 4)) >> 10;
        if (idx < 0 || idx >= MAX_SYNC_ITEMS) return;
        g_itm_ent[idx] = e;
        g_itm_kind[idx] = kind;
        g_itm_seen[idx] = g_frame;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static ENTITY *itm_hide_ent(int idx, int kind) {
    static void *(__cdecl *p_ptr_for_handle)(var);
    ENTITY *e = 0;
    if (idx < 0 || idx >= MAX_SYNC_ITEMS) return 0;
    if (g_itm_ent[idx] && g_itm_seen[idx] == g_frame) e = g_itm_ent[idx];
    if (!e) return 0;
    if (!p_ptr_for_handle) p_ptr_for_handle = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "ptr_for_handle");
    __try {
        e->flags = (e->flags & ~TRASH_FLAG2_F) | INVISIBLE_F | PASSABLE_F;
        *(DWORD *)((BYTE *)e + 96) &= ~257u;
        if (kind == 3 || g_itm_kind[idx] == 3) {
            *(var *)((BYTE *)e + 148 + 8 * 4) = 0;   // pfcan_bonus_parcalan:105045 elmas aktif bayrağı (LOD geri görünür yapmasın)
        }
        if (kind == 1 || g_itm_kind[idx] == 1) {
            var h_icon = *(var *)((BYTE *)e + 148 + 31 * 4);
            *(var *)((BYTE *)e + 148 + 25 * 4) = 2 << 10;
            if (h_icon && p_ptr_for_handle) {
                ENTITY *ic = (ENTITY *)p_ptr_for_handle(h_icon);
                if (ic && is_entity((DWORD)ic)) ic->flags |= INVISIBLE_F | PASSABLE_F;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return e;
}

static void itm_rx(const BYTE *buf, int len) {
    ITEMPKT p; int i; DWORD ebx = ebx_base(); ENTITY *e = 0;
    if (len < (int)sizeof(ITEMPKT) || !ebx) return;
    memcpy(&p, buf, sizeof p);
    if (p.magic != ITEM_MAGIC || p.idx < 0 || p.idx >= MAX_SYNC_ITEMS) return;
    for (i = 0; i < 32; i++) if (g_itm_rx_seen[i] == p.seq) return;
    g_itm_rx_seen[g_itm_rx_head++ & 31] = p.seq;
    __try { G_ITM_TBL(ebx, p.idx) = 0; } __except (EXCEPTION_EXECUTE_HANDLER) {}
    g_itm_prev[p.idx] = 0;
    e = itm_hide_ent(p.idx, p.kind);
    logf("[itm] rx hid idx=%d kind=%d ent=%p\n", (int)p.idx, (int)p.kind, e);
}

static void itm_tick(const char *level, int in_level, int peer_active) {
    DWORD ebx = ebx_base(); int i; long cnt = 0;
    if (strcmp(g_itm_level, level)) {
        strncpy(g_itm_level, level, sizeof g_itm_level - 1); g_itm_level[sizeof g_itm_level - 1] = 0;
        memset(g_itm_ent, 0, sizeof g_itm_ent);
        memset(g_itm_kind, 0, sizeof g_itm_kind);
        memset(g_itm_seen, 0, sizeof g_itm_seen);
        for (i = 0; i < MAX_SYNC_ITEMS; i++) g_itm_prev[i] = -1;
        g_itm_logged = 0;
        memset(g_itm_left, 0, sizeof g_itm_left); memset(g_itm_rx_seen, 0, sizeof g_itm_rx_seen);
    }
    if (!in_level || !peer_active || !ebx) return;
    __try { cnt = G_ITM_CNT(ebx) >> 10; } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (cnt <= 0) return;
    if (cnt > MAX_SYNC_ITEMS) cnt = MAX_SYNC_ITEMS;
    if (!g_itm_logged) {
        int act = 0, map_n = 0;
        for (i = 0; i < (int)cnt; i++) {
            var v = 0; __try { v = G_ITM_TBL(ebx, i); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            if (v) act++;
            if (g_itm_kind[i]) map_n++;
        }
        if (map_n > 0) {
            g_itm_logged = 1;
            logf("[itm] G_ITM_TBL tracking %ld slots on '%s' (active=%d mapped=%d)\n", cnt, level, act, map_n);
        }
    }
    for (i = 0; i < (int)cnt; i++) {
        int cur = 0;
        __try { cur = (G_ITM_TBL(ebx, i) != 0) ? 1 : 0; } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
        if (g_itm_prev[i] < 0) { g_itm_prev[i] = cur; continue; }
        if (g_itm_prev[i] == 1 && cur == 0) {
            g_itm_prev[i] = 0;
            if (g_itm_kind[i] == 1 || g_itm_kind[i] == 3) {
                int slot;
                logf("[itm] local taken idx=%d kind=%d\n", i, g_itm_kind[i]);
                for (slot = 0; slot < ITM_Q_MAX && g_itm_left[slot]; slot++) {}
                if (slot < ITM_Q_MAX) {
                    DWORD s = (++g_itm_tx_seq ^ (GetTickCount() << 8)) & 0x7FFFFFFFu;
                    if (is_host()) s |= 0x80000000u;
                    g_itm_q[slot].magic = ITEM_MAGIC;
                    g_itm_q[slot].seq = s;
                    g_itm_q[slot].idx = (short)i;
                    g_itm_q[slot].kind = (short)g_itm_kind[i];
                    g_itm_left[slot] = 3;
                    g_itm_due[slot] = GetTickCount();
                }
            }
        } else if (g_itm_prev[i] == 0 && cur == 1) {
            g_itm_prev[i] = 1;
        } else if (g_itm_prev[i] == 0 && cur == 0 && g_itm_ent[i] && g_itm_seen[i] == g_frame && (g_itm_kind[i] == 1 || g_itm_kind[i] == 3)) {
            // Toplanmış çöp/elması oyunun LOD/mesafe yöneticisi tekrar görünür yapmaya kalkarsa bastır (çevre çöpü ikonu dahil)
            int vis = 0;
            __try { vis = (g_itm_ent[i]->flags & INVISIBLE_F) == 0; } __except (EXCEPTION_EXECUTE_HANDLER) {}
            if (vis) itm_hide_ent(i, g_itm_kind[i]);
        }
    }
    {   DWORD now = GetTickCount();
        for (i = 0; i < ITM_Q_MAX; i++) if (g_itm_left[i] && (long)(now - g_itm_due[i]) >= 0) {
            net_send_peer((BYTE *)&g_itm_q[i], sizeof(ITEMPKT));
            g_itm_left[i]--;
            g_itm_due[i] = now + (g_itm_left[i] == 2 ? 50 : 70);
        }
    }
}

// Ortak Puan, Elmas, Ceza & Çöp Sayaçları Senkronu ('SST2') — G_NANO her oyuncuda ayrı kalır!
// Client kümülatif kazancını (client_tot_*) gönderir; host son işlediği toplamla farkı ekler ve onayını geri yansıtır (çift sayma ve kayıp delta olmaz).
#define STAT_MAGIC 0x32545353 /* 'SST2' */
typedef struct {
    DWORD magic, seq;
    var score, diamonds, missed_trash, trash_rbt, trash_env, trash_lvl;
    var client_tot_score, client_tot_diamonds;
} STATPKT;
static DWORD g_stat_tx_seq, g_stat_rx_seq, g_stat_cseq;
static int g_stat_init, g_stat_dirty;
static var g_stat_last_s, g_stat_last_d;
static var g_stat_tot_s, g_stat_tot_d, g_stat_acked_s, g_stat_acked_d;         // client tarafı
static var g_stat_host_seen_s, g_stat_host_seen_d;                             // host tarafı
static STATPKT g_stat_host_last;
static double g_stat_next_tx, g_stat_cnext_tx;
static char g_stat_level[64];

static void stat_client_capture(DWORD ebx) {
    var cs = G_SCORE(ebx), cd = G_DIAMONDS(ebx);
    if (!g_stat_init) { g_stat_last_s = cs; g_stat_last_d = cd; g_stat_init = 1; return; }
    if (cs > g_stat_last_s) { g_stat_tot_s += cs - g_stat_last_s; g_stat_dirty = 1; }
    if (cd > g_stat_last_d) { g_stat_tot_d += cd - g_stat_last_d; g_stat_dirty = 1; }
    g_stat_last_s = cs; g_stat_last_d = cd;
}

static void stat_rx(const BYTE *buf, int len) {
    STATPKT p; DWORD ebx = ebx_base();
    if (len < (int)sizeof(STATPKT) || !ebx) return;
    memcpy(&p, buf, sizeof p);
    if (p.magic != STAT_MAGIC) return;
    if (is_host()) {
        var ds = 0, dd = 0;
        if (p.client_tot_score > g_stat_host_seen_s) ds = p.client_tot_score - g_stat_host_seen_s;
        if (p.client_tot_diamonds > g_stat_host_seen_d) dd = p.client_tot_diamonds - g_stat_host_seen_d;
        if (ds <= 0 && dd <= 0) return;
        __try {
            if (ds > 0) { G_SCORE(ebx) += ds; g_stat_host_seen_s = p.client_tot_score; }
            if (dd > 0) { G_DIAMONDS(ebx) += dd; g_stat_host_seen_d = p.client_tot_diamonds; }
        } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
        logf("[stat] host added client delta score=+%.0f diamonds=+%.0f (tot=%.0f/%.0f)\n",
             VF(ds), VF(dd), VF(g_stat_host_seen_s), VF(g_stat_host_seen_d));
    } else {
        if (p.seq && p.seq <= g_stat_rx_seq && g_stat_rx_seq - p.seq < 1000) return;
        g_stat_rx_seq = p.seq;
        __try {
            var pend_s, pend_d;
            stat_client_capture(ebx);
            if (p.client_tot_score > g_stat_acked_s) g_stat_acked_s = p.client_tot_score;
            if (p.client_tot_diamonds > g_stat_acked_d) g_stat_acked_d = p.client_tot_diamonds;
            pend_s = (g_stat_tot_s > g_stat_acked_s) ? (g_stat_tot_s - g_stat_acked_s) : 0;
            pend_d = (g_stat_tot_d > g_stat_acked_d) ? (g_stat_tot_d - g_stat_acked_d) : 0;
            G_SCORE(ebx) = p.score + pend_s;
            G_DIAMONDS(ebx) = p.diamonds + pend_d;
            G_MISSED_TRASH(ebx) = p.missed_trash;
            G_TRASH_RBT(ebx) = p.trash_rbt;
            G_TRASH_ENV(ebx) = p.trash_env;
            G_TRASH_LVL(ebx) = p.trash_lvl;
            g_stat_last_s = p.score + pend_s;
            g_stat_last_d = p.diamonds + pend_d;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}

static void stat_tick(const char *level, int in_level, int peer_active) {
    DWORD ebx = ebx_base();
    // Kümülatif sayaçları yalnızca harita değişiminde veya menüye çıkıldığında sıfırla; kısa lag'da (peer_active==0) sıfırlama!
    if (!in_level || strcmp(g_stat_level, level)) {
        strncpy(g_stat_level, level, sizeof g_stat_level - 1); g_stat_level[sizeof g_stat_level - 1] = 0;
        g_stat_init = 0; g_stat_dirty = 0;
        g_stat_tot_s = g_stat_tot_d = g_stat_acked_s = g_stat_acked_d = 0;
        g_stat_host_seen_s = g_stat_host_seen_d = 0;
        memset(&g_stat_host_last, 0, sizeof g_stat_host_last);
        if (!in_level) return;
    }
    if (!peer_active || !ebx) return;
    if (is_host()) {
        double t = now_s(); STATPKT p; int chg = 0;
        memset(&p, 0, sizeof p);
        __try {
            p.score = G_SCORE(ebx); p.diamonds = G_DIAMONDS(ebx);
            p.missed_trash = G_MISSED_TRASH(ebx); p.trash_rbt = G_TRASH_RBT(ebx);
            p.trash_env = G_TRASH_ENV(ebx); p.trash_lvl = G_TRASH_LVL(ebx);
        } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
        p.client_tot_score = g_stat_host_seen_s;
        p.client_tot_diamonds = g_stat_host_seen_d;
        chg = (p.score != g_stat_host_last.score || p.diamonds != g_stat_host_last.diamonds ||
               p.missed_trash != g_stat_host_last.missed_trash || p.trash_rbt != g_stat_host_last.trash_rbt ||
               p.trash_env != g_stat_host_last.trash_env || p.trash_lvl != g_stat_host_last.trash_lvl ||
               p.client_tot_score != g_stat_host_last.client_tot_score ||
               p.client_tot_diamonds != g_stat_host_last.client_tot_diamonds);
        if (chg || t >= g_stat_next_tx) {
            p.magic = STAT_MAGIC; p.seq = ++g_stat_tx_seq;
            net_send_peer((BYTE *)&p, sizeof p);
            g_stat_host_last = p;
            g_stat_next_tx = t + 0.35;
        }
    } else {
        double t = now_s();
        __try { stat_client_capture(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        if (g_stat_dirty || ((g_stat_tot_s > g_stat_acked_s || g_stat_tot_d > g_stat_acked_d) && t >= g_stat_cnext_tx)) {
            static int csend_logs;
            STATPKT p;
            memset(&p, 0, sizeof p);
            p.magic = STAT_MAGIC;
            p.seq = ++g_stat_cseq;
            p.client_tot_score = g_stat_tot_s;
            p.client_tot_diamonds = g_stat_tot_d;
            net_send_peer((BYTE *)&p, sizeof p);
            if (g_stat_dirty && csend_logs++ < 40) {
                logf("[stat] client send tot score=%.0f diamonds=%.0f (acked=%.0f/%.0f)\n",
                     VF(g_stat_tot_s), VF(g_stat_tot_d), VF(g_stat_acked_s), VF(g_stat_acked_d));
            }
            g_stat_dirty = 0;
            g_stat_cnext_tx = t + 0.25;
        }
    }
}

// Late-Join / Reconnect Snapshot ('SSN1'): yeni peer run_id veya >5 sn kopma sonrası host tüm dünya ilerlemesini tek pakette gönderir
#define SSN_MAGIC 0x314E5353 /* 'SSN1' */
#pragma pack(push, 1)
typedef struct {
    DWORD magic, snap_id, peer_run_id;
    long mkr_state, alrm_state;
    var score, diamonds, missed_trash, trash_rbt, trash_env, trash_lvl;
    short itm_cnt;
    BYTE has_deve, has_kapi;
    var deve_hp, pil_hp[4];
    long kapi_flags;
    BYTE itm_mask[25];   // G_ITM_TBL[0..199] için 200 bit (1=aktif, 0=toplanmış)
    char level[32];
} SSNPKT;
#pragma pack(pop)

static DWORD g_ssn_tx_id, g_ssn_rx_id;
static int g_ssn_tx_left;
static DWORD g_ssn_tx_due;
static SSNPKT g_ssn_pkt;
static DWORD g_snd_rx_seen[16], g_cut_rx_seen[16], g_mus_rx_seq;
static int g_snd_rx_head, g_cut_rx_head;

static void net_on_peer_run_id(DWORD new_rid, DWORD old_rid) {
    g_last_rx_seq = 0;
    g_nsnap = 0; g_snap_head = 0; g_clk_init = 0;
    g_last_wseq = 0; g_nwsnap = 0; g_wsnap_head = 0;
    g_boss_rx_seq = 0;
    g_stat_rx_seq = 0;
    g_mkr_rx_state = -1;
    g_alrm_rx_state = -1;
    memset(g_hit_seen, 0, sizeof g_hit_seen); g_hit_seen_n = 0;
    memset(g_phit_seen, 0, sizeof g_phit_seen); g_phit_seen_n = 0;
    memset(g_itm_rx_seen, 0, sizeof g_itm_rx_seen); g_itm_rx_head = 0;
    g_ssn_rx_id = 0;
    memset(g_snd_rx_seen, 0, sizeof g_snd_rx_seen); g_snd_rx_head = 0;
    memset(g_cut_rx_seen, 0, sizeof g_cut_rx_seen); g_cut_rx_head = 0;
    g_mus_rx_seq = 0;
    g_stat_host_seen_s = g_stat_host_seen_d = 0;
    g_stat_tot_s = g_stat_tot_d = g_stat_acked_s = g_stat_acked_d = 0;
    g_stat_init = 0; g_stat_dirty = 0;
    if (old_rid != 0) hud_toast("YENIDEN BAGLANDI", 3.0);
    if (is_host()) g_ssn_pending = 1;
    logf("[net] new peer run_id=%08lx (was %08lx) -> reset seqs%s\n", new_rid, old_rid, is_host() ? " + queue SSN1" : "");
}

static void ssn_rx(const BYTE *buf, int len) {
    static void (__cdecl *p_hull)(ENTITY *, var);
    SSNPKT p; DWORD ebx = ebx_base(); char cur_lvl[64]; int i, hid = 0;
    if (is_host() || len < (int)sizeof(SSNPKT) || !ebx) return;
    memcpy(&p, buf, sizeof p);
    p.level[sizeof p.level - 1] = 0;
    if (p.magic != SSN_MAGIC) return;
    if (p.peer_run_id && p.peer_run_id != g_run_id) return;
    if (p.snap_id && p.snap_id == g_ssn_rx_id) return;
    current_level(cur_lvl, sizeof cur_lvl);
    if (p.level[0] && _stricmp(cur_lvl, p.level)) return;   // client henüz aynı haritaya girmediyse bekle (sonraki tekrarda uygulanır)
    g_ssn_rx_id = p.snap_id;
    // 1) Makara / köprü / kapı
    if (p.mkr_state >= 0 && p.mkr_state <= 2 && g_mkr_ent[0]) {
        long cur = 0;
        __try { cur = G_MKR_STATE(ebx) >> 10; } __except (EXCEPTION_EXECUTE_HANDLER) { cur = p.mkr_state; }
        if (p.mkr_state > cur) { g_mkr_rx_state = p.mkr_state; mkr_apply_state(p.mkr_state); }
    }
    // 2) Alarm / lazer / silo
    if (p.alrm_state >= 0 && p.alrm_state <= 3 && alrm_has_silo(ebx)) {
        long cur = 0;
        __try { cur = G_ALRM_STATE(ebx) >> 10; } __except (EXCEPTION_EXECUTE_HANDLER) { cur = p.alrm_state; }
        if (p.alrm_state > cur) { g_alrm_rx_state = p.alrm_state; alrm_apply_state(p.alrm_state); }
    }
    // 3) Ortak puan, elmas ve çöp sayaçları
    __try {
        G_SCORE(ebx) = p.score;
        G_DIAMONDS(ebx) = p.diamonds;
        G_MISSED_TRASH(ebx) = p.missed_trash;
        G_TRASH_RBT(ebx) = p.trash_rbt;
        G_TRASH_ENV(ebx) = p.trash_env;
        G_TRASH_LVL(ebx) = p.trash_lvl;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    g_stat_last_s = p.score; g_stat_last_d = p.diamonds; g_stat_init = 1;
    g_stat_tot_s = g_stat_tot_d = g_stat_acked_s = g_stat_acked_d = 0; g_stat_dirty = 0;
    // 4) Toplanmış çevre çöpleri ve elmaslar (200 bit maske)
    {   int cnt = p.itm_cnt;
        if (cnt > MAX_SYNC_ITEMS) cnt = MAX_SYNC_ITEMS;
        for (i = 0; i < cnt; i++) {
            int host_alive = (p.itm_mask[i >> 3] >> (i & 7)) & 1;
            if (!host_alive) {
                __try { G_ITM_TBL(ebx, i) = 0; } __except (EXCEPTION_EXECUTE_HANDLER) {}
                g_itm_prev[i] = 0;
                if (g_itm_ent[i] && (g_itm_kind[i] == 1 || g_itm_kind[i] == 3)) itm_hide_ent(i, g_itm_kind[i]);
                hid++;
            }
        }
    }
    // 5) Deve Boss + 4 Pil + Hangar Kapısı
    for (i = 0; i < 4; i++) {
        ENTITY *pe = 0, pc;
        if (p.pil_hp[i] < 0) continue;
        __try { pe = G_PIL_BOX(ebx, i); } __except (EXCEPTION_EXECUTE_HANDLER) { pe = 0; }
        if (!pe || !safe_ent(pe, &pc)) continue;
        __try {
            var *sk = (var *)((BYTE *)pe + 148), cur = sk[35];
            if (cur > 0 && p.pil_hp[i] < cur) {
                sk[35] = p.pil_hp[i] <= 0 ? 0 : p.pil_hp[i];
                sk[34] = p.pil_hp[i] <= 0 ? -102 : p.pil_hp[i];
                g_boss_last_pil[i] = p.pil_hp[i];
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    if (p.has_deve && p.deve_hp >= 0) {
        ENTITY *de = 0, dc;
        __try { de = G_DEVE_BOX(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { de = 0; }
        if (de && safe_ent(de, &dc)) __try {
            var *sk = (var *)((BYTE *)de + 148), cur = sk[35];
            if (cur > 0 && p.deve_hp < cur) {
                sk[35] = p.deve_hp <= 0 ? 0 : p.deve_hp;
                sk[34] = p.deve_hp <= 0 ? -102 : p.deve_hp;
                g_boss_last_deve = p.deve_hp;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    if (p.has_kapi) {
        ENTITY *ke = 0, kc;
        __try { ke = G_DEVE_KAPI(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { ke = 0; }
        if (ke && safe_ent(ke, &kc)) {
            long want = (kc.flags & ~(INVISIBLE_F | PASSABLE_F)) | (p.kapi_flags & (INVISIBLE_F | PASSABLE_F));
            if (want != kc.flags) {
                if (!p_hull) p_hull = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "c_updatehull");
                __try { ke->flags = want; if (p_hull) p_hull(ke, 0); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
    }
    logf("[ssn] client applied SSN1 id=%lu lvl='%s' mkr=%ld alrm=%ld hid_itm=%d score=%.0f diam=%.0f\n",
         p.snap_id, p.level, p.mkr_state, p.alrm_state, hid, VF(p.score), VF(p.diamonds));
}

static void ssn_tick(const char *level, int in_level, int peer_active) {
    DWORD ebx = ebx_base(); int i;
    if (!is_host()) { g_ssn_pending = 0; g_ssn_tx_left = 0; return; }
    if (g_ssn_pending && in_level && peer_active && ebx && level[0] && _stricmp(level, "menu.wmb")) {
        long cnt = 0;
        memset(&g_ssn_pkt, 0, sizeof g_ssn_pkt);
        g_ssn_pkt.magic = SSN_MAGIC;
        g_ssn_pkt.snap_id = ++g_ssn_tx_id;
        g_ssn_pkt.peer_run_id = g_peer_run_id;
        g_ssn_pkt.mkr_state = -1;
        g_ssn_pkt.alrm_state = -1;
        g_ssn_pkt.deve_hp = -1;
        for (i = 0; i < 4; i++) g_ssn_pkt.pil_hp[i] = -1;
        strncpy(g_ssn_pkt.level, level, sizeof g_ssn_pkt.level - 1);
        __try {
            if (g_mkr_ent[0]) g_ssn_pkt.mkr_state = G_MKR_STATE(ebx) >> 10;
            if (alrm_has_silo(ebx)) g_ssn_pkt.alrm_state = G_ALRM_STATE(ebx) >> 10;
            g_ssn_pkt.score = G_SCORE(ebx);
            g_ssn_pkt.diamonds = G_DIAMONDS(ebx);
            g_ssn_pkt.missed_trash = G_MISSED_TRASH(ebx);
            g_ssn_pkt.trash_rbt = G_TRASH_RBT(ebx);
            g_ssn_pkt.trash_env = G_TRASH_ENV(ebx);
            g_ssn_pkt.trash_lvl = G_TRASH_LVL(ebx);
            cnt = G_ITM_CNT(ebx) >> 10;
            if (cnt < 0) cnt = 0;
            if (cnt > MAX_SYNC_ITEMS) cnt = MAX_SYNC_ITEMS;
            g_ssn_pkt.itm_cnt = (short)cnt;
            for (i = 0; i < (int)cnt; i++) {
                if (G_ITM_TBL(ebx, i) != 0) g_ssn_pkt.itm_mask[i >> 3] |= (BYTE)(1u << (i & 7));
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        for (i = 0; i < 4; i++) {
            ENTITY *pe = 0, pc;
            __try { pe = G_PIL_BOX(ebx, i); } __except (EXCEPTION_EXECUTE_HANDLER) { pe = 0; }
            if (pe && safe_ent(pe, &pc)) g_ssn_pkt.pil_hp[i] = sk_get(pe, 36);
        }
        {   ENTITY *de = 0, dc;
            __try { de = G_DEVE_BOX(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { de = 0; }
            if (de && safe_ent(de, &dc)) { g_ssn_pkt.has_deve = 1; g_ssn_pkt.deve_hp = sk_get(de, 36); }
        }
        {   ENTITY *ke = 0, kc;
            __try { ke = G_DEVE_KAPI(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { ke = 0; }
            if (ke && safe_ent(ke, &kc)) { g_ssn_pkt.has_kapi = 1; g_ssn_pkt.kapi_flags = kc.flags; }
        }
        g_ssn_pending = 0;
        g_ssn_tx_left = 3;
        g_ssn_tx_due = GetTickCount();
        logf("[ssn] host built SSN1 id=%lu for peer=%08lx lvl='%s' mkr=%ld alrm=%ld itm_cnt=%d score=%.0f diam=%.0f\n",
             g_ssn_pkt.snap_id, g_ssn_pkt.peer_run_id, g_ssn_pkt.level, g_ssn_pkt.mkr_state, g_ssn_pkt.alrm_state,
             (int)g_ssn_pkt.itm_cnt, VF(g_ssn_pkt.score), VF(g_ssn_pkt.diamonds));
    }
    if (g_ssn_tx_left > 0 && (long)(GetTickCount() - g_ssn_tx_due) >= 0) {
        net_send_peer((BYTE *)&g_ssn_pkt, sizeof g_ssn_pkt);
        g_ssn_tx_left--;
        g_ssn_tx_due = GetTickCount() + (g_ssn_tx_left == 2 ? 150 : 650);   // 0 / +150 / +800 ms (main_check_load tamamlandıktan sonra da ulaşsın)
    }
}

// 3D Ses Senkronu ('SND1'): oyunun entPlaySound(pos, handle, vol) çağrısını kancalar,
// G_SND_TBL[0..329] sabit indeksi + 3D konum + ses seviyesi ile karşı tarafa aktarır (400 ms aynı-ses filtresi).
#define SND_MAGIC 0x31444E53 /* 'SND1' */
#pragma pack(push, 1)
typedef struct {
    DWORD magic, seq;
    short idx, vol;
    float x, y, z;
} SNDPKT;
#pragma pack(pop)

static void *g_snd_fn;
static DWORD g_snd_ret, g_snd_tx_seq, g_snd_tx_cnt, g_snd_rx_cnt;
static DWORD g_snd_last_tx[SND_TBL_MAX], g_snd_win_ms;
static int g_snd_win_n, g_snd_in_replay, g_snd_active, g_snd_cfg = -1;

static void __stdcall snd_on_ent_play(DWORD *sp) {
    DWORD ebx, now; var *pos, handle, vol; int i, idx = -1;
    if (g_closing || g_snd_in_replay || !g_snd_active || !sp) return;
    ebx = ebx_base();
    if (!ebx) return;
    pos = (var *)sp[1]; handle = (var)sp[2]; vol = (var)sp[3];
    if (!pos || !handle || vol <= 0) return;
    __try {
        for (i = 0; i < SND_TBL_MAX; i++) {
            if (G_SND_TBL(ebx, i) == handle) { idx = i; break; }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (idx < 0) return;
    now = GetTickCount();
    if (g_snd_last_tx[idx] && now - g_snd_last_tx[idx] < 400) return;   // aynı ses için 400 ms tekrar filtresi
    if (now - g_snd_win_ms >= 1000) { g_snd_win_ms = now; g_snd_win_n = 0; }
    if (g_snd_win_n >= 12) return;   // saniyede maks 12 ses paketi (flood koruması)
    {   SNDPKT p; float x = 0, y = 0, z = 0; long v = vol >> 10;
        __try { x = (float)VF(pos[0]); y = (float)VF(pos[1]); z = (float)VF(pos[2]); } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
        if (v < 1) v = 1;
        if (v > 255) v = 255;
        p.magic = SND_MAGIC;
        p.seq = ++g_snd_tx_seq;
        p.idx = (short)idx;
        p.vol = (short)v;
        p.x = x; p.y = y; p.z = z;
        g_snd_last_tx[idx] = now;
        g_snd_win_n++;
        g_snd_tx_cnt++;
        net_send_peer((BYTE *)&p, sizeof p);
        if (g_snd_tx_cnt <= 16) logf("[snd] tx idx=%d vol=%d pos=(%.0f %.0f %.0f)\n", idx, (int)v, x, y, z);
    }
}

static __declspec(naked) void stub_ent_playsound(void) { __asm {
    pushfd
    pushad
    lea eax, [esp + 36]
    push eax
    call snd_on_ent_play
    popad
    popfd
    push ebp
    push ebx
    push esi
    push edi
    push dword ptr [g_snd_ret]
    jmp dword ptr [g_snd_ret]
} }

static void snd_hook_install(void) {
    static int done; static void *(__cdecl *p_getscript)(char *);
    static const BYTE pro[9] = {0x55,0x53,0x56,0x57,0xE8,0,0,0,0};
    BYTE *a; DWORD old; int ok = 0;
    if (done || g_frame % 60) return;
    if (g_snd_cfg < 0) { char b[8]; g_snd_cfg = !(cfg("COOP_SOUND_SYNC", "sound_sync", b, sizeof b) && b[0] == '0'); }
    if (!g_snd_cfg) { done = 1; return; }
    if (!p_getscript) p_getscript = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
    if (!p_getscript) return;
    a = 0; __try { a = p_getscript("entPlaySound"); } __except (EXCEPTION_EXECUTE_HANDLER) { a = 0; }
    if (!a) return;
    done = 1;
    __try { ok = !memcmp(a, pro, 9); } __except (EXCEPTION_EXECUTE_HANDLER) { ok = 0; }
    if (!ok) { logf("[snd] entPlaySound at %p: prologue mismatch, not hooking\n", a); return; }
    g_snd_fn = a; g_snd_ret = (DWORD)(a + 9);
    VirtualProtect(a, 9, PAGE_EXECUTE_READWRITE, &old);
    a[0] = 0xE9; *(DWORD *)(a + 1) = (DWORD)stub_ent_playsound - (DWORD)(a + 5); memset(a + 5, 0x90, 4);
    VirtualProtect(a, 9, old, &old); FlushInstructionCache(GetCurrentProcess(), a, 9);
    logf("[snd] hooked entPlaySound at %p (ret=%08lx)\n", a, g_snd_ret);
}

static void snd_rx(const BYTE *buf, int len) {
    SNDPKT p; DWORD ebx = ebx_base(); var handle = 0; int i; char cur_lvl[64];
    if (g_closing || len < (int)sizeof(SNDPKT) || !ebx || !g_snd_fn || !g_snd_active) return;
    current_level(cur_lvl, sizeof cur_lvl);
    if (!cur_lvl[0] || !_stricmp(cur_lvl, "menu.wmb")) return;   // kapanış / menü geçişinde ölü FMOD handle çağrısını önle
    memcpy(&p, buf, sizeof p);
    if (p.magic != SND_MAGIC || p.idx < 0 || p.idx >= SND_TBL_MAX) return;
    for (i = 0; i < 16; i++) if (g_snd_rx_seen[i] == p.seq) return;
    g_snd_rx_seen[g_snd_rx_head++ & 15] = p.seq;
    // Çok uzaktaki (>2500 birim) sesleri atla
    if (g_local_count) {
        ENTITY lc;
        if (safe_ent(g_local_ent[0], &lc)) {
            double dx = p.x - VF(lc.x), dy = p.y - VF(lc.y), dz = p.z - VF(lc.z);
            if (dx * dx + dy * dy + dz * dz > 2500.0 * 2500.0) return;
        }
    }
    __try { handle = G_SND_TBL(ebx, p.idx); } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (!handle) return;
    {   var pos[3] = { (var)(p.x * 1024.0f), (var)(p.y * 1024.0f), (var)(p.z * 1024.0f) };
        var vol = (var)((p.vol < 1 ? 1 : (p.vol > 255 ? 255 : p.vol)) << 10);
        g_snd_in_replay = 1;
        __try { ((var (__cdecl *)(var *, var, var))g_snd_fn)(pos, handle, vol); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        g_snd_in_replay = 0;
        g_snd_rx_cnt++;
        if (g_snd_rx_cnt <= 16) logf("[snd] rx idx=%d vol=%d pos=(%.0f %.0f %.0f)\n", (int)p.idx, (int)p.vol, p.x, p.y, p.z);
    }
}

static void snd_tick(int in_level, int peer_active) {
    snd_hook_install();
    g_snd_active = (g_snd_cfg == 1 && in_level && peer_active);
}

// Ara Sahne Geçme Bildirimi & Senkronu ('SCT1'): bir oyuncu ara sahnede (G_CTRL_ENABLED==0) ESC ile geçerse diğerine ESC enjekte edilir
#define CUT_MAGIC 0x31544353 /* 'SCT1' */
typedef struct { DWORD magic, seq; BYTE action, pad[3]; } CUTPKT;
static DWORD g_cut_tx_seq, g_cut_tx_due;
static int g_cut_tx_left, g_cut_esc_prev, g_cut_in_frames;
static CUTPKT g_cut_pkt;
static void cut_inject_esc(void);

static void cut_rx(const BYTE *buf, int len) {
    CUTPKT p; DWORD ebx = ebx_base(); int i, in_cut = 0;
    if (len < (int)sizeof(CUTPKT) || !ebx) return;
    memcpy(&p, buf, sizeof p);
    if (p.magic != CUT_MAGIC || p.action != 1) return;
    for (i = 0; i < 16; i++) if (g_cut_rx_seen[i] == p.seq) return;
    g_cut_rx_seen[g_cut_rx_head++ & 15] = p.seq;
    __try { in_cut = (G_CTRL_ENABLED(ebx) == 0); } __except (EXCEPTION_EXECUTE_HANDLER) { in_cut = 0; }
    // Yalnız en az ~0.5 sn'dir (30 kare) gerçekten ara sahnedeysek ESC enjekte et (harita yükleme fade-in'inde pause menüsü açılmasın)
    if (in_cut && g_cut_in_frames >= 30) {
        cut_inject_esc();
        hud_toast("ARKADAS ARA SAHNEYI GECTI", 2.5);
        logf("[cut] rx skip -> injected ESC (6 frames)\n");
    } else {
        logf("[cut] rx skip ignored (in_cut=%d frames=%d)\n", in_cut, g_cut_in_frames);
    }
}

static void cut_tick(int in_level, int peer_active) {
    DWORD ebx = ebx_base(); int in_cut = 0, esc;
    if (g_closing || !in_level || !peer_active || !ebx) { g_cut_tx_left = 0; g_cut_in_frames = 0; g_cut_esc_prev = 0; return; }
    // Oyuncu (0,0,0) konumunda takılı / yüklemedeyse (ör. tunel.wmb tünel koşusu veya hatalı yükleme) sahte ara sahne ESC spam'i yapma
    if (g_local_count) {
        ENTITY lc;
        if (safe_ent(g_local_ent[0], &lc) && lc.x == 0 && lc.y == 0 && lc.z == 0) {
            g_cut_tx_left = 0; g_cut_in_frames = 0; g_cut_esc_prev = 0; return;
        }
    }
    __try { in_cut = (G_CTRL_ENABLED(ebx) == 0); } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (in_cut) g_cut_in_frames++; else g_cut_in_frames = 0;
    esc = hud_focused() && (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    // Yalnız oyuncu gerçekten ara sahnedeyken (>=30 kare) ESC tuşuna bastığında peer'a bildir (harita başı fade-in'de sahte tetikleme olmaz)
    if (in_cut && g_cut_in_frames >= 30 && esc && !g_cut_esc_prev) {
        g_cut_pkt.magic = CUT_MAGIC;
        g_cut_pkt.seq = ++g_cut_tx_seq;
        g_cut_pkt.action = 1;
        g_cut_pkt.pad[0] = g_cut_pkt.pad[1] = g_cut_pkt.pad[2] = 0;
        g_cut_tx_left = 3;
        g_cut_tx_due = GetTickCount();
        logf("[cut] local pressed ESC in cutscene -> notifying peer\n");
    }
    g_cut_esc_prev = esc;
    if (g_cut_tx_left > 0 && (long)(GetTickCount() - g_cut_tx_due) >= 0) {
        net_send_peer((BYTE *)&g_cut_pkt, sizeof g_cut_pkt);
        g_cut_tx_left--;
        g_cut_tx_due = GetTickCount() + (g_cut_tx_left == 2 ? 60 : 90);
    }
}

// 2. Müzik Kanalı Senkronu ('SMU1'): music_channel2_loop (özel bölge / boss müziği) host otoriteli eşitleme
#define MUS_MAGIC 0x31554D53 /* 'SMU1' */
typedef struct { DWORD magic, seq; var on, trk, vol; } MUSPKT;
static DWORD g_mus_tx_seq;
static var g_mus_last_on = -1, g_mus_last_trk = -1, g_mus_last_vol = -1;
static double g_mus_next_tx;

static void mus_rx(const BYTE *buf, int len) {
    static void *(__cdecl *p_gs)(char *);
    MUSPKT p; DWORD ebx = ebx_base(); var cur_on = 0, cur_trk = 0;
    if (is_host() || len < (int)sizeof(MUSPKT) || !ebx) return;
    memcpy(&p, buf, sizeof p);
    if (p.magic != MUS_MAGIC) return;
    if (p.seq && p.seq <= g_mus_rx_seq && g_mus_rx_seq - p.seq < 1000) return;
    g_mus_rx_seq = p.seq;
    __try {
        cur_on = G_MUS2_ON(ebx); cur_trk = G_MUS2_TRK(ebx);
        G_MUS2_TRK(ebx) = p.trk;
        G_MUS2_VOL(ebx) = p.vol;
        G_MUS2_ON(ebx) = p.on;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (p.on != 0 && cur_on == 0) {
        // Client'ta music_channel2_loop görevi hiç başlamamışsa (g_07fcbf61==0 ve görev listesinde yoksa) bir kez başlat
        var ch2_h = 0; int running = 0; long n;
        __try { ch2_h = *(var *)((BYTE *)ebx + 0x28bf1c); } __except (EXCEPTION_EXECUTE_HANDLER) { ch2_h = 1; }
        if (!ch2_h && p_engine_gettaskinfo) {
            for (n = 0; n < 512; n++) {
                void *a = 0, *b = 0, *r = safe_task(n, &a, &b), *so = 0; char nm[64];
                if (!r && !a && !b) break;
                if (a && a != (void *)-1 && safe_scriptinfo(a, &so) && so && so != (void *)-1) {
                    safe_str((char *)so, nm, sizeof nm);
                    if (!strcmp(nm, "music_channel2_loop")) { running = 1; break; }
                }
            }
            if (!running) {
                void *fn;
                if (!p_gs) p_gs = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
                if (!p_callscript4) p_callscript4 = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_callscript4");
                fn = p_gs ? p_gs("music_channel2_loop") : 0;
                if (fn && p_callscript4) __try { p_callscript4((long)fn, 0, 0, 0, 0); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
    }
    if (cur_on != p.on || cur_trk != p.trk) {
        logf("[mus] client sync ch2 on=%ld trk=%ld vol=%ld\n", p.on >> 10, p.trk >> 10, p.vol >> 10);
    }
}

static void mus_tick(int in_level, int peer_active) {
    DWORD ebx = ebx_base(); double t = now_s(); var on = 0, trk = 0, vol = 0;
    if (!in_level || !peer_active || !ebx || !is_host()) {
        if (!in_level) { g_mus_last_on = g_mus_last_trk = g_mus_last_vol = -1; }
        return;
    }
    __try { on = G_MUS2_ON(ebx); trk = G_MUS2_TRK(ebx); vol = G_MUS2_VOL(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (on != g_mus_last_on || trk != g_mus_last_trk || vol != g_mus_last_vol || t >= g_mus_next_tx) {
        MUSPKT p;
        p.magic = MUS_MAGIC; p.seq = ++g_mus_tx_seq;
        p.on = on; p.trk = trk; p.vol = vol;
        net_send_peer((BYTE *)&p, sizeof p);
        if (on != g_mus_last_on || trk != g_mus_last_trk) {
            logf("[mus] host ch2 -> on=%ld trk=%ld vol=%ld\n", on >> 10, trk >> 10, vol >> 10);
        }
        g_mus_last_on = on; g_mus_last_trk = trk; g_mus_last_vol = vol;
        g_mus_next_tx = t + 2.0;
    }
}

// Bölüm açılışında level_metarials_apply() bitmeden kukla (ent_create) oluşturma!
// Aksi halde oyunun ent_for_file("supercan_*.mdl") çağrısı gerçek yerel karakter yerine kuklayı bulup
// BR_material_load()'u kuklaya uyguluyor; gerçek karakterin materyal/ışık/glow/cubemap'i boş kalıyor!
static int level_materials_ready(const char *level, int has_local) {
    static char s_lvl[64]; static int s_ready, s_frames;
    static void *(__cdecl *p_gs)(char *);
    static var (__cdecl *p_ps)(void *);
    static void *s_lma_fn;
    if (!level || !level[0] || !has_local) { s_lvl[0] = 0; s_ready = 0; s_frames = 0; return 0; }
    if (strcmp(s_lvl, level)) {
        strncpy(s_lvl, level, sizeof s_lvl - 1); s_lvl[sizeof s_lvl - 1] = 0;
        s_ready = 0; s_frames = 0;
    }
    if (s_ready) return 1;
    if (!_stricmp(level, "menu.wmb")) { s_ready = 1; return 1; }
    s_frames++;
    if (!p_gs) p_gs = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
    if (!p_ps) p_ps = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "proc_status");
    if (!s_lma_fn && p_gs) __try { s_lma_fn = p_gs("level_metarials_apply"); } __except (EXCEPTION_EXECUTE_HANDLER) { s_lma_fn = 0; }
    if (p_ps && s_lma_fn) {
        var ps = 0;
        __try { ps = p_ps(s_lma_fn); } __except (EXCEPTION_EXECUTE_HANDLER) { ps = 0; }
        if (ps > 0) return 0;   // level_metarials_apply hâlâ çalışıyor -> kukla oluşturma!
    }
    if (g_mtl_off > 0 && s_frames >= 5) {
        DWORD k;
        for (k = 0; k < g_local_count; k++) {
            ENTITY lc; char lt[32];
            if (!safe_ent(g_local_ent[k], &lc)) continue;
            safe_str(lc.type, lt, sizeof lt);
            if (!_stricmp(lt, "SUPERCAN_GOVDE.MDL")) {
                int ok = 0;
                __try {
                    void *m = *(void **)((BYTE *)g_local_ent[k] + g_mtl_off);
                    var *sk = (var *)((BYTE *)g_local_ent[k] + 148);
                    if (m && (sk[48] != 0 || sk[64] != 0 || sk[93] != 0)) ok = 1;
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
                if (ok) {
                    s_ready = 1;
                    logf("[gfx] level '%s' materials ready at frame +%d (puppets unlocked)\n", level, s_frames);
                    return 1;
                }
                break;
            }
        }
    }
    if (s_frames >= 90) {
        s_ready = 1;
        logf("[gfx] level '%s' materials fallback ready at frame +%d\n", level, s_frames);
        return 1;
    }
    return 0;
}

// Per tick, after the entity walk: host collects + sends; client hides its own robots and renders the host's.
static ENTITY *g_wcand[WMAX_PARTS * 2]; static DWORD g_nwcand;   // this tick's local world parts (from the walk)
static void world_tick(const char *level, int in_level, int peer_active, int mats_ready) {
    int host;
    if (g_world < 0) world_cfg();
    if (!g_world) return;
    strncpy(g_world_level, level, sizeof g_world_level - 1);
    host = is_host();
    // the calls recorded during this frame (scripts run inside engine_frame) become "the newest frame"
    memcpy(g_wlast, g_wrec, g_nwrec * sizeof(NETCALL)); g_nwlast = g_nwrec; g_nwrec = 0;
    if (host) {
        double t = now_s(); DWORD i;
        if (g_nwcand <= WMAX_PARTS) {
            g_wl_n = g_nwcand;
            memcpy(g_wl_ent, g_wcand, g_wl_n * sizeof(ENTITY *));
        } else {
            // 64 parça sınırı aşıldığında önce canlı düşmanları/taşıyıcıyı al, kalan slotlara çöpleri koy
            ENTITY c; char ty[32]; int pass;
            g_wl_n = 0;
            for (pass = 0; pass < 2 && g_wl_n < WMAX_PARTS; pass++) {
                for (i = 0; i < g_nwcand && g_wl_n < WMAX_PARTS; i++) {
                    int tr = 0;
                    if (safe_ent(g_wcand[i], &c)) { safe_str(c.type, ty, sizeof ty); tr = is_trash_type(ty); }
                    if ((pass == 0 && !tr) || (pass == 1 && tr)) g_wl_ent[g_wl_n++] = g_wcand[i];
                }
            }
        }
        g_world_client_active = 0;
        for (i = 0; i < WMAX_PARTS; i++) if (g_wp[i].e) { world_drop_puppets("became host"); break; }
        if (in_level) { hit_host_tick(); aim_host_tick(in_level, peer_active); }
        else if (g_aim_pup || g_aim_active) aim_remove_pup("left level");
        if (peer_active && in_level && t >= g_w_next_send) {
            static WPKT k; BYTE wire[WIRE_MAX]; int len; DWORD ebx = ebx_base(); int wave_busy = 0, mus_allow = 0;
            if (ebx) __try {
                wave_busy = (G_GBR_QUEUE(ebx) + G_ZBD_QUEUE(ebx) + G_ROBOT_ALIVE(ebx)) > 0;
                mus_allow = (G_MUSIC_ALLOW(ebx) != 0);
            } __except (EXCEPTION_EXECUTE_HANDLER) { wave_busy = 0; mus_allow = 0; }
            memset(&k, 0, sizeof k); k.magic = WPKT_MAGIC; k.seq = ++g_wseq; k.tms = (DWORD)(t * 1000.0);
            strncpy(k.level, level, sizeof k.level - 1);
            for (i = 0; i < g_wl_n; i++) { ENTITY c; WPART *p; char ty[32];
                if (!safe_ent(g_wl_ent[i], &c)) continue; safe_str(c.type, ty, sizeof ty);
                p = &k.part[k.count++]; p->id = (DWORD)g_wl_ent[i]; strncpy(p->type, ty, sizeof p->type - 1);
                p->x = (float)VF(c.x); p->y = (float)VF(c.y); p->z = (float)VF(c.z);
                p->pan = (float)VF(c.pan); p->tilt = (float)VF(c.tilt); p->roll = (float)VF(c.roll);
                p->sx = (float)VF(c.scale_x); p->sy = (float)VF(c.scale_y); p->sz = (float)VF(c.scale_z);
                p->frame = (float)VF(c.frame); p->nextframe = (float)VF(c.nextframe);
                p->flags = (c.flags & ~(WFLAG_WAVE_BUSY | WFLAG_MUS_ALLOW))
                         | (k.count == 1 && wave_busy ? WFLAG_WAVE_BUSY : 0)
                         | (k.count == 1 && mus_allow ? WFLAG_MUS_ALLOW : 0);
                if ((k.seq + i) % 10 == 1) world_get_look(g_wl_ent[i], p); }   // each part's look every 10th packet, spread out
            k.ncalls = g_nwlast; memcpy(k.call, g_wlast, g_nwlast * sizeof(NETCALL));
            {   // split into chunks that each fit WIRE_MAX: first find the ranges, then send with the final count
                int cs[32], ce[32], nch = 0, s = 0, e;
                while (s < (int)k.count && nch < 32) {
                    for (e = s + 1; e < (int)k.count && wpkt_encode(&k, s, e + 1, 0, 1, wire); e++) {}
                    cs[nch] = s; ce[nch] = e; nch++; s = e; }
                if (!k.count) { cs[0] = ce[0] = 0; nch = 1; }
                for (i = 0; i < (DWORD)nch; i++) {
                    len = wpkt_encode(&k, cs[i], ce[i], i, nch, wire);
                    if (len) { net_send_peer(wire, len); g_wtx++; g_wtx_bytes += len; if ((DWORD)len > g_wtx_max) g_wtx_max = len; }
                }
                if (nch > g_wtx_chunks) g_wtx_chunks = nch;
            }
            g_w_next_send = t + 0.05;
        }
    } else {
        DWORD i;
        if (g_aim_pup || g_aim_active) aim_remove_pup("became client");
        g_wl_n = 0;   // client records nothing
        {   int prev_active = g_world_client_active;
            int now_active = peer_active && in_level;
            if (prev_active != now_active) {
                logf("[world] client enemy spawns %s\n", now_active ? "BLOCKED (host runs enemies)" : "allowed (solo)");
                if (prev_active && !now_active) {
                    // Host başka bölüme/menüye geçti veya bağlantı koptu: kuklalar silindiği için G_ROBOT_ALIVE sıfırlanmalı
                    // (aksi halde client ESC -> Ana Menü'de while(G_ROBOT_ALIVE > 0) döngüsünde takılır!)
                    DWORD ebx = ebx_base();
                    if (ebx) __try {
                        if (G_ROBOT_ALIVE(ebx) > 0) {
                            logf("[world] client peer left level/disconnected -> clearing stale G_ROBOT_ALIVE=%.0f to 0\n", VF(G_ROBOT_ALIVE(ebx)));
                            G_ROBOT_ALIVE(ebx) = 0;
                        }
                    } __except (EXCEPTION_EXECUTE_HANDLER) {}
                }
            }
            g_world_client_active = now_active;
        }
        if (peer_active && in_level) for (i = 0; i < g_nwcand; i++)   // hide our own robots: the host's are shown instead
            __try { if ((g_wcand[i]->flags & (INVISIBLE_F | PASSABLE_F)) != (INVISIBLE_F | PASSABLE_F)) { g_wcand[i]->flags |= INVISIBLE_F | PASSABLE_F; g_whidden++; } }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        world_client_render(level, peer_active && in_level && mats_ready);
        if (peer_active && in_level) {
            world_client_sync_counters();
            hit_client_tick();
        } else {
            // Host başka bölümdeyken/menüdeyken client ESC -> Ana Menü (E_smart_btn_yes2) veya yüklemeye girerse
            // G_MUSIC_ALLOW=0 / G_GAME_ACTIVE=0 olur; artık G_ROBOT_ALIVE'ı 0 tut ki menüye dönüş asla kilitlenmesin.
            DWORD ebx = ebx_base();
            if (ebx) __try {
                if ((G_GAME_ACTIVE(ebx) == 0 || G_MUSIC_ALLOW(ebx) == 0) && G_ROBOT_ALIVE(ebx) > 0) {
                    logf("[world] client solo/diff-level menu-exit -> clearing G_ROBOT_ALIVE=%.0f to 0\n", VF(G_ROBOT_ALIVE(ebx)));
                    G_ROBOT_ALIVE(ebx) = 0;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
    if (g_frame % 600 == 0) {
        int np = 0, i; for (i = 0; i < WMAX_PARTS; i++) np += g_wp[i].e != 0;
        logf("[world] role=%s local_parts=%lu calls/frame=%lu drop=%lu | tx=%lu avg=%luB max=%luB chunks<=%d | rx=%lu (chunks %lu, partial %lu) puppets=%d hidden=%lu blocked=%lu\n",
             host ? "host" : "client", g_nwcand, g_nwlast, g_wrec_drop, g_wtx, g_wtx ? g_wtx_bytes / g_wtx : 0, g_wtx_max, g_wtx_chunks, g_wrx, g_wchunk_rx, g_wpartial, np, g_whidden, g_create_blocked);
        g_wtx = g_wtx_bytes = g_wtx_max = 0; g_whidden = 0; g_wtx_chunks = 0;
    }
}
