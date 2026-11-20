// [SC1] Olum/Canlanma (Secenek A) ve .wmb harita gecis senkronu
static HWND (WINAPI *o_GetForegroundWindow)(void);
static int hud_focused(void) { return (!g_hwnd || !o_GetForegroundWindow) ? 1 : (o_GetForegroundWindow() == g_hwnd); }
static void hud_toast(const char *msg, double sec);
static int g_revive_break_olme;
// Ölüm / Canlanma (Seçenek A): Co-op aktifken canı 1'in altına düşen oyuncu Game Over / checkpoint yüklemesi olmadan
// diğer oyuncunun (peer) yanında 100 HP + 3 sn dokunulmazlıkla canlanır.
// pfcan_mod_olme (decomp.c:106100) aynı karede s36<=1<<10 görür görmez başlar ve while(iStack_4b4<92/100 && g_07fab1c1!=0)
// döngüsü bitince Oyun_gameover_ac çağırır; bunu kırmak için 1 kare G_CTRL_ENABLED=0 yapıp (pfcan_mod_pasif'e düşürür)
// ertesi kare G_CTRL_ENABLED=1<<10 ile pfcan_mod_bekle'ye temiz geçiş yaptırıyoruz.
static void revive_tick(int in_level, int peer_active) {
    static var (__cdecl *pX_setpos)(ENTITY *, var *);
    DWORD ebx = ebx_base(); ENTITY *pb, *cpup = 0, pc; var s36, ghp; double t = now_s(); int i;
    if (g_revive_break_olme && ebx) {
        g_revive_break_olme = 0;
        __try {
            G_CTRL_ENABLED(ebx) = 1 << 10;
            *(var *)((BYTE *)ebx + 0x264630) = -1 << 10;   // g_07fa4675 = -1 (su/uçurum ölüm nedenini sıfırla)
            *(var *)((BYTE *)ebx + 0x26094c) = 1 << 10;    // g_07fa0991 = 1 (HUD açık)
            *(DWORD *)(0x7d4ccad + 28) &= ~16384u;         // kararma panelini gizle
            *(var *)(0x7d4ccad + 32) = 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    if (!in_level || !peer_active || !ebx) return;
    pb = player_box();
    if (!pb) return;
    __try { if (G_CTRL_ENABLED(ebx) == 0) return; } __except (EXCEPTION_EXECUTE_HANDLER) {}   // diyalog / sinematik sırasında ışınlama yapma
    if (t < g_revive_invuln_until) {
        __try {
            var *sk = (var *)((BYTE *)pb + 148);
            sk[34] = 100 << 10; sk[35] = 100 << 10;
            G_PLAYER_HP(ebx) = 100 << 10;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        return;
    }
    __try {
        s36 = ((var *)((BYTE *)pb + 148))[35];
        ghp = G_PLAYER_HP(ebx);
        if (G_DEATH_TYPE(ebx) != (-1 << 10)) return;   // su / uçurum ölümü: oyunun kendi ölümü işlesin, arkadaş yanına ışınlama
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (s36 > (1 << 10) && ghp > (1 << 10)) return;
    for (i = 0; i < MAX_PARTS; i++) {
        if (g_puppet[i] && !_stricmp(g_puppet_type[i], "SUPERCAN_GOVDE.MDL")) { cpup = g_puppet[i]; break; }
    }
    if (!cpup) cpup = g_puppet[0];
    if (!cpup || !safe_ent(cpup, &pc)) return;
    if (!pX_setpos) pX_setpos = (void *)GetProcAddress(GetModuleHandleA("ackphysX.dll"), "pXent_setposition");
    __try {
        var *sk = (var *)((BYTE *)pb + 148);
        var pos[3] = { pc.x, pc.y, pc.z + (40 << 10) };
        sk[34] = 100 << 10; sk[35] = 100 << 10;
        if (sk[36] == (6 << 10)) sk[36] = 0;
        G_PLAYER_HP(ebx) = 100 << 10;
        *(var *)((BYTE *)ebx + 0x264630) = -1 << 10;   // g_07fa4675 = -1
        *(var *)((BYTE *)ebx + 0x26094c) = 1 << 10;    // g_07fa0991 = 1
        *(DWORD *)(0x7d4ccad + 28) &= ~16384u;
        *(var *)(0x7d4ccad + 32) = 0;
        G_CTRL_ENABLED(ebx) = 0;                       // pfcan_mod_olme döngüsünü ilk wait(1)'de kır (iStack_4b0==0 iken)
        g_revive_break_olme = 1;
        pb->x = pos[0]; pb->y = pos[1]; pb->z = pos[2];   // SUPERCAN_BOX c_move karakter kutusu (decomp.c:78780)
        if (pX_setpos) pX_setpos(pb, pos);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    g_revive_invuln_until = t + 3.0;
    hud_toast("CANLANDIN! (3 SN DOKUNULMAZ)", 2.5);
    logf("[revive] local player downed (s36=%.1f ghp=%.1f) -> revived near peer at (%.0f %.0f %.0f) (3s invuln)\n",
         VF(s36), VF(ghp), VF(pc.x), VF(pc.y), VF(pc.z + (40 << 10)));
}

// Asıl .wmb harita geçişi senkronu: bir oyuncu orman.wmb -> tunel.wmb geçerse diğerini de otomatik geçirir
static void level_sync_tick(const char *level, int in_level) {
    static char last_lvl[64];
    static DWORD tunel_seen_ms;
    static int tunel_triggered;
    if (strcmp(last_lvl, level)) {
        logf("[lvl] level changed '%s' -> '%s'\n", last_lvl, level);
        strncpy(last_lvl, level, sizeof last_lvl - 1); last_lvl[sizeof last_lvl - 1] = 0;
        tunel_seen_ms = 0; tunel_triggered = 0;
    }
    if (!in_level || !g_have_remote || GetTickCount() - g_last_rx_tick >= 2000) { tunel_seen_ms = 0; return; }
    if (!_stricmp(level, "orman.wmb") && !_stricmp(g_remote.level, "tunel.wmb") && !tunel_triggered) {
        DWORD now = GetTickCount();
        if (!tunel_seen_ms) tunel_seen_ms = now;
        else if (now - tunel_seen_ms >= 2000) {
            static void *(__cdecl *p_gs)(char *);
            void *fn;
            tunel_triggered = 1;
            if (!p_gs) p_gs = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
            if (!p_callscript4) p_callscript4 = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_callscript4");
            fn = p_gs ? p_gs("main_alarm_load") : 0;
            logf("[lvl] peer entered 'tunel.wmb' -> triggering main_alarm_load (fn=%p callscript4=%p)\n", fn, p_callscript4);
            if (fn && p_callscript4) __try { p_callscript4((long)fn, 0, 0, 0, 0); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    } else {
        tunel_seen_ms = 0;
    }
}
