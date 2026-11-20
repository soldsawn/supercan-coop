// [SC1] SuperCan 1 lite-C EBX tabani ve global ofset makrolari
// Lite-C runtime EBX base (PIC base register shared by all 857 compiled lite-C functions; see decomp.c / HANDOFF).
static DWORD g_ebx_base;
static DWORD ebx_base(void) {
    static void *(__cdecl *p_getscript)(char *);
    static const BYTE pro[14] = {0x55,0x53,0x56,0x57,0xE8,0,0,0,0,0x58,0x8B,0xD8,0x81,0xEB};
    BYTE *fn;
    if (g_ebx_base) return g_ebx_base;
    if (!p_getscript) p_getscript = (void *)GetProcAddress(GetModuleHandleA("acknex.dll"), "engine_getscript");
    if (!p_getscript) return 0;
    __try {
        fn = (BYTE *)p_getscript("AIF_GBR_fizik");
        if (fn && !memcmp(fn, pro, 14)) {
            g_ebx_base = (DWORD)fn + 9 - *(DWORD *)(fn + 14);
            logf("[coop] lite-C ebx_base = %08lx (from AIF_GBR_fizik %p)\n", g_ebx_base, fn);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return g_ebx_base;
}
#define G_PLAYER_HP(ebx)   (*(var *)((BYTE *)(ebx) + 0x264628)) /* g_07fa466d: oyuncu canı (0..100<<10) */
#define G_NANO(ebx)        (*(var *)((BYTE *)(ebx) + 0x26462c))
#define G_SCORE(ebx)       (*(var *)((BYTE *)(ebx) + 0x264634)) /* g_07fa4679: toplam puan */
#define G_DIAMONDS(ebx)    (*(var *)((BYTE *)(ebx) + 0x265d98)) /* g_07fa5ddd: elmas sayacı */
#define G_MKR_STATE(ebx)   (*(var *)((BYTE *)(ebx) + 0x262528)) /* g_07fa256d: 0=başlangıç, 1=kol çekildi/köprü açık, 2=kapı açık */
#define G_ALRM_STATE(ebx)  (*(var *)((BYTE *)(ebx) + 0x262524)) /* g_07fa2569: 0=başlangıç, 1=alarm/saldırı, 2=dos, 3=kapandı */
#define G_ALRM_SILO(ebx)   (*(ENTITY **)((BYTE *)(ebx) + 0x28caf8)) /* g_07fccb3d: nesne_alrm_Silo */
#define G_ALRM_ALET_ST(ebx, i) (*(var *)((BYTE *)(ebx) + 0x264d84 + (i) * 4)) /* g_07fa4dc9[0..3]: 4 hack terminali durumu (0 -> 2<<10) */
#define G_ALRM_ALET_ENT(ebx, i) (*(ENTITY **)((BYTE *)(ebx) + 0x28cad4 + (i) * 4)) /* g_07fccb19..25: nesne_alrm_alet1..4 */
#define G_ALRM_ISIN_ENT(ebx, i) (*(ENTITY **)((BYTE *)(ebx) + 0x28cae8 + (i) * 4)) /* g_07fccb2d..39: nesne_alrm_isin1..4 */
#define G_ALRM_KAPAK(ebx)  (*(ENTITY **)((BYTE *)(ebx) + 0x28cad0)) /* g_07fccb15: silo kapağı */
#define G_ALRM_TIMER(ebx)  (*(var *)((BYTE *)(ebx) + 0x2642e8)) /* g_07fa432d: alarm geri sayım sayacı (7500<<10'dan geriye) */
#define G_DEVE_BOX(ebx)    (*(ENTITY **)((BYTE *)(ebx) + 0x01a228)) /* g_07d5a26d: AIF_DEVE_fizik boss kutusu (düz entity, PhysX değil!) */
#define G_DEVE_PHASE2(ebx) (*(var *)((BYTE *)(ebx) + 0x01a22c))     /* g_07d5a271: 0=ilk can barı (500->1), 1<<10=ikinci can barı (201->1, gerçek ölüm) */
#define G_DEVE_PAN(ebx)    (*(var *)((BYTE *)(ebx) + 0x01a230))     /* g_07d5a275: Deve Boss gerçek yön açısı (pan) */
#define G_DEVE_HP(ebx)     (*(var *)((BYTE *)(ebx) + 0x264638))     /* g_07fa467d: Deve Boss gerçek canı (500<<10, QTE'de -18 düşer, HUD barı bunu okur) */
#define G_DEVE_GOVDE(ebx)  (*(ENTITY **)((BYTE *)(ebx) + 0x26a310)) /* g_07faa355: AIF_DEVE_govde boss modeli */
#define G_DEVE_ISARET(ebx) (*(ENTITY **)((BYTE *)(ebx) + 0x26a318)) /* g_07faa35d: AIF_DEVE_isaret Faz 2 QTE tutunma işareti */
#define G_DEVE_KAPI(ebx)   (*(ENTITY **)((BYTE *)(ebx) + 0x26a320)) /* g_07faa365: AIF_DEVE_kapiblok hangar kapısı */
#define G_LSR_ISIN1(ebx)   (*(ENTITY **)((BYTE *)(ebx) + 0x26a338)) /* g_07faa37d: arena üst lazer ışını 1 */
#define G_LSR_SAP1(ebx)    (*(ENTITY **)((BYTE *)(ebx) + 0x26a340)) /* g_07faa385: arena üst lazer rayı 1 */
#define G_LSR_SAP2(ebx)    (*(ENTITY **)((BYTE *)(ebx) + 0x26a344)) /* g_07faa389: arena üst lazer rayı 2 */
#define G_HANGAR_LSR1(ebx) (*(ENTITY **)((BYTE *)(ebx) + 0x26a348)) /* g_07faa38d: hangar giriş lazeri 1 */
#define G_HANGAR_LSR2(ebx) (*(ENTITY **)((BYTE *)(ebx) + 0x26a34c)) /* g_07faa391: hangar giriş lazeri 2 */
#define G_ALT_BLOK(ebx)    (*(ENTITY **)((BYTE *)(ebx) + 0x26a2c8)) /* g_07faa30d: nesne_alt_blok (merdiven arası görünmez duvar) */
#define G_ALT_LSR(ebx)     (*(ENTITY **)((BYTE *)(ebx) + 0x26a2cc)) /* g_07faa311: nesne_alt_laser (merdiven arası kırmızı lazer) */
#define G_HANGAR_STAGE(ebx) (*(var *)((BYTE *)(ebx) + 0x26a328))    /* g_07faa36d: 0=giriş lazeri hasarlı, 1=lazer kapalı/arena açık, 2=boss aktif, 3=boss öldü */
#define G_LSR_TIMER(ebx)   (*(var *)((BYTE *)(ebx) + 0x26a334))     /* g_07faa379: Deve üst lazer saldırı zamanlayıcısı (client'ta 0 tutulur) */
#define G_PIL_DEAD(ebx)    (*(var *)((BYTE *)(ebx) + 0x26a354))     /* g_07faa399: patlayan pil sayısı (0..4<<10) */
#define G_PIL_BOX(ebx, i)  (*(ENTITY **)((BYTE *)(ebx) + 0x26a358 + (i) * 4)) /* g_07faa39d[0..3]: nesne_pil_box0..3 */
#define G_PIL_CAM(ebx, i)  (*(ENTITY **)((BYTE *)(ebx) + 0x26a368 + (i) * 4)) /* g_07faa3ad[0..3]: nesne_pil_cam0..3 */
#define G_LIGHT_POS(ebx)   ((var *)((BYTE *)(ebx) + 0x25f70c))                /* 0x7f9f751: karakter dinamik ışık konumu [x,y,z] */
#define G_LIGHT_RNG(ebx)   (*(var *)((BYTE *)(ebx) + 0x25f72c))               /* g_07f9f771: karakter dinamik ışık menzili */
#define G_ITM_TBL(ebx, i)  (*(var *)((BYTE *)(ebx) + 0x26a3d8 + (i) * 4)) /* g_07faa41d[0..199]: kayıtlı çöp/elmas durumu (1=sahada, 0=toplandı) */
#define G_ITM_CNT(ebx)     (*(var *)((BYTE *)(ebx) + 0x26a6f8)) /* g_07faa73d: haritadaki kayıtlı eşya sayısı (<=200<<10) */
#define G_CTRL_ENABLED(ebx) (*(var *)((BYTE *)(ebx) + 0x26b17c)) /* g_07fab1c1: 0=pasif/diyalog/sinematik kilidi, !=0=normal kontrol */
#define G_SND_TBL(ebx, i)  (*(var *)((BYTE *)(ebx) + 0x28d264 + (i) * 4)) /* g_07fcd2a9[0..320]: FMOD ses handle tablosu */
#define SND_TBL_MAX 330
#define G_MUS2_TRK(ebx)    (*(var *)((BYTE *)(ebx) + 0x28c4f0)) /* g_07fcc535: 2. müzik kanalı parça indeksi */
#define G_MUS2_ON(ebx)     (*(var *)((BYTE *)(ebx) + 0x28c4f4)) /* g_07fcc539: 2. müzik kanalı açık/kapalı (0/1<<10) */
#define G_MUS2_VOL(ebx)    (*(var *)((BYTE *)(ebx) + 0x28c4f8)) /* g_07fcc53d: 2. müzik kanalı ses seviyesi */
#define G_MISSED_TRASH(ebx) (*(var *)((BYTE *)(ebx) + 0x265e88)) /* g_07fa5ecd: süpürülmeyen robot çöpü cezası */
#define G_TRASH_RBT(ebx)   (*(var *)((BYTE *)(ebx) + 0x264614)) /* g_07fa4659: toplanan robot çöpü */
#define G_TRASH_ENV(ebx)   (*(var *)((BYTE *)(ebx) + 0x26461c)) /* g_07fa4661: toplanan çevre çöpü */
#define G_TRASH_LVL(ebx)   (*(var *)((BYTE *)(ebx) + 0x264668)) /* g_07fa46ad: bölüm çöp sayacı */
#define G_ROBOT_ALIVE(ebx) (*(var *)((BYTE *)(ebx) + 0x260068))
#define G_GBR_QUEUE(ebx)   (*(var *)((BYTE *)(ebx) + 0x28e654))
#define G_ZBD_QUEUE(ebx)   (*(var *)((BYTE *)(ebx) + 0x28e658))
#define G_ROBOT_MAX(ebx)   (*(var *)((BYTE *)(ebx) + 0x28e65c))
#define G_PATH_SEG(ebx)    (*(var *)((BYTE *)(ebx) + 0x26e8b0)) /* g_07fae8f5: camera_path_loop mevcut yol/dalga segmenti */
#define G_LVL_SLOT(ebx)    (*(var *)((BYTE *)(ebx) + 0x282200)) /* g_07fc2245: bölüm tablosu sütun indeksi */
#define G_SUB_LVL(ebx)     (*(var *)((BYTE *)(ebx) + 0x262420)) /* g_07fa2465: alt-bölüm / checkpoint indeksi (0..5 orman, 6..9 tunel/hangar) */
#define G_GAME_ACTIVE(ebx) (*(var *)((BYTE *)(ebx) + 0x26241c)) /* g_07fa2461: 0=main_check_load yükleme aşaması, 1<<10=oyun aktif */
#define G_MUSIC_ALLOW(ebx) (*(var *)((BYTE *)(ebx) + 0x25f784)) /* g_07f9f7c9: savaş/bölüm müziği izin bayrağı */
#define G_GAMMA_MODE(ebx)  (*(var *)((BYTE *)(ebx) + 0x260adc)) /* g_07fa0b21: devicereset harici DLL callback modu */
#define G_DEATH_TYPE(ebx)  (*(var *)((BYTE *)(ebx) + 0x264630)) /* g_07fa4675: -1<<10=normal, !=-1<<10=su/uçurum ölümü */
#define G_PLAYER_BOX(ebx)  (*(ENTITY **)((BYTE *)(ebx) + 0x26b184))
static volatile int g_closing;
static volatile DWORD g_d3d_reset_gen;
#define FLAG2_F            (1 << 4) /* 16: Acknex FLAG5; c_scan / c_trace düşman hedef bayrağı */
#define TRASH_FLAG2_F      (1 << 1) /* 2: Acknex FLAG2; c_scan(0x140001 = SCAN_FLAG2) çöp tarama bayrağı */

