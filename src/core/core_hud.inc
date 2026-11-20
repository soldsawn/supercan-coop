// [CORE] Acknex native Co-op HUD (SHD2), F5 isinlanma, F8 Steam Lobi & Davet paneli, F9 gizle

// Co-op HUD / Overlay veri senkronu ('SHD2') + oturum kimliği (run_id) + baş üstü isim + yön oku + F5 ışınlanma
#define HUD_MAGIC 0x32444853 /* 'SHD2' */
#define HUD_F_INLEVEL  1
#define HUD_F_CUTSCENE 2
#define HUD_F_INVULN   4
#pragma pack(push, 1)
typedef struct {
    DWORD magic, run_id, tx_ms, echo_ms;
    short hp, nano;
    BYTE flags, pad;
    char name[24];
} HUDPKT;
#pragma pack(pop)

typedef struct {
    int enabled, in_level, connected, same_level, is_host;
    int ever_connected, disconn_sec;
    int rtt_ms;
    int peer_hp, peer_nano, peer_in_level, peer_cutscene, peer_invuln;
    float local_invuln_rem;
    char peer_name[24];
    int head_valid, head_on_screen;
    float head_x, head_y, dist_m;
    int off_dir;   // 0=sol, 1=sağ, 2=üst, 3=alt
    float off_x, off_y;
    char toast[64];
    double toast_until;
} HUDSTATE;

static HUDSTATE g_hud;
static int g_hud_init_cfg = -1, g_hud_ever_conn, g_hud_was_lost5;
static DWORD g_hud_peer_tx_ms, g_hud_last_rx_ms;
static double g_hud_rtt_f = -1.0, g_hud_next_tx, g_hud_tp_cd;
static int g_f5_prev;

static void hud_ascii_copy(const char *src, char *dst, int cap) {
    int o = 0; const unsigned char *s = (const unsigned char *)src;
    if (!dst || cap <= 0) return;
    if (!src) { dst[0] = 0; return; }
    while (*s && o < cap - 1) {
        unsigned char c = *s++;
        if (c == 0xC3 && *s) {   // UTF-8 Türkçe harfler
            unsigned char n = *s++;
            if (n == 0x87 || n == 0xA7) c = 'C';
            else if (n == 0x96 || n == 0xB6) c = 'O';
            else if (n == 0x9C || n == 0xBC) c = 'U';
            else c = '?';
        } else if (c == 0xC4 && *s) {
            unsigned char n = *s++;
            if (n == 0x9E || n == 0x9F) c = 'G';
            else if (n == 0xB0 || n == 0xB1) c = 'I';
            else c = '?';
        } else if (c == 0xC5 && *s) {
            unsigned char n = *s++;
            if (n == 0x9E || n == 0x9F) c = 'S';
            else c = '?';
        } else if (c >= 0x80) {   // CP1254 yedek
            if (c == 0xC7 || c == 0xE7) c = 'C';
            else if (c == 0xD0 || c == 0xF0) c = 'G';
            else if (c == 0xDD || c == 0xFD) c = 'I';
            else if (c == 0xD6 || c == 0xF6) c = 'O';
            else if (c == 0xDE || c == 0xFE) c = 'S';
            else if (c == 0xDC || c == 0xFC) c = 'U';
            else c = ' ';
        }
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 'A');
        if (c < 32 || c > 126) c = ' ';
        dst[o++] = (char)c;
    }
    dst[o] = 0;
}

static void hud_toast(const char *msg, double sec) {
    hud_ascii_copy(msg, g_hud.toast, sizeof g_hud.toast);
    g_hud.toast_until = now_s() + sec;
}

static void hud_local_name(char *out, int cap) {
    char b[64];
    if (cfg("COOP_NAME", "name", b, sizeof b) && b[0]) { hud_ascii_copy(b, out, cap); return; }
    if (g_net_steam == 2 && st_GetPersonaName && g_st_friends) {
        const char *sn = st_GetPersonaName(g_st_friends);
        if (sn && sn[0]) { hud_ascii_copy(sn, out, cap); return; }
    }
    hud_ascii_copy(is_host() ? "HOST" : "CLIENT", out, cap);
}

static void hud_rx(const BYTE *buf, int len) {
    HUDPKT p; DWORD now = GetTickCount();
    if (len < (int)sizeof(HUDPKT)) return;
    memcpy(&p, buf, sizeof p);
    if (p.magic != HUD_MAGIC) return;
    p.name[sizeof p.name - 1] = 0;
    if (p.run_id && p.run_id != g_peer_run_id) {
        DWORD old_rid = g_peer_run_id;
        g_peer_run_id = p.run_id;
        net_on_peer_run_id(p.run_id, old_rid);
    }
    g_hud_peer_tx_ms = p.tx_ms;
    g_hud_last_rx_ms = now;
    if (p.echo_ms) {
        long rtt = (long)(now - p.echo_ms);
        if (rtt < 0) rtt = 0;
        if (rtt < 2000) {
            if (g_hud_rtt_f < 0) g_hud_rtt_f = (double)rtt;
            else g_hud_rtt_f = g_hud_rtt_f * 0.7 + (double)rtt * 0.3;
            g_hud.rtt_ms = (int)(g_hud_rtt_f + 0.5);
        }
    }
    g_hud.peer_hp = p.hp < 0 ? 0 : (p.hp > 100 ? 100 : p.hp);
    g_hud.peer_nano = p.nano < 0 ? 0 : (p.nano > 100 ? 100 : p.nano);
    g_hud.peer_in_level = (p.flags & HUD_F_INLEVEL) != 0;
    g_hud.peer_cutscene = (p.flags & HUD_F_CUTSCENE) != 0;
    g_hud.peer_invuln = (p.flags & HUD_F_INVULN) != 0;
    if (p.name[0]) hud_ascii_copy(p.name, g_hud.peer_name, sizeof g_hud.peer_name);
}

typedef var (__cdecl *fn_draw_quad_t)(void *, var *, var *, var *, var *, var *, var, var);
typedef var (__cdecl *fn_draw_text_t)(char *, var, var, var *);

static void hud_rect(fn_draw_quad_t q, float x, float y, float w, float h, var *col, int alpha) {
    var p[3], s[3];
    if (w <= 0.0f || h <= 0.0f) return;
    p[0] = (var)(x * 1024.0f); p[1] = (var)(y * 1024.0f); p[2] = 0;
    s[0] = (var)(w * 1024.0f); s[1] = (var)(h * 1024.0f); s[2] = 0;
    q(0, p, 0, s, 0, col, alpha << 10, 0);
}

static void hud_frame(fn_draw_quad_t q, float x, float y, float w, float h, var *col, int alpha, float t) {
    hud_rect(q, x, y, w, t, col, alpha);
    hud_rect(q, x, y + h - t, w, t, col, alpha);
    hud_rect(q, x, y + t, t, h - t * 2.0f, col, alpha);
    hud_rect(q, x + w - t, y + t, t, h - t * 2.0f, col, alpha);
}

static void hud_text_s(fn_draw_text_t dt, const char *txt, float x, float y, var *col) {
    var shd[3] = {8 << 10, 8 << 10, 12 << 10};
    dt((char *)txt, (var)((x + 2.0f) * 1024.0f), (var)((y + 2.0f) * 1024.0f), shd);
    dt((char *)txt, (var)(x * 1024.0f), (var)(y * 1024.0f), col);
}

static void hud_bar(fn_draw_quad_t q, float x, float y, float w, float h, int pct, var *col_fill, var *col_bg) {
    var c_hi[3] = {255 << 10, 255 << 10, 255 << 10};
    var c_sep[3] = {18 << 10, 14 << 10, 12 << 10};
    var c_frm[3] = {95 << 10, 85 << 10, 75 << 10};
    int i;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    hud_rect(q, x, y, w, h, col_bg, 88);
    if (pct > 0) {
        float fw = w * (pct / 100.0f);
        hud_rect(q, x, y, fw, h, col_fill, 95);
        hud_rect(q, x, y, fw, h * 0.36f, c_hi, 24);   // cam/parlak üst şerit
    }
    for (i = 1; i <= 3; i++) hud_rect(q, x + w * (i * 0.25f), y + 1.0f, 1.5f, h - 2.0f, c_sep, 55);
    hud_frame(q, x, y, w, h, c_frm, 65, 1.0f);
}

static void hud_ping_bars(fn_draw_quad_t q, float x, float y, int rtt_ms) {
    var c_off[3] = {65 << 10, 60 << 10, 58 << 10};
    var c_grn[3] = {90 << 10, 235 << 10, 85 << 10};
    var c_yel[3] = {65 << 10, 220 << 10, 255 << 10};
    var c_red[3] = {75 << 10, 75 << 10, 255 << 10};
    int lvl = rtt_ms < 0 ? 0 : (rtt_ms < 50 ? 4 : (rtt_ms < 100 ? 3 : (rtt_ms < 160 ? 2 : 1)));
    var *col = lvl >= 3 ? c_grn : (lvl == 2 ? c_yel : c_red);
    int i;
    for (i = 0; i < 4; i++) {
        float bh = 5.0f + i * 3.5f;
        hud_rect(q, x + i * 6.0f, y + (16.0f - bh), 4.0f, bh, (i < lvl) ? col : c_off, (i < lvl) ? 95 : 55);
    }
}

static void hud_tick(const char *level, int in_level, int peer_active) {
    static var *(__cdecl *p_vec_to_screen)(var *, void *);
    static var (__cdecl *pX_setpos)(ENTITY *, var *);
    DWORD ebx = ebx_base(); double t = now_s(); int i;
    if (g_hud_init_cfg < 0) {
        char b[8];
        g_hud_init_cfg = !(cfg("COOP_HUD", "hud", b, sizeof b) && b[0] == '0');
        g_hud.enabled = g_hud_init_cfg;
        strcpy(g_hud.peer_name, "ARKADAS");
    }
    g_hud.in_level = in_level;
    g_hud.is_host = is_host();
    g_hud.connected = g_have_remote && (GetTickCount() - g_last_rx_tick < 2500);
    g_hud.same_level = peer_active;
    g_hud.local_invuln_rem = (t < g_revive_invuln_until) ? (float)(g_revive_invuln_until - t) : 0.0f;
    {   DWORD since_rx = g_have_remote ? (GetTickCount() - g_last_rx_tick) : 0;
        if (g_hud.connected) {
            if (g_hud_was_lost5) {
                hud_toast("YENIDEN BAGLANDI", 3.0);
                logf("[net] peer reconnected after >5s gap\n");
                if (is_host()) g_ssn_pending = 1;
                g_hud_was_lost5 = 0;
            }
            g_hud_ever_conn = 1;
            g_hud.disconn_sec = 0;
        } else if (g_hud_ever_conn) {
            g_hud.disconn_sec = (int)(since_rx / 1000);
            if (since_rx >= 5000) g_hud_was_lost5 = 1;
        }
        g_hud.ever_connected = g_hud_ever_conn;
    }
    if (!g_hud.connected) { g_hud.rtt_ms = -1; g_hud_rtt_f = -1.0; }
    // ~4 Hz HUD durum/ping paketi gönder
    if ((g_net_steam == 2 ? g_st_peer != 0 : g_peer_known) && t >= g_hud_next_tx) {
        HUDPKT p; int hp = 100, nano = 0, cut = 0;
        DWORD now_ms = GetTickCount(), hold = g_hud_last_rx_ms ? (now_ms - g_hud_last_rx_ms) : 0;
        memset(&p, 0, sizeof p);
        p.magic = HUD_MAGIC;
        p.run_id = g_run_id;
        p.tx_ms = now_ms;
        p.echo_ms = (g_hud_peer_tx_ms && hold < 2000) ? (g_hud_peer_tx_ms + hold) : 0;   // 250ms bekleme süresini düşerek gerçek ağ RTT'sini ölç
        if (in_level && ebx) {
            ENTITY *pb = player_box();
            __try {
                var s36 = pb ? ((var *)((BYTE *)pb + 148))[35] : G_PLAYER_HP(ebx);
                hp = (int)VF(s36);
                nano = (int)VF(G_NANO(ebx));
                cut = (G_CTRL_ENABLED(ebx) == 0);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        p.hp = (short)(hp < 0 ? 0 : (hp > 100 ? 100 : hp));
        p.nano = (short)(nano < 0 ? 0 : (nano > 100 ? 100 : nano));
        p.flags = (BYTE)((in_level ? HUD_F_INLEVEL : 0) | (cut ? HUD_F_CUTSCENE : 0) | (t < g_revive_invuln_until ? HUD_F_INVULN : 0));
        hud_local_name(p.name, sizeof p.name);
        net_send_peer((BYTE *)&p, sizeof p);
        g_hud_next_tx = t + 0.25;
    }
    // Baş üstü isim + ekran dışı yön/mesafe hesabı (yalnız oyun iş parçacığında)
    g_hud.head_valid = 0; g_hud.head_on_screen = 0;
    if (in_level && peer_active) {
        ENTITY *cpup = 0, pc, lc;
        for (i = 0; i < MAX_PARTS; i++) {
            if (g_puppet[i] && !_stricmp(g_puppet_type[i], "SUPERCAN_GOVDE.MDL")) { cpup = g_puppet[i]; break; }
        }
        if (!cpup) cpup = g_puppet[0];
        if (cpup && safe_ent(cpup, &pc)) {
            double dist = 0;
            if (g_local_count && safe_ent(g_local_ent[0], &lc)) {
                double dx = VF(pc.x) - VF(lc.x), dy = VF(pc.y) - VF(lc.y), dz = VF(pc.z) - VF(lc.z);
                dist = sqrt(dx * dx + dy * dy + dz * dz);
                g_hud.dist_m = (float)(dist / 32.0);   // yaklaşık metre
            }
            {   static int v2s_logged;
                long vt = 0; void *cp = safe_getvar("camera", &vt), *cam = cp ? (void *)safe_long(cp) : 0;
                var v[3]; var *r = 0;
                float bw = g_bb_w ? (float)g_bb_w : 1920.0f, bh = g_bb_h ? (float)g_bb_h : 1080.0f;
                if (!p_vec_to_screen) p_vec_to_screen = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "vec_to_screen");
                v[0] = pc.x; v[1] = pc.y; v[2] = pc.z + (54 << 10);
                if (p_vec_to_screen && cam) __try { r = p_vec_to_screen(v, cam); } __except (EXCEPTION_EXECUTE_HANDLER) { r = 0; }
                g_hud.head_valid = 1;
                if (!v2s_logged && cam) {
                    v2s_logged = 1;
                    logf("[hud] v2s cam=%p r=%p v=(%.1f %.1f %.1f) bb=%.0fx%.0f\n", cam, r, VF(v[0]), VF(v[1]), VF(v[2]), bw, bh);
                }
                if (r) {
                    float sx = (float)VF(v[0]), sy = (float)VF(v[1]);
                    if (sx >= 20.0f && sx <= bw - 20.0f && sy >= 20.0f && sy <= bh - 20.0f) {
                        g_hud.head_on_screen = 1;
                        g_hud.head_x = sx;
                        g_hud.head_y = sy;
                    }
                }
                if (!g_hud.head_on_screen) {
                    // Gerçek kamera (VIEW@40: x,y,z; @52: pan) yönüne göre 360 derece ekran kenarı projeksiyonu
                    double __cdecl cos(double); double __cdecl sin(double);
                    double cx = 0, cy = 0, cpan = 0, dx, dy, rad, fx, fy, lx, ly, fwd, right, norm;
                    int have_cam = 0;
                    if (cam) __try {
                        cx = VF(*(var *)((BYTE *)cam + 40));
                        cy = VF(*(var *)((BYTE *)cam + 44));
                        cpan = VF(*(var *)((BYTE *)cam + 52));
                        have_cam = 1;
                    } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    if (!have_cam && g_local_count && safe_ent(g_local_ent[0], &lc)) {
                        cx = VF(lc.x); cy = VF(lc.y); cpan = VF(lc.pan); have_cam = 1;
                    }
                    dx = VF(pc.x) - cx; dy = VF(pc.y) - cy;
                    rad = cpan * 0.017453292519943295;
                    fx = cos(rad); fy = sin(rad);
                    lx = -fy;      ly = fx;
                    fwd = dx * fx + dy * fy;
                    right = -(dx * lx + dy * ly);   // Acknex +pan = sol, sağ = -sol
                    norm = sqrt(fwd * fwd + right * right);
                    if (norm < 0.001) { fwd = -1.0; right = 0.0; norm = 1.0; }
                    fwd /= norm; right /= norm;
                    if (ABSF(right) >= ABSF(fwd)) {
                        g_hud.off_dir = right < 0 ? 0 : 1;   // 0=sol, 1=sağ
                        g_hud.off_x = right < 0 ? 22.0f : (bw - 212.0f);
                        g_hud.off_y = bh * 0.5f - (float)(fwd * (bh * 0.42f));
                    } else {
                        g_hud.off_dir = fwd >= 0 ? 2 : 3;    // 2=üst, 3=alt
                        g_hud.off_x = bw * 0.5f + (float)(right * (bw * 0.42f)) - 95.0f;
                        g_hud.off_y = fwd >= 0 ? 22.0f : (bh - 52.0f);
                    }
                    if (g_hud.off_x < 22.0f) g_hud.off_x = 22.0f;
                    if (g_hud.off_x > bw - 212.0f) g_hud.off_x = bw - 212.0f;
                    if (g_hud.off_y < 22.0f) g_hud.off_y = 22.0f;
                    if (g_hud.off_y > bh - 52.0f) g_hud.off_y = bh - 52.0f;
                }
            }
            // F5: Arkadaşın yanına ışınlan (takılma kurtarma QoL, 2 sn cooldown, yalnız odaktaki pencerede)
            {   int f5 = hud_focused() && (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
                if (f5 && !g_f5_prev && t >= g_hud_tp_cd && ebx) {
                    int ctrl_ok = 1;
                    __try { ctrl_ok = (G_CTRL_ENABLED(ebx) != 0); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    if (ctrl_ok) {
                        ENTITY *pb = player_box();
                        if (pb) {
                            var pos[3] = { pc.x, pc.y, pc.z + (40 << 10) };
                            if (!pX_setpos) pX_setpos = (void *)GetProcAddress(GetModuleHandleA("ackphysX.dll"), "pXent_setposition");
                            __try {
                                pb->x = pos[0]; pb->y = pos[1]; pb->z = pos[2];   // SUPERCAN_BOX c_move karakter kutusu (decomp.c:78780)
                                if (pX_setpos) pX_setpos(pb, pos);
                            } __except (EXCEPTION_EXECUTE_HANDLER) {}
                            g_revive_invuln_until = t + 1.5;
                            g_hud_tp_cd = t + 2.0;
                            hud_toast("ARKADASIN YANINA ISINLANDIN!", 2.5);
                            logf("[hud] F5 teleported to peer at (%.0f %.0f %.0f)\n", VF(pos[0]), VF(pos[1]), VF(pos[2]));
                        }
                    } else {
                        hud_toast("ARA SAHNEDE ISINLANMA KAPALI", 1.8);
                    }
                }
                g_f5_prev = f5;
            }
        }
    } else {
        g_f5_prev = hud_focused() && (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
    }
    // Acknex yerel 2D çizimi (draw_quad + draw_textmode + draw_text): D3D9 Present vtable'ından bağımsız her karede çalışır
    if (g_hud.enabled) {
        static var (__cdecl *p_draw_quad)(void *, var *, var *, var *, var *, var *, var, var);
        static var (__cdecl *p_draw_textmode)(char *, var, var, var);
        static var (__cdecl *p_draw_text)(char *, var, var, var *);
        static int hud_draw_logged;
        if (!p_draw_quad) p_draw_quad = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "draw_quad");
        if (!p_draw_textmode) p_draw_textmode = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "draw_textmode");
        if (!p_draw_text) p_draw_text = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "draw_text");
        if (!hud_draw_logged) {
            hud_draw_logged = 1;
            logf("[hud] acknex draw APIs: quad=%p textmode=%p text=%p bb=%ux%u\n", p_draw_quad, p_draw_textmode, p_draw_text, g_bb_w, g_bb_h);
        }
        if (p_draw_quad && p_draw_textmode && p_draw_text) {
            float bw = g_bb_w ? (float)g_bb_w : 1920.0f, bh = g_bb_h ? (float)g_bb_h : 1080.0f;
            HUDSTATE hs = g_hud;
            char buf[96];
            var c_bg[3]    = {26 << 10, 19 << 10, 15 << 10};    // koyu arduvaz BGR
            var c_hdr[3]   = {18 << 10, 13 << 10, 10 << 10};    // daha koyu başlık şeridi
            var c_barbg[3] = {38 << 10, 32 << 10, 28 << 10};
            var c_frm[3]   = {115 << 10, 95 << 10, 72 << 10};   // ince metalik kenar
            var c_wht[3]   = {255 << 10, 255 << 10, 255 << 10};
            var c_gry[3]   = {188 << 10, 182 << 10, 175 << 10};
            var c_cyn[3]   = {255 << 10, 218 << 10, 72 << 10};  // canlı cyan
            var c_gld[3]   = {55 << 10, 200 << 10, 255 << 10};  // sıcak altın/turuncu
            var c_grn[3]   = {85 << 10, 232 << 10, 75 << 10};   // canlı yeşil
            var c_red[3]   = {72 << 10, 72 << 10, 252 << 10};   // uyarı kırmızısı
            var c_blu[3]   = {255 << 10, 178 << 10, 38 << 10};  // nano mavisi
            var c_blk[3]   = {14 << 10, 12 << 10, 12 << 10};
            __try {
                // 1) Sol alt Modern Kompakt Takım Kartı (oyunun sağ alt süre/bilgi sayacını kapatmaz; D.E.V.E boss barında yukarı kayar)
                int lost5 = (!hs.connected && hs.ever_connected && hs.disconn_sec >= 5);
                int boss_bar = (in_level && level && !_stricmp(level, "tunel.wmb") && ebx && (G_SUB_LVL(ebx) >> 10) >= 9);
                var *accent = lost5 ? c_red : (hs.connected ? (hs.is_host ? c_cyn : c_gld) : c_gld);
                float pw = hs.connected ? 392.0f : 344.0f, ph = hs.connected ? 138.0f : 74.0f;
                float px = 22.0f, py = bh - ph - (boss_bar ? 92.0f : 22.0f);

                // Kart gövdesi + üst başlık şeridi + sol dikey durum şeridi + ince çerçeve
                hud_rect(p_draw_quad, px + 3.0f, py + 4.0f, pw, ph, c_blk, 42);     // yumuşak dış gölge
                hud_rect(p_draw_quad, px, py, pw, ph, c_bg, 80);
                hud_rect(p_draw_quad, px, py, pw, 36.0f, c_hdr, 86);
                hud_rect(p_draw_quad, px, py, 5.0f, ph, accent, 96);                // sol dikey vurgu şeridi
                hud_frame(p_draw_quad, px, py, pw, ph, lost5 ? c_red : c_frm, 72, 1.5f);

                // Rol rozeti + başlık satırı
                {   const char *role_txt = hs.is_host ? "HOST" : "P2";
                    float rw = hs.is_host ? 62.0f : 38.0f;
                    hud_rect(p_draw_quad, px + 14.0f, py + 7.0f, rw, 22.0f, accent, 92);
                    p_draw_textmode("Tahoma", 1 << 10, 17 << 10, 100 << 10);
                    p_draw_text((char *)role_txt, (var)((px + 21.0f) * 1024), (var)((py + 8.0f) * 1024), c_blk);
                    if (hs.connected) {
                        const char *pnm = hs.peer_name[0] ? hs.peer_name : "ARKADAS";
                        p_draw_textmode("Tahoma", 1 << 10, 21 << 10, 100 << 10);
                        hud_text_s(p_draw_text, pnm, px + 14.0f + rw + 10.0f, py + 6.0f, c_wht);

                        // Sağ üst: 4 çubuklu ping sinyali + ms
                        hud_ping_bars(p_draw_quad, px + pw - 102.0f, py + 10.0f, hs.rtt_ms);
                        p_draw_textmode("Tahoma", 1 << 10, 17 << 10, 100 << 10);
                        {   var *pcol = hs.rtt_ms < 0 ? c_gry : (hs.rtt_ms < 60 ? c_grn : (hs.rtt_ms < 120 ? c_gld : c_red));
                            if (hs.rtt_ms >= 0) sprintf(buf, "%dms", hs.rtt_ms); else strcpy(buf, "--");
                            hud_text_s(p_draw_text, buf, px + pw - 70.0f, py + 8.0f, pcol);
                        }
                    } else {
                        p_draw_textmode("Tahoma", 1 << 10, 19 << 10, 100 << 10);
                        hud_text_s(p_draw_text, "CO-OP", px + 14.0f + rw + 10.0f, py + 7.0f, c_wht);
                    }
                }

                if (hs.connected) {
                    // CAN barı (parlak cam efektli + 4 segmentli)
                    {   float bx = px + 76.0f, by = py + 47.0f, bwid = 248.0f, bh2 = 17.0f;
                        var *hcol = hs.peer_hp > 50 ? c_grn : (hs.peer_hp > 25 ? c_gld : c_red);
                        p_draw_textmode("Tahoma", 1 << 10, 18 << 10, 100 << 10);
                        hud_text_s(p_draw_text, "CAN", px + 14.0f, by - 2.0f, c_wht);
                        hud_bar(p_draw_quad, bx, by, bwid, bh2, hs.peer_hp, hcol, c_barbg);
                        sprintf(buf, "%d", hs.peer_hp);
                        hud_text_s(p_draw_text, buf, bx + bwid + 10.0f, by - 2.0f, c_wht);
                    }
                    // NANO barı
                    {   float bx = px + 76.0f, by = py + 74.0f, bwid = 248.0f, bh2 = 17.0f;
                        p_draw_textmode("Tahoma", 1 << 10, 18 << 10, 100 << 10);
                        hud_text_s(p_draw_text, "NANO", px + 14.0f, by - 2.0f, c_cyn);
                        hud_bar(p_draw_quad, bx, by, bwid, bh2, hs.peer_nano, c_blu, c_barbg);
                        sprintf(buf, "%d", hs.peer_nano);
                        hud_text_s(p_draw_text, buf, bx + bwid + 10.0f, by - 2.0f, c_cyn);
                    }
                    // Alt bilgi / kısayol şeridi (kalın 18pt Tahoma — küçük pencerede de jilet gibi net)
                    hud_rect(p_draw_quad, px + 5.0f, py + 102.0f, pw - 6.0f, 35.0f, c_hdr, 78);
                    p_draw_textmode("Tahoma", 1 << 10, 16 << 10, 100 << 10);
                    hud_text_s(p_draw_text, g_net_steam == 2 ? "F5 ISINLAN | F8 LOBI | F9 GIZLE" : "F5 ISINLAN   |   F9 GIZLE", px + 12.0f, py + 110.0f, c_gry);
                    if (!hs.same_level && hs.in_level) hud_text_s(p_draw_text, "BASKA BOLUM", px + pw - 120.0f, py + 110.0f, c_gld);
                    else if (hs.peer_cutscene) hud_text_s(p_draw_text, "ARA SAHNE", px + pw - 104.0f, py + 110.0f, c_cyn);
                    else if (hs.peer_invuln) hud_text_s(p_draw_text, "KORUMA", px + pw - 82.0f, py + 110.0f, c_grn);
                    else if (hs.head_valid) { sprintf(buf, "%.0fm", hs.dist_m); hud_text_s(p_draw_text, buf, px + pw - 54.0f, py + 110.0f, c_cyn); }
                } else if (lost5) {
                    p_draw_textmode("Tahoma", 1 << 10, 19 << 10, 100 << 10);
                    sprintf(buf, "BAGLANTI KESILDI (%dsn)", hs.disconn_sec);
                    hud_text_s(p_draw_text, buf, px + 16.0f, py + 44.0f, c_red);
                } else {
                    p_draw_textmode("Tahoma", 1 << 10, 18 << 10, 100 << 10);
                    hud_text_s(p_draw_text, g_net_steam == 2 ? "ARKADAS BEKLENIYOR... (F8: LOBI)" : "ARKADAS BEKLENIYOR...", px + 14.0f, py + 44.0f, c_gld);
                }

                // 2) Baş üstü isim etiketi (Deep Rock Galactic tarzı, ok uçlu) veya ekran dışı 360° işaretçi
                if (hs.in_level && hs.same_level && hs.head_valid) {
                    if (hs.head_on_screen) {
                        const char *nm = hs.peer_name[0] ? hs.peer_name : "ARKADAS";
                        float bw2 = (float)strlen(nm) * 12.0f + 30.0f;
                        float x0, y0;
                        var *hcol = hs.peer_hp > 50 ? c_grn : (hs.peer_hp > 25 ? c_gld : c_red);
                        if (bw2 < 112.0f) bw2 = 112.0f;
                        x0 = hs.head_x - bw2 * 0.5f; y0 = hs.head_y - 34.0f;
                        if (y0 < 10.0f) y0 = 10.0f;
                        hud_rect(p_draw_quad, x0, y0, bw2, 38.0f, c_bg, 72);
                        hud_rect(p_draw_quad, x0, y0, 3.0f, 38.0f, c_cyn, 95);
                        hud_frame(p_draw_quad, x0, y0, bw2, 38.0f, c_frm, 68, 1.0f);
                        // Alt orta küçük işaretçi ok ucu
                        hud_rect(p_draw_quad, hs.head_x - 4.0f, y0 + 38.0f, 8.0f, 3.0f, c_cyn, 90);
                        hud_rect(p_draw_quad, hs.head_x - 2.0f, y0 + 41.0f, 4.0f, 3.0f, c_cyn, 90);
                        p_draw_textmode("Tahoma", 1 << 10, 19 << 10, 100 << 10);
                        hud_text_s(p_draw_text, nm, x0 + 12.0f, y0 + 4.0f, c_wht);
                        hud_bar(p_draw_quad, hs.head_x - 44.0f, y0 + 27.0f, 88.0f, 6.0f, hs.peer_hp, hcol, c_barbg);
                    } else {
                        const char *ar = hs.off_dir == 0 ? "< " : (hs.off_dir == 1 ? "> " : (hs.off_dir == 2 ? "^ " : "v "));
                        float ow = 196.0f, oh = 32.0f;
                        sprintf(buf, "%s%.10s %.0fm", ar, hs.peer_name[0] ? hs.peer_name : "ARKADAS", hs.dist_m);
                        hud_rect(p_draw_quad, hs.off_x, hs.off_y, ow, oh, c_bg, 80);
                        // Kenara bakan tarafta parlak cyan şerit
                        if (hs.off_dir == 0) hud_rect(p_draw_quad, hs.off_x, hs.off_y, 4.0f, oh, c_cyn, 96);
                        else if (hs.off_dir == 1) hud_rect(p_draw_quad, hs.off_x + ow - 4.0f, hs.off_y, 4.0f, oh, c_cyn, 96);
                        else if (hs.off_dir == 2) hud_rect(p_draw_quad, hs.off_x, hs.off_y, ow, 4.0f, c_cyn, 96);
                        else hud_rect(p_draw_quad, hs.off_x, hs.off_y + oh - 4.0f, ow, 4.0f, c_cyn, 96);
                        hud_frame(p_draw_quad, hs.off_x, hs.off_y, ow, oh, c_frm, 70, 1.0f);
                        p_draw_textmode("Tahoma", 1 << 10, 19 << 10, 100 << 10);
                        hud_text_s(p_draw_text, buf, hs.off_x + 12.0f, hs.off_y + 5.0f, c_cyn);
                    }
                }

                // 3) Alt-orta modern banner toast bildirimi (F5 ışınlanma / canlanma / yeniden bağlanma / ara sahne / Steam davet)
                if (hs.toast[0] && t < hs.toast_until) {
                    float tw = (float)strlen(hs.toast) * 13.5f + 48.0f;
                    float tx = (bw - tw) * 0.5f, ty = bh - 98.0f;
                    hud_rect(p_draw_quad, tx + 3.0f, ty + 4.0f, tw, 42.0f, c_blk, 45);
                    hud_rect(p_draw_quad, tx, ty, tw, 42.0f, c_hdr, 86);
                    hud_rect(p_draw_quad, tx, ty, tw, 2.5f, c_gld, 96);
                    hud_rect(p_draw_quad, tx, ty + 39.5f, tw, 2.5f, c_gld, 96);
                    p_draw_textmode("Tahoma", 1 << 10, 22 << 10, 100 << 10);
                    hud_text_s(p_draw_text, hs.toast, tx + 24.0f, ty + 9.0f, c_gld);
                }

                // 4) Oyun içi Steam Lobi & Arkadaş Davet Paneli (F8 ile aç/kapa; Steam Overlay açılmadığı için yerel panel)
                {   static int s_f8_prev, s_key_prev[12];
                    int foc = hud_focused();
                    int f8 = foc && (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
                    if (f8 && !s_f8_prev) {
                        g_st_ui_open = !g_st_ui_open;
                        if (g_st_ui_open && g_net_steam == 2) {
                            steam_lobby_refresh_friends();
                            if (!g_st_lobby && !g_st_lobby_creating && g_st_mm) steam_lobby_create();
                        }
                    }
                    s_f8_prev = f8;
                    if (g_st_ui_open) {
                        float lw = 520.0f, lh = 390.0f;
                        float lx = (bw - lw) * 0.5f, ly = (bh - lh) * 0.5f;
                        int fi;
                        // Panel tuşları: 1..8 davet et, R yenile, K ID kopyala, C lobi kur, L ayrıl
                        if (foc && g_net_steam == 2) {
                            for (fi = 0; fi < ST_MAX_UI_FRIENDS; fi++) {
                                int k = (GetAsyncKeyState('1' + fi) & 0x8000) != 0;
                                if (k && !s_key_prev[fi] && fi < g_st_ui_nfriend) steam_lobby_invite(fi);
                                s_key_prev[fi] = k;
                            }
                            {   int kr = (GetAsyncKeyState('R') & 0x8000) != 0;
                                int kk = (GetAsyncKeyState('K') & 0x8000) != 0;
                                int kc = (GetAsyncKeyState('C') & 0x8000) != 0;
                                int kl = (GetAsyncKeyState('L') & 0x8000) != 0;
                                if (kr && !s_key_prev[8]) { steam_lobby_refresh_friends(); hud_toast("ARKADAS LISTESI YENILENDI", 1.8); }
                                if (kk && !s_key_prev[9]) steam_lobby_copy_id();
                                if (kc && !s_key_prev[10]) steam_lobby_create();
                                if (kl && !s_key_prev[11]) steam_lobby_leave();
                                s_key_prev[8] = kr; s_key_prev[9] = kk; s_key_prev[10] = kc; s_key_prev[11] = kl;
                            }
                        }
                        hud_rect(p_draw_quad, lx + 4.0f, ly + 5.0f, lw, lh, c_blk, 55);
                        hud_rect(p_draw_quad, lx, ly, lw, lh, c_bg, 90);
                        hud_rect(p_draw_quad, lx, ly, lw, 40.0f, c_hdr, 95);
                        hud_rect(p_draw_quad, lx, ly, 5.0f, lh, c_cyn, 96);
                        hud_frame(p_draw_quad, lx, ly, lw, lh, c_frm, 85, 1.5f);
                        p_draw_textmode("Tahoma", 1 << 10, 21 << 10, 100 << 10);
                        hud_text_s(p_draw_text, "STEAM CO-OP LOBI & DAVET", lx + 18.0f, ly + 8.0f, c_cyn);
                        p_draw_textmode("Tahoma", 1 << 10, 16 << 10, 100 << 10);
                        hud_text_s(p_draw_text, "[F8 KAPAT]", lx + lw - 102.0f, ly + 12.0f, c_gry);
                        if (g_net_steam != 2) {
                            p_draw_textmode("Tahoma", 1 << 10, 19 << 10, 100 << 10);
                            hud_text_s(p_draw_text, "STEAM AKTIF DEGIL (UDP MODU)", lx + 20.0f, ly + 64.0f, c_gld);
                            hud_text_s(p_draw_text, "coop.ini icinde net=steam yapin ve Steam'i acin.", lx + 20.0f, ly + 96.0f, c_gry);
                        } else {
                            p_draw_textmode("Tahoma", 1 << 10, 17 << 10, 100 << 10);
                            if (g_st_lobby) {
                                sprintf(buf, "LOBI ID: %I64u   (%d/2 UYE)", g_st_lobby, g_st_lobby_members > 0 ? g_st_lobby_members : 1);
                                hud_text_s(p_draw_text, buf, lx + 18.0f, ly + 50.0f, c_grn);
                            } else if (g_st_lobby_creating) {
                                hud_text_s(p_draw_text, "LOBI KURULUYOR...", lx + 18.0f, ly + 50.0f, c_gld);
                            } else {
                                hud_text_s(p_draw_text, "LOBI YOK  ([C] ile lobi kur)", lx + 18.0f, ly + 50.0f, c_gld);
                            }
                            hud_text_s(p_draw_text, "CEVRIMICI ARKADASLAR (DAVET ICIN 1-8 BAS):", lx + 18.0f, ly + 82.0f, c_wht);
                            if (g_st_ui_nfriend == 0) {
                                hud_text_s(p_draw_text, "  Cevrimici arkadas bulunamadi. ([R] Yenile)", lx + 18.0f, ly + 114.0f, c_gry);
                            } else {
                                for (fi = 0; fi < g_st_ui_nfriend; fi++) {
                                    float ry = ly + 112.0f + fi * 28.0f;
                                    hud_rect(p_draw_quad, lx + 16.0f, ry - 2.0f, lw - 32.0f, 24.0f, c_hdr, 58);
                                    sprintf(buf, "[%d]  %.24s%s", fi + 1, g_st_ui_friends[fi].name,
                                            g_st_ui_friends[fi].in_480 ? "   (OYUNDA)" : "");
                                    hud_text_s(p_draw_text, buf, lx + 24.0f, ry, g_st_ui_friends[fi].in_480 ? c_grn : c_wht);
                                }
                            }
                            hud_rect(p_draw_quad, lx + 5.0f, ly + lh - 40.0f, lw - 6.0f, 39.0f, c_hdr, 85);
                            p_draw_textmode("Tahoma", 1 << 10, 15 << 10, 100 << 10);
                            hud_text_s(p_draw_text, "[1-8] DAVET  |  [R] YENILE  |  [K] ID KOPYALA  |  [C] KUR  |  [L] AYRIL", lx + 14.0f, ly + lh - 29.0f, c_cyn);
                        }
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
    (void)level;
}
