// [SC1] Dunya/dusman senkronu (SCW3), vurus kuyrugu (SHT1), 2 oyuncu hedefleme (SPH1), makara (SMK2), alarm (SAL2)
// ======================= WORLD SYNC (host-authoritative enemies, step 1: visuals) =======================
// The host runs the enemies; it sends their visible parts (GBR_* robot models) in a separate 'SCW1' packet at 20 Hz,
// with the animation calls made on them in the newest frame. The client hides its own robots (INVISIBLE|PASSABLE,
// its AI still runs: step 2 neutralises it) and mirrors the host's as puppets, rendered at the same render time
// as the player puppet (same sender clock). Role: role=host|client in coop.ini, default auto (UDP: lower listen
// port is host; Steam: lower SteamID). world=0 turns it off.
#define WMAX_PARTS 64
#define WMAX_CALLS 128
#define WPKT_MAGIC 0x33574353 /* 'SCW3': one world state split over chunks (chunk i of n, same seq), each <= WIRE_MAX */
#define WMAX_SK 16
typedef struct { DWORD id; char type[28]; float x, y, z, pan, tilt, roll, sx, sy, sz, frame, nextframe; long flags;
                 int has_look; BYTE look[32]; char mtl[32]; BYTE nsk, skidx[WMAX_SK]; var skv[WMAX_SK]; } WPART;   // look: every 10th packet
typedef struct { DWORD magic, seq, tms; BYTE chunk, nchunks; char level[32]; DWORD count, ncalls; WPART part[WMAX_PARTS]; NETCALL call[WMAX_CALLS]; } WPKT;
static int g_world = -1, g_role_cfg;          // g_role_cfg: 0 auto, 1 host, 2 client
static ENTITY *g_wl_ent[WMAX_PARTS]; static DWORD g_wl_n;        // host: local world parts (last tick), index = packet part
static NETCALL g_wrec[WMAX_CALLS]; static DWORD g_nwrec, g_wrec_drop; // calls on world parts in the current frame
static NETCALL g_wlast[WMAX_CALLS]; static DWORD g_nwlast;            // calls of the newest completed frame
static double g_w_next_send; static DWORD g_wseq, g_wtx, g_wtx_bytes, g_wtx_max, g_wrx, g_whidden; static int g_wtx_chunks;
static DWORD g_wchunk_rx, g_wpartial;
typedef struct { DWORD id; ENTITY *e; ENTITY *icon; char type[28]; int posed, looked, trash_init; var last_s39, last_s35; long last_hit_frame; } WPUP;
static WPUP g_wp[WMAX_PARTS];
static void world_apply_look(WPUP *q, const WPART *p);
#define NWSNAP 16
typedef struct { double t; WPKT k; } WSNAP;
static WSNAP *g_wsnap; static int g_wsnap_head, g_nwsnap; static DWORD g_last_wseq;
#define WSNAPI(i) (&g_wsnap[(g_wsnap_head + (i)) % NWSNAP])

static int is_enemy_body_type(const char *ty) {
    return !_stricmp(ty, "GBR_GOVDE.MDL") || !_stricmp(ty, "ZBD_GOVDE.MDL") || !_stricmp(ty, "UCAN_GOVDE.MDL");
}
static int is_enemy_box_type(const char *ty) {
    return !_stricmp(ty, "GBR_BOX.MDL") || !_stricmp(ty, "ZBD_BOX.MDL") || !_stricmp(ty, "UCAN_BOX.MDL");
}
static int is_trash_type(const char *ty) {
    // cop_robot_fizik (decomp.c:133848) yalnız YANIK_COP.MDL için çalışır (toplanabilir robot çöpü + kırmızı metal_icon.tga)
    return !_stricmp(ty, "YANIK_COP.MDL");
}
static int is_debris_type(const char *ty) {
    // Robot patladığında saçılan toplanamaz dekoratif parçalar & boss ateş topları (kırmızı ikon/vakum almaz)
    return !_strnicmp(ty, "COP_GBR_", 8) || !_strnicmp(ty, "COP_ZBD_", 8) || !_stricmp(ty, "COP_ROBOT_PARCA.MDL")
        || !_stricmp(ty, "BOSS_FIRE_GLOW.TGA") || !_stricmp(ty, "BOSS_FIRE_ALEV+32.TGA");
}
static int is_medkit_type(const char *ty) {
    return !_stricmp(ty, "ENRJ_PAKET.MDL");
}
static int is_world_type(const char *ty) {   // enemy/drone/trash/debris/medkit parts that are drawn (AI box and physics body stay out)
    if (!_strnicmp(ty, "GBR_", 4)) return _stricmp(ty, "GBR_BOX.MDL") && _stricmp(ty, "GBR_FIZIK.MDL");
    if (!_strnicmp(ty, "ZBD_", 4)) return _stricmp(ty, "ZBD_BOX.MDL") && _stricmp(ty, "ZBD_FIZIK.MDL");
    if (!_strnicmp(ty, "UCAN_", 5)) return _stricmp(ty, "UCAN_BOX.MDL") != 0;
    if (!_strnicmp(ty, "TASIYICI_", 9)) return _stricmp(ty, "TASIYICI_BOX.MDL") != 0;   // robot-dropping drone
    if (!_strnicmp(ty, "PLAZMA_KURE", 11) || !_strnicmp(ty, "PLAZMA_YILD", 11)) return 1;   // taşıyıcı altındaki elektrik kubbesi/ışınları
    if (is_trash_type(ty) || is_debris_type(ty)) return 1;
    if (is_medkit_type(ty)) return 1;   // robot/pil sağlık paketi (+50 HP, AIF_RbtEnrj_ekle)
    return 0;
}
static int is_wpuppet(ENTITY *e) { int i; for (i = 0; i < WMAX_PARTS; i++) if (g_wp[i].e == e) return 1; return 0; }
static int world_index(ENTITY *e) { DWORD i; for (i = 0; i < g_wl_n; i++) if (g_wl_ent[i] == e) return (int)i; return -1; }
static void wrecord(ENTITY *e, int op, const char *scene, var a, var b, var c, ENTITY *src) {
    int i = g_wl_n ? world_index(e) : -1; NETCALL *k;
    if (i < 0) return;
    if (g_nwrec >= WMAX_CALLS) { g_wrec_drop++; return; }
    k = &g_wrec[g_nwrec++]; memset(k, 0, sizeof *k); k->op = (BYTE)op; k->part = (BYTE)i; k->a = a; k->b = b; k->c = c;
    if (op == OP_ANIMATEFROM) k->c = src ? world_index(src) : -1;
    k->pose = safe_long((BYTE *)e + POSE_OFF);
    if (scene) safe_str(scene, k->scene, sizeof k->scene);
    if (op == OP_ANIMATE) {
        static char seen_sc[16][32]; static int nseen_sc;
        int j; for (j = 0; j < nseen_sc && strcmp(seen_sc[j], k->scene); j++) {}
        if (j == nseen_sc && nseen_sc < 16) {
            ENTITY ec; char ty[32] = "";
            if (safe_ent(e, &ec)) safe_str(ec.type, ty, sizeof ty);
            strncpy(seen_sc[nseen_sc++], k->scene, 31);
            logf("[wanim] '%s' scene='%s' pct=%.1f pose=%.0f\n", ty, k->scene, VF(a), VF(k->pose));
        }
    }
}
static int is_host(void) {
    if (g_role_cfg) return g_role_cfg == 1;
    if (g_net_steam == 2) return g_st_peer && g_st_me < g_st_peer;
    if (!g_peer_known) return 0;
    if (g_lport != ntohs(g_peer.sin_port)) return g_lport < ntohs(g_peer.sin_port);
    return ntohl(g_my_ip[0]) < ntohl(g_peer.sin_addr.s_addr);
}
static void world_cfg(void) {
    char b[16];
    g_world = !(cfg("COOP_WORLD", "world", b, sizeof b) && b[0] == '0');
    if (cfg("COOP_ROLE", "role", b, sizeof b)) g_role_cfg = !_stricmp(b, "host") ? 1 : !_stricmp(b, "client") ? 2 : 0;
    logf("[world] world sync %s, role %s\n", g_world ? "ON" : "off", g_role_cfg == 1 ? "host" : g_role_cfg == 2 ? "client" : "auto");
}
// Encode parts [p0, p1) of k with the calls on them (part indices made chunk-local) as chunk `chunk` of `nchunks`.
// Returns 0 when it does not fit in WIRE_MAX (the caller makes the chunk smaller).
static int wpkt_encode(const WPKT *full, int p0, int p1, int chunk, int nchunks, BYTE *buf) {
    static WPKT sub; const WPKT *k = &sub;
    char tab[251][32]; int nt = 0, i; BYTE ti[WMAX_PARTS], ci[WMAX_CALLS]; DWORD nc, np; WBUF w; BYTE b;
    memcpy(&sub, full, offsetof(WPKT, count)); sub.chunk = (BYTE)chunk; sub.nchunks = (BYTE)nchunks;
    sub.count = p1 - p0; memcpy(sub.part, full->part + p0, (p1 - p0) * sizeof(WPART)); sub.ncalls = 0;
    for (i = 0; i < (int)full->ncalls; i++) { const NETCALL *c = &full->call[i]; NETCALL *d;
        if (c->part < p0 || c->part >= p1) continue;
        d = &sub.call[sub.ncalls++]; *d = *c; d->part = (BYTE)(c->part - p0);
        if (c->op == OP_ANIMATEFROM) d->c = (c->c >= p0 && c->c < p1) ? c->c - p0 : -1; }
    nc = k->ncalls; np = k->count;
    str_index(tab, &nt, k->level);
    for (i = 0; i < (int)np; i++) ti[i] = (BYTE)str_index(tab, &nt, k->part[i].type);
    for (i = 0; i < (int)nc; i++) ci[i] = k->call[i].scene[0] ? (BYTE)str_index(tab, &nt, k->call[i].scene) : 255;
    { BYTE mi[WMAX_PARTS];
    for (i = 0; i < (int)np; i++) mi[i] = (k->part[i].has_look && k->part[i].mtl[0]) ? (BYTE)str_index(tab, &nt, k->part[i].mtl) : 255;
    {   int size = 4 + 4 + 4 + 2 + 1 + 1 + (int)np * 54 + 1 + (int)nc * 16;
        for (i = 0; i < (int)np; i++) if (k->part[i].has_look) size += 1 + 32 + 1 + k->part[i].nsk * 5;
        for (i = 0; i < nt; i++) size += 1 + (int)strlen(tab[i]);
        if (size > WIRE_MAX || nt > 250) return 0;
    }
    w.p = buf; w.end = buf + WIRE_MAX; w.bad = 0;
    w_put(&w, &k->magic, 4); w_put(&w, &k->seq, 4); w_put(&w, &k->tms, 4); w_put(&w, &k->chunk, 1); w_put(&w, &k->nchunks, 1);
    b = (BYTE)nt; w_put(&w, &b, 1);
    for (i = 0; i < nt; i++) { b = (BYTE)strlen(tab[i]); w_put(&w, &b, 1); w_put(&w, tab[i], b); }
    b = (BYTE)np; w_put(&w, &b, 1);
    for (i = 0; i < (int)np; i++) { const WPART *p = &k->part[i]; BYTE hl = (BYTE)(p->has_look != 0);
        w_put(&w, &p->id, 4); w_put(&w, &ti[i], 1); w_put(&w, &p->x, 11 * 4); w_put(&w, &p->flags, 4); w_put(&w, &hl, 1);
        if (hl) { int j; w_put(&w, &mi[i], 1); w_put(&w, p->look, 32); w_put(&w, &p->nsk, 1);
            for (j = 0; j < p->nsk; j++) { w_put(&w, &p->skidx[j], 1); w_put(&w, &p->skv[j], 4); } } }
    b = (BYTE)nc; w_put(&w, &b, 1);
    for (i = 0; i < (int)nc; i++) { const NETCALL *c = &k->call[i]; BYTE pose = (BYTE)(c->pose >> 10), part = c->part;
        if (part >= np) part = 255;   // its part was dropped
        w_put(&w, &c->op, 1); w_put(&w, &part, 1); w_put(&w, &ci[i], 1); w_put(&w, &pose, 1);
        w_put(&w, &c->a, 4); w_put(&w, &c->b, 4); w_put(&w, &c->c, 4); }
    return w.bad ? 0 : (int)(w.p - buf); }
}
static int wpkt_decode(const BYTE *buf, int len, WPKT *k) {
    char tab[256][32]; int nt, i; BYTE b; WBUF w;
    w.p = (BYTE *)buf; w.end = (BYTE *)buf + len; w.bad = 0; memset(k, 0, sizeof *k);
    w_get(&w, &k->magic, 4); if (k->magic != WPKT_MAGIC) return 0;
    w_get(&w, &k->seq, 4); w_get(&w, &k->tms, 4); w_get(&w, &k->chunk, 1); w_get(&w, &k->nchunks, 1);
    if (!k->nchunks || k->chunk >= k->nchunks) return 0;
    w_get(&w, &b, 1); nt = b;
    for (i = 0; i < nt; i++) { w_get(&w, &b, 1); if (b > 31) return 0; w_get(&w, tab[i], b); tab[i][b] = 0; }
    if (!nt || w.bad) return 0;
    strcpy(k->level, tab[0]);
    w_get(&w, &b, 1); k->count = b; if (k->count > WMAX_PARTS) return 0;
    for (i = 0; i < (int)k->count; i++) { WPART *p = &k->part[i]; BYTE t;
        BYTE hl; w_get(&w, &p->id, 4); w_get(&w, &t, 1); if (t >= nt) return 0; strncpy(p->type, tab[t], sizeof p->type - 1);
        w_get(&w, &p->x, 11 * 4); w_get(&w, &p->flags, 4); w_get(&w, &hl, 1); p->has_look = hl;
        if (hl) { BYTE m; int j; w_get(&w, &m, 1); if (m != 255) { if (m >= nt) return 0; strncpy(p->mtl, tab[m], sizeof p->mtl - 1); }
            w_get(&w, p->look, 32); w_get(&w, &p->nsk, 1); if (p->nsk > WMAX_SK) return 0;
            for (j = 0; j < p->nsk; j++) { w_get(&w, &p->skidx[j], 1); w_get(&w, &p->skv[j], 4); if (p->skidx[j] >= 100) return 0; } } }
    w_get(&w, &b, 1); k->ncalls = b; if (k->ncalls > WMAX_CALLS) return 0;
    for (i = 0; i < (int)k->ncalls; i++) { NETCALL *c = &k->call[i]; BYTE s, pose;
        w_get(&w, &c->op, 1); w_get(&w, &c->part, 1); w_get(&w, &s, 1); w_get(&w, &pose, 1);
        w_get(&w, &c->a, 4); w_get(&w, &c->b, 4); w_get(&w, &c->c, 4);
        c->pose = (var)pose << 10; if (s != 255) { if (s >= nt) return 0; strncpy(c->scene, tab[s], sizeof c->scene - 1); } }
    return !w.bad && w.p == w.end;
}
static void world_commit(WPKT *asmk, int complete);
static void world_rx(const BYTE *buf, int len) {   // from the receive loop: 'SCW3' chunks from the peer
    static WPKT k, asmk; static DWORD asm_seq, asm_mask; static int asm_have;
    DWORD i;
    if (!g_wsnap) g_wsnap = (WSNAP *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WSNAP) * NWSNAP);
    if (!g_wsnap || !wpkt_decode(buf, len, &k)) return;
    if (k.seq <= g_last_wseq && g_last_wseq - k.seq < 1000) return;   // old/reordered: skip (20 Hz state, no need)
    g_wchunk_rx++;
    if (asm_have && k.seq != asm_seq) { world_commit(&asmk, 0); asm_have = 0; }   // a newer state started: use what arrived
    if (!asm_have) { memcpy(&asmk, &k, offsetof(WPKT, count)); asmk.count = asmk.ncalls = 0; asm_seq = k.seq; asm_mask = 0; asm_have = 1; }
    if (k.chunk >= 32 || (asm_mask & (1u << k.chunk))) return;
    asm_mask |= 1u << k.chunk;
    {   DWORD base = asmk.count;
        for (i = 0; i < k.count && asmk.count < WMAX_PARTS; i++) asmk.part[asmk.count++] = k.part[i];
        for (i = 0; i < k.ncalls && asmk.ncalls < WMAX_CALLS; i++) { NETCALL c = k.call[i];
            if (c.part < k.count) c.part = (BYTE)(c.part + base); else c.part = 255;
            if (c.op == OP_ANIMATEFROM && c.c >= 0 && (DWORD)c.c < k.count) c.c += base; else if (c.op == OP_ANIMATEFROM) c.c = -1;
            asmk.call[asmk.ncalls++] = c; } }
    if (asm_mask == (k.nchunks >= 32 ? 0xffffffffu : (1u << k.nchunks) - 1)) { world_commit(&asmk, 1); asm_have = 0; }
}
static void world_commit(WPKT *kp, int complete) {
    WPKT *k = kp; double t; DWORD i, j;
    if (k->seq <= g_last_wseq && g_last_wseq - k->seq < 1000) return;
    if (!complete && g_nwsnap) {   // missing chunk: keep the parts it had from the previous state (no puppet flicker)
        const WPKT *pk = &WSNAPI(g_nwsnap - 1)->k; g_wpartial++;
        if (!strcmp(pk->level, k->level))
            for (i = 0; i < pk->count && k->count < WMAX_PARTS; i++) {
                for (j = 0; j < k->count && k->part[j].id != pk->part[i].id; j++) {}
                if (j == k->count) k->part[k->count++] = pk->part[i]; }
    }
    g_last_wseq = k->seq; t = k->tms / 1000.0;
    if (g_nwsnap && (strcmp(WSNAPI(g_nwsnap - 1)->k.level, k->level) || t < WSNAPI(g_nwsnap - 1)->t - 5.0)) g_nwsnap = 0;   // new level / restart
    if (g_nwsnap == NWSNAP) { g_wsnap_head = (g_wsnap_head + 1) % NWSNAP; g_nwsnap--; }
    WSNAPI(g_nwsnap)->t = t; WSNAPI(g_nwsnap)->k = *k; g_nwsnap++; g_wrx++;
    if (g_wrx == 1) logf("[world] first world packet: level='%s' parts=%lu calls=%lu chunks=%d\n", k->level, k->count, k->ncalls, k->nchunks);
}
static int is_entity(DWORD v);
static ENTITY *g_icon_del_e[64]; static long g_icon_del_at[64];
static void icon_defer_remove(ENTITY *ic) {
    int i;
    if (!ic) return;
    __try { ic->flags |= INVISIBLE_F | PASSABLE_F; } __except (EXCEPTION_EXECUTE_HANDLER) {}
    for (i = 0; i < 64; i++) if (!g_icon_del_e[i]) { g_icon_del_e[i] = ic; g_icon_del_at[i] = g_frame + 20; return; }
    __try { p_ent_remove(ic); } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
static void icon_defer_flush(int force) {
    int i;
    for (i = 0; i < 64; i++) if (g_icon_del_e[i] && (force || g_frame >= g_icon_del_at[i])) {
        if (is_entity((DWORD)g_icon_del_e[i])) __try { p_ent_remove(g_icon_del_e[i]); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        g_icon_del_e[i] = 0;
    }
}
static void world_drop_puppets(const char *why) {
    int i; for (i = 0; i < WMAX_PARTS; i++) {
        if (g_wp[i].icon) { __try { p_ent_remove(g_wp[i].icon); } __except (EXCEPTION_EXECUTE_HANDLER) {} g_wp[i].icon = 0; }
        if (g_wp[i].e) { __try { p_ent_remove(g_wp[i].e); } __except (EXCEPTION_EXECUTE_HANDLER) {} g_wp[i].e = 0; }
    }
    icon_defer_flush(1);
    if (why) logf("[world] puppets removed (%s)\n", why);
}
static WPUP *wp_find(DWORD id) { int i; for (i = 0; i < WMAX_PARTS; i++) if (g_wp[i].e && g_wp[i].id == id) return &g_wp[i]; return 0; }
static const WPART *wpart_by_id(const WPKT *k, DWORD id) { DWORD i; for (i = 0; i < k->count; i++) if (k->part[i].id == id) return &k->part[i]; return 0; }
static void wreplay(const WPKT *k, const NETCALL *c) {
    WPUP *q, *src = 0;
    if (c->part >= k->count || !(q = wp_find(k->part[c->part].id))) return;
    if (c->op == OP_ANIMATEFROM && (DWORD)c->c < k->count) src = wp_find(k->part[c->c].id);
    replay_on(q->e, src ? src->e : 0, &q->posed, c);
}
// Client: mirror the host's world parts at render time g_last_rt (same sender clock as the player packets).
static void world_client_render(const char *level, int active) {
    static DWORD last_reset_gen;
    WSNAP *a = 0, *b = 0; float f = 1; int i; DWORD j;
    if (!active || !g_nwsnap || strcmp(WSNAPI(g_nwsnap - 1)->k.level, level)) {
        for (i = 0; i < WMAX_PARTS; i++) if (g_wp[i].e) { world_drop_puppets(active ? "host in another level" : "no host"); break; }
        return;
    }
    if (last_reset_gen != g_d3d_reset_gen) {
        last_reset_gen = g_d3d_reset_gen;
        for (i = 0; i < WMAX_PARTS; i++) if (g_wp[i].e) g_wp[i].looked = 0;
        logf("[world] D3D reset detected (gen=%lu) -> refreshing puppet looks\n", last_reset_gen);
    }
    {   double rt = g_last_rt; int n;
        for (n = 0; n < g_nwsnap && WSNAPI(n)->t <= rt; n++) {}
        if (n == 0) a = b = WSNAPI(0);
        else if (n == g_nwsnap) a = b = WSNAPI(g_nwsnap - 1);
        else { a = WSNAPI(n - 1); b = WSNAPI(n); f = (float)((rt - a->t) / (b->t - a->t)); if (f < 0) f = 0; if (f > 1) f = 1; } }
    icon_defer_flush(0);
    // remove puppets whose host part is gone (a robot died) or changed model
    for (i = 0; i < WMAX_PARTS; i++) if (g_wp[i].e) {
        const WPART *p = wpart_by_id(&a->k, g_wp[i].id);
        if (!p || strcmp(p->type, g_wp[i].type)) {
            __try {
                *(DWORD *)((BYTE *)g_wp[i].e + 96) &= ~256u;
                ((var *)((BYTE *)g_wp[i].e + 148))[22] = 0;
                ((var *)((BYTE *)g_wp[i].e + 148))[25] = 2 << 10;   // pfcan_copicon_goster çalışıyorsa güvenle çıksın
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
            if (g_wp[i].icon) { icon_defer_remove(g_wp[i].icon); g_wp[i].icon = 0; }
            __try { p_ent_remove(g_wp[i].e); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            logf("[world] puppet '%s' id=%08lx removed\n", g_wp[i].type, g_wp[i].id);
            g_wp[i].e = 0; }
    }
    for (j = 0; j < a->k.count; j++) {
        WPART r = a->k.part[j]; const WPART *pb = wpart_by_id(&b->k, r.id); WPUP *q = wp_find(r.id);
        if (pb && a != b && !strcmp(pb->type, r.type)) {
            if (ABSF(pb->x - r.x) + ABSF(pb->y - r.y) + ABSF(pb->z - r.z) < 400) { r.x = lerpf(r.x, pb->x, f); r.y = lerpf(r.y, pb->y, f); r.z = lerpf(r.z, pb->z, f); }
            r.pan = lerp_ang(r.pan, pb->pan, f); r.tilt = lerp_ang(r.tilt, pb->tilt, f); r.roll = lerp_ang(r.roll, pb->roll, f);
            if (f >= 0.5f) r.flags = pb->flags;
        }
        if (!q) {
            var pos[3]; ENTITY *ne = 0; char file[32];
            for (i = 0; i < WMAX_PARTS && g_wp[i].e; i++) {}
            if (i == WMAX_PARTS) continue;
            pos[0] = (var)(r.x * 1024.0f); pos[1] = (var)(r.y * 1024.0f); pos[2] = (var)(r.z * 1024.0f);
            strncpy(file, r.type, sizeof file - 1); file[sizeof file - 1] = 0; _strlwr(file);
            if (!safe_create(file, pos, &ne) || !ne) { logf("[world] ent_create('%s') failed\n", file); continue; }
            q = &g_wp[i]; q->e = ne; q->icon = 0; q->id = r.id; q->posed = 0; q->looked = 0; q->trash_init = 0; q->last_s39 = 0; q->last_s35 = 100 << 10; strcpy(q->type, r.type);
            logf("[world] puppet '%s' id=%08lx -> %p\n", file, r.id, ne);
        }
        if (q->e && a->k.part[j].has_look) world_apply_look(q, &a->k.part[j]);
        __try { ENTITY *e = q->e;
            if (is_trash_type(q->type) && ((var *)((BYTE *)e + 148))[25] != 0) {
                // Client bu çöp parçasını vakumluyor (pfcan_kutu_cek skill[25]=1/2 yaptı): konumu boruya doğru çekilsin, host üzerine yazmasın
            } else {
                e->x = (var)(r.x * 1024.0f); e->y = (var)(r.y * 1024.0f); e->z = (var)(r.z * 1024.0f);
                e->pan = (var)(r.pan * 1024.0f); e->tilt = (var)(r.tilt * 1024.0f); e->roll = (var)(r.roll * 1024.0f);
                e->scale_x = (var)(r.sx * 1024.0f); e->scale_y = (var)(r.sy * 1024.0f); e->scale_z = (var)(r.sz * 1024.0f);
                e->frame = (var)(r.frame * 1024.0f); e->nextframe = (var)(r.nextframe * 1024.0f);
            }
            if (is_enemy_body_type(q->type)) {
                // Client kukla çarpışması: c_scan (hedef kilidi: flags |= FLAG2 ve emask@96 |= ENABLE_SCAN(256)) ve
                // c_trace (nano/kıvılcım/ses/s39) için PASSABLE kapalı; skill[37]=0 tutulur ki AIF_GBR/ZBD_plazma ilk satırda güvenle çıksın.
                e->flags = (e->flags & ~(INVISIBLE_F | PASSABLE_F)) | FLAG2_F | (r.flags & INVISIBLE_F);
                *(DWORD *)((BYTE *)e + 96) |= 256;   // ENABLE_SCAN (1<<8): c_scan hedef kilidinin kuklayı görmesi için zorunlu
                ((var *)((BYTE *)e + 148))[2]  = -1 << 10;
                ((var *)((BYTE *)e + 148))[37] = 0;
                if (((var *)((BYTE *)e + 148))[34] == 0) ((var *)((BYTE *)e + 148))[34] = 100 << 10;   // taramalı taret (topgit_taramali) s35 -= 0.1 algılaması için taban değer
            } else if (is_trash_type(q->type)) {
                // Çöp kuklası: cop_robot_fizik / cop_model_fizik ile birebir c_setminmax + vec_for_min/max + vec_mul(scale) + FLAG2 (bit 1 = 2) + metal_icon.tga
                static void (__cdecl *p_setminmax)(ENTITY *);
                static void (__cdecl *p_vmin)(var *, ENTITY *);
                static void (__cdecl *p_vmax)(var *, ENTITY *);
                static void (__cdecl *p_vmul)(var *, var *);
                static var (__cdecl *p_handle)(void *);
                var s26 = ((var *)((BYTE *)e + 148))[25];
                if (s26 == 0) {
                    // FLAG1 (1) kapalı, FLAG2 (2) açık (pfcan_mod_CopBekle c_scan 0x140001 = SCAN_FLAG2 bunu arar), PASSABLE açık
                    e->flags = (e->flags & ~(INVISIBLE_F | 1)) | PASSABLE_F | TRASH_FLAG2_F | (r.flags & INVISIBLE_F);
                } else if (s26 == (2 << 10) && q->icon) {
                    __try { q->icon->flags |= INVISIBLE_F; } __except (EXCEPTION_EXECUTE_HANDLER) {}
                }
                if (!p_setminmax) p_setminmax = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "c_setminmax");
                if (!p_vmin) p_vmin = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "vec_for_min");
                if (!p_vmax) p_vmax = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "vec_for_max");
                if (!p_vmul) p_vmul = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "vec_mul");
                if (!p_handle) p_handle = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "handle");
                if (!q->trash_init && p_setminmax) {
                    q->trash_init = 1;
                    p_setminmax(e);
                    if (p_vmin && p_vmax && p_vmul) {
                        p_vmin((var *)((BYTE *)e + 104), e); p_vmax((var *)((BYTE *)e + 116), e);
                        p_vmul((var *)((BYTE *)e + 104), &e->scale_x); p_vmul((var *)((BYTE *)e + 116), &e->scale_x);
                    }
                    if (!q->icon && p_handle) {
                        var ipos[3] = { e->x, e->y, e->z + (20 << 10) };
                        ENTITY *ic = 0;
                        if (safe_create("metal_icon.tga", ipos, &ic) && ic) {
                            var ih = 0;
                            __try {
                                // cop_robot_fizik (decomp.c:132571) ile birebir ölçek (0.05 = 52) ve bayraklar (PASSABLE|INVISIBLE|FLAG1|128)
                                ic->scale_x = 52; ic->scale_y = 52; ic->scale_z = 52;
                                ic->flags |= PASSABLE_F | INVISIBLE_F | 1 | 128;
                                ih = p_handle(ic);
                                if (ih) {
                                    q->icon = ic;
                                    ((var *)((BYTE *)e + 148))[31] = ih;
                                } else {
                                    p_ent_remove(ic);
                                }
                            } __except (EXCEPTION_EXECUTE_HANDLER) {}
                        }
                    }
                    logf("[trash] init '%s' id=%08lx flags=%08lx icon=%p min=(%.1f %.1f %.1f) max=(%.1f %.1f %.1f)\n",
                         q->type, q->id, e->flags, q->icon,
                         VF(*(var *)((BYTE *)e + 104)), VF(*(var *)((BYTE *)e + 108)), VF(*(var *)((BYTE *)e + 112)),
                         VF(*(var *)((BYTE *)e + 116)), VF(*(var *)((BYTE *)e + 120)), VF(*(var *)((BYTE *)e + 124)));
                }
                if (s26 == 0) *(DWORD *)((BYTE *)e + 96) |= 256;
                ((var *)((BYTE *)e + 148))[7]  = 1 << 10;   // pfcan_kutu_cek dalı: +15 nano, +7 puan (skill[7]!=2)
                ((var *)((BYTE *)e + 148))[22] = q->icon ? (1 << 10) : 0;   // yerel metal_icon bağlıysa kırmızı çöp logosunu göster
            } else if (is_medkit_type(q->type)) {
                // Sağlık paketi (enrj_paket.mdl — AIF_RbtEnrj_ekle ile birebir: skill[22]=11, skill[7]=7, FLAG1 açık, FLAG2 kapalı, ENABLE_SCAN)
                static void (__cdecl *p_setminmax)(ENTITY *);
                if (!p_setminmax) p_setminmax = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "c_setminmax");
                if (!q->trash_init) {
                    q->trash_init = 1;
                    if (p_setminmax) p_setminmax(e);
                    e->flags = (e->flags & ~(INVISIBLE_F | TRASH_FLAG2_F)) | PASSABLE_F | 1 | (r.flags & INVISIBLE_F);
                    *(DWORD *)((BYTE *)e + 96) |= 256;   // ENABLE_SCAN: pfcan_detect_event -> pfcan_rbtenrj_al (+50 HP)
                    ((var *)((BYTE *)e + 148))[7]  = 7 << 10;
                    ((var *)((BYTE *)e + 148))[22] = 11 << 10;
                    q->last_s39 = 1;   // 1 = henüz client tarafından alınmadı
                    logf("[medkit] init '%s' id=%08lx flags=%08lx\n", q->type, q->id, e->flags);
                } else if (q->last_s39 == 1) {
                    e->flags = (e->flags & ~(INVISIBLE_F | TRASH_FLAG2_F)) | PASSABLE_F | 1 | (r.flags & INVISIBLE_F);
                }
            } else {
                long vmask = INVISIBLE_F | 1 | 1024 | 2048 | 4096 | (1 << 17) | (1 << 20) | (1 << 21);   // toportak_DEVE: 1|131072|1048576|2097152 + TRANSLUCENT|BRIGHT|OVERLAY
                e->flags = (e->flags & ~vmask) | PASSABLE_F | (r.flags & vmask);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { logf("[world] puppet write fault\n"); q->e = 0; }
    }
    // animation: a's newest-frame calls, matched per-(part_id, op, scene) against b's calls so one robot's state change never breaks another's interpolation
    {   DWORD k, m;
        for (k = 0; k < a->k.ncalls; k++) {
            NETCALL c = a->k.call[k]; const NETCALL *y = 0;
            if (a != b && c.part < a->k.count) {
                DWORD pid = a->k.part[c.part].id;
                for (m = 0; m < b->k.ncalls; m++) {
                    const NETCALL *cand = &b->k.call[m];
                    if (cand->op == c.op && cand->part < b->k.count && b->k.part[cand->part].id == pid && !strcmp(cand->scene, c.scene)) { y = cand; break; }
                }
            }
            if (y) switch (c.op) {
                case OP_ANIMATE: case OP_ANIMATEFROM: c.a = lerp_pct_v(c.a, y->a, f, 1); break;
                case OP_BONEROTATE: c.a = lerp_ang_v(c.a, y->a, f); c.b = lerp_ang_v(c.b, y->b, f); c.c = lerp_ang_v(c.c, y->c, f); break;
                case OP_BLENDPOSE: c.c = lerp_pct_v(c.c, y->c, f, 0); break;
            }
            wreplay(&a->k, &c);
        }
        for (i = 0; i < WMAX_PARTS; i++) if (g_wp[i].e) __try { *(var *)((BYTE *)g_wp[i].e + POSE_OFF) = 1 << 10; } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    // Kemik kilidi (AIF_GBR_boneupdate / AIF_ZBD_boneupdate karşılığı): GBR_GOVDE ve ZBD_GOVDE animasyonu oynatıldıktan hemen sonra
    // göz, teker, kafa, ışık ve sopa kuklalarını doğrudan gövde kuklasının kemiğine (vec_for_bone / ang_for_bone) yapıştır.
    {   static var *(__cdecl *p_vec_for_bone)(var *, ENTITY *, char *);
        static var *(__cdecl *p_ang_for_bone)(var *, ENTITY *, char *);
        BYTE used[WMAX_PARTS] = {0}; int u;
        if (!p_vec_for_bone) p_vec_for_bone = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "vec_for_bone");
        if (!p_ang_for_bone) p_ang_for_bone = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "ang_for_bone");
        if (p_vec_for_bone && p_ang_for_bone) {
            for (i = 0; i < WMAX_PARTS; i++) if (g_wp[i].e) {
                ENTITY *gov = g_wp[i].e, gc;
                if (!_stricmp(g_wp[i].type, "GBR_GOVDE.MDL") && safe_ent(gov, &gc)) {
                    static const char *types[2] = {"GBR_GOZ.MDL", "GBR_LASTIK.MDL"}; int t;
                    for (t = 0; t < 2; t++) {
                        int best = -1; double bd = 150.0 * 150.0;
                        for (u = 0; u < WMAX_PARTS; u++) if (g_wp[u].e && !used[u] && !_stricmp(g_wp[u].type, types[t])) {
                            ENTITY pc; double dx, dy, dz, d;
                            if (!safe_ent(g_wp[u].e, &pc)) continue;
                            dx = VF(pc.x) - VF(gc.x); dy = VF(pc.y) - VF(gc.y); dz = VF(pc.z) - VF(gc.z); d = dx * dx + dy * dy + dz * dz;
                            if (d < bd) { bd = d; best = u; }
                        }
                        if (best >= 0) {
                            ENTITY *pe = g_wp[best].e; used[best] = 1;
                            __try {
                                if (t == 0) { p_vec_for_bone(&pe->x, gov, "Bone01"); p_ang_for_bone(&pe->pan, gov, "Bone01"); }
                                else { p_vec_for_bone(&pe->x, gov, "Bone06"); pe->pan = gov->pan; pe->roll = gov->roll; }
                            } __except (EXCEPTION_EXECUTE_HANDLER) {}
                        }
                    }
                } else if (!_stricmp(g_wp[i].type, "ZBD_GOVDE.MDL") && safe_ent(gov, &gc)) {
                    static const char *types[3] = {"ZBD_KAFA.MDL", "ZBD_LIGHT.MDL", "ZBD_SOPA0.MDL"}; int t;
                    for (t = 0; t < 3; t++) {
                        int best = -1; double bd = 150.0 * 150.0;
                        for (u = 0; u < WMAX_PARTS; u++) if (g_wp[u].e && !used[u] && !_stricmp(g_wp[u].type, types[t])) {
                            ENTITY pc; double dx, dy, dz, d;
                            if (!safe_ent(g_wp[u].e, &pc)) continue;
                            dx = VF(pc.x) - VF(gc.x); dy = VF(pc.y) - VF(gc.y); dz = VF(pc.z) - VF(gc.z); d = dx * dx + dy * dy + dz * dz;
                            if (d < bd) { bd = d; best = u; }
                        }
                        if (best >= 0) {
                            ENTITY *pe = g_wp[best].e; char *bn = (t < 2) ? "Bip01 Head" : "Bip01 R Hand"; used[best] = 1;
                            __try { p_vec_for_bone(&pe->x, gov, bn); p_ang_for_bone(&pe->pan, gov, bn); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                        }
                    }
                }
            }
        }
    }
}
// Look of a world part: skin..rgb (entity bytes 64..95), material by name, nonzero skills (shader parameters).
static void *(__cdecl *p_engine_getobj)(char *, long *);
// Unnamed materials: the game takes them from template models (MTRL_DUMMY_*, MTRL_DYNAMIC_*, ...). Name the material
// by "@<type>" of a non-world entity that uses the same MATERIAL; the receiver takes that type's material from its own copy.
static char g_world_level[32];
static long mtl_of(ENTITY *e) { return g_mtl_off > 0 ? safe_long((BYTE *)e + g_mtl_off) : 0; }
static int mtl_source(long m, char *out, int n) {
    static struct { long m; char ty[32]; char lvl[32]; } c[32]; static int nc, next;
    int i, best = 0; ENTITY *e, ce; char ty[32]; out[0] = 0;
    for (i = 0; i < nc; i++) if (c[i].m == m && !strcmp(c[i].lvl, g_world_level)) { strncpy(out, c[i].ty, n - 1); out[n - 1] = 0; return out[0] != 0; }
    for (e = safe_next(0); e && e != (ENTITY *)-1; e = safe_next(e)) {
        if (!safe_ent(e, &ce) || mtl_of(e) != m || is_puppet(e) || is_wpuppet(e)) continue;
        safe_str(ce.type, ty, sizeof ty);
        if (!ty[0] || is_world_type(ty) || is_player_part(ty) || !_strnicmp(ty, "GBR_", 4) || !_strnicmp(ty, "ZBD_", 4) || !_strnicmp(ty, "UCAN_", 5) || !_strnicmp(ty, "TASIYICI_", 9)) continue;
        if (!_strnicmp(ty, "MTRL_", 5)) { sprintf(out, "@%.29s", ty); best = 2; break; }
        if (!best) { sprintf(out, "@%.29s", ty); best = 1; }
    }
    i = nc < 32 ? nc++ : (next++ % 32); c[i].m = m; strcpy(c[i].lvl, g_world_level); strcpy(c[i].ty, out);
    logf("[world] material %08lx -> source '%s'\n", m, out[0] ? out : "(none)");
    return out[0] != 0;
}
static void *mtl_for_source(const char *src) {   // receiver: material of our own entity of that type
    static struct { char ty[32]; char lvl[32]; long m; } c[32]; static int nc, next;
    int i; ENTITY *e, ce; char ty[32]; long m = 0;
    for (i = 0; i < nc; i++) if (!strcmp(c[i].ty, src) && !strcmp(c[i].lvl, g_world_level)) return (void *)c[i].m;
    for (e = safe_next(0); e && e != (ENTITY *)-1; e = safe_next(e)) {
        if (!safe_ent(e, &ce) || is_puppet(e) || is_wpuppet(e)) continue;
        safe_str(ce.type, ty, sizeof ty);
        if (!_stricmp(ty, src + 1)) { m = mtl_of(e); if (m > 0x10000 && m != 0x7badbad) break; m = 0; }
    }
    i = nc < 32 ? nc++ : (next++ % 32); strcpy(c[i].ty, src); strcpy(c[i].lvl, g_world_level); c[i].m = m;
    logf("[world] material source '%s' -> %08lx here\n", src, m);
    return (void *)m;
}
static void world_get_look(ENTITY *e, WPART *p) {
    int j; var sk[100];
    __try { memcpy(p->look, (BYTE *)e + 64, 32); memcpy(sk, (BYTE *)e + 148, sizeof sk); } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    p->has_look = 1; p->nsk = 0;
    for (j = 0; j < 100 && p->nsk < WMAX_SK; j++) if (sk[j]) { p->skidx[p->nsk] = (BYTE)j; p->skv[p->nsk++] = sk[j]; }
    if (g_mtl_off > 0) { long m = mtl_of(e); if (m > 0x10000 && m != 0x7badbad) {
        safe_str((char *)safe_long((BYTE *)m + 8), p->mtl, sizeof p->mtl);
        if (!p->mtl[0]) mtl_source(m, p->mtl, sizeof p->mtl); } }
}
static void world_apply_look(WPUP *q, const WPART *p) {
    static char miss[8][32]; static int nmiss;
    __try {
        int j; memcpy((BYTE *)q->e + 64, p->look, 32);
        for (j = 0; j < p->nsk; j++) {
            BYTE idx = p->skidx[j];
            if (idx == 2 || idx == 7 || idx == 22 || idx == 25 || idx == 31 || (idx >= 34 && idx <= 39)) continue;   // kukla çarpışma/hasar/çöp/ikon skill'lerini ezme
            *(var *)((BYTE *)q->e + 148 + 4 * idx) = p->skv[j];
        }
        if (p->mtl[0] && g_mtl_off > 0) {
            long t = 0; void *m = 0;
            if (!p_engine_getobj) p_engine_getobj = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getobj");
            if (p->mtl[0] == '@') m = mtl_for_source(p->mtl);
            else if (p_engine_getobj) m = p_engine_getobj((char *)p->mtl, &t);
            if (m && ((safe_long(m) >> 24) & 0xff) != 13) m = 0;   // not a MATERIAL (object class 13): don't touch
            if (m) *(void **)((BYTE *)q->e + g_mtl_off) = m;
            else { int k; for (k = 0; k < nmiss && strcmp(miss[k], p->mtl); k++) {}
                if (k == nmiss && nmiss < 8) { strcpy(miss[nmiss++], p->mtl); logf("[world] material '%s' not found here (type %ld)\n", p->mtl, t); } }
        }
        if (!q->looked) { q->looked = 1; logf("[world] look applied to '%s': mtl='%s' skills=%d\n", q->type, p->mtl, p->nsk); }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
static void net_send_peer(const BYTE *wire, int len) {
    if (g_net_steam == 2) { if (g_st_peer) st_send_to(g_st_peer, wire, len); }
    else if (g_sock != INVALID_SOCKET && g_peer_known) sendto(g_sock, (const char *)wire, len, 0, (struct sockaddr *)&g_peer, sizeof g_peer);
}
// ---- Client -> host hit forwarding. The client's own robots are blocked, so its attacks hit nothing real. When a LowAtak_*
// task starts on the local player and a robot puppet is within reach, the client sends 'SHT1' (puppet id = host entity,
// damage). The host applies what the game's own attack code does to a robot (measured, [wskill] log): GBR_BOX skills
// s34 +1 (hit count), s35/s36 -~9 per damage point (health), s37 hurt state, s39 +damage (death at ~10); GBR_PLAZMA s39 too.
double __cdecl sqrt(double);
#define HIT_MAGIC 0x31544853 /* 'SHT1' */
#define HIT_Q_MAX 32
typedef struct { DWORD magic, seq, id; long dmg; } HITPKT;
static DWORD g_hit_seq; static HITPKT g_hit_q[HIT_Q_MAX]; static BYTE g_hit_left[HIT_Q_MAX];
static DWORD g_hit_due[HIT_Q_MAX];   // GetTickCount() when the queued hit may go out (0 / +50 / +120 ms aralıklı tekrar)
static DWORD g_hit_seen[32]; static int g_hit_seen_n; static DWORD g_hit_tx, g_hit_rx_n;
static void hit_enqueue(DWORD id, long dmg) {
    int slot;
    for (slot = 0; slot < HIT_Q_MAX && g_hit_left[slot]; slot++) {}
    if (slot < HIT_Q_MAX) {
        g_hit_q[slot].magic = HIT_MAGIC;
        g_hit_q[slot].seq = ++g_hit_seq ^ (GetTickCount() << 8);
        g_hit_q[slot].id = id;
        g_hit_q[slot].dmg = dmg;
        g_hit_left[slot] = 3;
        g_hit_due[slot] = GetTickCount();
    } else {
        static int full_logs;
        if (full_logs++ < 16) logf("[hit] tx queue full (max=%d), dropped id=%08lx dmg=%ld\n", HIT_Q_MAX, id, dmg);
    }
}
static void sk_add(ENTITY *e, int n, long delta, long lo) { __try { var *s = (var *)((BYTE *)e + 148) + (n - 1);   /* n = lite-C skillN (1-based, as in the [wskill] log) */ long v = *s + delta; *s = v < lo ? lo : v; } __except (EXCEPTION_EXECUTE_HANDLER) {} }
static long sk_get(ENTITY *e, int n) { __try { return ((var *)((BYTE *)e + 148))[n - 1]; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; } }
// ---- AIF_GBR_plazma: the game's own hit entry (lite-C, 2 args). Inline hook logs every real call (args, me, you, caller)
// and keeps the last host-side call as a template. Replay maps each role by entity type: GBR_* -> same type in the target
// robot's spawn group, SUPERCAN_BOX -> our player box, anything else as captured; sets me/you, runs it via engine_callscript4.
// Prologue 55 53 56 57 E8 00000000 (call $+5 = position-dependent): the stub re-executes the 4 pushes + pushes orig+9.
static ENTITY *player_box(void);
typedef struct { DWORD raw; char type[32]; long idx; } PZROLE;
static PZROLE g_pz[4];   // 0 arg0, 1 arg1, 2 me, 3 you
static int g_pz_have, g_pz_inreplay; static DWORD g_pz_logs, g_pz_calls; static BYTE *g_pz_fn; static DWORD g_pz_ret;
static void **g_pz_meslot, **g_pz_youslot;
static const char *g_pz_rn[4] = {"arg0", "arg1", "me", "you"};
static int is_entity(DWORD v) { ENTITY *e; if (v < 0x10000) return 0;
    for (e = safe_next(0); e && e != (ENTITY *)-1; e = safe_next(e)) if ((DWORD)e == v) return 1; return 0; }
static void pz_role(DWORD v, PZROLE *r) { ENTITY c; r->raw = v; r->type[0] = 0; r->idx = 0;
    if (is_entity(v) && safe_ent((ENTITY *)v, &c)) { safe_str(c.type, r->type, sizeof r->type); r->idx = c.link.index; } }
static void pz_slots(void) { long t = 0;
    if (!g_pz_meslot) g_pz_meslot = (void **)safe_getvar("me", &t);
    if (!g_pz_youslot) g_pz_youslot = (void **)safe_getvar("you", &t); }
static void script_where(DWORD a, char *out, int n) {   // "name+0xoff" for a lite-C code address (getscriptinfo returns the start, name in *o)
    void *o = 0, *st; char nm[48] = "";
    out[0] = 0; if (!p_engine_getscriptinfo || a < 0x10000) return;
    st = safe_scriptinfo((void *)a, &o);
    if (!st || st == (void *)-1 || !o || o == (void *)-1) { strcpy(out, "?"); return; }
    safe_str((char *)o, nm, sizeof nm); _snprintf(out, n, "%s+0x%lx", nm, a - (DWORD)st); out[n - 1] = 0;
}
#define PZH_N 2   // hooked lite-C functions: 0 = AIF_GBR_plazma (template for replay), 1 = AIF_GBR_yaralan (log only)
static const char *g_pzh_name[PZH_N] = {"AIF_GBR_plazma", "AIF_GBR_yaralan"};
static BYTE *g_pzh_fn[PZH_N]; static DWORD g_pzh_ret[PZH_N];
static void __stdcall pz_log(DWORD *sp, int which) {   // sp = esp at function entry: [0] return address, [1] arg0, [2] arg1
    PZROLE r[4]; char cn[64]; int i;
    g_pz_calls++; pz_slots();
    __try { pz_role(sp[1], &r[0]); pz_role(sp[2], &r[1]); } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    pz_role(g_pz_meslot ? (DWORD)safe_long(g_pz_meslot) : 0, &r[2]); pz_role(g_pz_youslot ? (DWORD)safe_long(g_pz_youslot) : 0, &r[3]);
    script_where(sp[0], cn, sizeof cn);
    if (which == 0 && !g_pz_inreplay) { memcpy(g_pz, r, sizeof g_pz); g_pz_have = 1; }
    if (g_pz_logs++ < 300) {
        logf("[pz] %s #%lu%s caller=%08lx (%s)\n", g_pzh_name[which], g_pz_calls, g_pz_inreplay ? " (REPLAY)" : "", sp[0], cn);
        for (i = 0; i < 4; i++) logf("[pz]   %-4s = %08lx %s idx=%08lx\n", g_pz_rn[i], r[i].raw, r[i].type[0] ? r[i].type : "(not an entity)", r[i].idx); }
}
__declspec(naked) static void stub_pz0(void) { __asm {
    pushfd
    pushad
    lea eax, [esp + 36]
    push 0
    push eax
    call pz_log
    popad
    popfd
    push ebp
    push ebx
    push esi
    push edi
    push dword ptr [g_pzh_ret]
    jmp dword ptr [g_pzh_ret]
} }
__declspec(naked) static void stub_pz1(void) { __asm {
    pushfd
    pushad
    lea eax, [esp + 36]
    push 1
    push eax
    call pz_log
    popad
    popfd
    push ebp
    push ebx
    push esi
    push edi
    push dword ptr [g_pzh_ret + 4]
    jmp dword ptr [g_pzh_ret + 4]
} }
static void pz_install(void) {   // lazily, once engine_getscript knows the functions; pz_hook=0 disables
    static int done[PZH_N], off = -1; static void *(__cdecl *p_getscript)(char *); static const BYTE pro[9] = {0x55,0x53,0x56,0x57,0xE8,0,0,0,0};
    static void *stubs[PZH_N] = {stub_pz0, stub_pz1};
    BYTE *a; DWORD old; int ok, k;
    if (g_frame % 60) return;
    if (off < 0) { char b[8]; off = cfg("COOP_PZ_HOOK", "pz_hook", b, sizeof b) && b[0] == '0'; }
    if (off) return;
    if (!p_getscript) p_getscript = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
    if (!p_getscript) return;
    for (k = 0; k < PZH_N; k++) {
        if (done[k]) continue;
        a = 0; __try { a = p_getscript((char *)g_pzh_name[k]); } __except (EXCEPTION_EXECUTE_HANDLER) { a = 0; }
        if (!a) continue;
        done[k] = 1; ok = 0;
        __try { ok = !memcmp(a, pro, 9); } __except (EXCEPTION_EXECUTE_HANDLER) { ok = 0; }
        if (!ok) { logf("[pz] %s at %p: prologue mismatch, not hooking\n", g_pzh_name[k], a); continue; }
        g_pzh_fn[k] = a; g_pzh_ret[k] = (DWORD)(a + 9);
        VirtualProtect(a, 9, PAGE_EXECUTE_READWRITE, &old);
        a[0] = 0xE9; *(DWORD *)(a + 1) = (DWORD)stubs[k] - (DWORD)(a + 5); memset(a + 5, 0x90, 4);
        VirtualProtect(a, 9, old, &old); FlushInstructionCache(GetCurrentProcess(), a, 9);
        logf("[pz] hooked %s at %p (byte at +9 = %02x)\n", g_pzh_name[k], a, a[9]);
        if (k == 0) g_pz_fn = a;
    }
}
// ---- Data breakpoints (debug registers on the game thread): `watchhp` logs every write to the nearest robot's GBR_BOX
// s34/s35/s37/s39 with the writing lite-C function and the script return addresses found on the stack. `watchoff` clears.
static long g_dbw_last[4]; static DWORD g_dbw_addr[4], g_dbw_hits, g_dbw_tid, g_dbw_lo, g_dbw_hi; static int g_dbw_n; static char g_dbw_lbl[4][8]; static PVOID g_dbw_veh;
static LONG CALLBACK dbw_veh(EXCEPTION_POINTERS *x) {
    CONTEXT *c = x->ContextRecord; DWORD d6 = c->Dr6; int i;
    if (x->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP || !(d6 & 0xF)) return EXCEPTION_CONTINUE_SEARCH;
    c->Dr6 = 0;
    for (i = 0; i < g_dbw_n; i++) if (d6 & (1 << i)) {
        char w[64], st[6][64]; int k = 0, j; DWORD *sp = (DWORD *)c->Esp, v; long val = 0;
        __try { val = *(long *)g_dbw_addr[i]; } __except (EXCEPTION_EXECUTE_HANDLER) {}
        if (val == g_dbw_last[i] || g_dbw_hits++ >= 200) { g_dbw_last[i] = val; continue; }   // only changes (AIF_GBR_kos rewrites s34 = 0 every frame)
        g_dbw_last[i] = val;
        script_where(c->Eip, w, sizeof w);
        for (j = 0; j < 0x400 && k < 6; j++) { __try { v = sp[j]; } __except (EXCEPTION_EXECUTE_HANDLER) { break; }
            if (v >= g_dbw_lo && v < g_dbw_hi) { script_where(v, st[k], 64); if (st[k][0] && st[k][0] != '?') k++; } }
        for (j = k; j < 6; j++) st[j][0] = 0;
        logf("[dbw] %s -> %.2f by eip=%08lx %s | stack: %s %s %s %s %s %s\n", g_dbw_lbl[i], VF(val), c->Eip, w, st[0], st[1], st[2], st[3], st[4], st[5]);
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}
static DWORD WINAPI dbw_setter(LPVOID unused) {   // runs on a helper thread: suspend the game thread, write its debug registers
    HANDLE h = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, g_dbw_tid); CONTEXT c; DWORD d7 = 0, ok; int i;
    if (!h) return 1;
    SuspendThread(h); memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_DEBUG_REGISTERS; GetThreadContext(h, &c);
    c.Dr0 = g_dbw_n > 0 ? g_dbw_addr[0] : 0; c.Dr1 = g_dbw_n > 1 ? g_dbw_addr[1] : 0;
    c.Dr2 = g_dbw_n > 2 ? g_dbw_addr[2] : 0; c.Dr3 = g_dbw_n > 3 ? g_dbw_addr[3] : 0;
    for (i = 0; i < g_dbw_n; i++) d7 |= (1u << (i * 2)) | (1u << (16 + i * 4)) | (3u << (18 + i * 4));   // local enable, break on write, 4 bytes
    c.Dr7 = d7; c.Dr6 = 0; ok = SetThreadContext(h, &c);
    ResumeThread(h); CloseHandle(h); return ok ? 0 : 2;
}
static void dbw_apply(void) {
    HANDLE t; DWORD rc = 99;
    g_dbw_tid = GetCurrentThreadId();
    if (!g_dbw_veh) g_dbw_veh = AddVectoredExceptionHandler(1, dbw_veh);
    if (g_pz_fn) { g_dbw_lo = (DWORD)g_pz_fn - 0x800000; g_dbw_hi = (DWORD)g_pz_fn + 0x800000; }
    t = CreateThread(0, 0, dbw_setter, 0, 0, 0);
    if (t) { WaitForSingleObject(t, 3000); GetExitCodeThread(t, &rc); CloseHandle(t); }
    logf("[dbw] %d watch(es) set, rc=%lu (0 = ok), code range %08lx..%08lx\n", g_dbw_n, rc, g_dbw_lo, g_dbw_hi);
}
static int pz_filter(EXCEPTION_POINTERS *x) {
    char nm[64]; void *ad = x->ExceptionRecord->ExceptionAddress;
    script_where((DWORD)ad, nm, sizeof nm);
    logf("[pz] FAULT code %08lx at %p (%s), access %08lx -- session tainted, restart before drawing conclusions\n",
         x->ExceptionRecord->ExceptionCode, ad, nm, x->ExceptionRecord->NumberParameters > 1 ? (DWORD)x->ExceptionRecord->ExceptionInformation[1] : 0);
    return EXCEPTION_EXECUTE_HANDLER;
}
static DWORD pz_subst(const PZROLE *r, ENTITY *box, int *bad) {   // GBR_BOX -> the target box; other GBR_* -> same type nearest to it
    ENTITY *e, c, bc, *best = 0; char ty[32]; double bd = 250.0 * 250.0;
    if (!r->type[0]) return r->raw;
    if (!_stricmp(r->type, "GBR_BOX.MDL")) return (DWORD)box;
    if (!_strnicmp(r->type, "GBR_", 4)) {
        if (safe_ent(box, &bc))
            for (e = safe_next(0); e && e != (ENTITY *)-1; e = safe_next(e)) {
                double d; if (!safe_ent(e, &c)) continue; safe_str(c.type, ty, sizeof ty);
                if (_stricmp(ty, r->type)) continue;
                d = (VF(c.x) - VF(bc.x)) * (VF(c.x) - VF(bc.x)) + (VF(c.y) - VF(bc.y)) * (VF(c.y) - VF(bc.y)) + (VF(c.z) - VF(bc.z)) * (VF(c.z) - VF(bc.z));
                if (d < bd) { bd = d; best = e; } }
        if (best) return (DWORD)best;
        logf("[pz] replay: no %s near box %p\n", r->type, box); *bad = 1; return 0;
    }
    if (!is_entity(r->raw) && !_stricmp(r->type, "SUPERCAN_BOX.MDL")) { ENTITY *b = player_box(); if (b) return (DWORD)b; }   // captured box first: player_box() can pick a puppet
    if (!is_entity(r->raw)) { logf("[pz] replay: captured %s %08lx is gone\n", r->type, r->raw); *bad = 1; return 0; }
    return r->raw;
}
static var (__cdecl *p_callscript4)(long, long, long, long, long);
static int pz_call(BYTE *fn, DWORD a0, DWORD a1, DWORD me, DWORD you) {   // run a script function with me/you set, restore after
    void *om = 0, *oy = 0; int ok = 1;
    if (!p_callscript4) p_callscript4 = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_callscript4");
    pz_slots(); if (!p_callscript4 || !g_pz_meslot || !g_pz_youslot) { logf("[pz] no engine_callscript4 / me / you slot\n"); return 0; }
    __try { om = *g_pz_meslot; oy = *g_pz_youslot; *g_pz_meslot = (void *)me; *g_pz_youslot = (void *)you; g_pz_inreplay = 1;
        p_callscript4((long)fn, (long)a0, (long)a1, 0, 0); }
    __except (pz_filter(GetExceptionInformation())) { ok = 0; }
    g_pz_inreplay = 0;
    __try { *g_pz_meslot = om; *g_pz_youslot = oy; } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return ok;
}
// Doğrudan box (GBR_BOX.MDL / ZBD_BOX.MDL) ile plazma/kıvılcım çağrısı (GBR_FIZIK.MDL üzerinde skill[21]=0 çökmesini önler).
static int pz_spark(ENTITY *box, var s37) {
    static void *(__cdecl *p_getscript)(char *);
    static BYTE *fn_gbr, *fn_zbd;
    long cls = sk_get(box, 38);   // skill[37]: 1<<10 = GBR, 2<<10 = ZBD
    var rate = (s37 >= (4 << 10) || s37 == 3482) ? (4 << 10) : (1 << 10);
    BYTE *fn = 0;
    if (cls != (1 << 10) && cls != (2 << 10)) return 0;
    if (!sk_get(box, 22) || !sk_get(box, 31)) return 0;   // skill[21] (plazma handle) ve skill[30] (govde handle) dolu olmalı
    if (!p_getscript) p_getscript = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
    if (cls == (1 << 10)) {
        if (!fn_gbr && p_getscript) __try { fn_gbr = (BYTE *)p_getscript("AIF_GBR_plazma"); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        fn = g_pz_fn ? g_pz_fn : fn_gbr;
    } else {
        if (!fn_zbd && p_getscript) __try { fn_zbd = (BYTE *)p_getscript("AIF_ZBD_plazma"); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        fn = fn_zbd;
    }
    if (!fn) return 0;
    return pz_call(fn, (DWORD)box, (DWORD)rate, (DWORD)player_box(), (DWORD)box);
}
// Host robot başına vuruş kuyruğu: AIF_GBR/ZBD_yaralan içinde anim_progress <= 50 iken hafif vuruş (s37 < 4) gelirse
// else dalı (081d7d21) her karede s35 = s36 yazarak hasar bayrağını siliyor. Robotun gerçek canı (skill[35] = s36)
// düşene kadar her karede s37 ve s35 = orig_s36 - 0.1 yazmaya devam ediyoruz.
#define HITQ_MAX 16
typedef struct { ENTITY *box; var s37, orig_s36; int active, frames; } HOSTHIT;
static HOSTHIT g_hq[HITQ_MAX]; static int g_nhq;
static void hit_host_drop(int idx) {
    if (idx < 0 || idx >= g_nhq) return;
    memmove(&g_hq[idx], &g_hq[idx + 1], (g_nhq - 1 - idx) * sizeof(HOSTHIT));
    g_nhq--;
}
static void hit_host_tick(void) {
    static long stamp;
    static void *(__cdecl *p_ptr_for_handle)(var);
    int i, j; ENTITY c; char ty[32];
    if (!p_ptr_for_handle) p_ptr_for_handle = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "ptr_for_handle");
    // 1) Aktif vuruşları kontrol et: gerçek can (s36 = skill[35]) düştüyse tamamlandı, düşmediyse bayrağı (s35 -= 0.1) yenile
    //    Not: Robot yere yuvarlandığında (AIF_GBR_yuvarlan / AIF_ZBD_yuvarlan) hasar box yerine ptr_for_handle(box->skill[27]) üzerinde dinlenir!
    for (i = 0; i < g_nhq; ) {
        HOSTHIT *h = &g_hq[i]; var cur_s36, g_s36 = 0; long cls; ENTITY *gov = 0;
        if (!h->active) { i++; continue; }
        if (!is_entity((DWORD)h->box) || !safe_ent(h->box, &c)) { hit_host_drop(i); continue; }
        safe_str(c.type, ty, sizeof ty);
        if (!is_enemy_box_type(ty)) { hit_host_drop(i); continue; }
        cur_s36 = sk_get(h->box, 36); cls = sk_get(h->box, 38);
        if (p_ptr_for_handle) {
            var h27 = sk_get(h->box, 28);   // skill[27]
            if (h27) {
                ENTITY *cand = 0;
                __try { cand = (ENTITY *)p_ptr_for_handle(h27); } __except (EXCEPTION_EXECUTE_HANDLER) { cand = 0; }
                if (cand && cand != h->box && is_entity((DWORD)cand)) { gov = cand; g_s36 = sk_get(gov, 36); }
            }
        }
        if (cur_s36 <= 0 || cur_s36 < h->orig_s36 || (gov && g_s36 > 0 && g_s36 < h->orig_s36) || (cls == (3 << 10) && h->frames >= 2) || h->frames > 40) {
            logf("[hit] host queue done on box %p (%s) s37=%.2f: s36 %.1f -> %.1f (gov=%.1f) after %d frame(s)\n",
                 h->box, ty, VF(h->s37), VF(h->orig_s36), VF(cur_s36), VF(g_s36), h->frames);
            hit_host_drop(i);
            continue;
        }
        h->frames++;
        __try {
            ((var *)((BYTE *)h->box + 148))[36] = h->s37;
            ((var *)((BYTE *)h->box + 148))[34] = cur_s36 - 102;
            if (gov && g_s36 > 0) {
                ((var *)((BYTE *)gov + 148))[36] = h->s37;
                ((var *)((BYTE *)gov + 148))[34] = g_s36 - 102;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        i++;
    }
    // 2) Aktif vuruşu olmayan her kutu için sıradaki bekleyen vuruşu başlat
    for (i = 0; i < g_nhq; i++) {
        HOSTHIT *h = &g_hq[i]; int busy = 0; var orig_s36, new_s39; ENTITY *gov = 0;
        if (h->active) continue;
        for (j = 0; j < g_nhq; j++) if (g_hq[j].active && g_hq[j].box == h->box) { busy = 1; break; }
        if (busy) continue;
        if (!is_entity((DWORD)h->box) || !safe_ent(h->box, &c)) { hit_host_drop(i--); continue; }
        safe_str(c.type, ty, sizeof ty);
        orig_s36 = sk_get(h->box, 36);
        if (orig_s36 <= 0) { hit_host_drop(i--); continue; }
        if (p_ptr_for_handle) {
            var h27 = sk_get(h->box, 28);   // skill[27]
            if (h27) {
                ENTITY *cand = 0;
                __try { cand = (ENTITY *)p_ptr_for_handle(h27); } __except (EXCEPTION_EXECUTE_HANDLER) { cand = 0; }
                if (cand && cand != h->box && is_entity((DWORD)cand)) gov = cand;
            }
        }
        h->active = 1; h->frames = 0; h->orig_s36 = orig_s36;
        new_s39 = (1000 + (++stamp)) << 10;
        __try {
            ((var *)((BYTE *)h->box + 148))[36] = h->s37;
            ((var *)((BYTE *)h->box + 148))[34] = orig_s36 - 102;
            ((var *)((BYTE *)h->box + 148))[38] = new_s39;
            if (gov) {
                var gs36 = ((var *)((BYTE *)gov + 148))[35];
                if (gs36 > 0) {
                    ((var *)((BYTE *)gov + 148))[36] = h->s37;
                    ((var *)((BYTE *)gov + 148))[34] = gs36 - 102;
                    ((var *)((BYTE *)gov + 148))[38] = new_s39;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        pz_spark(h->box, h->s37);
        logf("[hit] host hit active on box %p (%s): s37=%.2f s36=%.1f -> s35=%.2f gov=%p\n",
             h->box, ty, VF(h->s37), VF(orig_s36), VF(sk_get(h->box, 35)), gov);
    }
}
#define HIT_DMG_TRASH  (99 << 10)  /* HITPKT özel kodu: client bir çöp parçasını (cop_*) vakumladı */
#define HIT_DMG_WAVE   (100 << 10) /* HITPKT özel kodu: client haritada ilerleyip yeni bir robot dalga segmentini (id=seg) tetikledi */
#define HIT_DMG_MEDKIT (101 << 10) /* HITPKT özel kodu: client bir sağlık paketini (enrj_paket.mdl) aldı */
static long g_wave_seg_override = -1; static ENTITY *g_tsyc_box;
static void host_trigger_wave_seg(long seg) {   // camera_path_loop (97885..97919) dalga tetiklemesinin birebir karşılığı
    DWORD ebx = ebx_base();
    if (!ebx || seg < 0 || seg >= (64 << 10)) return;
    __try {
        long s = seg >> 10;
        long lvl = G_LVL_SLOT(ebx) >> 10;
        if (lvl < 0 || lvl >= 10) return;
        {   var *gbr_cell = (var *)((BYTE *)ebx + 0x26c970 + ((2 * s + 0) * 10 + lvl) * 4);
            var *zbd_cell = (var *)((BYTE *)ebx + 0x26c970 + ((2 * s + 1) * 10 + lvl) * 4);
            var *max_cell = (var *)((BYTE *)ebx + 0x26e8b4 + ((10 * s + 2) * 10 + lvl) * 4);
            var gbr = *gbr_cell, zbd = *zbd_cell, rmax = *max_cell;
            if (gbr > 0 || zbd > 0) {
                if (gbr > 0) { *gbr_cell = -gbr; G_GBR_QUEUE(ebx) += gbr; }
                if (zbd > 0) { *zbd_cell = -zbd; G_ZBD_QUEUE(ebx) += zbd; }
                if (rmax > G_ROBOT_MAX(ebx)) G_ROBOT_MAX(ebx) = rmax;
                g_wave_seg_override = seg;   // camera_path_loop her karede ezdiği için TASIYICI_BOX görevinde de korunur
                G_PATH_SEG(ebx) = (var)seg;   // AIF_TSYC_kos taşıyıcı dronu bu segmentin noktalarına robot bıraksın
                logf("[world] client triggered wave at seg=%ld (lvl=%ld): +gbr=%.0f +zbd=%.0f max=%.0f\n",
                     s, lvl, gbr > 0 ? VF(gbr) : 0.0, zbd > 0 ? VF(zbd) : 0.0, VF(G_ROBOT_MAX(ebx)));
            } else if (G_GBR_QUEUE(ebx) + G_ZBD_QUEUE(ebx) > 0 && seg > G_PATH_SEG(ebx)) {
                g_wave_seg_override = seg;
                G_PATH_SEG(ebx) = (var)seg;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
static int hit_apply(ENTITY *part, long s37) {   // part = vurulan düşman parçası, s37 = tam var vuruş kodu (0..5<<10 veya 3482)
    ENTITY *e, c, *box = 0; long pi; char ty[32]; double px, py, pz, bd = 250.0 * 250.0;
    if (!safe_ent(part, &c)) return 0;
    safe_str(c.type, ty, sizeof ty);
    if (s37 == HIT_DMG_TRASH) {
        DWORD ebx = ebx_base();
        if (!is_trash_type(ty)) return 0;
        __try {
            *(DWORD *)((BYTE *)part + 96) &= ~256u;
            ((var *)((BYTE *)part + 148))[25] = 2 << 10;   // cop_robot_fizik döngüsünü kırıp kendi metal_icon'uyla birlikte temizler
            if (ebx) G_TRASH_RBT(ebx) += 1 << 10;          // ortak robot çöpü sayacı SST2 ile geri düşmesin
            logf("[hit] client vacuumed trash '%s' %p -> skill[25]=2\n", ty, part);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        return 1;
    }
    if (s37 == HIT_DMG_MEDKIT) {
        if (!is_medkit_type(ty)) return 0;
        __try {
            *(DWORD *)((BYTE *)part + 96) &= ~256u;
            part->flags |= INVISIBLE_F | PASSABLE_F;
            ((var *)((BYTE *)part + 148))[25] = 2 << 10;
            logf("[medkit] client picked up '%s' %p -> hidden on host\n", ty, part);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        return 1;
    }
    pi = c.link.index; px = VF(c.x); py = VF(c.y); pz = VF(c.z);
    for (e = safe_next(0); e && e != (ENTITY *)-1; e = safe_next(e)) {
        double d; if (!safe_ent(e, &c)) continue; safe_str(c.type, ty, sizeof ty);
        if (!is_enemy_box_type(ty)) continue;
        d = (VF(c.x) - px) * (VF(c.x) - px) + (VF(c.y) - py) * (VF(c.y) - py) + (VF(c.z) - pz) * (VF(c.z) - pz);
        if (d < bd) { bd = d; box = e; }
    }
    if (!box) { logf("[hit] no enemy BOX within 250 of part idx=%08lx\n", pi); return 0; }
    if (g_nhq < HITQ_MAX) {
        HOSTHIT *h = &g_hq[g_nhq++];
        memset(h, 0, sizeof *h); h->box = box; h->s37 = (var)s37;
    }
    hit_host_tick();
    return 1;
}
static void hit_rx(const BYTE *buf, int len) {   // host: a client hit arrives (sent 3x: dedupe by seq)
    HITPKT h; int i;
    if (len != (int)sizeof h || !is_host()) return;
    memcpy(&h, buf, sizeof h);
    if (h.magic != HIT_MAGIC || (h.dmg != HIT_DMG_TRASH && h.dmg != HIT_DMG_WAVE && h.dmg != HIT_DMG_MEDKIT && (h.dmg < 0 || h.dmg > (8 << 10)))) return;
    for (i = 0; i < g_hit_seen_n && i < 32; i++) if (g_hit_seen[i] == h.seq) return;
    g_hit_seen[g_hit_seen_n++ % 32] = h.seq; g_hit_rx_n++;
    if (h.dmg == HIT_DMG_WAVE) { host_trigger_wave_seg((long)h.id); return; }
    if (world_index((ENTITY *)h.id) < 0) { logf("[hit] client hit on unknown part %08lx ignored\n", h.id); return; }
    hit_apply((ENTITY *)h.id, h.dmg);
}
static void hit_client_tick(void) {   // client: oyunun kendi kılıç c_trace'i kuklanın s39'unu (skill[38]) değiştirdiği anda 0 ms gecikmeyle gönder
    int i; DWORD ebx = ebx_base();
    for (i = 0; i < WMAX_PARTS; i++) {
        WPUP *q = &g_wp[i]; var s39, s37;
        if (!q->e) continue;
        if (is_trash_type(q->type)) {
            var s26 = sk_get(q->e, 26);   // skill[25]: pfcan_kutu_cek vakum tamamlanınca 2<<10 yazar
            if (s26 != q->last_s39) {
                q->last_s39 = s26;
                if (s26 == (2 << 10)) {
                    hit_enqueue(q->id, HIT_DMG_TRASH);
                    logf("[hit] client vacuumed trash puppet '%s' id=%08lx nano=%.0f\n",
                         q->type, q->id, ebx ? VF(safe_long((BYTE *)ebx + 0x26462c)) : -1.0);
                }
            }
            continue;
        }
        if (is_medkit_type(q->type)) {
            if (q->last_s39 == 1) {
                DWORD em = 0; long fl = 0;
                __try { em = *(DWORD *)((BYTE *)q->e + 96); fl = q->e->flags; } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
                if ((em & 256u) == 0 || (fl & INVISIBLE_F)) {
                    q->last_s39 = 2;
                    hit_enqueue(q->id, HIT_DMG_MEDKIT);
                    logf("[medkit] client took '%s' id=%08lx hp=%.0f -> notifying host\n",
                         q->type, q->id, ebx ? VF(G_PLAYER_HP(ebx)) : -1.0);
                }
            }
            continue;
        }
        if (!is_enemy_body_type(q->type)) continue;
        s39 = sk_get(q->e, 39);   // skill[38] = saldırı sayacı
        if (s39 != q->last_s39) {
            q->last_s39 = s39;
            if (s39 != 0) {
                s37 = sk_get(q->e, 37);   // skill[36] = s37 (0..5<<10 veya 3482)
                q->last_hit_frame = g_frame;
                __try { ((var *)((BYTE *)q->e + 148))[34] = 100 << 10; } __except (EXCEPTION_EXECUTE_HANDLER) {}
                q->last_s35 = 100 << 10;
                hit_enqueue(q->id, s37);
                logf("[hit] client c_trace hit puppet '%s' id=%08lx: s37=%.2f (raw=%ld) s39=%.0f nano=%.0f\n",
                     q->type, q->id, VF(s37), (long)s37, VF(s39), ebx ? VF(safe_long((BYTE *)ebx + 0x26462c)) : -1.0);
            }
        }
        // Taramalı taret (topgit_taramali) s39'a dokunmaz, yalnız skill[34] (s35) -= 0.1 ve skill[36] (s37) = 1<<10 yazar
        {   var s35 = sk_get(q->e, 35);
            if (s35 > 0 && s35 < (100 << 10) && s35 != q->last_s35) {
                s37 = sk_get(q->e, 37);
                __try { ((var *)((BYTE *)q->e + 148))[34] = 100 << 10; } __except (EXCEPTION_EXECUTE_HANDLER) {}
                q->last_s35 = 100 << 10;
                if ((g_frame - q->last_hit_frame) > 6 && s37 == (1 << 10)) {
                    hit_enqueue(q->id, 1 << 10);
                    logf("[hit] client turret hit puppet '%s' id=%08lx: s37=%.2f s35=%.2f\n",
                         q->type, q->id, VF(s37), VF(s35));
                }
            }
        }
    }
    {   DWORD now = GetTickCount();
        for (i = 0; i < HIT_Q_MAX; i++) if (g_hit_left[i] && (long)(now - g_hit_due[i]) >= 0) {
            net_send_peer((BYTE *)&g_hit_q[i], sizeof(HITPKT));
            g_hit_left[i]--;
            g_hit_due[i] = now + (g_hit_left[i] == 2 ? 50 : 70);
            g_hit_tx++;
        }
    }
}
#define WFLAG_WAVE_BUSY (1 << 30) /* host: G_GBR_QUEUE + G_ZBD_QUEUE + G_ROBOT_ALIVE > 0 (dalga devam ediyor) */
#define WFLAG_MUS_ALLOW (1 << 29) /* host: G_MUSIC_ALLOW (g_07f9f7c9) != 0 (savaş/spawn müziği izni) */
// Client: yerel taşıyıcının robot kuyruklarını (G_GBR_QUEUE, G_ZBD_QUEUE) sıfırla ve sahadaki canlı robot sayısını
// (G_ROBOT_ALIVE = g_07fa00ad) kukla robot varlığı + host dalga durumuyla senkronla.
static void world_client_sync_counters(void) {
    static int last_alive = -1;
    DWORD ebx = ebx_base(); int i, alive = 0, mus_allow = -1;
    if (!ebx) return;
    for (i = 0; i < WMAX_PARTS; i++) if (g_wp[i].e && is_enemy_body_type(g_wp[i].type)) alive++;
    if (g_nwsnap > 0) {
        const WPKT *pk = &WSNAPI(g_nwsnap - 1)->k;
        if (pk->count > 0) {
            if (!alive && (pk->part[0].flags & WFLAG_WAVE_BUSY)) alive = 1;   // host taşıyıcısında sırada bekleyen robot var
            mus_allow = (pk->part[0].flags & WFLAG_MUS_ALLOW) ? 1 : 0;
        }
    }
    __try {
        var gbr_q = G_GBR_QUEUE(ebx), zbd_q = G_ZBD_QUEUE(ebx);
        if (gbr_q > 0 || zbd_q > 0) {
            // Client haritada ilerleyip yerel camera_path_loop içinde yeni bir dalga bölgesini tetikledi: segmenti host'a bildir
            var seg = G_PATH_SEG(ebx);
            hit_enqueue((DWORD)seg, HIT_DMG_WAVE);
            logf("[world] client local wave triggered at seg=%.0f (gbr=%.0f zbd=%.0f) -> forwarding to host\n",
                 VF(seg), VF(gbr_q), VF(zbd_q));
            if (!alive) alive = 1;
        }
        G_GBR_QUEUE(ebx) = 0;
        G_ZBD_QUEUE(ebx) = 0;
        if (G_GAME_ACTIVE(ebx) == 0 || G_MUSIC_ALLOW(ebx) == 0) {
            // main_check_load (decomp.c:217523) ve ESC -> Ana Menüye Dön (decomp.c:51412..51420)
            // çıkış sırasında G_MUSIC_ALLOW=0 yapıp while(G_ROBOT_ALIVE > 0) wait(1) bekler!
            G_ROBOT_ALIVE(ebx) = 0;
            if (last_alive != 0) {
                logf("[world] client in load/menu-exit (game_active=%ld mus_allow=%ld) -> holding G_ROBOT_ALIVE=0\n",
                     G_GAME_ACTIVE(ebx) >> 10, G_MUSIC_ALLOW(ebx) >> 10);
                last_alive = 0;
            }
        } else if (g_nwsnap > 0) {
            G_ROBOT_ALIVE(ebx) = (var)(alive << 10);
            // NOT: G_MUSIC_ALLOW (g_07f9f7c9) oyunun "Ana Menüye Dön" çıkış durdurma bayrağıdır; ezilirse client menüye dönemez!
            if (alive != last_alive) {
                logf("[world] client robot counters synced: alive=%d (queues zeroed)\n", alive);
                last_alive = alive;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ---- 1) Düşmanların iki oyuncuyu da hedeflemesi + 2) Düşman saldırılarının client'a da hasar vermesi ('SPH1') ----
// Host'ta acknex görev zamanlayıcısının `me` değişkenini güncellediği 3 nokta (`0x10018d3d`, `0x10018cc9`, `0x10018ce9`)
// kancalanır. Aktif görev bir düşman kutusuysa (`GBR_BOX`, `ZBD_BOX`, `UCAN_BOX`) `G_PLAYER_BOX(ebx)` o robota daha yakın
// olan oyuncuya (host gerçek `SUPERCAN_BOX` veya client kukla kutusu `g_aim_pup`) yönlendirilir ve kukla kutu yalnız robot
// görevleri çalışırken katı + `FLAG2` yapılır (host kendi görevlerinde kuklaya takılmaz ve kılıcıyla kuklayı vurmaz).
#define PHIT_MAGIC 0x31485053 /* 'SPH1': host -> client oyuncu hasar paketi */
#define PHIT_Q_MAX 16
typedef struct { DWORD magic, seq; var s37, n, ax, ay, az, bx, by, bz, pan; } PHITPKT;
static DWORD g_phit_seq, g_phit_tx, g_phit_rx_n, g_phit_seen[32]; static int g_phit_seen_n;
static PHITPKT g_phit_q[PHIT_Q_MAX]; static BYTE g_phit_left[PHIT_Q_MAX]; static DWORD g_phit_due[PHIT_Q_MAX];
#define AIM_MAX 32
typedef struct { ENTITY *box; int target_client; } AIMRB;
static AIMRB g_aim[AIM_MAX]; static int g_naim;
static ENTITY *g_ebox_cand[AIM_MAX]; static int g_nebox_cand;
static ENTITY *g_aim_real, *g_aim_pup; static int g_aim_active;

static void aim_remove_pup(const char *why) {
    DWORD ebx = ebx_base();
    if (ebx && g_aim_real) __try { G_PLAYER_BOX(ebx) = g_aim_real; } __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (g_aim_pup) {
        __try { p_ent_remove(g_aim_pup); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        logf("[aim] removed client target box %p (%s)\n", g_aim_pup, why ? why : "");
        g_aim_pup = 0;
    }
    g_aim_active = 0;
    g_wave_seg_override = -1;
    g_tsyc_box = 0;
}
static void __stdcall sched_on_me(ENTITY *me) {
    DWORD ebx = g_ebx_base; int i, use_pup = 0;
    if (!ebx || (!g_aim_active && g_wave_seg_override < 0)) return;
    __try {
        if (g_wave_seg_override >= 0 && me && me == g_tsyc_box) {
            G_PATH_SEG(ebx) = (var)g_wave_seg_override;
        }
        if (g_aim_active && g_aim_real && g_aim_pup) {
            if (me) {
                for (i = 0; i < g_naim; i++) if (g_aim[i].box == me) { use_pup = g_aim[i].target_client; break; }
            }
            if (use_pup) {
                G_PLAYER_BOX(ebx) = g_aim_pup;
                g_aim_pup->flags = (g_aim_pup->flags & ~PASSABLE_F) | INVISIBLE_F | FLAG2_F;
            } else {
                G_PLAYER_BOX(ebx) = g_aim_real;
                g_aim_pup->flags = (g_aim_pup->flags & ~FLAG2_F) | INVISIBLE_F | PASSABLE_F;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
__declspec(naked) static void stub_sched_me_eax(void) { __asm {
    mov dword ptr ds:[0x1020c4e8], eax
    pushfd
    pushad
    push eax
    call sched_on_me
    popad
    popfd
    ret
} }
__declspec(naked) static void stub_sched_me_edx(void) { __asm {
    mov dword ptr ds:[0x1020c4e8], edx
    pushfd
    pushad
    push edx
    call sched_on_me
    popad
    popfd
    ret
} }
static void install_sched_hooks(HMODULE ack) {
    static int done;
    long t = 0; void *me_var;
    BYTE *b = (BYTE *)ack;
    // 10018d3d: A3 E8 C4 20 10       (mov [0x1020c4e8], eax — task resume)
    // 10018cc9: 89 15 E8 C4 20 10    (mov [0x1020c4e8], edx — callscript enter)
    // 10018ce9: A3 E8 C4 20 10       (mov [0x1020c4e8], eax — callscript leave)
    static const BYTE exp_eax[5] = {0xA3, 0xE8, 0xC4, 0x20, 0x10};
    static const BYTE exp_edx[6] = {0x89, 0x15, 0xE8, 0xC4, 0x20, 0x10};
    BYTE *p1 = b + 0x18d3d, *p2 = b + 0x18cc9, *p3 = b + 0x18ce9;
    DWORD old;
    if (done || !ack) return;
    done = 1;
    me_var = safe_getvar("me", &t);
    if (me_var != (void *)0x1020c4e8) {
        logf("[aim] me slot %p != 0x1020c4e8, skipping scheduler hooks\n", me_var);
        return;
    }
    __try {
        if (memcmp(p1, exp_eax, 5) || memcmp(p2, exp_edx, 6) || memcmp(p3, exp_eax, 5)) {
            logf("[aim] scheduler byte mismatch at %p/%p/%p, skipping\n", p1, p2, p3);
            return;
        }
        VirtualProtect(b + 0x18c80, 0x100, PAGE_EXECUTE_READWRITE, &old);
        p1[0] = 0xE8; *(DWORD *)(p1 + 1) = (DWORD)stub_sched_me_eax - (DWORD)(p1 + 5);
        p2[0] = 0xE8; *(DWORD *)(p2 + 1) = (DWORD)stub_sched_me_edx - (DWORD)(p2 + 5); p2[5] = 0x90;
        p3[0] = 0xE8; *(DWORD *)(p3 + 1) = (DWORD)stub_sched_me_eax - (DWORD)(p3 + 5);
        VirtualProtect(b + 0x18c80, 0x100, old, &old);
        FlushInstructionCache(GetCurrentProcess(), b + 0x18c80, 0x100);
        logf("[aim] installed acknex scheduler me hooks at %p, %p, %p\n", p1, p2, p3);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        logf("[aim] exception installing scheduler hooks\n");
    }
}
static double g_revive_invuln_until;
static void phit_rx(const BYTE *buf, int len) {   // client: host'taki bir robot bizim kuklamıza vurdu
    static DWORD diag_n;
    PHITPKT h; int i; DWORD ebx; ENTITY *pb;
    if (now_s() < g_revive_invuln_until) return;   // canlanma sonrası 3 sn dokunulmazlık
    if (len != (int)sizeof h || is_host()) {
        if (diag_n++ < 8) logf("[phit] drop len=%d (exp=%d) host=%d\n", len, (int)sizeof h, is_host());
        return;
    }
    memcpy(&h, buf, sizeof h);
    if (h.magic != PHIT_MAGIC || h.n <= 0 || h.n > 10) {
        if (diag_n++ < 8) logf("[phit] drop bad pkt magic=%08lx n=%ld\n", h.magic, (long)h.n);
        return;
    }
    for (i = 0; i < g_phit_seen_n && i < 32; i++) if (g_phit_seen[i] == h.seq) return;
    g_phit_seen[g_phit_seen_n++ % 32] = h.seq; g_phit_rx_n++;
    ebx = ebx_base(); pb = player_box();
    if (!ebx || !pb) {
        if (diag_n++ < 8) logf("[phit] drop ebx=%08lx pb=%p (local=%d)\n", ebx, pb, g_local_count);
        return;
    }
    __try {
        var *sk = (var *)((BYTE *)pb + 148);
        var s36 = sk[35];
        if (G_CTRL_ENABLED(ebx) == 0) {
            if (diag_n++ < 8) logf("[phit] drop during cinema/dialogue (G_CTRL_ENABLED=0)\n");
            return;   // diyalog / sinematik / terminal sırasında hasar alma
        }
        if (s36 <= (1 << 10)) {
            if (diag_n++ < 8) logf("[phit] drop low/zero s36=%.2f (raw=%ld) on %p\n", VF(s36), (long)s36, pb);
            return;   // zaten ölmek üzere / ölü
        }
        *(var *)((BYTE *)ebx + 0x261ed0) = h.ax;
        *(var *)((BYTE *)ebx + 0x261ed4) = h.ay;
        *(var *)((BYTE *)ebx + 0x261ed8) = h.az;
        *(var *)((BYTE *)ebx + 0x261ee0) = h.bx;
        *(var *)((BYTE *)ebx + 0x261ee4) = h.by;
        *(var *)((BYTE *)ebx + 0x261ee8) = h.bz;
        *(var *)((BYTE *)ebx + 0x261eec) = h.pan;
        sk[36] = h.s37;
        sk[34] = s36 - 102 * h.n;       // pfcan_mod_* döngüsü s35 < s36 görünce yaralan / yuvarlan / carpil başlatır
        logf("[aim] client took enemy hit: s37=%.1f n=%ld hp=%.1f (pb=%p)\n", VF(h.s37), (long)h.n, VF(s36), pb);
    } __except (EXCEPTION_EXECUTE_HANDLER) { logf("[phit] exception applying hit\n"); }
}
static void aim_host_tick(int in_level, int peer_active) {
    static void (__cdecl *p_c_setminmax)(ENTITY *);
    static void (__cdecl *p_c_updatehull)(ENTITY *, var);
    DWORD ebx = ebx_base(); int i, j; ENTITY rc, pc, *cpup = g_puppet[0]; var dz = 0;
    AIMRB next[AIM_MAX]; int nnext = 0;
    for (i = 0; i < MAX_PARTS; i++) {
        if (g_puppet[i] && !_stricmp(g_puppet_type[i], "SUPERCAN_GOVDE.MDL")) { cpup = g_puppet[i]; break; }
    }
    if (!in_level || !peer_active || !ebx || !cpup) {
        if (g_aim_pup || g_aim_active) aim_remove_pup("inactive");
        g_naim = 0;
        return;
    }
    // Gerçek oyuncu kutusunu öğren / doğrula
    __try {
        ENTITY *cur = G_PLAYER_BOX(ebx);
        if (cur && cur != g_aim_pup && is_entity((DWORD)cur)) g_aim_real = cur;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (!g_aim_real || !is_entity((DWORD)g_aim_real)) g_aim_real = player_box();
    if (!g_aim_real || !safe_ent(g_aim_real, &rc) || !safe_ent(cpup, &pc)) {
        if (g_aim_pup || g_aim_active) aim_remove_pup("no player box");
        return;
    }
    // Host'taki SUPERCAN_GOVDE -> SUPERCAN_BOX Z farkını ölçerek g_aim_pup'ı tam kutu merkezine hizala
    for (i = 0; i < g_local_count; i++) {
        ENTITY lc; char lty[32];
        if (!safe_ent(g_local_ent[i], &lc)) continue;
        safe_str(lc.type, lty, sizeof lty);
        if (!_stricmp(lty, "SUPERCAN_GOVDE.MDL")) { dz = rc.z - lc.z; break; }
    }
    // Client için hedef SUPERCAN_BOX.MDL kuklası oluştur / güncelle
    if (!g_aim_pup || !is_entity((DWORD)g_aim_pup)) {
        var pos[3] = { pc.x, pc.y, pc.z + dz }; ENTITY *ne = 0;
        g_aim_pup = 0;
        if (!safe_create("supercan_box.mdl", pos, &ne) || !ne) return;
        g_aim_pup = ne;
        if (!p_c_setminmax) p_c_setminmax = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "c_setminmax");
        if (!p_c_updatehull) p_c_updatehull = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "c_updatehull");
        __try {
            if (p_c_setminmax) p_c_setminmax(ne);
            memcpy((BYTE *)ne + 104, (BYTE *)g_aim_real + 104, 24);   // min_x..max_z sınırlayıcı kutu
            if (p_c_updatehull) p_c_updatehull(ne, 0);
            ne->flags = INVISIBLE_F | PASSABLE_F;
            ((var *)((BYTE *)ne + 148))[34] = 100 << 10;
            ((var *)((BYTE *)ne + 148))[35] = 100 << 10;
            ((var *)((BYTE *)ne + 148))[36] = 0;
            ((var *)((BYTE *)ne + 148))[37] = 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        logf("[aim] created client target box %p (real player box %p, body=%p, dz=%.1f)\n", ne, g_aim_real, cpup, VF(dz));
    }
    // Kukla kutuya bir önceki karede robot vuruşu geldi mi? (skill[34] < skill[35])
    __try {
        var *sk = (var *)((BYTE *)g_aim_pup + 148);
        var s35 = sk[34], s36 = sk[35];
        g_aim_pup->x = pc.x; g_aim_pup->y = pc.y; g_aim_pup->z = pc.z + dz;
        g_aim_pup->pan = pc.pan; g_aim_pup->tilt = 0; g_aim_pup->roll = 0;
        memcpy((BYTE *)g_aim_pup + 104, (BYTE *)g_aim_real + 104, 24);
        g_aim_pup->flags = (g_aim_pup->flags & ~FLAG2_F) | INVISIBLE_F | PASSABLE_F;
        G_PLAYER_BOX(ebx) = g_aim_real;   // tick sonunda ve robot dışı tüm görevlerde gerçek kutu seçili kalsın
        if (s35 < s36) {
            long diff = s36 - s35;
            var n = (var)((diff + 50) / 102);
            var s37 = sk[36];
            int slot;
            if (n < 1) n = 1; else if (n > 5) n = 5;
            sk[34] = 100 << 10; sk[35] = 100 << 10; sk[37] = 0;
            for (slot = 0; slot < PHIT_Q_MAX && g_phit_left[slot]; slot++) {}
            if (slot < PHIT_Q_MAX) {
                PHITPKT *p = &g_phit_q[slot];
                p->magic = PHIT_MAGIC;
                p->seq   = ++g_phit_seq ^ (GetTickCount() << 8);
                p->s37   = s37;
                p->n     = n;
                p->ax    = *(var *)((BYTE *)ebx + 0x261ed0);
                p->ay    = *(var *)((BYTE *)ebx + 0x261ed4);
                p->az    = *(var *)((BYTE *)ebx + 0x261ed8);
                p->bx    = *(var *)((BYTE *)ebx + 0x261ee0);
                p->by    = *(var *)((BYTE *)ebx + 0x261ee4);
                p->bz    = *(var *)((BYTE *)ebx + 0x261ee8);
                p->pan   = *(var *)((BYTE *)ebx + 0x261eec);
                g_phit_left[slot] = 3;
                g_phit_due[slot] = GetTickCount();
            }
            logf("[aim] host enemy hit client puppet: s37=%.1f n=%ld (diff=%ld)\n", VF(s37), (long)n, diff);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    {   DWORD now = GetTickCount();
        for (i = 0; i < PHIT_Q_MAX; i++) if (g_phit_left[i] && (long)(now - g_phit_due[i]) >= 0) {
            net_send_peer((BYTE *)&g_phit_q[i], sizeof(PHITPKT));
            g_phit_left[i]--;
            g_phit_due[i] = now + (g_phit_left[i] == 2 ? 50 : 70);
            g_phit_tx++;
        }
    }
    // Her robot kutusu (ve varsa Deve Boss kutusu + AIF_DEVE_* AI'ının me varlığı olan FISH.MDL) için daha yakın oyuncuyu seç (%20 histerezis)
    {   ENTITY *deve = 0, dc; int deve_tc = -1;
        __try { deve = G_DEVE_BOX(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { deve = 0; }
        if (deve && safe_ent(deve, &dc) && g_nebox_cand < AIM_MAX) {
            for (i = 0; i < g_nebox_cand && g_ebox_cand[i] != deve; i++) {}
            if (i == g_nebox_cand) g_ebox_cand[g_nebox_cand++] = deve;
        }
        {   double hx = VF(rc.x), hy = VF(rc.y), cx = VF(pc.x), cy = VF(pc.y);
            long h_dead = sk_get(g_aim_real, 36) <= (1 << 10);
            for (i = 0; i < g_nebox_cand && nnext < AIM_MAX; i++) {
                ENTITY *eb = g_ebox_cand[i], ec; double rx, ry, dh, dc2; int prev = 0, tc;
                if (!safe_ent(eb, &ec)) continue;
                rx = VF(ec.x); ry = VF(ec.y);
                dh = (rx - hx) * (rx - hx) + (ry - hy) * (ry - hy);
                dc2 = (rx - cx) * (rx - cx) + (ry - cy) * (ry - cy);
                for (j = 0; j < g_naim; j++) if (g_aim[j].box == eb) { prev = g_aim[j].target_client; break; }
                if (h_dead) tc = 1;
                else if (prev) tc = (dc2 <= dh * 1.44);   // 1.2^2: client'ı hedefliyorsa host %20 daha yakın olana dek koru
                else           tc = (dc2 * 1.44 < dh);    // host'u hedefliyorsa client %20 daha yakın olana dek koru
                if (eb == deve) deve_tc = tc;
                next[nnext].box = eb; next[nnext].target_client = tc; nnext++;
            }
            // AIF_DEVE_kos/bekle/doldur/ziplasaldir/deve_silah_cevir görevlerinin me pointer'ı AIF_Hangar_dummy1/2 (FISH.MDL) olduğu için
            // boss aktifken (G_HANGAR_STAGE >= 2) aynı Deve hedef kararını FISH.MDL'ye de bağla ki Deve client'a da dönsün/saldırsın!
            if (deve_tc >= 0 && g_boss_dummy_ent && nnext < AIM_MAX) {
                var hst = 0; ENTITY fc;
                __try { hst = G_HANGAR_STAGE(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { hst = 0; }
                if (hst >= (2 << 10) && safe_ent(g_boss_dummy_ent, &fc)) {
                    next[nnext].box = g_boss_dummy_ent; next[nnext].target_client = deve_tc; nnext++;
                }
            }
        }
    }
    memcpy(g_aim, next, nnext * sizeof(AIMRB)); g_naim = nnext;
    g_aim_active = 1;
    // Client tarafından tetiklenen dalga varsa taşıyıcı dron (TASIYICI_BOX.MDL) kuyruğu bitene dek g_tsyc_box'ı takip et
    if (g_wave_seg_override >= 0) {
        var qtot = 0;
        __try { qtot = G_GBR_QUEUE(ebx) + G_ZBD_QUEUE(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { qtot = 0; }
        if (qtot <= 0) {
            g_wave_seg_override = -1;
        } else if (!g_tsyc_box || !is_entity((DWORD)g_tsyc_box)) {
            ENTITY *e, ec; char ty[32]; g_tsyc_box = 0;
            for (e = safe_next(0); e && e != (ENTITY *)-1; e = safe_next(e)) {
                if (!safe_ent(e, &ec)) continue;
                safe_str(ec.type, ty, sizeof ty);
                if (!_stricmp(ty, "TASIYICI_BOX.MDL")) { g_tsyc_box = e; break; }
            }
        }
    }
}
#define MKR_MAGIC 0x324B4D53 /* 'SMK2': makara/kol/köprü/kapı aşama durumu (G_MKR_STATE = g_07fa256d: 0->1->2) */
typedef struct { DWORD magic, seq; long state, dummy; } MKRPKT;
static long g_mkr_state_last = -1, g_mkr_rx_state = -1;
static int g_mkr_tx_left;
static DWORD g_mkr_seq;
static MKRPKT g_mkr_pkt;
static char g_mkr_level[64];

static void mkr_collect_ent(ENTITY *e) {
    __try {
        var s20 = *(var *)((BYTE *)e + 148 + 19 * 4);
        long id = s20 >> 10;
        if (id >= 100 && id <= 106 && (id << 10) == s20) g_mkr_ent[id - 100] = e;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static void mkr_apply_state(long st) {
    static void *(__cdecl *p_gs)(char *);
    static void (__cdecl *p_hull)(ENTITY *, var);
    DWORD ebx = ebx_base(); void *fn; int i;
    if (!ebx || st < 0 || st > 2 || !g_mkr_ent[0]) return;
    if (!p_gs) p_gs = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
    if (!p_hull) p_hull = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "c_updatehull");
    __try { G_MKR_STATE(ebx) = (var)(st << 10); } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    fn = p_gs ? p_gs("main_mkr_reset") : 0;
    if (fn) __try { ((void (__cdecl *)(void))fn)(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (p_hull) for (i = 0; i < 7; i++) if (g_mkr_ent[i]) __try { p_hull(g_mkr_ent[i], 0); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    g_mkr_state_last = st;
    logf("[mkr] applied G_MKR_STATE=%ld via main_mkr_reset (%p)\n", st, fn);
}

static void mkr_rx(const BYTE *buf, int len) {
    MKRPKT p; DWORD ebx = ebx_base(); long cur = 0;
    if (len < (int)sizeof(MKRPKT) || !ebx || !g_mkr_ent[0]) return;
    memcpy(&p, buf, sizeof p);
    if (p.magic != MKR_MAGIC || p.state < 0 || p.state > 2) return;
    __try { cur = G_MKR_STATE(ebx) >> 10; } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (p.state > cur && p.state > g_mkr_rx_state) {
        g_mkr_rx_state = p.state;
        mkr_apply_state(p.state);
    }
}

static void mkr_tick(const char *level, int in_level, int peer_active) {
    DWORD ebx = ebx_base(); long cur = 0;
    if (strcmp(g_mkr_level, level)) {
        strncpy(g_mkr_level, level, sizeof g_mkr_level - 1); g_mkr_level[sizeof g_mkr_level - 1] = 0;
        g_mkr_state_last = g_mkr_rx_state = -1;
        g_mkr_tx_left = 0; g_mkr_seq = 0;
    }
    if (!in_level || !peer_active || !ebx || !g_mkr_ent[0]) return;
    __try { cur = G_MKR_STATE(ebx) >> 10; } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (cur < 0 || cur > 2) return;
    if (g_mkr_state_last < 0) { g_mkr_state_last = cur; return; }
    if (cur > g_mkr_state_last) {
        g_mkr_state_last = cur;
        g_mkr_pkt.magic = MKR_MAGIC;
        g_mkr_pkt.seq = ++g_mkr_seq;
        g_mkr_pkt.state = cur;
        g_mkr_pkt.dummy = 0;
        g_mkr_tx_left = 3;
        logf("[mkr] local G_MKR_STATE -> %ld, sending to peer\n", cur);
    }
    if (g_mkr_tx_left > 0) {
        net_send_peer((BYTE *)&g_mkr_pkt, sizeof(MKRPKT));
        g_mkr_tx_left--;
    }
}

// Alarm / Lazer / Silo + 4 Hack Terminali + Geri Sayım Sayacı Senkronu ('SAL2')
#define ALRM_MAGIC 0x324C4153 /* 'SAL2' */
typedef struct { DWORD magic, seq; long state; var alet_st[4], timer; BYTE from_host, pad[3]; } ALRMPKT;
static long g_alrm_state_last = -1, g_alrm_rx_state = -1;
static var g_alrm_alet_last[4] = {-1, -1, -1, -1};
static int g_alrm_tx_left;
static DWORD g_alrm_seq;
static double g_alrm_next_timer_tx;
static ALRMPKT g_alrm_pkt;
static char g_alrm_level[64];

static int alrm_has_silo(DWORD ebx) {
    ENTITY *silo = 0, c; char cur_lvl[64];
    if (!ebx) return 0;
    current_level(cur_lvl, sizeof cur_lvl);
    if (_stricmp(cur_lvl, "orman.wmb")) return 0;   // silo yalnız orman.wmb'dedir (tunel.wmb'de eski pointer'la main_alarm_reset çalışmasın)
    __try { silo = G_ALRM_SILO(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return silo && safe_ent(silo, &c);
}

static void alrm_apply_state(long st) {
    static void *(__cdecl *p_gs)(char *);
    DWORD ebx = ebx_base(); void *fn = 0; long old_st = 0; int called_reset = 0;
    if (!ebx || st < 0 || st > 3 || !alrm_has_silo(ebx)) return;
    if (!p_gs) p_gs = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
    __try {
        old_st = G_ALRM_STATE(ebx) >> 10;
        G_ALRM_STATE(ebx) = (var)(st << 10);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if ((old_st == 0 && st >= 1) || (old_st < 3 && st == 3)) {
        if (st == 3) {
            ENTITY *silo = 0, *kap = 0, sc, kc;
            __try { silo = G_ALRM_SILO(ebx); kap = G_ALRM_KAPAK(ebx); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            if (silo && safe_ent(silo, &sc)) __try { *(DWORD *)((BYTE *)silo + 96) |= 256u; } __except (EXCEPTION_EXECUTE_HANDLER) {}
            if (kap && safe_ent(kap, &kc)) __try { kap->flags |= INVISIBLE_F | PASSABLE_F; } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        fn = p_gs ? p_gs("main_alarm_reset") : 0;
        if (fn && old_st == 0 && st >= 1) { called_reset = 1; __try { ((void (__cdecl *)(void))fn)(); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
    }
    g_alrm_state_last = st;
    logf("[alrm] applied G_ALRM_STATE %ld -> %ld (reset=%d fn=%p)\n", old_st, st, called_reset, fn);
}

static void alrm_rx(const BYTE *buf, int len) {
    ALRMPKT p; DWORD ebx = ebx_base(); long cur = 0; int i;
    if (len < (int)sizeof(ALRMPKT) || !ebx || !alrm_has_silo(ebx)) return;
    memcpy(&p, buf, sizeof p);
    if (p.magic != ALRM_MAGIC || p.state < 0 || p.state > 3) return;
    __try { cur = G_ALRM_STATE(ebx) >> 10; } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (p.state > cur && p.state > g_alrm_rx_state) {
        g_alrm_rx_state = p.state;
        alrm_apply_state(p.state);
    }
    // 4 hack terminali (g_07fa4dc9[0..3]): karşı taraf bir terminali hack'lediyse burada da kapat + lazer ışınını gizle + kapalı animasyon
    for (i = 0; i < 4; i++) {
        var cur_a = 0;
        __try { cur_a = G_ALRM_ALET_ST(ebx, i); } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
        if (p.alet_st[i] > cur_a) {
            ENTITY *ae = 0, *ie = 0, ac, ic;
            __try {
                G_ALRM_ALET_ST(ebx, i) = p.alet_st[i];
                ae = G_ALRM_ALET_ENT(ebx, i);
                ie = G_ALRM_ISIN_ENT(ebx, i);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
            if (p.alet_st[i] >= (2 << 10)) {
                if (ae && safe_ent(ae, &ac)) __try {
                    *(DWORD *)((BYTE *)ae + 96) &= ~256u;
                    if (t_ent_animate) t_ent_animate(ae, "kapali", 100 << 10, 0);
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
                if (ie && safe_ent(ie, &ic)) __try { ie->flags |= INVISIBLE_F | PASSABLE_F; } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
            g_alrm_alet_last[i] = p.alet_st[i];
            logf("[alrm] rx hack terminal[%d] %.0f -> %.0f\n", i, VF(cur_a), VF(p.alet_st[i]));
        }
    }
    // Alarm geri sayım sayacı (host otoriteli, fark > 60 birimse eşitle)
    if (p.from_host && !is_host() && p.timer > 0) {
        __try {
            var cur_t = G_ALRM_TIMER(ebx);
            if (cur_t > 0 && ABSF(VF(cur_t) - VF(p.timer)) > 60.0) G_ALRM_TIMER(ebx) = p.timer;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}

static void alrm_tick(const char *level, int in_level, int peer_active) {
    DWORD ebx = ebx_base(); long cur = 0; int i, chg = 0; var alet[4] = {0, 0, 0, 0}, tmr = 0; double t = now_s();
    if (strcmp(g_alrm_level, level)) {
        strncpy(g_alrm_level, level, sizeof g_alrm_level - 1); g_alrm_level[sizeof g_alrm_level - 1] = 0;
        g_alrm_state_last = g_alrm_rx_state = -1;
        for (i = 0; i < 4; i++) g_alrm_alet_last[i] = -1;
        g_alrm_tx_left = 0; g_alrm_seq = 0; g_alrm_next_timer_tx = 0;
    }
    if (!in_level || !peer_active || !ebx || !alrm_has_silo(ebx)) return;
    __try {
        cur = G_ALRM_STATE(ebx) >> 10;
        for (i = 0; i < 4; i++) alet[i] = G_ALRM_ALET_ST(ebx, i);
        tmr = G_ALRM_TIMER(ebx);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (cur < 0 || cur > 3) return;
    if (g_alrm_state_last < 0) {
        g_alrm_state_last = cur;
        for (i = 0; i < 4; i++) g_alrm_alet_last[i] = alet[i];
        logf("[alrm] silo active on '%s', initial G_ALRM_STATE=%ld\n", level, cur);
        return;
    }
    if (cur > g_alrm_state_last) { g_alrm_state_last = cur; chg = 1; }
    for (i = 0; i < 4; i++) if (alet[i] > g_alrm_alet_last[i]) { g_alrm_alet_last[i] = alet[i]; chg = 1; }
    if (chg || (is_host() && cur >= 1 && cur < 3 && tmr > 0 && t >= g_alrm_next_timer_tx)) {
        memset(&g_alrm_pkt, 0, sizeof g_alrm_pkt);
        g_alrm_pkt.magic = ALRM_MAGIC;
        g_alrm_pkt.seq = ++g_alrm_seq;
        g_alrm_pkt.state = cur;
        for (i = 0; i < 4; i++) g_alrm_pkt.alet_st[i] = alet[i];
        g_alrm_pkt.timer = tmr;
        g_alrm_pkt.from_host = (BYTE)(is_host() != 0);
        if (chg) {
            g_alrm_tx_left = 3;
            logf("[alrm] local state=%ld alet=(%.0f,%.0f,%.0f,%.0f) -> sending to peer\n",
                 cur, VF(alet[0]), VF(alet[1]), VF(alet[2]), VF(alet[3]));
        } else {
            net_send_peer((BYTE *)&g_alrm_pkt, sizeof(ALRMPKT));
        }
        g_alrm_next_timer_tx = t + 0.5;
    }
    if (g_alrm_tx_left > 0) {
        net_send_peer((BYTE *)&g_alrm_pkt, sizeof(ALRMPKT));
        g_alrm_tx_left--;
    }
}
