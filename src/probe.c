// SuperCan Co-op Mod — Moduler Giris Noktasi (Unity Build: core/ ortak cekirdek + sc1/ oyuna ozgu katman)
// Derleme: local/src/probe/build.bat (VS2022 x86 -> local/build/dinput8.dll)
// SuperCan 2 tasimasi icin core/ modulleri ortak kalir; yalnizca sc2/ katmani eklenir.

#include "core/core_acknex.inc"
#include "core/core_net.inc"
#include "sc1/sc1_offsets.h"
#include "sc1/sc1_world_combat.inc"
#include "sc1/sc1_boss.inc"
#include "sc1/sc1_revive.inc"
#include "core/core_hud.inc"
#include "sc1/sc1_sync_extra.inc"

static void coop_tick(void) {
    NETPKT out, in; ENTITY *e, c; int n = 0, i, seen[MAX_PARTS] = {0}; char ty[96];
    fps_tick(); pz_install();
    if (g_net_steam == 1) {   // lazy Steam start on the game thread; on failure fall back to UDP
        g_net_steam = steam_init() ? 2 : 0;
        if (!g_net_steam) logf("[coop] steam unavailable -> %s\n", g_sock != INVALID_SOCKET ? "falling back to UDP" : "co-op off (no UDP listen port)");
    }
    if ((g_sock == INVALID_SOCKET && !g_net_steam) || !p_ent_next) return;
    memset(&out, 0, sizeof out); out.magic = PKT_MAGIC;
    current_level(out.level, sizeof out.level);

    // 1) walk entities: collect local player parts (and world parts: enemies), check which puppets still exist
    g_nwcand = 0; g_nebox_cand = 0; memset(g_mkr_ent, 0, sizeof g_mkr_ent);
    for (e = safe_next(0); e && e != (ENTITY *)-1 && n < 30000; e = safe_next(e), n++) {
        if (!safe_ent(e, &c)) continue;
        if (e == g_aim_pup) continue;                      // client hedef kutusu kuklası
        for (i = 0; i < MAX_PARTS; i++) if (g_puppet[i] == e) { seen[i] = 1; break; }
        if (i < MAX_PARTS) continue;                       // it's one of our puppets
        mkr_collect_ent(e);                                // makara/kol/köprü/kapı/blok (0x04 level entity'leri dahil)
        safe_str(c.type, ty, sizeof ty);
        if (!is_wpuppet(e)) itm_collect_ent(e, ty);        // çevre çöpleri (cop_model_fizik) ve elmas/bonus kutuları (0x04/0x05)
        if ((c.link.index >> 24) != 5) continue;           // runtime-created entities only (level ents are 0x04)
        if (is_wpuppet(e)) continue;
        if (is_enemy_box_type(ty)) { if (g_nebox_cand < AIM_MAX) g_ebox_cand[g_nebox_cand++] = e; continue; }
        if (!_stricmp(ty, "FISH.MDL") && VF(c.x) > -2020.0 && VF(c.x) < -1980.0 && VF(c.y) > -20.0 && VF(c.y) < 20.0 && VF(c.z) > 480.0 && VF(c.z) < 520.0) { g_boss_dummy_ent = e; continue; }
        if (!_stricmp(ty, "BOSS_FIRE_GLOW.TGA")) {
            if (g_nebox_cand < AIM_MAX) g_ebox_cand[g_nebox_cand++] = e;   // ateş topu client'a da çarpabilsin
            if (g_nwcand < WMAX_PARTS * 2) g_wcand[g_nwcand++] = e;        // ateş topu client'ta da görünsün
            continue;
        }
        if (is_world_type(ty)) { if (g_nwcand < WMAX_PARTS * 2) g_wcand[g_nwcand++] = e; continue; }
        if (!is_player_part(ty) || out.count >= MAX_PARTS) continue;
        { NETPART *p = &out.part[out.count++];
          strncpy(p->type, ty, sizeof p->type - 1);
          p->x = (float)VF(c.x); p->y = (float)VF(c.y); p->z = (float)VF(c.z);
          p->pan = (float)VF(c.pan); p->tilt = (float)VF(c.tilt); p->roll = (float)VF(c.roll);
          p->sx = (float)VF(c.scale_x); p->sy = (float)VF(c.scale_y); p->sz = (float)VF(c.scale_z);
          p->frame = (float)VF(c.frame); p->nextframe = (float)VF(c.nextframe); p->flags = c.flags;
          p->animated = g_part_animated[out.count - 1]; }
        if (out.count <= MAX_PARTS) g_local_ent[out.count - 1] = e;
        if (g_mtl_off < 0 && !_stricmp(ty, "SUPERCAN_GOVDE.MDL")) find_material_offset(e);
    }
    for (i = 0; i < MAX_PARTS; i++) if (g_puppet[i] && !seen[i]) { logf("[coop] puppet %d (%s) vanished (level change?)\n", i, g_puppet_type[i]); g_puppet[i] = 0; }
    if (out.count != g_local_count) memset(g_part_animated, 0, sizeof g_part_animated);
    g_local_count = out.count;

    // 2) send at the configured rate, with all animation calls recorded since the last send (in order).
    //    Between sends the calls keep accumulating in g_rec; if it fills up, record() drops and counts.
    {   double t = now_s(); BYTE wire[WIRE_MAX]; int len;
        int known = g_net_steam ? g_st_peer != 0 : g_peer_known;
        int due = known ? (out.count && t >= g_next_send) : (t >= g_next_discover);   // discovery also from the menu
        if (due) {
            out.seq = ++g_seq; out.tms = (DWORD)(t * 1000.0); out.last = g_frame_mark; out.ncalls = g_nrec; memcpy(out.call, g_rec, g_nrec * sizeof(NETCALL)); g_nrec = 0;
            len = pkt_encode(&out, wire);
            if (len && known) {
                if (g_net_steam) st_send_to(g_st_peer, wire, len);
                else sendto(g_sock, (char *)wire, len, 0, (struct sockaddr *)&g_peer, sizeof g_peer);
                g_tx_count++; g_tx_bytes += len; if ((DWORD)len > g_tx_max) g_tx_max = len; }
            else if (len) { if (g_net_steam) steam_send(wire, len); else discover_send((char *)wire, len); }
            g_next_send = (t - g_next_send < 0.1) ? g_next_send + g_send_interval : t + g_send_interval;   // steady cadence
            if (g_next_send < t) g_next_send = t;
            g_next_discover = t + 1.0;
        }
        if (!out.count) g_nrec = 0;   // not in a level: nothing to animate
        g_frame_mark = g_nrec;        // calls recorded from here on belong to the next frame
    }

    // 3) receive (drain; newest pose wins, animation calls from every packet are replayed in order)
    if (g_net_steam) steam_callbacks();
    for (;;) {
        struct sockaddr_in from; int fl = sizeof from; BYTE wire[WIRE_MAX + 64]; int r; u64 sid = 0;
        if (g_net_steam) {
            r = steam_recv(wire, sizeof wire, &sid);
            if (r < 0) break;
            if (r >= 4 && *(DWORD *)wire == HIT_MAGIC) { if (sid && sid == g_st_peer) hit_rx(wire, r); continue; }
            if (r >= 4 && *(DWORD *)wire == PHIT_MAGIC) { if (sid && sid == g_st_peer) phit_rx(wire, r); continue; }
            if (r >= 4 && *(DWORD *)wire == MKR_MAGIC) { if (sid && sid == g_st_peer) mkr_rx(wire, r); continue; }
            if (r >= 4 && *(DWORD *)wire == ALRM_MAGIC) { if (sid && sid == g_st_peer) alrm_rx(wire, r); continue; }
            if (r >= 4 && *(DWORD *)wire == BOSS_MAGIC) { if (sid && sid == g_st_peer) boss_rx(wire, r); continue; }
            if (r >= 4 && *(DWORD *)wire == ITEM_MAGIC) { if (sid && sid == g_st_peer) itm_rx(wire, r); continue; }
            if (r >= 4 && *(DWORD *)wire == STAT_MAGIC) { if (sid && sid == g_st_peer) stat_rx(wire, r); continue; }
            if (r >= 4 && *(DWORD *)wire == SSN_MAGIC) { if (sid && sid == g_st_peer) ssn_rx(wire, r); continue; }
            if (r >= 4 && *(DWORD *)wire == SND_MAGIC) { if (sid && sid == g_st_peer) snd_rx(wire, r); continue; }
            if (r >= 4 && *(DWORD *)wire == CUT_MAGIC) { if (sid && sid == g_st_peer) cut_rx(wire, r); continue; }
            if (r >= 4 && *(DWORD *)wire == MUS_MAGIC) { if (sid && sid == g_st_peer) mus_rx(wire, r); continue; }
            if (r >= 4 && *(DWORD *)wire == HUD_MAGIC) { if (sid && sid == g_st_peer) hud_rx(wire, r); continue; }
            if (r >= 4 && *(DWORD *)wire == WPKT_MAGIC) { if (sid && sid == g_st_peer) world_rx(wire, r); continue; }
            if (!r || !pkt_decode(wire, r, &in) || !sid || sid == g_st_me) continue;
            if (g_st_cfg_peer && sid != g_st_cfg_peer) continue;
            if (g_st_lobby && sid != g_st_peer) continue;   // Lobi aktifken yalnız lobi üyesinden kabul et
            if (sid != g_st_peer) {
                logf("[steam] peer is now %I64u '%s' (was %I64u)\n", sid, st_name(sid), g_st_peer);
                g_st_peer = sid; g_last_rx_seq = 0;
            }
            goto have_packet;
        }
        r = recvfrom(g_sock, (char *)wire, sizeof wire, 0, (struct sockaddr *)&from, &fl);
        if (r == SOCKET_ERROR && WSAGetLastError() == WSAEMSGSIZE) continue;   // oversized datagram: not ours
        if (r <= 0) break;
        if (r >= 4 && *(DWORD *)wire == HIT_MAGIC) {
            if (g_peer_known && from.sin_addr.s_addr == g_peer.sin_addr.s_addr && from.sin_port == g_peer.sin_port) hit_rx(wire, r);
            continue; }
        if (r >= 4 && *(DWORD *)wire == PHIT_MAGIC) {
            if (g_peer_known && from.sin_addr.s_addr == g_peer.sin_addr.s_addr && from.sin_port == g_peer.sin_port) phit_rx(wire, r);
            continue; }
        if (r >= 4 && *(DWORD *)wire == MKR_MAGIC) {
            if (g_peer_known && from.sin_addr.s_addr == g_peer.sin_addr.s_addr && from.sin_port == g_peer.sin_port) mkr_rx(wire, r);
            continue; }
        if (r >= 4 && *(DWORD *)wire == ALRM_MAGIC) {
            if (g_peer_known && from.sin_addr.s_addr == g_peer.sin_addr.s_addr && from.sin_port == g_peer.sin_port) alrm_rx(wire, r);
            continue; }
        if (r >= 4 && *(DWORD *)wire == BOSS_MAGIC) {
            if (g_peer_known && from.sin_addr.s_addr == g_peer.sin_addr.s_addr && from.sin_port == g_peer.sin_port) boss_rx(wire, r);
            continue; }
        if (r >= 4 && *(DWORD *)wire == ITEM_MAGIC) {
            if (g_peer_known && from.sin_addr.s_addr == g_peer.sin_addr.s_addr && from.sin_port == g_peer.sin_port) itm_rx(wire, r);
            continue; }
        if (r >= 4 && *(DWORD *)wire == STAT_MAGIC) {
            if (g_peer_known && from.sin_addr.s_addr == g_peer.sin_addr.s_addr && from.sin_port == g_peer.sin_port) stat_rx(wire, r);
            continue; }
        if (r >= 4 && *(DWORD *)wire == SSN_MAGIC) {
            if (g_peer_known && from.sin_addr.s_addr == g_peer.sin_addr.s_addr && from.sin_port == g_peer.sin_port) ssn_rx(wire, r);
            continue; }
        if (r >= 4 && *(DWORD *)wire == SND_MAGIC) {
            if (g_peer_known && from.sin_addr.s_addr == g_peer.sin_addr.s_addr && from.sin_port == g_peer.sin_port) snd_rx(wire, r);
            continue; }
        if (r >= 4 && *(DWORD *)wire == CUT_MAGIC) {
            if (g_peer_known && from.sin_addr.s_addr == g_peer.sin_addr.s_addr && from.sin_port == g_peer.sin_port) cut_rx(wire, r);
            continue; }
        if (r >= 4 && *(DWORD *)wire == MUS_MAGIC) {
            if (g_peer_known && from.sin_addr.s_addr == g_peer.sin_addr.s_addr && from.sin_port == g_peer.sin_port) mus_rx(wire, r);
            continue; }
        if (r >= 4 && *(DWORD *)wire == HUD_MAGIC) {
            if (g_peer_known && from.sin_addr.s_addr == g_peer.sin_addr.s_addr && from.sin_port == g_peer.sin_port) hud_rx(wire, r);
            continue; }
        if (r >= 4 && *(DWORD *)wire == WPKT_MAGIC) {
            if (g_peer_known && from.sin_addr.s_addr == g_peer.sin_addr.s_addr && from.sin_port == g_peer.sin_port) world_rx(wire, r);
            continue; }
        if (!pkt_decode(wire, r, &in)) continue;
        if (is_self(&from)) continue;
        if (!g_peer_known || from.sin_addr.s_addr != g_peer.sin_addr.s_addr || from.sin_port != g_peer.sin_port) {
            logf("[coop] peer is now %s:%d (was %s)\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port), g_peer_known ? "another address" : "unknown");
            g_peer = from; g_peer_known = 1; g_last_rx_seq = 0;
        }
    have_packet:
        if (sim_rx(&in)) continue;   // simulated network: queued or dropped
        rx_accept(&in);
    }
    sim_release();

    // 4) mirror remote parts as puppets (only when both are in the same level, we are in-level, and level_metarials_apply finished)
    {   int active = g_have_remote && out.count && GetTickCount() - g_last_rx_tick < 2000 && !strcmp(g_remote.level, out.level);
        int mats_ready = level_materials_ready(out.level, out.count > 0);
        int puppet_ok = active && mats_ready;
        SNAP *sa = 0, *sb = 0; float sf = 1; int have = puppet_ok && snap_pick(&sa, &sb, &sf);
        for (i = 0; i < MAX_PARTS; i++) {
            NETPART ip = have && (DWORD)i < g_remote.count ? part_at(i, sa, sb, sf) : g_remote.part[i], *p = &ip;
            int want = puppet_ok && (DWORD)i < g_remote.count;
            if (g_puppet[i] && (!want || strcmp(g_puppet_type[i], p->type))) {
                __try { p_ent_remove(g_puppet[i]); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                logf("[coop] removed puppet %d (%s)\n", i, g_puppet_type[i]); g_puppet[i] = 0;
            }
            if (!want) continue;
            if (!g_puppet[i]) {
                var pos[3]; ENTITY *ne = 0; char file[32];
                pos[0] = (var)(p->x * 1024.0f); pos[1] = (var)(p->y * 1024.0f); pos[2] = (var)(p->z * 1024.0f);
                strncpy(file, p->type, sizeof file - 1); file[sizeof file - 1] = 0; _strlwr(file);
                if (!safe_create(file, pos, &ne) || !ne) { logf("[coop] ent_create('%s') failed\n", file); continue; }
                g_puppet[i] = ne; strcpy(g_puppet_type[i], p->type);
                g_puppet_posed[i] = 0;
                g_puppet_looked[i] = 0;
                logf("[coop] created puppet %d '%s' -> %p\n", i, file, ne);
            }
            {   ENTITY *q = g_puppet[i];
                __try {
                    q->x = (var)(p->x * 1024.0f); q->y = (var)(p->y * 1024.0f); q->z = (var)(p->z * 1024.0f);
                    q->pan = (var)(p->pan * 1024.0f); q->tilt = (var)(p->tilt * 1024.0f); q->roll = (var)(p->roll * 1024.0f);
                    q->scale_x = (var)(p->sx * 1024.0f); q->scale_y = (var)(p->sy * 1024.0f); q->scale_z = (var)(p->sz * 1024.0f);
                    if (!p->animated) { q->frame = (var)(p->frame * 1024.0f); q->nextframe = (var)(p->nextframe * 1024.0f); }
                    q->flags = (q->flags & ~INVISIBLE_F) | PASSABLE_F | (p->flags & INVISIBLE_F);
                } __except (EXCEPTION_EXECUTE_HANDLER) { logf("[coop] puppet %d write fault\n", i); g_puppet[i] = 0; }
                if (g_puppet[i] && (!g_puppet_looked[i] || (g_frame % 30) == 0)) {
                    DWORD k; for (k = 0; k < g_local_count; k++) {   // look: same-type local part on this side (ilk oluşumda + 30 karede bir)
                        char lt[32]; ENTITY lc;
                        if (safe_ent(g_local_ent[k], &lc)) { safe_str(lc.type, lt, sizeof lt);
                            if (!_stricmp(lt, p->type)) { copy_look(q, g_local_ent[k]); g_puppet_looked[i] = 1; break; } } }
                }
            }
        }
        if (have) anim_at(sa, sb, sf);   // animation: one-off replays + newest frame interpolated
        world_tick(out.level, out.count > 0, active, mats_ready);   // enemies: host sends, client mirrors (same render time)
        mkr_tick(out.level, out.count > 0, active);     // makara/kol/köprü/kapı/blok senkronu
        alrm_tick(out.level, out.count > 0, active);    // alarm/lazer/silo aşama senkronu
        boss_tick(out.level, out.count > 0, active);    // Deve/Hangar boss + 4 pil ortak can senkronu
        itm_tick(out.level, out.count > 0, active);     // çevre çöpleri + elmas/bonus kutuları senkronu
        stat_tick(out.level, out.count > 0, active);    // ortak puan, elmas, ceza ve çöp sayaçları (nano ayrı)
        ssn_tick(out.level, out.count > 0, active);     // late-join / reconnect tek seferlik dünya snapshot'ı ('SSN1')
        snd_tick(out.count > 0, active);                // 3D ses senkronu ('SND1', entPlaySound kancası)
        cut_tick(out.count > 0, active);                // ara sahne geçme bildirimi ve senkronu ('SCT1')
        mus_tick(out.count > 0, active);                // 2. müzik kanalı (özel bölge / boss) senkronu ('SMU1')
        revive_tick(out.count > 0, active);             // ölüm/canlanma (Seçenek A: diğer oyuncunun yanında 100 HP canlan)
        level_sync_tick(out.level, out.count > 0);      // asıl .wmb harita geçişi (orman.wmb -> tunel.wmb) senkronu
        hud_tick(out.level, out.count > 0, active);     // Co-op HUD/Overlay verisi + ping/RTT + baş üstü isim + F5 ışınlanma
        // Karakter materyal cubemap (skin1@+252) koruması: client'ta g_07faa159 erken sıfırlandığı için SUPERCAN_GOVDE/CANTA/SWORD
        // material->skin1 NULL kalıp karakter tüm bölümlerde koyu/mat görünüyordu -> 0x7fcd231[0] cubemap'ini otomatik onar!
        if (out.count > 0 && g_mtl_off > 0 && (g_frame % 60) == 15) {
            static void (__cdecl *p_b2c)(void *);
            DWORD ebx = ebx_base();
            if (ebx) {
                void *cmap = 0;
                __try { cmap = *(void **)((BYTE *)ebx + 0x28d1ec); } __except (EXCEPTION_EXECUTE_HANDLER) { cmap = 0; }
                if (cmap) {
                    DWORD k;
                    if (!p_b2c) p_b2c = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "bmap_to_cubemap");
                    for (k = 0; k < g_local_count; k++) {
                        ENTITY lc; char lt[32];
                        if (!safe_ent(g_local_ent[k], &lc)) continue;
                        safe_str(lc.type, lt, sizeof lt);
                        if (!_stricmp(lt, "SUPERCAN_GOVDE.MDL") || !_stricmp(lt, "SUPERCAN_CANTA.MDL") || !_stricmp(lt, "SUPERCAN_SWORD.MDL")) {
                            __try {
                                BYTE *m = *(BYTE **)((BYTE *)g_local_ent[k] + g_mtl_off);
                                if (m && *(void **)(m + 252) == 0) {
                                    *(void **)(m + 252) = cmap;
                                    if (p_b2c) p_b2c(cmap);
                                    if (!_stricmp(out.level, "tunel.wmb") && !_stricmp(lt, "SUPERCAN_GOVDE.MDL")) {
                                        *(var *)((BYTE *)ebx + 0x26a114) = 1 << 10;   // g_07faa159 = 1 -> supercan_tunel_mtrl(0) shader+cubemap yenile
                                        boss_call_script("supercan_tunel_mtrl", 0, 0);
                                    }
                                    logf("[gfx-fix] restored %s mtl=%p cubemap skin1=%p\n", lt, m, cmap);
                                }
                            } __except (EXCEPTION_EXECUTE_HANDLER) {}
                        }
                    }
                }
            }
        }
        // Grafik/ışık bozulması teşhis logu (oyun haritasına girip out.count > 0 olduktan 120 kare sonra bir kez)
        {   static char gfx_lvl[64]; static long gfx_due;
            if (!out.count || !_stricmp(out.level, "menu.wmb")) {
                gfx_lvl[0] = 0; gfx_due = 0;
            } else if (strcmp(gfx_lvl, out.level)) {
                strncpy(gfx_lvl, out.level, sizeof gfx_lvl - 1); gfx_lvl[sizeof gfx_lvl - 1] = 0;
                gfx_due = g_frame + 120;
            }
            if (gfx_due && g_frame >= gfx_due) {
                DWORD ebx = ebx_base();
                gfx_due = 0;
                if (ebx) __try {
                    long shd = *(var *)((BYTE *)ebx + 0x260bcc) >> 10;   // g_07fa0c11 (gölge ayarı)
                    long q25 = *(var *)((BYTE *)ebx + 0x260be0) >> 10;   // g_07fa0c25
                    long gam = *(var *)((BYTE *)ebx + 0x260af4) >> 10;   // g_07fa0b39 (gamma)
                    long b45 = *(var *)((BYTE *)ebx + 0x260b00) >> 10;   // g_07fa0b45
                    void *sv = *(void **)((BYTE *)ebx + 0x28d194);       // g_07fcd1d9 (sc_smSunSetup view)
                    DWORD svf = sv ? *(DWORD *)((BYTE *)sv + 28) : 0;
                    var *sl = engine_var("sun_light"), *sc = engine_var("sun_color"), *ac = engine_var("ambient_color");
                    logf("[gfx] lvl='%s' sub=%ld shd=%ld q25=%ld gam=%ld b45=%ld sunView=%p(f=%08lx) sun_light=%.1f sun=(%.0f,%.0f,%.0f) amb=(%.0f,%.0f,%.0f) ctrl=%ld lpos=(%.0f,%.0f,%.0f) lrng=%.0f\n",
                         out.level, G_SUB_LVL(ebx) >> 10, shd, q25, gam, b45, sv, svf,
                         sl ? VF(*sl) : -1.0,
                         sc ? VF(sc[0]) : -1.0, sc ? VF(sc[1]) : -1.0, sc ? VF(sc[2]) : -1.0,
                         ac ? VF(ac[0]) : -1.0, ac ? VF(ac[1]) : -1.0, ac ? VF(ac[2]) : -1.0,
                         G_CTRL_ENABLED(ebx) >> 10,
                         VF(G_LIGHT_POS(ebx)[0]), VF(G_LIGHT_POS(ebx)[1]), VF(G_LIGHT_POS(ebx)[2]), VF(G_LIGHT_RNG(ebx)));
                    {   DWORD k;
                        for (k = 0; k < g_local_count; k++) {
                            ENTITY lc; char lt[32];
                            if (!safe_ent(g_local_ent[k], &lc)) continue;
                            safe_str(lc.type, lt, sizeof lt);
                            if (!_stricmp(lt, "SUPERCAN_GOVDE.MDL") || !_stricmp(lt, "SUPERCAN_YUZ.MDL")) {
                                BYTE *m = g_mtl_off > 0 ? *(BYTE **)((BYTE *)g_local_ent[k] + g_mtl_off) : 0;
                                float *ms = m ? (float *)(m + 96) : 0;
                                void *sk1 = m ? *(void **)(m + 252) : 0;
                                logf("[gfx-char] %s fl=%08lx amb=%.1f alb=%.1f mtl=%p sk1=%p msk=(%.2f,%.2f,%.2f,%.2f | %.2f,%.2f,%.2f)\n",
                                     lt, lc.flags, VF(*(var *)((BYTE *)g_local_ent[k] + 68)), VF(*(var *)((BYTE *)g_local_ent[k] + 72)),
                                     m, sk1,
                                     ms ? ms[0] : -1.f, ms ? ms[1] : -1.f, ms ? ms[2] : -1.f, ms ? ms[3] : -1.f,
                                     ms ? ms[8] : -1.f, ms ? ms[9] : -1.f, ms ? ms[10] : -1.f);
                            }
                        }
                    }
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
    }
    if (g_frame % 600 == 0) {
        DWORD ebx = ebx_base();
        int itm_act = 0, itm_done = 0, itm_tot = 0, k;
        long mkr_st = -1, alrm_st = -1;
        double sc = -1, dm = -1, rbt = -1, env = -1, nano = -1;
        for (k = 0; k < MAX_SYNC_ITEMS; k++) {
            if (!g_itm_kind[k]) continue;
            itm_tot++;
            if (g_itm_prev[k] == 1) itm_act++; else if (g_itm_prev[k] == 0) itm_done++;
        }
        if (ebx) __try {
            mkr_st = G_MKR_STATE(ebx) >> 10; alrm_st = G_ALRM_STATE(ebx) >> 10;
            sc = VF(G_SCORE(ebx)); dm = VF(G_DIAMONDS(ebx));
            rbt = VF(G_TRASH_RBT(ebx)); env = VF(G_TRASH_ENV(ebx)); nano = VF(G_NANO(ebx));
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        logf("[coop] f%ld level='%s' local_parts=%lu rx=%lu remote_level='%s' remote_parts=%lu | tx=%lu avg=%luB max=%luB trunc=%lu recdrop=%lu\n",
             g_frame, out.level, out.count, g_rx_count, g_remote.level, g_remote.count,
             g_tx_count, g_tx_count ? g_tx_bytes / g_tx_count : 0, g_tx_max, g_tx_trunc, g_rec_dropped);
        logf("[sync] hit=%lu/%lu phit=%lu/%lu mkr=%ld alrm=%ld itm=%d/%d(tot %d) | score=%.0f diam=%.0f rbt=%.0f env=%.0f nano=%.0f\n",
             g_hit_tx, g_hit_rx_n, g_phit_tx, g_phit_rx_n, mkr_st, alrm_st, itm_act, itm_done, itm_tot, sc, dm, rbt, env, nano);
        logf("[interp] frames=%lu held=%lu clk_resets=%lu snaps=%d delay=%.0fms late_used=%lu late_old=%lu simdrop=%lu\n", g_interp_frames, g_interp_hold, g_clk_resets, g_nsnap, g_interp_delay * 1000, g_snap_late_used, g_snap_late_old, g_sim_dropped);
        g_interp_frames = g_interp_hold = 0;
        g_tx_count = g_tx_bytes = g_tx_max = 0; }
    // diagnostics: puppet vs remote values, and our own local parts (for comparing the two logs)
    if (g_frame % 240 == 0) {
        DWORD k;
        for (k = 0; k < g_remote.count && k < MAX_PARTS; k++) {
            ENTITY pc; NETPART *p = &g_remote.part[k]; long pose = 0, bones = -1;
            if (!g_puppet[k] || !safe_ent(g_puppet[k], &pc)) continue;
            pose = safe_long((BYTE *)g_puppet[k] + POSE_OFF);
            if (p_ent_bones) __try { bones = p_ent_bones(g_puppet[k]) >> 10; } __except (EXCEPTION_EXECUTE_HANDLER) { bones = -2; }
            logf("[diag] puppet%lu %-20s pos=(%.1f %.1f %.1f) want=(%.1f %.1f %.1f) flags=%08lx want=%08lx pose=%.0f scale=(%.2f %.2f %.2f) frame=%.2f bones=%ld anim=%ld\n",
                 k, p->type, VF(pc.x), VF(pc.y), VF(pc.z), p->x, p->y, p->z, pc.flags, p->flags, VF(pose),
                 VF(pc.scale_x), VF(pc.scale_y), VF(pc.scale_z), VF(pc.frame), bones, p->animated);
        }
        for (k = 0; k < g_local_count; k++) {
            ENTITY lc; char lt[32]; long bones = -1;
            if (!safe_ent(g_local_ent[k], &lc)) continue; safe_str(lc.type, lt, sizeof lt);
            if (p_ent_bones) __try { bones = p_ent_bones(g_local_ent[k]) >> 10; } __except (EXCEPTION_EXECUTE_HANDLER) { bones = -2; }
            logf("[diag] local%lu   %-20s pos=(%.1f %.1f %.1f) flags=%08lx pose=%.0f scale=(%.2f %.2f %.2f) frame=%.2f bones=%ld\n",
                 k, lt, VF(lc.x), VF(lc.y), VF(lc.z), lc.flags, VF(safe_long((BYTE *)g_local_ent[k] + POSE_OFF)),
                 VF(lc.scale_x), VF(lc.scale_y), VF(lc.scale_z), VF(lc.frame), bones);
        }
    }
}

// ======================= WORLD WATCH (research for enemy/world sync) =======================
// watch=1 in coop.ini (or COOP_WATCH=1): every 0.5 s walk all entities and log what changes in the world, so we can
// tell enemies / pickups / breakables apart. Read-only. Log tags: [watch] (events), [wsum] (per-type summary every
// 10 s), [wtask] (script function running on an entity, first time seen), [wskill] (skill[] changes, capped).
// F11 writes a MARK line with the player position (press it when something interesting happens).
#define W_MAX 8192
#define W_SKILL_OFF 148
#define W_NSKILL 100
typedef struct { ENTITY *e; char type[32]; float x0, y0, z0, x, y, z, pan; long flags; long seen; float moved;
                 int nskill_log, nflag_log, moving, logged_move; var skill[W_NSKILL]; char task[40]; } WENT;
static WENT g_w[W_MAX]; static int g_wn; static int g_watch = -1; static double g_w_next, g_w_sum;
static long g_w_gen; static char g_w_level[32]; static float g_plx, g_ply, g_plz;
double __cdecl sqrt(double); double __cdecl fabs(double);   /* no math.h (clashes with our logf) */
static int g_w_hint; static char g_w_matched[W_MAX];
static int w_find_i(ENTITY *e) {   // entity order is stable between scans: try the slot after the last hit first
    int i; if (g_w_hint < g_wn && g_w[g_w_hint].e == e) return g_w_hint++;
    for (i = 0; i < g_wn; i++) if (g_w[i].e == e) { g_w_hint = i + 1; return i; } return -1; }
static WENT *w_find(ENTITY *e) { int i = w_find_i(e); return i < 0 ? 0 : &g_w[i]; }
static float w_dist(const WENT *w) { float dx = w->x - g_plx, dy = w->y - g_ply, dz = w->z - g_plz; return (float)sqrt(dx * dx + dy * dy + dz * dz); }
static void w_read_skills(ENTITY *e, var *out) { __try { memcpy(out, (BYTE *)e + W_SKILL_OFF, sizeof(var) * W_NSKILL); } __except (EXCEPTION_EXECUTE_HANDLER) { memset(out, 0, sizeof(var) * W_NSKILL); } }
static void w_player(void) {
    long t = 0; void *pp = safe_getvar("player", &t); ENTITY *pl, c;
    pl = pp ? (ENTITY *)safe_long(pp) : 0;
    if (!pl || (long)pl == 0x7badbad) pl = g_local_count ? g_local_ent[0] : 0;   // this game leaves `player` NULL: use our own body part
    if (pl && safe_ent(pl, &c)) { g_plx = (float)VF(c.x); g_ply = (float)VF(c.y); g_plz = (float)VF(c.z); }
}
static void w_tasks(void) {   // remember which script function runs on which entity (task "me")
    long n; if (!p_engine_gettaskinfo) return;
    for (n = 0; n < 4000; n++) {
        void *a = 0, *b = 0, *r = safe_task(n, &a, &b); char fn[40]; WENT *w;
        if (!r && !a && !b) break;
        if (!b || b == (void *)-1 || !(w = w_find((ENTITY *)b))) continue;
        safe_str((char *)a, fn, sizeof fn);
        if (fn[0] && strcmp(w->task, fn)) {
            logf("[wtask] %-28s on %-22s %p pos=(%.0f %.0f %.0f) dist=%.0f\n", fn, w->type, w->e, w->x, w->y, w->z, w_dist(w));
            strcpy(w->task, fn); }
    }
}
static void watch_tick(void) {
    static WENT nw[W_MAX]; int nn = 0, i, j, born = 0, gone = 0, bulk; ENTITY *e, c; char lvl[32]; double t = now_s();
    if (g_watch < 0) { char b[8]; g_watch = cfg("COOP_WATCH", "watch", b, sizeof b) && b[0] == '1'; if (g_watch) logf("[watch] world watch ON\n"); }
    if (!g_watch || !p_ent_next || t < g_w_next) return;
    g_w_next = t + 0.5; g_w_gen++; g_w_hint = 0; memset(g_w_matched, 0, sizeof g_w_matched);
    current_level(lvl, sizeof lvl); w_player();
    if (strcmp(lvl, g_w_level)) { logf("[watch] level '%s' -> '%s' (table reset)\n", g_w_level, lvl); strcpy(g_w_level, lvl); g_wn = 0; g_w_sum = t + 10; }
    for (e = safe_next(0); e && e != (ENTITY *)-1 && nn < W_MAX; e = safe_next(e)) {
        WENT *w, *o; char ty[32];
        if (!safe_ent(e, &c) || is_puppet(e) || is_wpuppet(e)) continue;
        safe_str(c.type, ty, sizeof ty);
        if (is_player_part(ty)) continue;
        w = &nw[nn++]; { int oi = w_find_i(e); o = oi >= 0 ? &g_w[oi] : 0; if (o && !strcmp(o->type, ty)) g_w_matched[oi] = 1; else o = 0; }
        if (o) *w = *o; else { memset(w, 0, sizeof *w); w->e = e; strcpy(w->type, ty); w->x0 = (float)VF(c.x); w->y0 = (float)VF(c.y); w->z0 = (float)VF(c.z);
            w->x = w->x0; w->y = w->y0; w->z = w->z0; w->flags = c.flags; w->pan = (float)VF(c.pan); w_read_skills(e, w->skill); w->seen = -1; born++; }
        w->seen = w->seen == -1 ? -1 : g_w_gen;
    }
    bulk = born > 200 || g_wn == 0;   // level load: don't log thousands of spawns
    // removed entities
    for (i = 0; i < g_wn; i++) {
        WENT *o = &g_w[i];
        if (!g_w_matched[i]) { gone++; if (!bulk) logf("[watch] GONE  %-22s %p pos=(%.0f %.0f %.0f) dist=%.0f moved=%.0f task=%s\n", o->type, o->e, o->x, o->y, o->z, w_dist(o), o->moved, o->task); }
    }
    // spawns, moves, flag and skill changes
    for (j = 0; j < nn; j++) {
        WENT *w = &nw[j]; var sk[W_NSKILL]; float x, y, z, d;
        if (!safe_ent(w->e, &c)) continue;
        x = (float)VF(c.x); y = (float)VF(c.y); z = (float)VF(c.z);
        if (w->seen == -1) { w->seen = g_w_gen; if (!bulk) logf("[watch] NEW   %-22s %p pos=(%.0f %.0f %.0f) dist=%.0f flags=%08lx idx=%08lx\n", w->type, w->e, x, y, z, w_dist(w), c.flags, c.link.index); continue; }
        d = (float)sqrt((x - w->x) * (x - w->x) + (y - w->y) * (y - w->y) + (z - w->z) * (z - w->z));
        w->moving = d > 1.0f || fabs(VF(c.pan) - w->pan) > 1.0;
        w->moved += d;
        if (w->moving && !w->logged_move) { w->logged_move = 1;
            logf("[watch] MOVES %-22s %p from=(%.0f %.0f %.0f) now=(%.0f %.0f %.0f) dist=%.0f task=%s\n", w->type, w->e, w->x0, w->y0, w->z0, x, y, z, w_dist(w), w->task); }
        w->x = x; w->y = y; w->z = z; w->pan = (float)VF(c.pan);
        if (c.flags != w->flags) {
            if (w->nflag_log < 6 && ++w->nflag_log) logf("[watch] FLAGS %-22s %p %08lx -> %08lx (changed %08lx) dist=%.0f\n", w->type, w->e, w->flags, c.flags, w->flags ^ c.flags, w_dist(w));
            w->flags = c.flags; }
        w_read_skills(w->e, sk);
        if (memcmp(sk, w->skill, sizeof sk)) {
            if (w->nskill_log < 12 && (w->moving || w->task[0])) {
                char line[512]; int k, p = 0; w->nskill_log++;
                for (k = 0; k < W_NSKILL && p < 440; k++) if (sk[k] != w->skill[k]) p += sprintf(line + p, " s%d:%.2f->%.2f", k + 1, VF(w->skill[k]), VF(sk[k]));
                logf("[wskill] %-22s %p%s\n", w->type, w->e, line); }
            memcpy(w->skill, sk, sizeof sk); }
    }
    memcpy(g_w, nw, nn * sizeof(WENT)); g_wn = nn;
    if (bulk && born) logf("[watch] bulk load: %d entities (level '%s')\n", born, lvl);
    w_tasks();
    if (t >= g_w_sum) {   // per-type summary: only types where something moves
        char seenty[512][32]; int nt = 0, k;
        g_w_sum = t + 10;
        logf("[wsum] ---- %d entities, player=(%.0f %.0f %.0f)\n", g_wn, g_plx, g_ply, g_plz);
        for (j = 0; j < g_wn; j++) {
            int tot = 0, mov = 0, ever = 0; float nearest_d = 1e9f;
            if (!g_w[j].logged_move) continue;
            for (k = 0; k < nt && strcmp(seenty[k], g_w[j].type); k++);
            if (k < nt || nt >= 512) continue;
            strcpy(seenty[nt++], g_w[j].type);
            for (k = 0; k < g_wn; k++) if (!strcmp(g_w[k].type, g_w[j].type)) { tot++; mov += g_w[k].moving; ever += g_w[k].logged_move; if (w_dist(&g_w[k]) < nearest_d) nearest_d = w_dist(&g_w[k]); }
            logf("[wsum] %-22s n=%d moving_now=%d ever_moved=%d nearest=%.0f\n", g_w[j].type, tot, mov, ever, nearest_d);
        }
    }
}

static void intro_skip_install(void) {
    static int done; static void *(__cdecl *p_gs)(char *);
    static const BYTE pro[9] = {0x55,0x53,0x56,0x57,0xE8,0,0,0,0};
    char b[8]; BYTE *a = 0; DWORD old; int ok = 0;
    if (done) return;
    if (cfg("COOP_SKIP_INTRO", "skip_intro", b, sizeof b) && b[0] == '0') { done = 1; return; }
    if (!p_gs) p_gs = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
    if (!p_gs) return;
    __try { a = (BYTE *)p_gs("Oyun_intro_start"); } __except (EXCEPTION_EXECUTE_HANDLER) { a = 0; }
    if (!a) return;
    done = 1;
    __try { ok = !memcmp(a, pro, 9); } __except (EXCEPTION_EXECUTE_HANDLER) { ok = 0; }
    if (!ok) { logf("[intro] Oyun_intro_start at %p: prologue mismatch, not skipping\n", a); return; }
    VirtualProtect(a, 1, PAGE_EXECUTE_READWRITE, &old);
    a[0] = 0xC3;   // ret: açılış Sobee videosunu ve NVIDIA/splash logolarını atla (Oyun_video_ac ara sahnelerine dokunmaz)
    VirtualProtect(a, 1, old, &old); FlushInstructionCache(GetCurrentProcess(), a, 1);
    logf("[intro] Oyun_intro_start skipped at %p (skip_intro=1)\n", a);
}
static void remote_tick(void);
static int g_f9, g_f10, g_f11;
static int __cdecl hook_engine_frame(void) {
    if (g_frame < 5) intro_skip_install();
    int r = real_engine_frame();
    g_frame++;
    if (g_frame == 1) { logf("first engine_frame returned %d\n", r); dump_vars(); }
    if (g_frame == 100) { HMODULE ack = GetModuleHandleA("acknex.dll"); install_anim_hooks(ack); install_spy_hooks(ack); install_sched_hooks(ack); boss_ai_hooks_install(); }
    coop_tick();
    watch_tick();
    remote_tick();
    {   int foc = hud_focused();
        int f9 = foc && (GetAsyncKeyState(VK_F9) & 0x8000) != 0, f10 = foc && (GetAsyncKeyState(VK_F10) & 0x8000) != 0, f11 = foc && (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
        int ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        if (f9 && !g_f9) {
            if (ctrl) { logf("---- Ctrl+F9 dump\n"); dump_vars(); dump_entities(); dump_tasks(); }
            else { g_hud.enabled = !g_hud.enabled; logf("[hud] F9 toggle -> %s\n", g_hud.enabled ? "ON" : "off"); }
        }
        if (f10 && !g_f10) { logf("---- F10 player\n"); log_player(); }
        if (f11 && !g_f11) { w_player(); logf("[watch] ======== MARK (F11) frame %ld player=(%.0f %.0f %.0f)\n", g_frame, g_plx, g_ply, g_plz); }
        g_f9 = f9; g_f10 = f10; g_f11 = f11; }
    return r;
}

// IAT hook on the main exe ---------------------------------------------------
static int hook_iat(HMODULE mod, const char *dll, const char *fn, void *newfn, void **orig) {
    BYTE *base = (BYTE *)mod;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + dd.VirtualAddress);
    for (; imp->Name; imp++) {
        IMAGE_THUNK_DATA *ilt, *iat;
        if (_stricmp((char *)(base + imp->Name), dll)) continue;
        ilt = (IMAGE_THUNK_DATA *)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        iat = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
        for (; ilt->u1.AddressOfData; ilt++, iat++) {
            IMAGE_IMPORT_BY_NAME *ibn;
            if (IMAGE_SNAP_BY_ORDINAL(ilt->u1.Ordinal)) continue;
            ibn = (IMAGE_IMPORT_BY_NAME *)(base + ilt->u1.AddressOfData);
            if (strcmp((char *)ibn->Name, fn)) continue;
            { DWORD old; VirtualProtect(&iat->u1.Function, 4, PAGE_READWRITE, &old);
              *orig = (void *)iat->u1.Function; iat->u1.Function = (DWORD)(ULONG_PTR)newfn;
              VirtualProtect(&iat->u1.Function, 4, old, &old); }
            return 1;
        }
    }
    return 0;
}

// API tracer (acknex.dll IAT + GetProcAddress results for lite-C) -------------
#include <tlhelp32.h>
static void *(WINAPI *o_GetProcAddress)(HMODULE, LPCSTR);
static HANDLE (WINAPI *o_CreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static HANDLE (WINAPI *o_CreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static LONG (WINAPI *o_RegOpenKeyExA)(HKEY, LPCSTR, DWORD, REGSAM, PHKEY);
static LONG (WINAPI *o_RegQueryValueExA)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
static LONG (WINAPI *o_RegSetValueExA)(HKEY, LPCSTR, DWORD, DWORD, const BYTE *, DWORD);
static HWND (WINAPI *o_FindWindowA)(LPCSTR, LPCSTR);
static HANDLE (WINAPI *o_OpenMutexA)(DWORD, BOOL, LPCSTR);
static HANDLE (WINAPI *o_CreateMutexA)(LPSECURITY_ATTRIBUTES, BOOL, LPCSTR);
static DWORD (WINAPI *o_GetEnvironmentVariableA)(LPCSTR, LPSTR, DWORD);
static int (WINAPI *o_MessageBoxA)(HWND, LPCSTR, LPCSTR, UINT);
static HANDLE (WINAPI *o_OpenFileMappingA)(DWORD, BOOL, LPCSTR);
static HANDLE (WINAPI *o_CreateToolhelp32Snapshot)(DWORD, DWORD);
static BOOL (WINAPI *o_Process32Next)(HANDLE, LPPROCESSENTRY32);
static DWORD (WINAPI *o_GetPrivateProfileStringA)(LPCSTR, LPCSTR, LPCSTR, LPSTR, DWORD, LPCSTR);

#define RA ((void*)_ReturnAddress())
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)
static void caller_name(void *ra, char *out, int n) {
    void *o = 0, *nm = 0; out[0] = 0;
    if (p_engine_getscriptinfo) { nm = safe_scriptinfo(ra, &o); safe_str((char *)nm, out, n); }
}
// The game rewrites its graphics settings file User\<player>_ga.gen (menu + exit). On this PC under the co-op DLL the
// rewritten file breaks lighting/graphics on the next start (2026-11-15, twice). keep_ga=1 (default): writes go to
// <file>.oyunyazdi instead, so the known-good file stays as it is and the game's version is kept for analysis.
static int is_ga_settings(const char *f) {
    size_t n = f ? strlen(f) : 0; char low[MAX_PATH];
    if (n < 8 || n >= MAX_PATH) return 0;
    strcpy(low, f); _strlwr(low);
    return !strcmp(low + n - 7, "_ga.gen") && strstr(low, "user\\") != 0;
}
static int g_api_log = -1;
static int api_log_on(void) {
    if (g_api_log < 0) { char b[8]; g_api_log = cfg("COOP_API_LOG", "api_log", b, sizeof b) && b[0] == '1'; }
    return g_api_log;
}
static HANDLE WINAPI h_CreateFileA(LPCSTR f, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD c, DWORD fl, HANDLE t) {
    HANDLE h; static int keep = -1;
    if (keep < 0) { char b[8]; keep = !(cfg("COOP_KEEP_GA", "keep_ga", b, sizeof b) && b[0] == '0'); }
    if (keep && (a & GENERIC_WRITE) && is_ga_settings(f)) {
        char alt[MAX_PATH + 16]; sprintf(alt, "%s.oyunyazdi", f);
        h = o_CreateFileA(alt, a, s, sa, c, fl, t);
        logf("[ga] game writes settings '%s' -> redirected to '%s' (keep_ga=1) -> %p\n", f, alt, h);
        return h;
    }
    h = o_CreateFileA(f, a, s, sa, c, fl, t);
    if (api_log_on()) logf("[api] CreateFileA('%s', acc=%08lx, disp=%lu) -> %p err=%lu ra=%p\n", f ? f : "", a, c, h, GetLastError(), _ReturnAddress());
    return h;
}
static HANDLE WINAPI h_CreateFileW(LPCWSTR f, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD c, DWORD fl, HANDLE t) {
    HANDLE h = o_CreateFileW(f, a, s, sa, c, fl, t);
    if (api_log_on()) logf("[api] CreateFileW('%ls', acc=%08lx, disp=%lu) -> %p\n", f ? f : L"", a, c, h);
    return h;
}
static LONG WINAPI h_RegOpenKeyExA(HKEY k, LPCSTR sub, DWORD o, REGSAM sam, PHKEY r) {
    LONG x = o_RegOpenKeyExA(k, sub, o, sam, r);
    if (api_log_on()) { char cn[64]; caller_name(_ReturnAddress(), cn, 64); logf("[api] RegOpenKeyExA(%p,'%s') -> %ld  caller='%s'\n", k, sub ? sub : "", x, cn); }
    return x;
}
static LONG WINAPI h_RegQueryValueExA(HKEY k, LPCSTR n, LPDWORD r, LPDWORD t, LPBYTE d, LPDWORD cb) {
    LONG x; char v[128] = ""; char cn[64];
    // Launcher handshake ("Hata 22"): the launcher writes ohc=<token> and the game consumes it.
    // Answer with the captured token so the game can be started directly (and several copies at once).
    // The token appears to be time-dependent (18249 accepted 14:43-14:59, rejected 15:05), so:
    // keep a real launcher-written value (and log it with the time, to learn the formula), else use COOP_OHC.
    if (n && !strcmp(n, "ohc") && d && cb && *cb >= 8) {
        DWORD cap = *cb; SYSTEMTIME st; char env[32], tf[MAX_PATH];
        GetTempPathA(MAX_PATH, tf); strcat(tf, "supercan_coop_token.txt");
        x = o_RegQueryValueExA(k, n, r, t, d, cb);
        GetLocalTime(&st);
        if (!x && d[0] && strcmp((char *)d, "0")) {
            // Copy launched by the real launcher: share its fresh token so a second copy can start directly
            // (the launcher terminates any running game copy, so only one copy may go through it).
            FILE *f = fopen(tf, "w"); if (f) { fputs((char *)d, f); fclose(f); }
            logf("[ohc] launcher token '%s' at %02d:%02d:%02d, shared via %s\n", (char *)d, st.wHour, st.wMinute, st.wSecond, tf);
            return x;
        }
        if (GetEnvironmentVariableA("COOP_OHC", env, sizeof env) && strlen(env) < cap) {
            strcpy((char *)d, env); *cb = (DWORD)strlen(env) + 1; if (t) *t = REG_SZ;
            logf("[ohc] using COOP_OHC '%s' at %02d:%02d\n", env, st.wHour, st.wMinute); return ERROR_SUCCESS;
        }
        {   WIN32_FILE_ATTRIBUTE_DATA fa; FILETIME now; ULARGE_INTEGER a, b; FILE *f;
            GetSystemTimeAsFileTime(&now);
            if (GetFileAttributesExA(tf, GetFileExInfoStandard, &fa)) {
                a.LowPart = fa.ftLastWriteTime.dwLowDateTime; a.HighPart = fa.ftLastWriteTime.dwHighDateTime;
                b.LowPart = now.dwLowDateTime; b.HighPart = now.dwHighDateTime;
                if (b.QuadPart - a.QuadPart < 10ULL * 60 * 10000000 && (f = fopen(tf, "r"))) {   // younger than 10 min
                    if (fgets(env, sizeof env, f) && env[0] && strlen(env) < cap) {
                        fclose(f); strcpy((char *)d, env); *cb = (DWORD)strlen(env) + 1; if (t) *t = REG_SZ;
                        logf("[ohc] using token shared by the launcher-started copy: '%s'\n", env); return ERROR_SUCCESS;
                    }
                    fclose(f);
                }
            }
        }
        logf("[ohc] no token (registry '%s', no COOP_OHC) at %02d:%02d\n", x ? "" : (char *)d, st.wHour, st.wMinute);
        return x;
    }
    x = o_RegQueryValueExA(k, n, r, t, d, cb);
    if (api_log_on()) { caller_name(_ReturnAddress(), cn, 64); if (!x && d && cb) safe_str((char *)d, v, sizeof v);
        logf("[api] RegQueryValueExA('%s') -> %ld val='%s' caller='%s'\n", n ? n : "", x, v, cn); }
    return x;
}
static LONG WINAPI h_RegSetValueExA(HKEY k, LPCSTR n, DWORD r, DWORD t, const BYTE *d, DWORD cb) {
    if (api_log_on()) { char v[128] = ""; char cn[64]; caller_name(_ReturnAddress(), cn, 64); if (d) safe_str((const char *)d, v, sizeof v);
        logf("[api] RegSetValueExA('%s','%s') caller='%s'\n", n ? n : "", v, cn); }
    return o_RegSetValueExA(k, n, r, t, d, cb);
}
static HWND WINAPI h_FindWindowA(LPCSTR c, LPCSTR w) {
    HWND h = o_FindWindowA(c, w);
    if (api_log_on()) { char cn[64]; caller_name(_ReturnAddress(), cn, 64);
        logf("[api] FindWindowA(class='%s', title='%s') -> %p caller='%s'\n", ((DWORD_PTR)c > 0xffff && c) ? c : "", w ? w : "", h, cn); }
    return h;
}
static HANDLE WINAPI h_OpenMutexA(DWORD a, BOOL i, LPCSTR n) {
    HANDLE h = o_OpenMutexA(a, i, n); if (api_log_on()) logf("[api] OpenMutexA('%s') -> %p err=%lu\n", n ? n : "", h, GetLastError()); return h;
}
static HANDLE WINAPI h_CreateMutexA(LPSECURITY_ATTRIBUTES s, BOOL o, LPCSTR n) {
    HANDLE h = o_CreateMutexA(s, o, n); if (api_log_on()) logf("[api] CreateMutexA('%s') -> %p err=%lu\n", n ? n : "", h, GetLastError()); return h;
}
static DWORD WINAPI h_GetEnvironmentVariableA(LPCSTR n, LPSTR b, DWORD s) {
    DWORD r = o_GetEnvironmentVariableA(n, b, s); if (api_log_on()) logf("[api] GetEnvironmentVariableA('%s') -> %lu '%s'\n", n ? n : "", r, (r && b) ? b : ""); return r;
}
static int WINAPI h_MessageBoxA(HWND w, LPCSTR t, LPCSTR c, UINT f) {
    char cn[64]; caller_name(_ReturnAddress(), cn, 64);
    logf("[api] MessageBoxA('%s','%s') ra=%p caller='%s'\n", t ? t : "", c ? c : "", _ReturnAddress(), cn);
    { char e[8]; if (GetEnvironmentVariableA("COOP_QUIET", e, sizeof e)) { logf("[api] (quiet: not shown)\n"); return IDOK; } }
    dump_vars(); dump_tasks();
    return o_MessageBoxA(w, t, c, f);
}
static HANDLE WINAPI h_OpenFileMappingA(DWORD a, BOOL i, LPCSTR n) {
    HANDLE h = o_OpenFileMappingA(a, i, n); if (api_log_on()) logf("[api] OpenFileMappingA('%s') -> %p\n", n ? n : "", h); return h;
}
static HANDLE WINAPI h_CreateToolhelp32Snapshot(DWORD f, DWORD p) {
    if (api_log_on()) logf("[api] CreateToolhelp32Snapshot(%lx,%lu)\n", f, p); return o_CreateToolhelp32Snapshot(f, p);
}
static BOOL WINAPI h_Process32Next(HANDLE h, LPPROCESSENTRY32 e) {
    BOOL r = o_Process32Next(h, e); if (r && api_log_on()) logf("[api] Process32Next -> pid=%lu ppid=%lu '%s'\n", e->th32ProcessID, e->th32ParentProcessID, e->szExeFile); return r;
}
static DWORD WINAPI h_GetPrivateProfileStringA(LPCSTR s, LPCSTR k, LPCSTR d, LPSTR o, DWORD n, LPCSTR f) {
    DWORD r = o_GetPrivateProfileStringA(s, k, d, o, n, f);
    if (api_log_on()) logf("[api] GetPrivateProfileStringA([%s] %s @ %s) -> '%s'\n", s ? s : "", k ? k : "", f ? f : "", o ? o : ""); return r;
}


#include "core/core_d3d_input.inc"

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r) {
    if (reason == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); init(h); }
    else if (reason == DLL_PROCESS_DETACH) { g_closing = 1; logf("unload frame=%ld\n", g_frame); if (g_log) fclose(g_log); }
    return TRUE;
}
