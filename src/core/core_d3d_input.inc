// [CORE] D3D9 pencere/tam ekran, h_wndproc kapanis korumasi, uzaktan kumanda, DirectInput8 vekili, IAT kancalari

// Optional windowed mode (D3D9) — sadece coop.ini içinde window=full veya window=x,y,w,h verilirse etkin!
#include <d3d9.h>
static int g_win_x = -1, g_win_y, g_win_w, g_win_h, g_force_windowed;   // from COOP_WINDOW / window=
static HWND g_hwnd;
static UINT g_bb_w, g_bb_h;
typedef HRESULT (STDMETHODCALLTYPE *CreateDevice_t)(IDirect3D9 *, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *, IDirect3DDevice9 **);
typedef HRESULT (STDMETHODCALLTYPE *Reset_t)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *);
static CreateDevice_t o_CreateDevice; static Reset_t o_Reset;
static IDirect3D9 *(WINAPI *o_Direct3DCreate9)(UINT);

static void force_windowed(D3DPRESENT_PARAMETERS *pp, const char *who) {
    logf("[d3d] %s: %ux%u fmt=%d windowed=%d refresh=%u interval=%x hwnd=%p (force=%d)\n", who, pp->BackBufferWidth, pp->BackBufferHeight,
         pp->BackBufferFormat, pp->Windowed, pp->FullScreen_RefreshRateInHz, pp->PresentationInterval, pp->hDeviceWindow, g_force_windowed);
    // Sadece window=full veya window=x,y,w,h açıkça ayarlandıysa pencere moduna zorla;
    // aksi halde (window=off veya ayar yoksa) oyunun kendi tam ekran/çözünürlük ayarına hiç dokunma!
    if (g_force_windowed) {
        pp->Windowed = TRUE; pp->FullScreen_RefreshRateInHz = 0;
    }
    g_bb_w = pp->BackBufferWidth; g_bb_h = pp->BackBufferHeight;
}
// Window subclass: scale client-space mouse message coordinates from window size to backbuffer size,
// so clicks land where GetCursorPos-based hover says they are.
static WNDPROC g_orig_wndproc;
static int g_click_logs;
static int bg_run(void); static void remote_hook_device(IDirect3DDevice9 *dev);
static LRESULT CALLBACK h_wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CLOSE || m == WM_DESTROY) {
        static void *(__cdecl *p_gs)(char *);
        DWORD ebx = ebx_base(); BYTE *dr = 0; DWORD old;
        g_closing = 1;
        if (ebx) __try { G_GAMMA_MODE(ebx) = 2 << 10; } __except (EXCEPTION_EXECUTE_HANDLER) {}
        if (!p_gs) p_gs = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
        if (p_gs) __try { dr = (BYTE *)p_gs("devicereset"); } __except (EXCEPTION_EXECUTE_HANDLER) { dr = 0; }
        if (dr && VirtualProtect(dr, 1, PAGE_EXECUTE_READWRITE, &old)) {
            dr[0] = 0xC3;   // kapanışta devicereset (+0x37 bmap_zbuffer ve +0x135) çökmesini tamamen önle
            VirtualProtect(dr, 1, old, &old); FlushInstructionCache(GetCurrentProcess(), dr, 1);
        }
    }
    if (bg_run()) {   // keep running in the background (two test copies, automated tests): swallow deactivation
        if (m == WM_ACTIVATEAPP && !w) return 0;
        if (m == WM_ACTIVATE && LOWORD(w) == WA_INACTIVE) return 0;
        if (m == WM_KILLFOCUS) return 0;
    }
    if (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST && m != WM_MOUSEWHEEL && g_bb_w && g_win_w && g_bb_w != (UINT)g_win_w) {
        int x = (short)LOWORD(l), y = (short)HIWORD(l);
        int sx = MulDiv(x, g_bb_w, g_win_w), sy = MulDiv(y, g_bb_h, g_win_h);
        if (m != WM_MOUSEMOVE && g_click_logs++ < 40) logf("[wnd] mouse msg %04x (%d,%d) -> (%d,%d)\n", m, x, y, sx, sy);
        l = MAKELPARAM(sx, sy);
    }
    return CallWindowProcA(g_orig_wndproc, h, m, w, l);
}
static void place_window(HWND h) {
    if (!h) return; g_hwnd = h;
    // window=off modunda da kapanış devicereset crash koruması için h_wndproc subclass'ını tak
    if (!g_orig_wndproc) {
        g_orig_wndproc = (WNDPROC)SetWindowLongA(h, GWL_WNDPROC, (LONG)(LONG_PTR)h_wndproc);
        logf("[wnd] subclassed %p (orig %p)\n", h, g_orig_wndproc);
    }
    if (g_win_x < 0) { RECT r; GetWindowRect(h, &r); logf("[d3d] window %p at %ld,%ld %ldx%ld (no COOP_WINDOW)\n", h, r.left, r.top, r.right - r.left, r.bottom - r.top); return; }
    SetWindowLongA(h, GWL_STYLE, (GetWindowLongA(h, GWL_STYLE) & ~(WS_CAPTION | WS_THICKFRAME)) | WS_POPUP | WS_VISIBLE);
    SetWindowPos(h, HWND_NOTOPMOST, g_win_x, g_win_y, g_win_w, g_win_h, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    logf("[d3d] window %p placed at %d,%d %dx%d\n", h, g_win_x, g_win_y, g_win_w, g_win_h);
}
static HRESULT STDMETHODCALLTYPE h_Reset(IDirect3DDevice9 *dev, D3DPRESENT_PARAMETERS *pp) {
    HRESULT hr; D3DPRESENT_PARAMETERS c = *pp; force_windowed(&c, "Reset"); hr = o_Reset(dev, &c);
    logf("[d3d] Reset -> %08lx\n", hr);
    if (hr == 0) g_d3d_reset_gen++;
    place_window(g_hwnd); return hr;
}
static void patch_vt(void **vt, int idx, void *fn, void **orig) {
    DWORD old; VirtualProtect(&vt[idx], 4, PAGE_EXECUTE_READWRITE, &old);
    if (!*orig) *orig = vt[idx]; if (vt[idx] != fn) vt[idx] = fn; VirtualProtect(&vt[idx], 4, old, &old);
}
static HRESULT STDMETHODCALLTYPE h_CreateDevice(IDirect3D9 *d, UINT ad, D3DDEVTYPE t, HWND w, DWORD f, D3DPRESENT_PARAMETERS *pp, IDirect3DDevice9 **out) {
    HRESULT hr; D3DPRESENT_PARAMETERS c = *pp; force_windowed(&c, "CreateDevice");
    hr = o_CreateDevice(d, ad, t, w, f, &c, out);
    logf("[d3d] CreateDevice -> %08lx dev=%p\n", hr, (hr == 0 && out) ? *out : 0);
    if (hr == 0 && out && *out) { patch_vt(*(void ***)*out, 16, h_Reset, (void **)&o_Reset); remote_hook_device(*out); }
    place_window(pp->hDeviceWindow ? pp->hDeviceWindow : w);
    return hr;
}
static IDirect3D9 *WINAPI h_Direct3DCreate9(UINT sdk) {
    IDirect3D9 *d = o_Direct3DCreate9(sdk);
    logf("[d3d] Direct3DCreate9(%u) -> %p\n", sdk, d);
    if (d) patch_vt(*(void ***)d, 16, h_CreateDevice, (void **)&o_CreateDevice);
    return d;
}

// Mouse scaling for shrunken windows: the engine renders at backbuffer size, the window is smaller.

static int g_mouse_focus = 1, g_win_full;
// ======================= REMOTE CONTROL (automated tests, local/tools/autotest.ps1) =======================
// Game\coop_cmd.txt is polled ~8x/s; its lines are run in order on the game thread and the file is deleted.
// Commands (coordinates in backbuffer pixels, e.g. 1920x1080):
//   click X Y        fake cursor at X,Y + left button down/up (no real mouse movement)
//   move X Y         fake cursor only;   cursor off   real cursor again
//   tp X Y Z         teleport the local player (physics box via pXent_setposition)
//   shot PATH        save the next presented frame (JPG) to PATH
//   mark TEXT        log line;   dump   entity/task dump (like F9)
// Game\coop_status.txt is rewritten every second: frame, level, player pos, peer/puppet state.
// bg_run=1 (coop.ini, default 1): the window keeps running when it is not focused (activation messages swallowed).
static POINT g_fake_pt; static int g_fake_cur, g_click_up_at, g_bg_run = -1;
static BYTE g_inj_key[256]; static long g_inj_key_until[256]; static int g_inj_btn; static long g_inj_btn_until;
static char g_shot_path[MAX_PATH]; static int g_shot_req;
typedef HRESULT (STDMETHODCALLTYPE *Present_t)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);
static Present_t o_Present;
static HRESULT (WINAPI *p_D3DXSaveSurfaceToFileA)(const char *, DWORD, IDirect3DSurface9 *, const PALETTEENTRY *, const RECT *);
static DWORD g_n_present, g_n_scpresent, g_n_endscene;
static IDirect3DDevice9 *g_dev;
static void shot_save(IDirect3DSurface9 *bb, const char *who) {
    HRESULT hr = -1; g_shot_req = 0;
    if (!p_D3DXSaveSurfaceToFileA) { HMODULE x = LoadLibraryA("d3dx9_42.dll"); if (!x) x = LoadLibraryA("d3dx9_30.dll");
        if (x) p_D3DXSaveSurfaceToFileA = (void *)GetProcAddress(x, "D3DXSaveSurfaceToFileA"); }
    if (p_D3DXSaveSurfaceToFileA && bb) hr = p_D3DXSaveSurfaceToFileA(g_shot_path, 1 /* D3DXIFF_JPG */, bb, 0, 0);
    logf("[cmd] shot (%s) '%s' -> %08lx\n", who, g_shot_path, hr);
}
// The runtime swaps the device vtable after Reset: keep the original Present per vtable and re-patch from the tick.
static struct { void **vt; Present_t orig; } g_pvt[8]; static int g_npvt;
static Present_t present_orig(IDirect3DDevice9 *dev) { int i; void **vt = *(void ***)dev; for (i = 0; i < g_npvt; i++) if (g_pvt[i].vt == vt) return g_pvt[i].orig; return o_Present; }
// Saf D3D9 Co-op HUD / Overlay çizim motoru (harici DLL / texture / D3DPOOL_DEFAULT kaynağı yok, Reset güvenli)
typedef struct { float x, y, z, rhw; DWORD color; } HUDVTX;
#define HUD_FVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)
#define HUD_MAX_VTX 16384
static HUDVTX g_hud_vtx[HUD_MAX_VTX];
static int g_hud_nvtx;

// 5x7 piksel bitmap font: ASCII 32 (' ') .. 95 ('_'), her satırda en soldaki piksel bit 4 (0x10)
static const BYTE g_font5x7[64][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x04,0x04,0x04,0x04,0x04,0x00,0x04}, // '!'
    {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00}, // '"'
    {0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0x00}, // '#'
    {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04}, // '$'
    {0x18,0x19,0x02,0x04,0x08,0x13,0x03}, // '%'
    {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D}, // '&'
    {0x04,0x04,0x00,0x00,0x00,0x00,0x00}, // '''
    {0x02,0x04,0x08,0x08,0x08,0x04,0x02}, // '('
    {0x08,0x04,0x02,0x02,0x02,0x04,0x08}, // ')'
    {0x00,0x04,0x15,0x0E,0x15,0x04,0x00}, // '*'
    {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}, // '+'
    {0x00,0x00,0x00,0x00,0x00,0x04,0x08}, // ','
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, // '-'
    {0x00,0x00,0x00,0x00,0x00,0x04,0x04}, // '.'
    {0x01,0x01,0x02,0x04,0x08,0x10,0x10}, // '/'
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, // '0'
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, // '1'
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}, // '2'
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}, // '3'
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, // '4'
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, // '5'
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, // '6'
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, // '7'
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, // '8'
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, // '9'
    {0x00,0x04,0x04,0x00,0x04,0x04,0x00}, // ':'
    {0x00,0x04,0x04,0x00,0x04,0x04,0x08}, // ';'
    {0x02,0x04,0x08,0x10,0x08,0x04,0x02}, // '<'
    {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}, // '='
    {0x08,0x04,0x02,0x01,0x02,0x04,0x08}, // '>'
    {0x0E,0x11,0x01,0x02,0x04,0x00,0x04}, // '?'
    {0x0E,0x11,0x17,0x15,0x17,0x10,0x0E}, // '@'
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, // 'A'
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, // 'B'
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, // 'C'
    {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}, // 'D'
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, // 'E'
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, // 'F'
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, // 'G'
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, // 'H'
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, // 'I'
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, // 'J'
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, // 'K'
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, // 'L'
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, // 'M'
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, // 'N'
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, // 'O'
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, // 'P'
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, // 'Q'
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, // 'R'
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, // 'S'
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, // 'T'
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, // 'U'
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, // 'V'
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}, // 'W'
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, // 'X'
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, // 'Y'
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, // 'Z'
    {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}, // '['
    {0x10,0x10,0x08,0x04,0x02,0x01,0x01}, // '\'
    {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}, // ']'
    {0x04,0x0A,0x11,0x00,0x00,0x00,0x00}, // '^'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}  // '_'
};

static void hud_push_rect(float x, float y, float w, float h, DWORD col) {
    HUDVTX *v;
    if (g_hud_nvtx + 6 > HUD_MAX_VTX || w <= 0 || h <= 0) return;
    v = &g_hud_vtx[g_hud_nvtx];
    v[0].x = x;     v[0].y = y;     v[0].z = 0; v[0].rhw = 1; v[0].color = col;
    v[1].x = x + w; v[1].y = y;     v[1].z = 0; v[1].rhw = 1; v[1].color = col;
    v[2].x = x + w; v[2].y = y + h; v[2].z = 0; v[2].rhw = 1; v[2].color = col;
    v[3].x = x;     v[3].y = y;     v[3].z = 0; v[3].rhw = 1; v[3].color = col;
    v[4].x = x + w; v[4].y = y + h; v[4].z = 0; v[4].rhw = 1; v[4].color = col;
    v[5].x = x;     v[5].y = y + h; v[5].z = 0; v[5].rhw = 1; v[5].color = col;
    g_hud_nvtx += 6;
}

static void hud_push_border(float x, float y, float w, float h, float t, DWORD col) {
    hud_push_rect(x, y, w, t, col);
    hud_push_rect(x, y + h - t, w, t, col);
    hud_push_rect(x, y, t, h, col);
    hud_push_rect(x + w - t, y, t, h, col);
}

static void hud_push_text(float x, float y, float sc, DWORD col, const char *s) {
    float cx = x;
    if (!s) return;
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 'A');
        if (c >= 32 && c <= 95) {
            const BYTE *g = g_font5x7[c - 32];
            int row, colb;
            for (row = 0; row < 7; row++) {
                BYTE bits = g[row];
                if (!bits) continue;
                for (colb = 0; colb < 5; colb++) {
                    if (bits & (1 << (4 - colb))) {
                        hud_push_rect(cx + colb * sc, y + row * sc, sc, sc, col);
                    }
                }
            }
        }
        cx += 6.0f * sc;
    }
}

static void hud_draw_d3d9(IDirect3DDevice9 *dev) {
    D3DVIEWPORT9 vp; float bw, bh, px, py, pw, ph;
    DWORD fvf = 0, s_zen = 0, s_ab = 0, s_sb = 0, s_db = 0, s_cull = 0, s_lit = 0, s_fog = 0, s_scis = 0;
    DWORD t_cop = 0, t_ca1 = 0, t_aop = 0, t_aa1 = 0;
    IDirect3DVertexShader9 *vs = 0; IDirect3DPixelShader9 *ps = 0; IDirect3DBaseTexture9 *tex = 0;
    HUDSTATE hs; char buf[96];
    if (!dev || !g_hud.enabled) return;
    hs = g_hud;
    __try {
        memset(&vp, 0, sizeof vp);
        IDirect3DDevice9_GetViewport(dev, &vp);
        bw = vp.Width ? (float)vp.Width : (g_bb_w ? (float)g_bb_w : 1280.0f);
        bh = vp.Height ? (float)vp.Height : (g_bb_h ? (float)g_bb_h : 720.0f);
        g_hud_nvtx = 0;

        // 1) Sağ üst köşe Co-op paneli
        pw = 248.0f; ph = hs.connected ? 86.0f : 44.0f;
        px = bw - pw - 14.0f; py = 14.0f;
        hud_push_rect(px, py, pw, ph, 0xB0101622);
        hud_push_border(px, py, pw, ph, 1.5f, hs.connected ? 0xDD38A8FF : 0xCC999999);
        // Satır 1: Rol + Ping
        hud_push_text(px + 8, py + 7, 1.5f, hs.is_host ? 0xFF66E0FF : 0xFFFFCC55, hs.is_host ? "[HOST]" : "[CLIENT]");
        if (hs.connected) {
            DWORD pcol = hs.rtt_ms < 0 ? 0xFFBBBBBB : (hs.rtt_ms < 60 ? 0xFF55FF77 : (hs.rtt_ms < 120 ? 0xFFFFDD44 : 0xFFFF5555));
            if (hs.rtt_ms >= 0) sprintf(buf, "PING:%3dMS", hs.rtt_ms); else strcpy(buf, "PING: --");
            hud_push_text(px + pw - 96.0f, py + 7, 1.5f, pcol, buf);
            // Satır 2: Arkadaş adı + durum
            hud_push_text(px + 8, py + 23, 1.5f, 0xFFFFFFFF, hs.peer_name[0] ? hs.peer_name : "ARKADAS");
            if (!hs.same_level && hs.in_level) hud_push_text(px + pw - 96.0f, py + 23, 1.2f, 0xFFFFAA44, "BASKA BOLUM");
            else if (hs.peer_cutscene) hud_push_text(px + pw - 86.0f, py + 23, 1.2f, 0xFF88DDFF, "ARA SAHNE");
            else if (hs.peer_invuln) hud_push_text(px + pw - 86.0f, py + 23, 1.2f, 0xFF88FFAA, "DOKUNULMAZ");
            else if (hs.head_valid) { sprintf(buf, "%.0fM", hs.dist_m); hud_push_text(px + pw - 42.0f, py + 23, 1.2f, 0xFFCCCCCC, buf); }
            // Satır 3: CAN barı
            {   float bx = px + 46.0f, by = py + 39.0f, bwid = 152.0f, bh2 = 9.0f;
                DWORD hcol = hs.peer_hp > 50 ? 0xFF44DD55 : (hs.peer_hp > 25 ? 0xFFFFBB33 : 0xFFFF4444);
                hud_push_text(px + 8, by, 1.3f, 0xFFDDDDDD, "CAN");
                hud_push_rect(bx, by, bwid, bh2, 0xCC222833);
                hud_push_rect(bx, by, bwid * (hs.peer_hp / 100.0f), bh2, hcol);
                hud_push_border(bx, by, bwid, bh2, 1.0f, 0x99FFFFFF);
                sprintf(buf, "%3d", hs.peer_hp);
                hud_push_text(bx + bwid + 6.0f, by, 1.3f, 0xFFFFFFFF, buf);
            }
            // Satır 4: NANO barı
            {   float bx = px + 46.0f, by = py + 53.0f, bwid = 152.0f, bh2 = 9.0f;
                hud_push_text(px + 8, by, 1.3f, 0xFF88DDFF, "NANO");
                hud_push_rect(bx, by, bwid, bh2, 0xCC182838);
                hud_push_rect(bx, by, bwid * (hs.peer_nano / 100.0f), bh2, 0xFF22BBFF);
                hud_push_border(bx, by, bwid, bh2, 1.0f, 0x9988DDFF);
                sprintf(buf, "%3d", hs.peer_nano);
                hud_push_text(bx + bwid + 6.0f, by, 1.3f, 0xFFAAEEFF, buf);
            }
            // Satır 5: Kısayol ipucu
            hud_push_text(px + 8, py + 70, 1.15f, 0xFF99AABB, "F5:ISINLAN   F9:HUD GIZLE");
        } else {
            hud_push_text(px + 8, py + 24, 1.35f, 0xFFFFCC66, "ARKADAS BEKLENIYOR...");
        }

        // 2) Baş üstü isim + küçük can barı (ekrandaysa) veya ekran dışı yön/mesafe göstergesi
        if (hs.in_level && hs.same_level && hs.head_valid) {
            if (hs.head_on_screen) {
                const char *nm = hs.peer_name[0] ? hs.peer_name : "ARKADAS";
                float tw = (float)strlen(nm) * 6.0f * 1.4f;
                float bw2 = tw > 64.0f ? tw + 10.0f : 74.0f;
                float x0 = hs.head_x - bw2 * 0.5f, y0 = hs.head_y - 26.0f;
                DWORD hcol = hs.peer_hp > 50 ? 0xFF44EE55 : (hs.peer_hp > 25 ? 0xFFFFBB33 : 0xFFFF4444);
                hud_push_rect(x0, y0, bw2, 22.0f, 0x99101622);
                hud_push_text(hs.head_x - tw * 0.5f, y0 + 3.0f, 1.4f, 0xFFFFFFFF, nm);
                hud_push_rect(hs.head_x - 30.0f, y0 + 15.0f, 60.0f, 4.0f, 0xCC222222);
                hud_push_rect(hs.head_x - 30.0f, y0 + 15.0f, 60.0f * (hs.peer_hp / 100.0f), 4.0f, hcol);
            } else {
                const char *ar = hs.off_dir == 0 ? "< " : (hs.off_dir == 1 ? "> " : (hs.off_dir == 2 ? "^ " : "V "));
                sprintf(buf, "%s%.10s %.0fM", ar, hs.peer_name[0] ? hs.peer_name : "ARKADAS", hs.dist_m);
                {   float tw = (float)strlen(buf) * 6.0f * 1.4f + 12.0f;
                    hud_push_rect(hs.off_x, hs.off_y, tw, 18.0f, 0xB8101828);
                    hud_push_border(hs.off_x, hs.off_y, tw, 18.0f, 1.0f, 0xDD38A8FF);
                    hud_push_text(hs.off_x + 6.0f, hs.off_y + 4.0f, 1.4f, 0xFF88EEFF, buf);
                }
            }
        }

        // 3) Alt-orta bildirim (F5 ışınlanma / canlanma toast)
        if (hs.toast[0] && now_s() < hs.toast_until) {
            float tw = (float)strlen(hs.toast) * 6.0f * 1.7f + 24.0f;
            float tx = (bw - tw) * 0.5f, ty = bh - 68.0f;
            hud_push_rect(tx, ty, tw, 24.0f, 0xC8101828);
            hud_push_border(tx, ty, tw, 24.0f, 1.5f, 0xFFFFDD44);
            hud_push_text(tx + 12.0f, ty + 6.0f, 1.7f, 0xFFFFEE66, hs.toast);
        }

        if (g_hud_nvtx >= 3) {
            IDirect3DDevice9_GetFVF(dev, &fvf);
            IDirect3DDevice9_GetVertexShader(dev, &vs);
            IDirect3DDevice9_GetPixelShader(dev, &ps);
            IDirect3DDevice9_GetTexture(dev, 0, &tex);
            IDirect3DDevice9_GetRenderState(dev, D3DRS_ZENABLE, &s_zen);
            IDirect3DDevice9_GetRenderState(dev, D3DRS_ALPHABLENDENABLE, &s_ab);
            IDirect3DDevice9_GetRenderState(dev, D3DRS_SRCBLEND, &s_sb);
            IDirect3DDevice9_GetRenderState(dev, D3DRS_DESTBLEND, &s_db);
            IDirect3DDevice9_GetRenderState(dev, D3DRS_CULLMODE, &s_cull);
            IDirect3DDevice9_GetRenderState(dev, D3DRS_LIGHTING, &s_lit);
            IDirect3DDevice9_GetRenderState(dev, D3DRS_FOGENABLE, &s_fog);
            IDirect3DDevice9_GetRenderState(dev, D3DRS_SCISSORTESTENABLE, &s_scis);
            IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_COLOROP, &t_cop);
            IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_COLORARG1, &t_ca1);
            IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_ALPHAOP, &t_aop);
            IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, &t_aa1);

            if (SUCCEEDED(IDirect3DDevice9_BeginScene(dev))) {
                IDirect3DDevice9_SetVertexShader(dev, 0);
                IDirect3DDevice9_SetPixelShader(dev, 0);
                IDirect3DDevice9_SetTexture(dev, 0, 0);
                IDirect3DDevice9_SetFVF(dev, HUD_FVF);
                IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, FALSE);
                IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, TRUE);
                IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
                IDirect3DDevice9_SetRenderState(dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
                IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
                IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, FALSE);
                IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, FALSE);
                IDirect3DDevice9_SetRenderState(dev, D3DRS_SCISSORTESTENABLE, FALSE);
                IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
                IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
                IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLELIST, g_hud_nvtx / 3, g_hud_vtx, sizeof(HUDVTX));
                IDirect3DDevice9_EndScene(dev);
            }

            IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, t_cop);
            IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, t_ca1);
            IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, t_aop);
            IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, t_aa1);
            IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, s_zen);
            IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, s_ab);
            IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND, s_sb);
            IDirect3DDevice9_SetRenderState(dev, D3DRS_DESTBLEND, s_db);
            IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, s_cull);
            IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, s_lit);
            IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, s_fog);
            IDirect3DDevice9_SetRenderState(dev, D3DRS_SCISSORTESTENABLE, s_scis);
            IDirect3DDevice9_SetFVF(dev, fvf);
            IDirect3DDevice9_SetTexture(dev, 0, tex);
            IDirect3DDevice9_SetVertexShader(dev, vs);
            IDirect3DDevice9_SetPixelShader(dev, ps);
            if (tex) IDirect3DBaseTexture9_Release(tex);
            if (vs) IDirect3DVertexShader9_Release(vs);
            if (ps) IDirect3DPixelShader9_Release(ps);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static HRESULT STDMETHODCALLTYPE h_Present(IDirect3DDevice9 *dev, const RECT *a, const RECT *b, HWND w, const RGNDATA *r) {
    g_n_present++;
    if (g_shot_req) { IDirect3DSurface9 *bb = 0;
        if (SUCCEEDED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) { shot_save(bb, "device"); IDirect3DSurface9_Release(bb); } }
    return present_orig(dev)(dev, a, b, w, r);
}
static void present_repatch(void) {
    void **vt; DWORD old;
    if (!g_dev) return;
    __try { int i; vt = *(void ***)g_dev; if (vt[17] == (void *)h_Present || g_npvt >= 8) return;
        for (i = 0; i < g_npvt; i++) if (g_pvt[i].vt == vt) return;   // already ours once; someone else (overlay) took it over: leave it (no chains/loops)
        g_pvt[g_npvt].vt = vt; g_pvt[g_npvt].orig = (Present_t)vt[17]; g_npvt++;
        VirtualProtect(&vt[17], 4, PAGE_EXECUTE_READWRITE, &old); vt[17] = (void *)h_Present; VirtualProtect(&vt[17], 4, old, &old);
        logf("[cmd] Present re-hooked on vtable %p (#%d)\n", vt, g_npvt);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
typedef HRESULT (STDMETHODCALLTYPE *SCPresent_t)(IDirect3DSwapChain9 *, const RECT *, const RECT *, HWND, const RGNDATA *, DWORD);
static SCPresent_t o_SCPresent;
static HRESULT STDMETHODCALLTYPE h_SCPresent(IDirect3DSwapChain9 *sc, const RECT *a, const RECT *b, HWND w, const RGNDATA *r, DWORD f) {
    g_n_scpresent++;
    if (g_shot_req) { IDirect3DSurface9 *bb = 0;
        if (SUCCEEDED(IDirect3DSwapChain9_GetBackBuffer(sc, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) { shot_save(bb, "swapchain"); IDirect3DSurface9_Release(bb); } }
    return o_SCPresent(sc, a, b, w, r, f);
}
typedef HRESULT (STDMETHODCALLTYPE *EndScene_t)(IDirect3DDevice9 *);
static EndScene_t o_EndScene;
static HRESULT STDMETHODCALLTYPE h_EndScene(IDirect3DDevice9 *dev) { g_n_endscene++; return o_EndScene(dev); }
static void remote_hook_device(IDirect3DDevice9 *dev) {
    IDirect3DSwapChain9 *sc = 0; g_dev = dev;
    if (!o_Present) { patch_vt(*(void ***)dev, 17, h_Present, (void **)&o_Present); g_pvt[0].vt = *(void ***)dev; g_pvt[0].orig = o_Present; g_npvt = 1; }
    if (!o_EndScene) patch_vt(*(void ***)dev, 42, h_EndScene, (void **)&o_EndScene);
    if (SUCCEEDED(IDirect3DDevice9_GetSwapChain(dev, 0, &sc)) && sc) {
        if (!o_SCPresent) patch_vt(*(void ***)sc, 3, h_SCPresent, (void **)&o_SCPresent);
        IDirect3DSwapChain9_Release(sc); }
    logf("[cmd] d3d hooks: Present %p SCPresent %p EndScene %p\n", o_Present, o_SCPresent, o_EndScene);
}

static HWND (WINAPI *o_GetForegroundWindow)(void);
static HWND (WINAPI *o_GetActiveWindow)(void);
static HWND (WINAPI *o_GetFocus)(void);
static int bg_cfg(void) { if (g_bg_run < 0) { char b[8]; g_bg_run = !(cfg("COOP_BG_RUN", "bg_run", b, sizeof b) && b[0] == '0'); } return g_bg_run; }
static int bg_run(void) { return bg_cfg() && g_hwnd; }
static HWND WINAPI h_GetForegroundWindow(void) { return bg_run() ? g_hwnd : o_GetForegroundWindow(); }
static HWND WINAPI h_GetActiveWindow(void) { return bg_run() ? g_hwnd : o_GetActiveWindow(); }
static HWND WINAPI h_GetFocus(void) { return bg_run() ? g_hwnd : o_GetFocus(); }

static void post_mouse(UINT msg, int bx, int by) {   // backbuffer coords -> window client coords (h_wndproc scales back)
    int cx = (g_bb_w && g_win_w) ? MulDiv(bx, g_win_w, g_bb_w) : bx, cy = (g_bb_h && g_win_h) ? MulDiv(by, g_win_h, g_bb_h) : by;
    PostMessageA(g_hwnd, msg, msg == WM_LBUTTONDOWN ? MK_LBUTTON : 0, MAKELPARAM(cx, cy));
}
static int g_key_msg_down[256];
static void post_key(int dik, int down) {   // the engine may read the keyboard from window messages (no DirectInput keyboard)
    UINT vk = MapVirtualKeyA(dik, 1 /* MAPVK_VSC_TO_VK */); LPARAM l = 1 | ((LPARAM)dik << 16) | (down ? 0 : (3u << 30));
    if (!g_hwnd || (!down && !g_key_msg_down[dik])) return;
    g_key_msg_down[dik] = down;
    PostMessageA(g_hwnd, down ? WM_KEYDOWN : WM_KEYUP, vk, l);
}
static void cut_inject_esc(void) {
    g_inj_key[0x01] = 1; g_inj_key_until[0x01] = g_frame + 6; post_key(0x01, 1);
}
static ENTITY *player_box(void) {   // the SUPERCAN_BOX nearest to our body part
    ENTITY *e, c, *best = 0; char ty[32]; double bd = 1e18, px, py, pz;
    DWORD ebx = ebx_base();
    if (g_aim_real && is_entity((DWORD)g_aim_real)) return g_aim_real;
    if (!g_aim_active && ebx) __try {
        ENTITY *cur = G_PLAYER_BOX(ebx);
        if (cur && cur != g_aim_pup && is_entity((DWORD)cur)) return cur;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (!g_local_count || !safe_ent(g_local_ent[0], &c)) return 0;
    px = VF(c.x); py = VF(c.y); pz = VF(c.z);
    for (e = safe_next(0); e && e != (ENTITY *)-1; e = safe_next(e)) {
        double d; if (e == g_aim_pup || !safe_ent(e, &c)) continue; safe_str(c.type, ty, sizeof ty);
        if (_stricmp(ty, "SUPERCAN_BOX.MDL")) continue;
        d = (VF(c.x) - px) * (VF(c.x) - px) + (VF(c.y) - py) * (VF(c.y) - py) + (VF(c.z) - pz) * (VF(c.z) - pz);
        if (d < bd) { bd = d; best = e; }
    }
    return best;
}
// dumpall <dir>: the whole compiled lite-C program for offline reading. Writes every committed region of the allocation that
// holds the script code (region_<base>.bin), the allocation holding the symbol names, functions.txt (start, name; from
// engine_getscriptinfo over the code) and acknex_exports.txt (runtime address, name) so [ebx+...] engine calls can be named.
static void dump_region_set(const char *dir, void *any, FILE *idx) {
    MEMORY_BASIC_INFORMATION m; BYTE *p; void *base; char fn[MAX_PATH]; FILE *f;
    if (!VirtualQuery(any, &m, sizeof m)) return;
    base = m.AllocationBase;
    for (p = (BYTE *)base; VirtualQuery(p, &m, sizeof m) && m.AllocationBase == base; p = (BYTE *)m.BaseAddress + m.RegionSize) {
        if (m.State != MEM_COMMIT || (m.Protect & (PAGE_NOACCESS | PAGE_GUARD))) continue;
        sprintf(fn, "%s/region_%08lx.bin", dir, (DWORD)m.BaseAddress);
        if ((f = fopen(fn, "wb"))) { __try { fwrite(m.BaseAddress, 1, m.RegionSize, f); } __except (EXCEPTION_EXECUTE_HANDLER) {} fclose(f); }
        fprintf(idx, "region %08lx size %08lx protect %08lx alloc %08lx\n", (DWORD)m.BaseAddress, (DWORD)m.RegionSize, m.Protect, (DWORD)base);
    }
}
static void dump_all_code(const char *dirarg) {
    static void *(__cdecl *p_getscript)(char *); char dir[MAX_PATH], fn[MAX_PATH]; FILE *idx, *fx; BYTE *a, *lo, *hi, *p;
    MEMORY_BASIC_INFORMATION m; void *last = 0, *namep = 0; DWORD nfun = 0;
    sscanf(dirarg, "%259[^\r\n]", dir); CreateDirectoryA(dir, 0);
    if (!p_getscript) p_getscript = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
    a = 0; if (p_getscript) __try { a = p_getscript("AIF_GBR_yaralan"); } __except (EXCEPTION_EXECUTE_HANDLER) { a = 0; }
    if (!a || !p_engine_getscriptinfo || !VirtualQuery(a, &m, sizeof m)) { logf("[cmd] dumpall: no script code found\n"); return; }
    sprintf(fn, "%s/index.txt", dir); if (!(idx = fopen(fn, "w"))) { logf("[cmd] dumpall: cannot write %s\n", fn); return; }
    fprintf(idx, "AIF_GBR_yaralan %08lx\n", (DWORD)a);
    dump_region_set(dir, a, idx);
    // functions: scan the code allocation, a new start = a new function
    lo = (BYTE *)m.AllocationBase; hi = lo;
    for (p = lo; VirtualQuery(p, &m, sizeof m) && m.AllocationBase == lo; p = (BYTE *)m.BaseAddress + m.RegionSize) hi = (BYTE *)m.BaseAddress + m.RegionSize;
    sprintf(fn, "%s/functions.txt", dir); fx = fopen(fn, "w");
    for (p = lo; fx && p < hi; p += 4) {
        void *o = 0, *st = safe_scriptinfo(p, &o); char nm[96];
        if (!st || st == (void *)-1 || st == last || !o || o == (void *)-1) continue;
        last = st; safe_str((char *)o, nm, sizeof nm); if (!namep) namep = o;
        fprintf(fx, "%08lx %s\n", (DWORD)st, nm); nfun++;
    }
    if (fx) fclose(fx);
    if (namep) { VirtualQuery(namep, &m, sizeof m); if (m.AllocationBase != (void *)lo) { fprintf(idx, "names alloc:\n"); dump_region_set(dir, namep, idx); } }
    {   // acknex exports at runtime addresses
        HMODULE h = GetModuleHandleA("acknex.dll"); IMAGE_DOS_HEADER *d = (IMAGE_DOS_HEADER *)h; IMAGE_NT_HEADERS *nt; IMAGE_EXPORT_DIRECTORY *ex; DWORD i, *names, *funcs; WORD *ords;
        sprintf(fn, "%s/acknex_exports.txt", dir);
        if (h && (fx = fopen(fn, "w"))) { __try {
            nt = (IMAGE_NT_HEADERS *)((BYTE *)h + d->e_lfanew);
            ex = (IMAGE_EXPORT_DIRECTORY *)((BYTE *)h + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
            names = (DWORD *)((BYTE *)h + ex->AddressOfNames); funcs = (DWORD *)((BYTE *)h + ex->AddressOfFunctions); ords = (WORD *)((BYTE *)h + ex->AddressOfNameOrdinals);
            fprintf(fx, "base %08lx\n", (DWORD)h);
            for (i = 0; i < ex->NumberOfNames; i++) fprintf(fx, "%08lx %s\n", (DWORD)((BYTE *)h + funcs[ords[i]]), (char *)h + names[i]);
        } __except (EXCEPTION_EXECUTE_HANDLER) {} fclose(fx); }
    }
    fprintf(idx, "code %08lx..%08lx functions %lu\n", (DWORD)lo, (DWORD)hi, nfun); fclose(idx);
    logf("[cmd] dumpall -> %s: code %08lx..%08lx, %lu functions\n", dir, (DWORD)lo, (DWORD)hi, nfun);
}
static void remote_cmd(char *line) {
    float x, y, z; char arg[MAX_PATH];
    logf("[cmd] %s\n", line);
    if (sscanf(line, "clickent %259s", arg) == 1 || sscanf(line, "moveent %259s", arg) == 1) {
        // clickent <model file, e.g. menu_oyuna_basla.mdl>: click where that entity is on screen (menus move with the camera)
        static var *(__cdecl *p_vec_to_screen)(var *, void *); ENTITY *e, c; char ty[48]; int found = 0;
        long t = 0; void *cp = safe_getvar("camera", &t), *cam = cp ? (void *)safe_long(cp) : 0;
        if (!p_vec_to_screen) p_vec_to_screen = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "vec_to_screen");
        for (e = safe_next(0); e && e != (ENTITY *)-1 && !found; e = safe_next(e)) {
            if (!safe_ent(e, &c)) continue; safe_str(c.type, ty, sizeof ty);
            if (_stricmp(ty, arg)) continue;
            found = 1;
            {   var v[3]; var *r = 0; v[0] = c.x; v[1] = c.y; v[2] = c.z;
                if (p_vec_to_screen && cam) __try { r = p_vec_to_screen(v, cam); } __except (EXCEPTION_EXECUTE_HANDLER) { r = 0; }
                if (!r) { logf("[cmd] %s: not on screen\n", arg); break; }
                x = (float)VF(v[0]); y = (float)VF(v[1]);
                logf("[cmd] %s on screen at %.0f,%.0f\n", arg, x, y);
                g_fake_pt.x = (LONG)x; g_fake_pt.y = (LONG)y; g_fake_cur = 1; post_mouse(WM_MOUSEMOVE, (int)x, (int)y);
                if (line[0] == 'c') { post_mouse(WM_LBUTTONDOWN, (int)x, (int)y); g_click_up_at = g_frame + 4; g_inj_btn = 1; g_inj_btn_until = g_frame + 4; } }
        }
        if (!found) logf("[cmd] %s: no such entity\n", arg);
    } else if (sscanf(line, "click %f %f", &x, &y) == 2) {
        g_fake_pt.x = (LONG)x; g_fake_pt.y = (LONG)y; g_fake_cur = 1;
        post_mouse(WM_MOUSEMOVE, (int)x, (int)y); post_mouse(WM_LBUTTONDOWN, (int)x, (int)y); g_click_up_at = g_frame + 4;
        g_inj_btn = 1; g_inj_btn_until = g_frame + 4;   // DirectInput mouse button too
    } else if (sscanf(line, "move %f %f", &x, &y) == 2) {
        g_fake_pt.x = (LONG)x; g_fake_pt.y = (LONG)y; g_fake_cur = 1; post_mouse(WM_MOUSEMOVE, (int)x, (int)y);
    } else if (!strncmp(line, "cursor off", 10)) g_fake_cur = 0;
    else if (sscanf(line, "tp %f %f %f", &x, &y, &z) == 3) {
        static var (__cdecl *pX_setpos)(ENTITY *, var *); ENTITY *b = player_box(); var v[3];
        if (!pX_setpos) pX_setpos = (void *)GetProcAddress(GetModuleHandleA("ackphysX.dll"), "pXent_setposition");
        v[0] = (var)(x * 1024); v[1] = (var)(y * 1024); v[2] = (var)(z * 1024);
        if (b && pX_setpos) { __try { pX_setpos(b, v); logf("[cmd] teleported box %p\n", b); } __except (EXCEPTION_EXECUTE_HANDLER) { logf("[cmd] tp fault\n"); } }
        else logf("[cmd] tp: no player box (%p) or pXent_setposition (%p)\n", b, pX_setpos);
    } else if (sscanf(line, "shot %259[^\r\n]", arg) == 1) { strcpy(g_shot_path, arg); g_shot_req = 1; }
    else if (!strncmp(line, "key ", 4) || !strncmp(line, "keydown ", 8) || !strncmp(line, "keyup ", 6)) {
        // key <DIK scancode, e.g. 0x11 W, 0x39 space, 0x1C enter, 0x01 esc> [frames=6];  keydown <code> (until keyup);  keyup <code>
        char *q = strchr(line, ' ') + 1, *e; long code = strtol(q, &e, 0), fr = strtol(e, 0, 0);
        if (code > 0 && code < 256) {
            if (line[3] == 'u') { g_inj_key[code] = 0; post_key(code, 0); }
            else { g_inj_key[code] = 1; g_inj_key_until[code] = line[3] == 'd' ? 0x7fffffff : g_frame + (fr > 0 ? fr : 6); post_key(code, 1); } }
    }
    else if (!strncmp(line, "hit", 3)) {   // hit [step]: apply combo step 1..6 (s37 = 0..5<<10) to the nearest robot
        long dmg = atol(line + 3), s37; ENTITY *e, c, pc, *best = 0; char ty[32]; double bd = 1e18;
        if (dmg < 1) dmg = 1;
        s37 = (dmg >= 1 && dmg <= 6) ? ((dmg - 1) << 10) : dmg;
        if (g_local_count && safe_ent(g_local_ent[0], &pc)) {
            for (e = safe_next(0); e && e != (ENTITY *)-1; e = safe_next(e)) {
                double d; if (!safe_ent(e, &c)) continue; safe_str(c.type, ty, sizeof ty);
                if (!is_enemy_body_type(ty)) continue;
                d = (VF(c.x) - VF(pc.x)) * (VF(c.x) - VF(pc.x)) + (VF(c.y) - VF(pc.y)) * (VF(c.y) - VF(pc.y));
                if (d < bd) { bd = d; best = e; } }
        }
        if (best) hit_apply(best, s37); else logf("[cmd] hit: no robot\n");
    }
    else if (!strncmp(line, "watchhp", 7)) {   // data breakpoints on the nearest robot's GBR_BOX s34 s35 s37 s39
        ENTITY *e, c, pc, *best = 0; char ty[32]; double bd = 1e18; static const int sk[4] = {34, 35, 37, 39}; int i;
        if (g_local_count && safe_ent(g_local_ent[0], &pc))
            for (e = safe_next(0); e && e != (ENTITY *)-1; e = safe_next(e)) {
                double d; if (!safe_ent(e, &c)) continue; safe_str(c.type, ty, sizeof ty);
                if (_stricmp(ty, "GBR_BOX.MDL")) continue;
                d = (VF(c.x) - VF(pc.x)) * (VF(c.x) - VF(pc.x)) + (VF(c.y) - VF(pc.y)) * (VF(c.y) - VF(pc.y));
                if (d < bd) { bd = d; best = e; } }
        if (!best) logf("[cmd] watchhp: no robot\n");
        else { for (i = 0; i < 4; i++) { g_dbw_addr[i] = (DWORD)best + 148 + 4 * (sk[i] - 1); sprintf(g_dbw_lbl[i], "s%d", sk[i]); }
            for (i = 0; i < 4; i++) g_dbw_last[i] = sk_get(best, sk[i]);
            g_dbw_n = 4; g_dbw_hits = 0; logf("[cmd] watchhp on GBR_BOX %p\n", best); dbw_apply(); }
    }
    else if (!strncmp(line, "watchoff", 8)) { g_dbw_n = 0; dbw_apply(); }
    else if (!strncmp(line, "yaralan", 7)) {   // yaralan: me = nearest robot's GBR_BOX, run AIF_GBR_yaralan with no args (untested variant)
        static void *(__cdecl *p_getscript)(char *); ENTITY *e, c, pc, *best = 0; char ty[32]; double bd = 1e18; BYTE *fn = 0;
        if (!p_getscript) p_getscript = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
        if (g_local_count && safe_ent(g_local_ent[0], &pc))
            for (e = safe_next(0); e && e != (ENTITY *)-1; e = safe_next(e)) {
                double d; if (!safe_ent(e, &c)) continue; safe_str(c.type, ty, sizeof ty);
                if (_stricmp(ty, "GBR_BOX.MDL")) continue;
                d = (VF(c.x) - VF(pc.x)) * (VF(c.x) - VF(pc.x)) + (VF(c.y) - VF(pc.y)) * (VF(c.y) - VF(pc.y));
                if (d < bd) { bd = d; best = e; } }
        if (p_getscript) __try { fn = p_getscript("AIF_GBR_yaralan"); } __except (EXCEPTION_EXECUTE_HANDLER) { fn = 0; }
        if (!best || !fn) logf("[cmd] yaralan: box %p fn %p\n", best, fn);
        else logf("[cmd] yaralan on box %p -> %s\n", best, pz_call(fn, 0, 0, (DWORD)best, (DWORD)player_box()) ? "ok" : "FAULT");
    }
    else if (!strncmp(line, "dumpfn ", 7)) {
        // dumpfn <script function> <dir>: write 8 KB of its code to <dir>\<name>_<addr>.bin (+ ebx = lite-C engine table base)
        static void *(__cdecl *p_getscript)(char *); char nm[64], dir[MAX_PATH], fn2[MAX_PATH]; void *a; FILE *f;
        if (sscanf(line, "dumpfn %63s %259[^\r\n]", nm, dir) == 2) {
            if (!p_getscript) p_getscript = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
            a = 0; if (p_getscript) __try { a = p_getscript(nm); } __except (EXCEPTION_EXECUTE_HANDLER) { a = 0; }
            if (!a) logf("[cmd] dumpfn %s: not found\n", nm);
            else { sprintf(fn2, "%s/%s_%08lx.bin", dir, nm, (DWORD)a);
                if ((f = fopen(fn2, "wb"))) { __try { fwrite(a, 1, 0x2000, f); } __except (EXCEPTION_EXECUTE_HANDLER) {} fclose(f); }
                logf("[cmd] dumpfn %s at %p -> %s\n", nm, a, fn2); }
        }
    }
    else if (!strncmp(line, "dumpall ", 8)) dump_all_code(line + 8);
    else if (!strncmp(line, "dump", 4)) { dump_vars(); dump_entities(); dump_tasks(); }
    else if (!strncmp(line, "mark", 4)) {}
    else logf("[cmd] unknown command\n");
}
static void remote_tick(void) {
    static double next_poll, next_status; double t = now_s(); char fn[MAX_PATH];
    present_repatch();
    if (bg_run() && g_frame % 60 == 0) {   // never-focused second window: tell it it is active, keep window_focus = 1
        static int logged; var *wf = engine_var("window_focus");
        if (g_frame % 600 == 0 || !logged) { PostMessageA(g_hwnd, WM_ACTIVATEAPP, TRUE, 0); PostMessageA(g_hwnd, WM_ACTIVATE, WA_ACTIVE, 0); PostMessageA(g_hwnd, WM_SETFOCUS, 0, 0); }
        if (wf) { __try { if (!logged) { logged = 1; logf("[cmd] window_focus=%.0f (forcing 1)\n", VF(*wf)); } *wf = 1 << 10; } __except (EXCEPTION_EXECUTE_HANDLER) {} }
        else if (!logged) { logged = 1; logf("[cmd] no window_focus var\n"); }
    }
    if (g_shot_req && ++g_shot_req > 6 && g_dev) {   // no Present seen: grab back buffer 0 between frames
        IDirect3DSurface9 *bb = 0;
        if (SUCCEEDED(IDirect3DDevice9_GetBackBuffer(g_dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) { shot_save(bb, "tick"); IDirect3DSurface9_Release(bb); }
        else { g_shot_req = 0; logf("[cmd] shot failed: no back buffer\n"); } }
    if (g_frame % 600 == 300) logf("[cmd] d3d calls: Present=%lu SCPresent=%lu EndScene=%lu\n", g_n_present, g_n_scpresent, g_n_endscene);
    if (g_click_up_at && g_frame >= g_click_up_at) { post_mouse(WM_LBUTTONUP, g_fake_pt.x, g_fake_pt.y); g_click_up_at = 0; }
    { int k; for (k = 1; k < 256; k++) if (g_key_msg_down[k] && (!g_inj_key[k] || g_frame > g_inj_key_until[k])) post_key(k, 0); }
    if (t >= next_poll) {
        FILE *f; next_poll = t + 0.12;
        sprintf(fn, "%scoop_cmd.txt", g_dir);
        if (GetFileAttributesA(fn) != INVALID_FILE_ATTRIBUTES && (f = fopen(fn, "r"))) {
            char lines[32][MAX_PATH]; int n = 0, i;
            while (n < 32 && fgets(lines[n], MAX_PATH, f)) { char *e = lines[n] + strlen(lines[n]); while (e > lines[n] && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) *--e = 0; if (lines[n][0]) n++; }
            fclose(f); DeleteFileA(fn);
            for (i = 0; i < n; i++) remote_cmd(lines[i]);
        }
    }
    if (t >= next_status) {
        FILE *f; char lvl[32]; ENTITY c; float px = 0, py = 0, pz = 0; int np = 0, nw = 0, i;
        next_status = t + 1.0; current_level(lvl, sizeof lvl);
        if (g_local_count && safe_ent(g_local_ent[0], &c)) { px = (float)VF(c.x); py = (float)VF(c.y); pz = (float)VF(c.z); }
        for (i = 0; i < MAX_PARTS; i++) np += g_puppet[i] != 0;
        for (i = 0; i < WMAX_PARTS; i++) nw += g_wp[i].e != 0;
        sprintf(fn, "%scoop_status.txt", g_dir);
        if ((f = fopen(fn, "w"))) {
            fprintf(f, "frame=%ld\nlevel=%s\nlocal_parts=%lu\nplayer=%.0f %.0f %.0f\nrole=%s\nrx=%lu\npuppets=%d\nworld_local=%lu\nworld_puppets=%d\nblocked=%lu\n",
                    g_frame, lvl, g_local_count, px, py, pz, is_host() ? "host" : "client", g_rx_count, np, g_nwcand, nw, g_create_blocked);
            fclose(f); }
    }
}
static BOOL (WINAPI *o_GetCursorPos)(LPPOINT);
static BOOL WINAPI h_GetCursorPos(LPPOINT p) {
    BOOL r = o_GetCursorPos(p);
    if (r && g_fake_cur) { p->x = (g_win_x > 0 ? g_win_x : 0) + g_fake_pt.x; p->y = (g_win_y > 0 ? g_win_y : 0) + g_fake_pt.y; return r; }   // remote control
    // two test windows side by side: the window the cursor is not in sees it far outside (no second cursor, no stolen hover).
    // Only for explicit x,y,w,h windows (window=full / no window= is untouched); mouse_focus=0 turns it off.
    if (r && g_mouse_focus && g_win_x >= 0 && !g_win_full && g_win_w &&
        (p->x < g_win_x || p->x >= g_win_x + g_win_w || p->y < g_win_y || p->y >= g_win_y + g_win_h)) { p->x = -5000; p->y = -5000; return r; }
    if (r && g_win_x >= 0 && g_bb_w && g_win_w && g_bb_w != (UINT)g_win_w) {
        p->x = g_win_x + MulDiv(p->x - g_win_x, g_bb_w, g_win_w);
        p->y = g_win_y + MulDiv(p->y - g_win_y, g_bb_h, g_win_h);
    }
    return r;
}

// The cursor is reported in backbuffer-scaled space anchored at the window origin (see h_GetCursorPos). The engine
// also checks it against GetWindowRect/GetClientRect of its window: report those in the same space, or the cursor
// counts as outside past the real window width (right window: "cursor stops at the half"). mouse_rect=0 turns it off.
static int g_mouse_rect = -1;
static int scaled_window(HWND h) {
    if (g_mouse_rect < 0) { char b[8]; g_mouse_rect = !(cfg("COOP_MOUSE_RECT", "mouse_rect", b, sizeof b) && b[0] == '0'); }
    return g_mouse_rect && h && h == g_hwnd && g_win_x >= 0 && g_bb_w && g_win_w && g_bb_w != (UINT)g_win_w;
}
static BOOL (WINAPI *o_GetWindowRect)(HWND, LPRECT);
static BOOL (WINAPI *o_GetClientRect)(HWND, LPRECT);
static BOOL (WINAPI *o_SetCursorPos)(int, int);
static BOOL WINAPI h_GetWindowRect(HWND h, LPRECT r) {
    BOOL ok = o_GetWindowRect(h, r);
    if (ok && scaled_window(h)) { r->left = g_win_x; r->top = g_win_y; r->right = g_win_x + g_bb_w; r->bottom = g_win_y + g_bb_h; }
    return ok;
}
static BOOL WINAPI h_GetClientRect(HWND h, LPRECT r) {
    BOOL ok = o_GetClientRect(h, r);
    if (ok && scaled_window(h)) { r->left = 0; r->top = 0; r->right = g_bb_w; r->bottom = g_bb_h; }
    return ok;
}
static BOOL WINAPI h_SetCursorPos(int x, int y) {   // engine coordinates (scaled space) -> real screen
    if (scaled_window(g_hwnd)) { x = g_win_x + MulDiv(x - g_win_x, g_win_w, g_bb_w); y = g_win_y + MulDiv(y - g_win_y, g_win_h, g_bb_h); }
    return o_SetCursorPos(x, y);
}
typedef struct { const char *name; void *hook; void **orig; } HOOKDEF;
static HOOKDEF g_hooks[] = {
    {"CreateFileA", h_CreateFileA, (void **)&o_CreateFileA},
    {"CreateFileW", h_CreateFileW, (void **)&o_CreateFileW},
    {"RegOpenKeyExA", h_RegOpenKeyExA, (void **)&o_RegOpenKeyExA},
    {"RegQueryValueExA", h_RegQueryValueExA, (void **)&o_RegQueryValueExA},
    {"RegSetValueExA", h_RegSetValueExA, (void **)&o_RegSetValueExA},
    {"FindWindowA", h_FindWindowA, (void **)&o_FindWindowA},
    {"OpenMutexA", h_OpenMutexA, (void **)&o_OpenMutexA},
    {"CreateMutexA", h_CreateMutexA, (void **)&o_CreateMutexA},
    {"GetEnvironmentVariableA", h_GetEnvironmentVariableA, (void **)&o_GetEnvironmentVariableA},
    {"MessageBoxA", h_MessageBoxA, (void **)&o_MessageBoxA},
    {"OpenFileMappingA", h_OpenFileMappingA, (void **)&o_OpenFileMappingA},
    {"CreateToolhelp32Snapshot", h_CreateToolhelp32Snapshot, (void **)&o_CreateToolhelp32Snapshot},
    {"Process32Next", h_Process32Next, (void **)&o_Process32Next},
    {"GetPrivateProfileStringA", h_GetPrivateProfileStringA, (void **)&o_GetPrivateProfileStringA},
    {"Direct3DCreate9", h_Direct3DCreate9, (void **)&o_Direct3DCreate9},
    {"GetCursorPos", h_GetCursorPos, (void **)&o_GetCursorPos},
    {"GetForegroundWindow", h_GetForegroundWindow, (void **)&o_GetForegroundWindow},
    {"GetActiveWindow", h_GetActiveWindow, (void **)&o_GetActiveWindow},
    {"GetFocus", h_GetFocus, (void **)&o_GetFocus},
    {"GetWindowRect", h_GetWindowRect, (void **)&o_GetWindowRect},
    {"GetClientRect", h_GetClientRect, (void **)&o_GetClientRect},
    {"SetCursorPos", h_SetCursorPos, (void **)&o_SetCursorPos},
    {0, 0, 0}};

static HOOKDEF *find_hook(const char *n) {
    int i; for (i = 0; g_hooks[i].name; i++) if (!strcmp(g_hooks[i].name, n)) return &g_hooks[i]; return 0;
}
static void *WINAPI h_GetProcAddress(HMODULE m, LPCSTR n) {
    void *r = o_GetProcAddress(m, n);
    if ((DWORD_PTR)n > 0xffff) {
        HOOKDEF *hd = find_hook(n);
        if (api_log_on() || (hd && r)) {
            char mn[MAX_PATH] = ""; GetModuleFileNameA(m, mn, MAX_PATH);
            logf("[gpa] %s!%s -> %p%s\n", strrchr(mn, '\\') ? strrchr(mn, '\\') + 1 : mn, n, r, (hd && r) ? " (HOOKED)" : "");
        }
        if (hd && r) { if (!*hd->orig) *hd->orig = r; return hd->hook; }
    }
    return r;
}

// Patch every import of `mod` whose name we hook (any DLL).
static void hook_all_imports(HMODULE mod) {
    BYTE *base = (BYTE *)mod;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + dd.VirtualAddress);
    for (; imp->Name; imp++) {
        IMAGE_THUNK_DATA *ilt = (IMAGE_THUNK_DATA *)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        IMAGE_THUNK_DATA *iat = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
        for (; ilt->u1.AddressOfData; ilt++, iat++) {
            const char *n; HOOKDEF *hd; void *newfn; void **orig;
            if (IMAGE_SNAP_BY_ORDINAL(ilt->u1.Ordinal)) continue;
            n = (const char *)((IMAGE_IMPORT_BY_NAME *)(base + ilt->u1.AddressOfData))->Name;
            if (!strcmp(n, "GetProcAddress")) { newfn = h_GetProcAddress; orig = (void **)&o_GetProcAddress; }
            else if ((hd = find_hook(n))) { newfn = hd->hook; orig = hd->orig; }
            else continue;
            { DWORD old; VirtualProtect(&iat->u1.Function, 4, PAGE_READWRITE, &old);
              if (!*orig) *orig = (void *)iat->u1.Function;
              iat->u1.Function = (DWORD)(ULONG_PTR)newfn; VirtualProtect(&iat->u1.Function, 4, old, &old); }
            logf("  hooked %s!%s\n", (char *)(base + imp->Name), n);
        }
    }
}

// dinput8 forwarding ----------------------------------------------------------
static HMODULE g_real_di;
typedef HRESULT(WINAPI *DI8C)(HINSTANCE, DWORD, REFIID, LPVOID *, LPVOID);
// ---- DirectInput wrapper (we are the dinput8 proxy): background input + injected keys/mouse button for automated tests ----
// The engine reads keyboard/mouse through DirectInput; with a foreground cooperative level a window that is not in
// front gets no input at all. With bg_run the level is forced to BACKGROUND|NONEXCLUSIVE, and remote commands
// (key/click) are merged into GetDeviceState. Real input is still passed through.
static void *g_di_kbd, *g_di_mouse; static int g_di_logged;
typedef HRESULT (STDMETHODCALLTYPE *DI_CreateDevice_t)(void *, const GUID *, void **, void *);
typedef HRESULT (STDMETHODCALLTYPE *DID_GetState_t)(void *, DWORD, void *);
typedef HRESULT (STDMETHODCALLTYPE *DID_GetData_t)(void *, DWORD, void *, DWORD *, DWORD);
typedef HRESULT (STDMETHODCALLTYPE *DID_SetCoop_t)(void *, HWND, DWORD);
typedef HRESULT (STDMETHODCALLTYPE *DID_Acquire_t)(void *);
static DI_CreateDevice_t o_DI_CreateDevice; static DID_GetState_t o_DID_GetState; static DID_GetData_t o_DID_GetData;
static DID_SetCoop_t o_DID_SetCoop; static DID_Acquire_t o_DID_Acquire;
static const GUID G_SysKeyboard = {0x6F1D2B61, 0xD5A0, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
static const GUID G_SysMouse    = {0x6F1D2B60, 0xD5A0, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
static int bg_run(void); static int bg_cfg(void);
static HRESULT STDMETHODCALLTYPE h_DID_SetCoop(void *d, HWND w, DWORD f) {
    DWORD nf = bg_cfg() ? (0x8 /*BACKGROUND*/ | 0x2 /*NONEXCLUSIVE*/) | (f & 0x10 /*NOWINKEY*/) : f;
    HRESULT hr = o_DID_SetCoop(d, w, nf);
    logf("[dinput] SetCooperativeLevel(%s, %lx -> %lx) -> %08lx\n", d == g_di_kbd ? "keyboard" : d == g_di_mouse ? "mouse" : "?", f, nf, hr);
    return hr;
}
static HRESULT STDMETHODCALLTYPE h_DID_GetState(void *d, DWORD cb, void *data) {
    HRESULT hr = o_DID_GetState(d, cb, data); int i;
    if (FAILED(hr) && bg_cfg()) { o_DID_Acquire(d); hr = o_DID_GetState(d, cb, data); }
    if (FAILED(hr)) { if (!(d == g_di_kbd || d == g_di_mouse)) return hr; memset(data, 0, cb); hr = 0; }   // lost: report "nothing pressed" + injections
    if (g_di_logged < 4) { g_di_logged++; logf("[dinput] GetDeviceState(%s, %lu) used\n", d == g_di_kbd ? "keyboard" : d == g_di_mouse ? "mouse" : "?", cb); }
    if (d == g_di_kbd && cb >= 256) for (i = 0; i < 256; i++) { if (g_inj_key[i] && g_frame > g_inj_key_until[i]) g_inj_key[i] = 0; if (g_inj_key[i]) ((BYTE *)data)[i] = 0x80; }
    if (d == g_di_mouse && cb >= 16) { if (g_inj_btn && g_frame > g_inj_btn_until) g_inj_btn = 0; if (g_inj_btn) ((BYTE *)data)[12] = 0x80; }
    return hr;
}
static HRESULT STDMETHODCALLTYPE h_DID_GetData(void *d, DWORD cb, void *od, DWORD *n, DWORD f) {
    HRESULT hr = o_DID_GetData(d, cb, od, n, f);
    if (FAILED(hr) && bg_cfg()) { o_DID_Acquire(d); hr = o_DID_GetData(d, cb, od, n, f); }
    if (g_di_logged < 8) { g_di_logged++; logf("[dinput] GetDeviceData(%s) used -> %08lx\n", d == g_di_kbd ? "keyboard" : d == g_di_mouse ? "mouse" : "?", hr); }
    if (FAILED(hr) && (d == g_di_kbd || d == g_di_mouse)) { if (n) *n = 0; hr = 0; }
    return hr;
}
static HRESULT STDMETHODCALLTYPE h_DI_CreateDevice(void *di, const GUID *g, void **dev, void *u) {
    HRESULT hr = o_DI_CreateDevice(di, g, dev, u);
    if (SUCCEEDED(hr) && dev && *dev) {
        void **vt = *(void ***)*dev;
        if (!memcmp(g, &G_SysKeyboard, sizeof(GUID))) g_di_kbd = *dev; else if (!memcmp(g, &G_SysMouse, sizeof(GUID))) g_di_mouse = *dev;
        if (!o_DID_GetState) { o_DID_Acquire = (DID_Acquire_t)vt[7];
            patch_vt(vt, 9, h_DID_GetState, (void **)&o_DID_GetState); patch_vt(vt, 10, h_DID_GetData, (void **)&o_DID_GetData);
            patch_vt(vt, 13, h_DID_SetCoop, (void **)&o_DID_SetCoop); }
        logf("[dinput] CreateDevice %s -> %p\n", *dev == g_di_kbd ? "keyboard" : *dev == g_di_mouse ? "mouse" : "other", *dev);
    }
    return hr;
}
__declspec(dllexport) HRESULT WINAPI DirectInput8Create(HINSTANCE h, DWORD v, REFIID r, LPVOID *o, LPVOID u) {
    static DI8C real;
    if (!real) {
        char p[MAX_PATH]; GetSystemDirectoryA(p, MAX_PATH); strcat(p, "\\dinput8.dll");
        g_real_di = LoadLibraryA(p); real = (DI8C)GetProcAddress(g_real_di, "DirectInput8Create");
        logf("real dinput8 %p DirectInput8Create %p\n", g_real_di, real);
    }
    {   HRESULT hr = real ? real(h, v, r, o, u) : E_FAIL;
        if (SUCCEEDED(hr) && o && *o && !o_DI_CreateDevice) patch_vt(*(void ***)*o, 3, h_DI_CreateDevice, (void **)&o_DI_CreateDevice);   // IDirectInput8::CreateDevice
        return hr; }
}

static HMODULE g_self_mod;
static LONG CALLBACK crash_veh(EXCEPTION_POINTERS *x) {
    static int nlog;
    EXCEPTION_RECORD *er = x ? x->ExceptionRecord : 0;
    CONTEXT *c = x ? x->ContextRecord : 0;
    DWORD code = er ? er->ExceptionCode : 0, eip = c ? c->Eip : 0;
    HMODULE hm = 0;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_STACK_OVERFLOW &&
        code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_INT_DIVIDE_BY_ZERO)
        return EXCEPTION_CONTINUE_SEARCH;
    if (eip) GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)eip, &hm);
    if (hm && hm == g_self_mod) return EXCEPTION_CONTINUE_SEARCH;   // safe_ent/safe_long kasıtlı yoklamaları
    if (nlog++ < 16) {
        char w[64] = "", mod[MAX_PATH] = "?", st[6][64]; int j, k = 0;
        DWORD *sp = c ? (DWORD *)c->Esp : 0;
        if (eip) script_where(eip, w, sizeof w);
        if (hm) GetModuleFileNameA(hm, mod, sizeof mod);
        if (sp) for (j = 0; j < 256 && k < 6; j++) {
            DWORD v = 0;
            __try { v = sp[j]; } __except (EXCEPTION_EXECUTE_HANDLER) { break; }
            script_where(v, st[k], 64);
            if (st[k][0] && st[k][0] != '?') k++;
        }
        for (j = k; j < 6; j++) st[j][0] = 0;
        logf("[crash-veh] EXC code=%08lx at eip=%08lx (%s+%08lx) script='%s' access=%08lx | st: %s %s %s %s %s %s\n",
             code, eip, mod, hm ? (eip - (DWORD)hm) : 0, w,
             (er && er->NumberParameters > 1) ? (DWORD)er->ExceptionInformation[1] : 0,
             st[0], st[1], st[2], st[3], st[4], st[5]);
        if (g_log) fflush(g_log);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void init(HMODULE self) {
    char path[MAX_PATH], *s; HMODULE ack;
    g_self_mod = self;
    GetModuleFileNameA(NULL, g_dir, MAX_PATH); s = strrchr(g_dir, '\\'); if (s) s[1] = 0;
    { SYSTEMTIME t; GetLocalTime(&t);
      sprintf(path, "%scoop_probe_%02d%02d%02d_%lu.log", g_dir, t.wHour, t.wMinute, t.wSecond, GetCurrentProcessId()); }
    g_log = fopen(path, "w");
    AddVectoredExceptionHandler(1, crash_veh);
    logf("SuperCan coop probe loaded. self=%p exe=%p cmd=%s\n", self, GetModuleHandleA(NULL), GetCommandLineA());
    { char w[64]; if (cfg("COOP_WINDOW", "window", w, sizeof w)) {
          if (!_stricmp(w, "off") || !_stricmp(w, "0") || !_stricmp(w, "fullscreen") || !_stricmp(w, "none")) {
              g_win_x = -1; g_force_windowed = 0;
          } else if (!_stricmp(w, "full")) {
              g_win_full = 1; g_force_windowed = 1; g_win_x = 0; g_win_y = 0;
              g_win_w = GetSystemMetrics(SM_CXSCREEN); g_win_h = GetSystemMetrics(SM_CYSCREEN);
          } else if (sscanf(w, "%d,%d,%d,%d", &g_win_x, &g_win_y, &g_win_w, &g_win_h) == 4) {
              g_force_windowed = 1;
          } else { g_win_x = -1; g_force_windowed = 0; } }
      { char m[8]; if (cfg("COOP_MOUSE_FOCUS", "mouse_focus", m, sizeof m) && m[0] == '0') g_mouse_focus = 0; }
      logf("COOP_WINDOW=%d,%d,%d,%d force_windowed=%d\n", g_win_x, g_win_y, g_win_w, g_win_h, g_force_windowed); }
    ack = GetModuleHandleA("acknex.dll");
    logf("acknex.dll=%p\n", ack);
    if (!ack) return;
    p_ent_next = (void *)GetProcAddress(ack, "ent_next");
    p_engine_getvar = (void *)GetProcAddress(ack, "engine_getvar");
    p_engine_gettaskinfo = (void *)GetProcAddress(ack, "engine_gettaskinfo");
    p_engine_getscriptinfo = (void *)GetProcAddress(ack, "engine_getscriptinfo");
    p_engine_frame = (void *)GetProcAddress(ack, "engine_frame");
    p_ent_create = (void *)GetProcAddress(ack, "ent_create");
    p_ent_remove = (void *)GetProcAddress(ack, "ent_remove");
    p_ent_bones = (void *)GetProcAddress(ack, "ent_bones");
    // (sys_getstr diagnostic hook no longer installed; anim hooks are installed at frame 100)
    coop_init();
    logf("ent_next=%p getvar=%p gettaskinfo=%p getscriptinfo=%p engine_frame=%p\n",
         p_ent_next, p_engine_getvar, p_engine_gettaskinfo, p_engine_getscriptinfo, p_engine_frame);
    if (hook_iat(GetModuleHandleA(NULL), "acknex.dll", "engine_frame", hook_engine_frame, (void **)&real_engine_frame))
        logf("IAT hook OK, orig=%p (export %p)\n", real_engine_frame, p_engine_frame);
    else logf("IAT hook FAILED\n");
    if (!o_GetProcAddress) o_GetProcAddress = (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetProcAddress");
    logf("tracing acknex.dll imports:\n"); hook_all_imports(ack);
    logf("tracing supercan.exe imports:\n"); hook_all_imports(GetModuleHandleA(NULL));
}
