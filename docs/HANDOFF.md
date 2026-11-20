# HANDOFF — 2026-11-20 (18:35)

İşlemler bitince **oyun dosyaları ve kişisel veriler (SteamID, IP, PC yolları, `.gen`, exe) olmadan GitHub'da public olarak yayınlanacak**. Kullanıcıyla Türkçe konuş.
Depo: https://github.com/soldsawn/supercan-coop (şu an özel, sıradaki adımda public; son commit **`0d522d0`**). Tam lite-C decompile çıktısı: `local/dump/code/decomp.c` (857 fonksiyon); ayrıntılı RE tabloları: `local/NOTES.md` sonu; SüperCan 2 taşıma rehberi: `docs/supercan2-hazirlik.md`.
**Çok modelli çalışma:** Claude Code (`CLAUDE.md`) + Gemini/Antigravity (`GEMINI.md`/`AGENTS.md`); kurallar `docs/agents/model-gecisi.md`, iş kaydı `AGENT_LOG.md` (son kayıt: Gemini, 2026-11-20 18:35; `İnceleme: bekliyor`).
Test akışı: Steam'de iki PC'de aynı `dinput8.dll` + `net=steam` + `window=full` (`SuperCan_Coop_Oyna.bat` ile başlatma, oyun içi `F8` Steam Lobi & Davet); tek PC'de `SuperCan1` + `SuperCan1b` (`net=udp`), ardından `python -I -X utf8 local\tools\diff_logs.py`.

## Şu an nerede
1. **SC1 Co-op (`310.272 B`, SHA-256 `05C9B32F15DD353A9DA41B406EE360DD68922597EA259A8EFF85E27467DC06C6`, commit `0d522d0`) TAMAM & PUSH'LANDI:**
   - Gerçek 2 PC Steam testinde (`172333` host / `172508` + `172818` client) Steam P2P bağlantısı (`SessionFailed -> closing broken session`), Deve Boss'un baştan sona öldürülmesi, son ara sahne (`nesne_lsr_ustengel`, `y > 650`) ve `menu.wmb`'ye dönüş **0 crash** ile doğrulandı.
   - Testte yakalanan son 3 sorun (client Ana Menü kilidi, Steam Lobi davetsiz bağlanma, `boss_tick` `menu.wmb` tetiklenmesi) çözülüp derlendi, 4 hedef klasöre dağıtıldı ve `0d522d0` ile `origin/main`'e push'landı.
2. **Sıradaki Öncelik (Kullanıcı Onaylı): GitHub Public Yayın (`v1.0.0`) EN ÖNE ALINDI!**
   - Kullanıcı planı ve 2 kritik kararı onayladı:
     - **Git geçmişi:** Temizlik sonrası **tek temiz başlangıç commit'i (`orphan` branch)** ile PII'siz tarihçe.
     - **Lisans:** **MIT Lisansı**.
   - `/clear` sonrası doğrudan GitHub Public yayın adımları uygulanacak.
3. **Protokol sürümleri:** `SCO6` (oyuncu), `SCW3` (dünya), `SAL2` (alarm/terminaller), `SB10` (boss/pil/QTE/lazer), `SHD2`/`SSN1`/`SND1`/`SCT1`/`SMU1`.

## Son oturumda yapılanlar (17:19–18:35)
1. **Steam P2P `SessionFailed -> EResult 35` kilidi çözüldü (`5a9b29c`, `core/core_net.inc`):**
   - `ST_SEND_UNRELIABLE_NONAGLE` → `(1 | 32)` (`NoNagle | AutoRestartBrokenSession`); `CloseSessionWithUser` (`st_Close`) yüklendi; `cb=1252` (`SessionFailed`) ve `r == 35` anında bozuk oturum kapatılıp yeniden başlatılıyor; lobi üyesi kilitlenince `st_Accept` çağrılıyor. **2 PC Steam testinde doğrulandı.**
2. **Client "Ana Menü" kilidi kesin çözümü + Steam Lobi davetsiz bağlanma engeli + `boss_tick` harita sınırı (`0d522d0`):**
   - `sc1/sc1_sync_extra.inc` (`world_tick`): Host başka bölüme/menüye geçtiğinde (`g_world_client_active: 1 -> 0`) veya `!peer_active` iken `G_GAME_ACTIVE == 0 || G_MUSIC_ALLOW == 0` görülürse client'ta `G_ROBOT_ALIVE = 0` temizlenir (client `172508` loguyla %100 doğrulanan kök neden).
   - `core/core_net.inc` + `probe.c`: Steam Lobisi kuruluyken (`g_st_lobby != 0`) eski AppID 480 arkadaş taraması kapatıldı; `SessionRequest` ve paket kabulü yalnız lobi üyesiyle sınırlandı.
   - `sc1/sc1_boss.inc`: `boss_rx` ve `boss_tick` yalnız `tunel.wmb` ile sınırlandı (`menu.wmb`'de tekrar tetiklenme engellendi).
3. **GitHub Public Yayın Planı hazırlandı ve kullanıcı tarafından onaylandı (`orphan` branch + `MIT` lisansı).**

## Doğrulama durumu
- **Gerçek iki PC Steam testinde doğrulananlar (`172333` host / `172508` + `172818` client):**
  - **0 crash** (host ve client), `SessionFailed -> closing broken session` ile anında Steam P2P bağlantısı.
  - **D.E.V.E. Boss baştan sona (`SB10`):** 4 pil, iki yönlü QTE, `G_DEVE_HP` (`500 -> 1`), ölüm (`stage=3`), client `nesne_lsr_ustengel` son ara sahne ve `menu.wmb` dönüşü çalıştı.
  - Client ışıklandırma & kılıç parlaması birebir doğru.
- **Henüz oyunda test EDİLMEDİ (`0d522d0`, `310.272 B`, SHA-256 `05C9B32F...06C6`):**
  - Host önce menüye döndükten sonra client'ın ESC → Ana Menü yapabilmesi (`G_ROBOT_ALIVE` sıfırlama),
  - `F8` Steam Lobisi kuruluyken davetsiz bağlanmama,
  - `menu.wmb`'de `boss_tick` tetiklenmemesi.

## Sıradaki adım (Öncelik Sırası — `/clear` Sonrası Hemen Uygula)
1. **GitHub Public Yayın Hazırlığı (`v1.0.0`, kullanıcı onaylı: `orphan` branch + `MIT` lisansı):**
   - **1.1 PII & Yol Temizliği:**
     - `docs/HANDOFF.md`: SteamID'ler, Steam nick'leri, yerel IP ve kişisel PC yollarını genel yer tutucularla değiştir.
     - `docs/TEST_LISTESI.txt`: Gerçek Steam nick'lerini temizle.
     - `package/coop.ini`: Örnek IP'yi `192.168.1.100:27015` yap.
     - `tools/autotest.ps1`, `tools/launch.ps1`, `tools/re/ohc_test.ps1`, `tools/start_second.ps1`: Sabit yolları `$PSScriptRoot` tabanlı göreli yollara çevir.
     - `.gitignore`: `local/` tamamen git-dışı; binary DLL/arşiv/log koruması.
   - **1.2 Lisans & Açık Kaynak Standartları:**
     - `LICENSE` (MIT), `THIRD_PARTY_LICENSES.md` (`steam_api.dll` Steamworks.NET 2025.164.1 x86 SHA-256 + AppID 480 notu + hayran modu telif uyarısı), `CHANGELOG.md` (`v1.0.0`).
     - `.github/ISSUE_TEMPLATE/bug_report.yml`, `feature_request.yml` ve `.github/PULL_REQUEST_TEMPLATE.md`.
   - **1.3 Profesyonel `README.md`:**
     - Rozetler, özellikler tablosu, 3 adımda kurulum & oynama, kısayollar (`F8`/`F9`/`F5`), dürüst test durumu, kaynak koddan derleme (`core/` + `sc1/`), yol haritası (SC2), telif uyarısı (`coop-anything` / `Jericho Co-op+` standardında).
   - **1.4 Oyuncu Dostu Kur/Kaldır + Anonim Log Toplayıcı + Release Üretici:**
     - `local/coop_paket/kur.ps1` (`-Uninstall` + masaüstü kısayolu), `KALDIR.bat`, `LOG_TOPLA.bat` (son 3 log + `coop.ini` + DLL/EXE hash'ini masaüstünde tek `.zip` yapar; SteamID, IP, Windows kullanıcı adı ve kişisel yolları otomatik maskeler).
     - `local/tools/build_release.ps1`: `build.bat` → `local/release/SuperCan-Coop-v1.0.0.zip` + `SHA256SUMS.txt` + otomatik PII/yasaklı dosya taraması.
   - **1.5 Doğrulama & Temiz Tarihçe (`orphan` branch):**
     - Temiz kopyada `KUR.bat` → `LOG_TOPLA.bat` → `KALDIR.bat` (orijinal exe hash doğrulaması) testi.
     - Tüm dosyalarda son PII taraması (`git ls-files`) → kullanıcı onayıyla `orphan` branch üzerinden tek temiz `v1.0.0` commit'i ile push (not: force-push yalnız kullanıcı açık onaylayınca!).
     - GitHub Repo Public açılışı, Topics ve `v1.0.0` Release (`SuperCan-Coop-v1.0.0.zip` + `SHA256SUMS.txt`).
2. **Son 3 düzeltmenin (`0d522d0`) kısa Steam doğrulaması** (istenirse `v1.0.0` öncesi/sonrası).
3. **Özel Co-op Başlatıcı İyileştirmesi (`SuperCan_Coop_Baslatici.ps1`):** Modern arayüz + orijinal grafik/gölge/doku/shader seçenekleri (`defGSQ..defGKT`).
4. **Oyuncu 2 Kıyafet Rengi Farklılaştırma:** Çalışma zamanı `ent_cloneskin`/`bmap_lock` hue kaydırma (`coop.ini`).
5. **Opsiyonel SC1 Kalite İyileştirmeleri:** Orman gün zamanı (`g_07fa2439`) senkronu, Alt-Tab gamma koruması.
6. **Faz 5 — SüperCan 2'ye Entegrasyon (`docs/supercan2-hazirlik.md`):** Aşama 5.1 → 5.5.

## Açık sorular / kullanıcıdan beklenen
- `/clear` sonrası yeni oturumda 1. maddeden (GitHub Public Yayın Hazırlığı) doğrudan başlanacak.
- `orphan` branch ile geçmişi sıfırlamadan hemen önce kullanıcıdan son `git push` onayı alınacak.

## Dikkat
- **DLL durumu:** `local/build/dinput8.dll` (**`310.272 B`**, SHA-256 `05C9B32F15DD353A9DA41B406EE360DD68922597EA259A8EFF85E27467DC06C6`) 4 hedef klasörde (`coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `SuperCan1b/Game`) birebir aynı. Protokol **`SB10`** / **`SCO6`**.
- **Test `coop.ini` durumu:** `SuperCan1/Game/coop.ini` Steam modunda (`net=steam`, `window=full`). `SuperCan1b/Game/coop.ini` tek PC UDP test ayarında (`listen=27016`, `window=960,0,960,540`).
- **Git:** `0d522d0` commit'ine kadar push'landı (`origin/main`). `local/` altında git dışı geçici dosyalar (`SuperCan_Coop_Steam_Paket.zip`, `*.rar`, `tmp_friend_logs/`, `coop_probe_*.log`) var — public/temizlik aşamasında `.gitignore` ile korunup yerelden temizlenecek.

## Geçmiş
- 2026-11-20 17:19–18:35: Steam P2P `SessionFailed -> EResult 35` kilidi çözüldü (`5a9b29c`), 2 PC Steam testinde Deve Boss ölümü + son ara sahne 0 crash doğrulandı, client Ana Menü `G_ROBOT_ALIVE` kilidi + Steam Lobi davetsiz bağlanma engeli + `boss_tick` `tunel.wmb` sınırı çözüldü (`0d522d0`, `310.272 B`), GitHub Public Yayın Planı onaylandı (`orphan` + `MIT`).
- 2026-11-20 14:45–17:15: 2 PC Steam testi (`76 ms`, 0 crash) → `SB10`, Steam Lobi + `F8` davet paneli, Özel Başlatıcı (`ohc`), `probe.c` → `core/` + `sc1/` modülerleştirme ve SC2 hazırlığı (`309.760 B`, `6d0cd81`).
- 2026-11-20 10:00–14:08: D.E.V.E. Boss `SBO7→SBO9`, client karakter ışıklandırma kök sebebi (`level_materials_ready`), opsiyonel `window=off` (`299.520 B`).
- 2026-11-20 02:02–03:17: 4 Steam testi + `SBO4→SBO7` (`290.304 B`): 0 crash (`devicereset` `ret` + `icon_defer_remove`), `YANIK_COP.MDL`, Hangar `G_ALT_BLOK`/`G_HANGAR_STAGE`, `G_DEVE_HP`.
- 2026-11-20 01:17–02:02: Hangar siyah ekran kesin çözümü (`G_GAME_ACTIVE`), sol alt HUD, kapanış crash, baygın robotlar, `SAL2` hack terminalleri, `SBO3` Deve boss, `[interp]`, `skip_intro=1` (`286.720 B`).
- 2026-11-19 21:54–23:35: Faz 2 (`run_id`/`SHD2` + `SSN1`), `revive_tick`, Faz 3 (`SND1` + `SMU1` + `SCT1`), Faz 4 (`diff_logs.py`), Acknex native Co-op HUD (`280.064 B`).
- 2026-11-19 19:51–21:52: `TRASH_FLAG2`, $O(1)$ `SIT3`, `SST2`, `SMK2`, taret + `SAL1`, `tunel.wmb` + `SBO2` (`267.264 B`).
- 2026-11-17 12:55–23:36: Tam decompile (`decomp.c`, 857 fonksiyon), `SCW3`, 2 oyunculu hedefleme + `SPH1`, `SMK1`, `SIT1`, `SST1` (`242.688 B`).
---
# Kalıcı bölümler (her handoff'ta yeniden yazılmaz)

## Kurulum / dağıtım
- **USB paketi:** `local/steamcoop_paket/` (git'te yok, `coop_paket`ten kopya). İçinde `KUR.bat` (çift tık: paket Game içindeyse
  orayı kurar, değilse yolu sorar; sürükle-bırak ve boşluklu yol test edildi), kur.ps1, dinput8.dll, steam_api.dll,
  coop.ini, KURULUM.txt, guvenlik_duvari.bat. **Yeni DLL derleyince bu klasöre de kopyala.**
- **Git paketi:** `package/` (betikler + ayar şablonu; binary DLL'ler `src/build.bat` ve `tools/build_release.ps1` ile üretilip GitHub Release `.zip`'inde paylaşılır).
- **kur.ps1 -Game <Game klasörü> [-Net steam|udp] [-Peer ip] [-Uninstall]:** exe hash'ini kontrol eder (orijinal AA91BD3B…, LAA 0AB2D5FF…),
  `.orig` yedekler, LAA bitini (PE Characteristics |= 0x20) yerinde yapar ve sonucu hash ile doğrular (exe asla paylaşılmaz),
  DLL'leri + `steam_appid.txt` (480) koyar. Var olan coop.ini'yi korur, yalnızca `net=` satırını yazar.
- Oyun dosyaları ve yerel çalışma alanı depoya GİRMEZ: `gamefiles/`, `local/`, `coop-anything/`, `*.exe`, `*.orig`, `*.bin`, `*.gen`, `*.log`, `*.dll` (`.gitignore`).
- Test ortamı: İki PC Steam (`net=steam`) veya tek PC bölünmüş ekran UDP (`127.0.0.1:27015` / `27016`). Steam hesapları ve yerel IP'ler kişisel veri olduğundan dokümanlara yazılmaz.

## Mimari (hepsi `local/src/probe/probe.c`; derleme `local/src/probe/build.bat`, VS2022 x86)
- `Game\dinput8.dll` vekili (gerçek DirectInput8Create'e yönlendirir). DllMain'de exe'nin `acknex!engine_frame` IAT'i kancalanır
  → her kare `coop_tick()`. acknex/supercan.exe import'ları ve GetProcAddress sonuçları izlenir/kancalanır.
- **Hata 22 (başlatıcı jetonu):** script `HKCU\Software\Sobee\SuperCan\ohc` okur. DLL kayıt defterindeki gerçek jetonu kullanır
  ve `%TEMP%\supercan_coop_token.txt`'ye yazar; yoksa `COOP_OHC` env'i ya da 10 dk'dan taze paylaşılmış jeton.
  Not: Özel Co-op başlatıcı (ayarlar + Steam lobi) için bu jetonun nasıl üretildiği/aşılabileceği ileride ayrıca incelenecek (yasak değil).
- **Başlatıcı (SuperCan_Oyna.exe) çalışan diğer supercan.exe'yi KAPATIR.** Tek PC testinde 2. kopya `local/tools/start_second.ps1` ile.
- **Pencere modu (opsiyonel):** Varsayılan **kapalı** (`window=off` / satır yok → D3D9 tam ekran/çözünürlüğüne dokunulmaz). Yalnızca `window=full` (çerçevesiz tam ekran) veya `window=x,y,w,h` verilirse Direct3DCreate9 → CreateDevice/Reset kancasında motor yapısının KOPYASINDA `Windowed=TRUE` yapılır (motorunkini değiştirmek E2005 veriyordu) ve fare koordinatları ölçeklenir.
- **Ayarlar:** önce env (`COOP_NET`, `COOP_LISTEN`, `COOP_PEER`, `COOP_WINDOW`, `COOP_RATE`, `COOP_STEAM_PEER`), yoksa
  `Game\coop.ini [coop]`: `net=steam|udp`, `listen=27015`, `peer=ip:port` (boş = yayınla bul), `window=off|full|x,y,w,h`, `rate=30` (0 = her kare),
  `steam_peer=<steamid64>` (isteğe bağlı, yalnızca o kişi), `skip_intro=1`, `keep_ga=1`.
- **Paket 'SCO6'** (eski sürümlerle uyumsuz; iki taraf aynı DLL olmalı): değişken uzunluk, ≤1200 B (`WIRE_MAX`, parçalanma yok).
  Paket başına ad tablosu (seviye, parça tipleri, sahne/kemik adları); parça 50 B (tip idx, 11 float, flags, animated),
  animasyon çağrısı 16 B (op, part, ad idx, pose, a/b/c). Her paket tam durum (delta yok). 30 Hz zamanla (QPC);
  aradaki karelerin çağrıları birikir (`MAX_CALLS` 96), sığmazsa EN ESKİLER atılır (`trunc`). `pkt_encode`/`pkt_decode`.
  Log her 600 karede: `rx= tx= avg= max= trunc= recdrop=`.
- **UDP taşıyıcı (net=udp):** peer biliniyorsa doğrudan; değilse ~1 sn'de bir 255.255.255.255 ve her yerel x.y.z.255'e yayın,
  kendi yankısı (yerel IP + kendi portu) atlanır; ilk geçerli paketi gönderen eş olur, adres değişirse güncellenir.
- **Steam taşıyıcı (net=steam):** `Game\steam_api.dll` (Steamworks.NET Standalone 2025.164.1 x86, SHA-256 fdafb7c6…5da),
  flat API çalışma anında yüklenir, ilk karede oyun iş parçacığında `SteamAPI_InitFlat` (DllMain'de değil); başarısızsa UDP'ye düşer.
  AppID 480 (`steam_appid.txt` + `SteamAppId`/`SteamGameId` env). ManualDispatch ile callback'ler: 1251 oturum isteği yalnızca
  arkadaştan (ilişki 3) ya da `steam_peer`'den kabul, 1252 loglanır. **Lobi yok:** eş bilinmezken ~1 sn'de bir paket, arkadaş
  listesinde AppID 480 çalıştıran herkese (`GetFriendGamePlayed`, appid = gameID & 0xFFFFFF); ilk geçerli paketi gönderen arkadaş
  eş olur. ISteamNetworkingMessages, Unreliable|NoNagle, kanal 0. Mesaj gönderen kimliği **ofset 12** (iki gerçek hesapla doğrulandı).
- **Kukla:** `ent_create` ile aynı modeller, PASSABLE; konum/açı/ölçek/görünürlük yazılır; görünüm (skin..rgb, skill[100],
  material@552) alıcının kendi aynı tip parçasından kopyalanır. Karşı taraf farklı bölümdeyse kukla yok.
- **Animasyon:** acknex export'larına inline detour (prolog doğrulamalı, kare 100'de): ent_animate, ent_blendpose,
  ent_animatefrom, ent_bonereset_all, ent_bonerotate. Çağrılar (pose @548 dahil) kaydedilip kuklada sırayla tekrarlanır;
  poz yuvaları 1..4 ilk animasyonla tohumlanır. Oyun her kare: bonereset_all + bonerotate×2 + ent_animate(+blendpose).
- ENTITY düzeni A6 SDK ile aynı (x..frame, material 552, pose 548). SDK başlıkları: `local/ref/kabus22/...` (git'te yok).

## Çözülmüş tuzaklar (tekrar düşme)
- **Grafik/ışık bozuk:** sebep kopyanın `User\oyuncu_ga.gen` (şifreli grafik ayarları) dosyasıydı. Orijinalinkini kopyala
  (test kopyalarındaki eskiler `.bozuk`, `.bozuk2`). **Oyunun kendisi** bu dosyayı her oturumda (menüde + çıkışta, CREATE_ALWAYS)
  yeniden yazıyor ve bu PC'de yazdığı içerik bozuk grafik veriyor (17:12'de tekrar oldu). Çözüm (17:20): DLL `keep_ga=1`
  (varsayılan) ile `User\*_ga.gen` YAZMALARINI `<dosya>.oyunyazdi`'ya yönlendiriyor; okuma gerçek dosyadan. Yan etki: oyun içi
  grafik ayarı değişiklikleri kalıcı olmaz (`keep_ga=0` ile kapatılır). Oyunun neden farklı yazdığı bilinmiyor (dosya şifreli).
- **kur.ps1 Game içinden çalışınca** DLL kendi üstüne kopyalanamıyordu → betik durup coop.ini'yi yazmıyordu. Düzeltildi (aynı dosyayı atlar).
- **E2005 "Can't create DirectX texture":** 2 GB adres alanı → LAA biti.
- **Hangar yükleme siyah ekran (`main_check_load` kilidi):** `main_check_load` (`decomp.c:217523`) `while (G_ROBOT_ALIVE > 0) wait(1)` döner; `G_GAME_ACTIVE(ebx) == 0` (`g_07fa2461`, `+0x26241c`) iken client'ta `G_ROBOT_ALIVE = 0` tutulmalı.
- **Client "Ana Menüye Dön" çalışmaması:** `g_07f9f7c9` (`G_MUSIC_ALLOW`) bölümden çıkış bayrağıdır; ESC → Ana Menü (`E_smart_btn_yes2`, `decomp.c:59058`) basınca oyun `G_MUSIC_ALLOW = 0` yapıp `while (G_ROBOT_ALIVE > 0) wait(1)` bekler. Host önce menüye/başka bölüme dönünce `active = !strcmp(remote, local)` `0` olduğu için `world_client_sync_counters()` durur ve eski `G_ROBOT_ALIVE > 0` kalıcı kalırdı. Çözüm: `g_world_client_active: 1 -> 0` anında ve `!peer_active` iken `G_GAME_ACTIVE == 0 || G_MUSIC_ALLOW == 0` olduğunda `G_ROBOT_ALIVE = 0` sıfırlanır.
- **Steam P2P `SessionFailed -> EResult 35` kalıcı kilidi:** `ISteamNetworkingMessages` oturumu bir kez `SessionFailed` (`cb=1252`) olunca `CloseSessionWithUser` çağrılmadan veya `AutoRestartBrokenSession` (`32`) bayrağı verilmeden sonraki tüm `SendMessageToUser` çağrıları sonsuza dek `EResult 35` (`k_EResultConnectFailed`) döner. Çözüm: `ST_SEND_UNRELIABLE_NONAGLE = (1 | 32)` ve `cb=1252` / `r == 35` anında `CloseSessionWithUser`.
- **Orman "gece/karanlık" görünümü & Alt-Tab kararması:** Orman bölümü yüklenirken oyun `0x7fa4629 = [0,0,1,0,0,1,0,2,0,1]` tablosundan (`g_07fa4655 = (g_07fa4655+1)%10`) gündüz (`0`)/akşam (`1`)/gece (`2`) seçer (bu bozuk grafik değil). Ayrıca oyun `0xcf711f0` ile donanım gamma ramp'ini değiştirdiği için Alt-Tab/ekran değişiminde ekranda karanlık görünüp BMP ekran görüntüsünde normal çıkabilir.
- **Hata 22 (`ohc`) kesin formülü (`decomp.c:220962..221005`):** `ohc = sum("UNKNOWN")*11 + sum("null")*7 + (year%100)*223 + month*91 + day*53 + hour*37 + (minute/10)*48 + 684` (`HKCU\Software\Sobee\SuperCan\ohc`). Özel başlatıcı (`SuperCan_Coop_Baslatici.ps1`) bunu hesaplayıp `SuperCan_Oyna.exe` olmadan doğrudan `supercan.exe`'yi başlatır.
- **Kapanış `devicereset+0x37/+0x135` crash:** `WM_CLOSE`/`WM_DESTROY` anında `devicereset[0] = 0xC3` (`ret`) yaması ile tamamen çözüldü.
- **`pfcan_copicon_goster+0x27a` NULL+0x10 crash & dev kırmızı ikon:** Çöp ikonu (`metal_icon.tga`) yalnız `YANIK_COP.MDL` içindir, ölçeği `52` (`0.05`) olmalıdır; silerken anında `ent_remove` yapma, 20 kare gecikmeli `icon_defer_remove` kullan.
- **D.E.V.E Boss gerçek canı (`G_DEVE_HP`) ve mimari tuzaklar:** `G_DEVE_BOX->skill[35]` hep `500.0` kalır; gerçek can **`g_07fa467d`** (`ebx + 0x264638`) globalindedir. `G_DEVE_BOX` PhysX aktörü değildir (`de->x/y/z` doğrudan yazılır), gerçek yönü `G_DEVE_PAN` (`g_07d5a275`, `+0x01a230`). `G_DEVE_GOVDE` animasyonunda temel tam gövde animasyonu ile `ANM_ADD` katmanı (`mode & (2<<10)`) ayrı tutulmalı (aksi halde model katlanır). Boss AI'ın gerçek `me` entity'si `(-2000, 0, 500)` konumundaki `FISH.MDL` (`AIF_Hangar_dummy1/2`) nesnesidir.
- **Client karakter ışıklandırma/karanlık tuzağı (`level_metarials_apply` / `ent_for_file`):** Tüm bölümlerde `level_metarials_apply()` `File_mat_<bölüm>`'ü okuyup `ent_for_file("supercan_*.mdl")` ile `BR_material_load()` çağırır. Bu fonksiyon bitmeden kukla (`ent_create`) oluşturulursa `ent_for_file` kuklayı döndürür ve gerçek yerel karakterin materyal/ışık/glow/cubemap'i boş kalır. Çözüm: `level_materials_ready()` ile `proc_status(level_metarials_apply) == 0` ve `SUPERCAN_GOVDE.MDL` materyal/skill yüklenene kadar kukla oluşturmayı ertele.
- Tek PC testine özgü: odakta olmayan pencerede oyun duruyor; küçük pencerede butonlar bazen tıklanmıyor.

## Test
- `local/testinstall/SuperCan1` (ana PC'nin oyun kopyası; şu an `net=steam`, `peer=`, `window=full`; bölünmüş ekran ayarı
  `coop.ini.splitscreen`) ve `SuperCan1b` (tek PC'de 2. kopya, listen 27016 → 127.0.0.1:27015).
  Not: tek PC'de iki Steam hesabı olmaz; bölünmüş ekran testi için iki kopyayı da `net=udp` yap.
- Loglar: `Game\coop_probe_HHMMSS_PID.log` (`[coop]`, `[steam]`, `[diag]`, `[anim]`, `[spy]`, `[ohc]`, `[d3d]`).
- Kodla/çöz testi ve Steam init testi scratchpad'deydi (silinebilir): `probe.c`'yi `#include` eden küçük exe'ler.
- `gamefiles/` orijinal kurulum — DOKUNMA.
- **Harici RE/Co-op araç ve belge deposu (`coop-anything/`, ayrı depo, commit'lenmez):**
  - `coop-anything/playbook/`: Co-op geliştirme rehberleri (`how-coop-mods-work.md`, `networking.md`, `networking-steam-spacewar.md`, `desync-hunting.md`, `actor-ownership.md`, `animation-sync-checks.md`, `effect-capture-story-sync.md`, `reverse-engineering.md`, `decomp-and-recomp.md`, `network-chaos-testing.md`, `automated-qa.md`, `hot-reloading-your-mod.md` vb.).
  - `coop-anything/tools/`: Hazır araçlar (`animcheck`, `bots`, `bridge`, `buildlock`, `chaos`, `fakeworld`, `flight`, `fuzz`, `gamepool`, `gameslot`, `hotreload`, `inspect`, `mcp`, `mutants`, `ownership`, `scenarios`).
  - `coop-anything/templates/` ve `.agents/skills/` / `.claude/skills/`: Şablonlar ve co-op skill'leri. Gerektiğinde buradaki belge ve araçlardan doğrudan yararlanılabilir.


## Otomatik test altyapısı
- **Uzaktan kumanda (DLL):** `Game\coop_cmd.txt` satırları oyun iş parçacığında çalışır, dosya silinir: `click X Y`, `move X Y`,
  `cursor off`, `tp X Y Z` (pXent_setposition, SUPERCAN_BOX), `shot PATH` (JPG, D3DXSaveSurfaceToFileA; Present kancası tablo
  değişince yeniden takılır, olmazsa tick'te back buffer), `key <DIK> [kare]`, `keydown/keyup <DIK>`, `dump`, `mark`. Koordinatlar backbuffer (1920x1080).
  `Game\coop_status.txt` her sn: frame, level, player, role, rx, puppets, world_*, blocked.
- **RE komutları ve Tam Decompile (`local/dump/code/decomp.c`):**
  - `dumpall <klasör>` (`Game\coop_cmd.txt`): derlenmiş lite-C kod bölgesini (`region_*.bin`), `functions.txt` (857 fonksiyon) ve `acknex_exports.txt` döker.
  - Çevrimdışı boru hattı: `tools/acknames.py` (`acknex_names.txt`) → `tools/mkpe.py` (`scscript.dll`: prolog `EBX` + 5035 adet satır içi `1024.0` double düzleştirme) → Ghidra `tools/ghidra/DecompAll.java` (`decomp_raw.c`) → `tools/scnames.py` (`local/dump/code/decomp.c`, 857 fonksiyonun tamamı).
  - Hızlı assembly: `python -I local/tools/disfn.py <fonksiyon_adı> [max_ins]`.
  - `watchhp`/`watchoff` (robot skill'lerine veri kesme noktası, `[dbw]`), `hit [dmg]` (host'ta en yakın robota client vuruşu taklidi), `yaralan`.
  - **Lite-C çalışma anı `EBX` tabanı:** `fn = engine_getscript("AIF_GBR_fizik")` → `ebx = (DWORD)fn + 9 - *(DWORD *)((BYTE *)fn + 14)` (dökümde `0x07d40045`).
    Önemli globaller (`ebx + ofset`): `+0x264628` oyuncu canı, `+0x26462c` **nano (`g_07fa4671`, 0..100<<10)**, `+0x260068` **canlı robot sayısı (`g_07fa00ad`)**, `+0x28e654` **bekleyen GBR (`g_07fce699`)**, `+0x28e658` **bekleyen ZBD (`g_07fce69d`)**, `+0x28e65c` **maks eşzamanlı robot (`g_07fce6a1`)**, `+0x26b184` **oyuncu kutusu (`g_07fab1c9`, `SUPERCAN_BOX.MDL`)**.
    Robot `ENTITY + 148 + idx*4` (`skill[idx]` = `s(idx+1)`): `skill[21]` plazma handle, `skill[27..31]` alt parça handle'ları, `skill[34]` (`s35`) tetikleyici can (`-=0.1`), `skill[35]` (`s36`) gerçek can (`GBR=72`, `ZBD=96`, `UCAN=100`), `skill[36]` (`s37`) vuruş kodu (`0..3`=A..D, `4`=HeavyA −20, `5`=HeavyB −40, `3482`=nanosuz Heavy −10), `skill[37]` (`s38`) sınıf (`1`=GBR, `2`=ZBD, `3`=UCAN; `0` ise `AIF_GBR/ZBD_plazma` ilk satırda güvenle çıkar), `skill[38]` (`s39`) saldırı sayacı, `skill[39]` (`s40`) gölge handle (**üzerine yazma**). Ayrıntılı tablo `local/NOTES.md` sonunda.
- **Arka plan:** `bg_run=1` (varsayılan): WM_ACTIVATE/ACTIVATEAPP/KILLFOCUS yutulur, GetForeground/Active/Focus = oyun penceresi,
  DirectInput (biz dinput8 vekiliyiz) SetCooperativeLevel BACKGROUND|NONEXCLUSIVE'e zorlanır, GetDeviceState'e enjekte tuş/fare
  düğmesi eklenir. **Tıklama ancak DirectInput fare düğmesi enjekte edilince çalıştı** (motor fareyi DInput'tan okuyor, konumu GetCursorPos).
  Host'ta menü tıklaması çalıştı; client (hiç odak almamış 2. pencere) tıklamaya tepki vermedi → 21:20 DLL: periyodik WM_ACTIVATE*
  + `window_focus`=1 zorlaması (TEST EDİLMEDİ).
- **Başlatıcı otomatik:** `tools/winctl.ps1` (gerçek fare yok): `-Proc SuperCan_Oyna -Button Tamam` (sertifika hatası), `-Button İptal`
  (güncelleme uyarısı), `-ClickX 440 -ClickY 310` (ÇALIŞTIR, pencere içi koordinat), `-Shot file.png` (PrintWindow), `-List`.
  Jeton 10 dk taze → bu sürede iki kopya da `tools/start_second.ps1 -Copy SuperCan1|SuperCan1b` ile başlatılabilir.
- **Menü yolu:** `clickent menu_oyuna_basla.mdl` → `clickent menu_devam.mdl` (vec_to_screen ile entity'nin ekran konumuna tıklar;
  sabit koordinat Skor Listesi'ne bastı, menü kamerası hareketli) →
  orman yüklenir → çizgi roman ara sahnesi (geçme yolu henüz bulunmadı; tıklama denendi). `tools/shot.ps1` masaüstü görüntüsü.

