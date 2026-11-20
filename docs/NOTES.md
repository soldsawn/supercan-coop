# SüperCan Co-op — çalışma notları (ÖZEL, paylaşılmayacak)

## Kimlik (SüperCan 1)
- Geliştirici: Sobee Studios (kapalı). Dil: Türkçe. Platform: Windows, 32-bit.
- Motor: 3D GameStudio **A8**, acknex.dll sürüm dizesi `8.20.1` (SüperCan 2: `8.30.5`).
- Fizik: PhysX 2.x (ackphysX.dll, NxCharacter). Ses: FMOD 3 (fmod3Wrapper.dll).
- SHA-256:
  - supercan.exe `aa91bd3b6d5ef81d82e4748c14e18a260e6090998b1ad3698dbd7a34ebb0c3b4`
  - acknex.dll   `83b39f0e1136b5d4664036f1b5360ece041a51b5c1ab417e4ec82cd8b3bc71d3`
  - acknet.dll   `1cbde9cbc2530efefce18f9464776b3518df235a8a4763f03b97572af41db7aa`
  - ackphysX.dll `724409e88210479472328a0d6a526c2a0fa3e4805997cf7221d3809950ed6487`
  - acknex.wdf   `274f4eaf61add2bf19dbc93a8bd4a0ab9ed509505d2ca3463439b435d947d248`
- Test kopyası: `local/testinstall/SuperCan1` (orijinal `gamefiles/` dokunulmaz).

## Gözlemler (doğrulanmış)
- `supercan.exe` = küçük stub (.text 28KB) + 2.33MB sıkıştırılmış overlay (lite-C kodu + gömülü kaynaklar, `i_main.c` izi).
  Exe yalnızca `acknex!engine_open/engine_frame/engine_close` import ediyor.
- `acknex.dll` 437 fonksiyon export ediyor (engine_getscript, engine_getvar, engine_getscriptinfo,
  engine_getobj, var_for_name, ent_next, ent_create, session_*, send_skill...). `DINPUT8!DirectInput8Create` import ediyor.
- `color.dll`: A6_DLLAPI eklentisi (get_gamma_ramp, set_gamma, set_gamma_ramp).
- `.gen` = WRS4 arşivi. Format çözüldü → `local/tools/wrs.py` (tüm girişler boyut doğrulamalı açılıyor).
  - "WRS4" | u32be data_end | girişler (her biri ayrı LZSS 4096/18/thr2, r=N-F) | 8 bayt boşluk | dizin
  - dizin: LZSS + XOR 0xAA, 40 bayt kayıt: name[32] | u32be csize | u32be usize
- `supercan.wdl`: kaynak arşivlerini `defGTQ_*` (doku kalitesi), `defGSQ_*` (shader), `defGPQ_*`, `defGKT_1` tanımlarıyla seçiyor → başlatıcı komut satırından -d ile geçiyor olmalı (DOĞRULANMADI).
- Haritalar (harita_v0.gen): menu.wmb, orman.wmb, test.wmb, tunel.wmb (WMB7).
- editor_v0.gen: ai_*.pak, path_*.pak, mat_*.pak, time.pak... (düz metin sayı listeleri).
- Oyuncu modeli parçaları: supercan_govde.mdl (10MB, animasyonlu), supercan_yuz/sac/canta/sword.mdl.
- Harita entity action isimleri: nesne_char_*, nesne_tramplen, kapi_level_gecis, nesne_geri_dnsm,
  nesne_tunel_ZBD, nesne_tunel_TOP, nesne_tunel_engel, OPT_anim_*, lvl_bitki_model...
- Araç zinciri: VS2022 Community, MSVC 14.42 Hostx64\x86 cl.exe mevcut. Python 3.14.

## Başlatma (doğrulandı 2026-11-15)
- Başlatıcı komut satırı: `supercan.exe -nx 270 -pl oyuncu -d defGSQ_3 -d defGPQ_3 -d defGTQ_2 -d defGMQ_3 -d defGGQ_1 -d defGEQ_2 -d defGKT_0 -ip UNKNOWN -sn null -d defGRX=1920 -d defGRY=1080`
- "Hata 22" = başlatıcı el sıkışması: script ilk karede `HKCU\Software\Sobee\SuperCan\ohc` okur, 0 ise hata verir, sonra 0 yazar.
  Başlatıcı `18249` yazmıştı (1 örnek). Aynı değer tekrar kullanılınca kabul edildi → probe DLL `ohc` sorgusuna `18249` döndürüyor.
- Ardından script `TX` (qd28bpr5173) ve `User\oyuncu_ga.gen` okuyor. WRS'den `.pak`'ları `Data\file_*.gen` geçici dosyalarına açıyor.
- "Hata k21: Geçersiz ekran çözünürlüğü" → defGRX/defGRY kayıt defterindeki `res` listesinde olmalı (1920x1080 OK, 960x1080 değil).
- Tam ekran özel mod iki kopyaya izin vermiyor (E1241 Can't open DirectX device). Çözüm: DLL Direct3DCreate9'u
  GetProcAddress'te yakalayıp CreateDevice/Reset'te Windowed=TRUE zorluyor, pencereyi COOP_WINDOW=x,y,w,h'ye yerleştiriyor.
- `local/tools/launch.ps1 -Copies SuperCan1,SuperCan1b -ResY 540` → iki kopya yan yana menüde (ekran görüntüsüyle doğrulandı).
- Motor değişkenleri: engine_getvar("player") çalışıyor (type=16 → ENTITY**). Menüde player=NULL.
- lite-C açılışta windows.h API'lerinin hepsini GetProcAddress ile çözüyor → API izleme GetProcAddress kancasıyla yapılabiliyor.

## Tahminler (doğrulanmadı)
- Oyuncu entity'si supercan_govde.mdl, diğer parçalar ona bağlı ayrı entity'ler.
- A8 `var` tipi fixed-point 22.10 olabilir → SDK makroları gerekecek.

## Bilinmeyenler
- Başlatıcının supercan.exe'ye verdiği tam komut satırı.
- İki kopya aynı anda çalışabiliyor mu, pencere modu var mı.
- Kayıt dosyası (User/oyuncu_*.gen) şifreleme şeması.

## Plan (Kilometre taşı 1: iki oyuncu aynı haritada birbirini görsün)
1. Komut satırını yakala; iki kopyayı aynı anda çalıştırmayı dene.
2. dinput8.dll proxy → supercan.exe IAT'inde engine_frame kancası (ana thread'de kare başı callback).
3. Prob DLL: ent_next ile entity listesi + script bilgisi logla → oyuncu entity'sini bul.
4. UDP 127.0.0.1 ile poz gönder; karşıda ent_create ile kukla.
5. Sonra: animasyon, Steam relay, düşman senkronu.

# Eski HANDOFF günlükleri (2026-11-15, HANDOFF'tan taşındı; kronolojik ayrıntı)

## Düşman senkronu günlüğü (20:15–21:00, ayrıntı)
- Son commit `28c0adc` (SCO6 yumuşatma, push EDİLMEDİ). Sonrasındaki her şey (fps_max, fare, uyarlanır gecikme, geç paket)
  `probe.c`'de COMMIT'SİZ; kullanıcı: commit + paket + push arkadaşın PC'sine git kurulunca TOPLU yapılacak.
- Paketlerdeki DLL 19:21 SCO6 sürümü; `local/build/dinput8.dll` ve test kopyalarındaki en yeni (20:11). Kablo formatı değişmedi (SCO6).
- Test kopyaları: ikisi `net=udp` bölünmüş ekran; sahte gecikme (sim_*) TEMİZLENDİ; hiçbir coop.ini'de yok.
  SuperCan1'in Steam ayarı `coop.ini.steam`'de. 1. kopya başlatıcıyla (kullanıcı açar), 2.si `tools/start_second.ps1`.
- **Sıradaki iş (kullanıcı seçti): düşman/dünya senkronu.** Henüz hiç başlanmadı; önce oyunun düşman varlıklarını/yapay zekâsını
  incelemek gerek (REA araçları kullanılabilir — kullanıcı "coop anything" araçlarını kullanabileceğimizi söyledi).
- **Dünya izleme (watch) eklendi (derlendi, test kopyalarında `watch=1`; COMMIT YOK):** `watch_tick()` 0,5 sn'de bir tüm varlıkları
  tarar (oyuncu parçaları/kuklalar hariç). Log: `[watch] NEW/GONE/MOVES/FLAGS` (oyuncuya uzaklıkla), `[wtask]` varlıkta çalışan script
  fonksiyonu, `[wskill]` skill[] değişimleri (varlık başına ≤12), `[wsum]` 10 sn'de bir hareket eden tipler. F11 = MARK satırı.
  Bölüm yüklemesi (>200 yeni) tek satır. İlk bulgular (eski orman dökümü): ~47 görev; `kus_ari_main`, `AIF_TSYC_kos/led` (TASIYICI_BOX),
  `alrm_alet_loop`; aday modeller MARTI, FISH, KELEBEK_A, KUS_ARI, KOC, PANZER_*, TASIYICI_*; toplanabilir ELMAS, ENRJ_*, SANDIK/VARIL/KOLI.
  `editor_v0.gen`: `ai_orman.pak`, `path_orman.pak` (düz metin nokta listeleri). Sıradaki: kullanıcı oynar → logu analiz et.
- **İlk watch oyunu (log `SuperCan1/Game/coop_probe_201954_18400.log`, orman):** düşman = **GBR çöp robotu**, dinamik doğuyor
  (ent_create, idx 05...): parçalar GBR_BOX (AI kutusu, görev burada), GBR_FIZIK, GBR_GOVDE/GOZ/LASTIK (görsel), GBR_PLAZMA.
  5 robot, dalga gibi: 1 tane (-26280,-20071) → öldü → 2 tane (-25200,-19840) → 2 tane yine ilk noktada. Durum = çalışan görev:
  `AIF_GBR_carpil` (doğuş) / `kos` / `atakA` / `atakB` / `bekleA` / `yaralan` / `toparlanA/B` / `sevinA`; vuruşta `AIF_GBR_plazma`
  oyuncunun SUPERCAN_BOX'unda çalışıyor. GBR_PLAZMA skill39 vuruş başına +1, 10'da robot GONE → COP_ROBOT_PARCA + COP_GBR_* (teker,
  kafa, govde) + ELMAS_PARCA + YANIK_COP (`cop_robot_fizik`) düşüyor = toplanacak çöp. GBR_BOX skill13 (açı?), 26, 33 (0..100) değişiyor.
  Bölüm başında TASIYICI (taşıyıcı) dinamik: `AIF_TSYC_fizik/bekle/kos/led`. Oyuncu: `pfcan_mod_*` (kos, bekle, LowAtak_A..D,
  yaralan, yuvarlan, CopYuru/CopBekle = çöp taşıma, kutu_cek). Flag 0x100 değişimleri çoğunlukla uzaklık gizlemesi
  (`model_DYNAMIC_off`, ağaçlar da) → toplama tespiti için kullanılamaz. Hata: `player` değişkeni NULL → dist yanlıştı; düzeltildi
  (yerel gövde parçası), 20:23 DLL test kopyalarında.
- **Dünya senkronu adım 1 (20:31, derlendi, kod/çöz testi geçti, OYUNDA TEST EDİLMEDİ; COMMIT YOK):** host-otoriter (kullanıcı onayladı).
  Ayrı paket **'SCW1'** (oyuncu paketi SCO6 değişmedi), 20 Hz: GBR_* görsel parçaları (GBR_BOX/FIZIK hariç), parça id = host'taki
  entity adresi, + robot parçalarına yapılan son karenin animasyon çağrıları (hook'lar `wrecord`; `replay_on` genelleştirildi).
  Client kendi robotlarını INVISIBLE|PASSABLE yapar (yapay zekâsı HÂLÂ çalışır → adım 2: client'ta robot doğuşunu etkisizleştir,
  ör. ent_create'te action'ı boş fonksiyonla değiştir), host'unkileri kukla gösterir, oyuncu kuklasıyla aynı render zamanında (g_last_rt).
  Ayarlar: `role=host|client` (yoksa otomatik: UDP'de küçük listen portu, Steam'de küçük SteamID host), `world=0` kapatır.
  Log: `[world] role= local_parts= calls/frame= tx= avg= max= rx= puppets= hidden=` her 600 karede; puppet oluştur/sil satırları.
  **Test 1 (20:32, bölünmüş ekran): ÇALIŞTI** — host robotları client'ta göründü, ölünce silindi (~335 B/paket, 2–3 çağrı/kare).
  Sorunlar: client'ın kendi gizli robotu yine saldırdı (yerde izleri vardı), robot ölünce düşen çöp parçaları client'ta yok.
- **Adım 2 (20:37, derlendi, TEST EDİLMEDİ):** `ent_create` detour (prolog 6 bayt `8b54240885d2`, sonrası jnz rel8 kopyalanmaz).
  `[create]` ilk 400 çağrıyı loglar (dosya, action adı, konum). Client paylaşılan bölümdeyken `gbr_box.mdl` action=NULL ile
  yaratılır (BLOCKED) + INVISIBLE|PASSABLE → client'ta robot AI'sı/parçaları hiç başlamaz. Risk: client'ın dalga mantığı takılabilir (önemsiz).
  Sonra: çöp parçaları (COP_*), client'ın kuklaya vuruşu → host (adım 3), robotların client'ı hedeflemesi.
  **Test 2 (20:42):** engelleme ÇALIŞMADI (blocked=0): oyun robotları/dronu `ent_createlocal` ile yaratıyor (ent_create ile aynı prolog).
  Client'ta robot kuklasının ışığı hatalı (yerel aynı tip parça yok → copy_look yok). Robotları bırakan bir DRONE (TASIYICI_*) var.
- **Adım 2b (20:49, derlendi, TEST EDİLMEDİ):** ent_createlocal da kancalı; client'ta gbr_box + tasiyici_box BLOCKED; TASIYICI_* (BOX hariç)
  dünya tipi (dron kukla olur). Paket **SCW2**: her parçanın görünüşü (bayt 64..95 skin..rgb, malzeme ADI → client'ta engine_getobj,
  sınıf 13 doğrulamalı, sıfır olmayan skill'ler ≤16) 10 pakette bir, parçalara dağıtılmış. Log: [world] look applied / material not found.
  **Test 3 (20:53):** client orman yüklenirken KAPANDI, hemen `tasiyici_box` BLOCKED sonrası (bölüm betiği dronun parçalarını kullanıyor)
  → dron engellenmiyor (20:54 DLL, test kopyalarında); client'ta kendi dronu çalışır ama gizlenir (world tipi), host'unki kukla.
  **Test 4 (20:57): ÇALIŞTI** — client yüklendi, `GBR_box` (action AIF_GBR_fizik) BLOCKED, görünmez saldırı yok; dron kukla oldu.
  Client'ta doğuş elektrik efekti hâlâ görünüyor (ayrı nesne, sonra). Malzemeler ADSIZ (mtl='') → 21:02 DLL: host, malzemeyi aynı
  MATERIAL'i kullanan robot-dışı bir entity tipiyle adlandırır ('@MTRL_...', önce MTRL_* tercih), client kendi o tip entity'sinin
  malzemesini kullanır. Log: [world] material X -> source / material source -> here. TEST EDİLMEDİ.
  **Kullanıcı fikri (ileride):** iki oyuncu için düşmanları mantıklı yerlerde 2 katına çıkar — host'ta, `[create]` logundan
  öğrenilen gbr_box doğuşunu aynı action ile yakın bir noktada bir kez daha `ent_create` ederek (yalnızca host; client zaten kukla görür).


## Yumuşatma (SCO6) — TEST EDİLDİ, PAKETLERDE (19:50)
- Protokol **SCO6** (SCO5 ile uyumsuz; iki taraf aynı DLL olmalı). Alıcı snapshot halkası + render zamanı = şimdi − saat farkı − `interp_ms` (varsayılan 100).
  Ayarlar (coop.ini / env): `interp_ms=`, `sim_lag=` `sim_jitter=` (ms), `sim_loss=` (%) — alınan paketlere yapay ağ koşulu (tek PC testi için).
  Log her 600 karede: `[interp] frames= held= clk_resets= snaps= delay= simdrop=` (held = yeni snapshot olmadığı için beklenen kare).
- Sonuç, tek PC bölünmüş ekran (net=udp): sim yok → held=0; sim 60 ms + 0–80 ms jitter + %5 kayıp → held %2–4. Kullanıcı iki pencerede de
  takılma/duraksama görmedi (sim'li pencerede bile). Gerçek internet testi (arkadaşla) henüz yapılmadı; `held` yüksekse `interp_ms` artır (150–160).
- Paketler (`coop_paket`, `steamcoop_paket`) SCO6 DLL ile güncellendi. `g_pend`/`g_npend` artık kullanılmıyor (silinebilir).
- Test kopyaları şu an `net=udp` bölünmüş ekran (Steam ayarı `SuperCan1/Game/coop.ini.steam`'de); sim ayarları temizlendi.
- Başlatıcı jetonu: doğrudan exe "Hata 22" verir; 1. kopya `SuperCan_Oyna.exe` ile elle açılır, 2.si `tools/start_second.ps1`.

## 19:35–19:50 eklenenler (derlendi, test kopyalarında; COMMIT/PAKET YOK — kullanıcı: arkadaşın PC'sine git kurulunca toplu yapılacak)
- **FPS:** `fps_max=` ayarı. Yoksa 144, `0` = oyunun kendi değeri (61), N = N. Motor değişkeni `engine_getvar` type 19 → yuva gerçek
  var'ın adresini tutar (`engine_var()`). `fps_lock=0` → hız normal kalıyor (kullanıcı doğruladı). Ölçüm: 144'te ~120, 1000'de 85–140
  (arka planda GPU işi vardı). Log: `[fps] fps_max=.. fps_lock=..` bir kez, `[fps] measured N fps` her 5 sn.
- **Fare (bölünmüş ekran):** `window=x,y,w,h` iken imleç pencerenin dışındaysa GetCursorPos −5000 döner (`mouse_focus=0` kapatır;
  `window=full`'da etkisiz). Çift imleç sorunu çözüldü gibi. Kalan: sağ pencerede imleç yatay yarıdan sonra kayboluyor — muhtemelen
  eski ölçekleme (ekran x → g_win_x + (x−g_win_x)·bb_w/win_w, sağ pencerede 1920'yi aşıyor). Önemsiz, sonra bakılacak.

## 20:00–20:11 paket kaybı / jitter dayanıklılığı (derlendi; COMMIT/PAKET YOK)
- Analiz: konum/açı her pakette tam; oyundaki animasyon çağrıları her kare tekrarlanıyor (durum gibi), tek seferlikler yalnızca bölüm
  başında (poz 2 'Bekle' + blend) ve kukla tohumlama bunu kapatıyor → animasyon tarafında ek tekrar/yedek GEREKMEDİ.
- Stres (80 ms + 0–150 ms jitter + %20 kayıp): eski kodda held ~%30 (kullanıcı "çok ufak takılmalar" gördü), çünkü sırası bozuk
  paketler atılıyordu (kullanılan ~16/30 sn).
- Değişiklikler: (1) sırası bozuk paket, henüz çizilmemiş zamana aitse snapshot halkasına zaman sırasıyla eklenir (`g_remote` güncellenmez);
  (2) uyarlanır gecikme: held karede +0,5 s/s (yalnızca 0,3 sn'den kısa beklemelerde; uzun olan yükleme/donma), aksi −5 ms/s,
  sınır `interp_ms` (100) … `interp_max` (400), `interp_adapt=0` kapatır; (3) render zamanı duvar saatiyle akar, hedefe %5/kare
  çekilir (geri sıçramaz); (4) yeni bölümde gecikme interp_ms'ye sıfırlanır, 3 sn ayarlanmaz (bu son kısım TEST EDİLMEDİ).
- Sonuç: kullanıcı "gecikme var ama takılma yok". Sim'li taraf 180–250 ms, held %0,3–2; sim'siz taraf 100'e iniyor.
  Log: `[interp] ... delay= late_used= late_old=`.

## Kullanıcı istekleri / sıradaki
- Her testte başlatıcıyı elle açmak zahmetli → başlatıcıyı otomatik tıklayan betik (jetonu ÇÖZME kararı geçerli).

## Neredeyiz
**İNTERNET ÜZERİNDEN İKİ PC ÇALIŞIYOR (17:05, kullanıcı doğruladı; farklı Wi-Fi ağlarından da denendi).**
Steam (Spacewar 480) üzerinden iki oyuncu aynı bölümde birbirini kukla olarak görüyor. Konum, açı, animasyon, kemik
pozları, aksesuarlar ve malzeme doğru. Ana PC logu (`testinstall/SuperCan1/Game/coop_probe_170017_8264.log`):
arkadaş kendiliğinden bulundu, ~30 paket/sn iki yönde, kayıp yok, ortalama 726 B / en fazla 837 B, `trunc=0`. ≈22 KB/s.

Kilometre taşları: (1) bölünmüş ekran tek PC ✔ → yerel ağ iki PC ✔ (16:22) → paket küçültme ✔ → internet/Steam ✔.

## Sıradaki işler (kullanıcının sırasıyla; 1 ve 4 bitti)
2. Gecikme yumuşatma: kuklanın konum/açısını gelen paketler arasında interpolasyonla yürüt (şu an her paket geldiğinde zıplıyor
   olabilir; 30 Hz gönderim, 60 fps oyun). Kullanıcıdan "takılma var mı" geri bildirimi alınmadı.
3. Paket kaybına dayanıklılık (durum zaten her pakette tam; animasyon çağrıları kaybolunca poz bir paketlik bozulabilir).
5. Düşman/dünya senkronu, ardından SüperCan 2'ye taşıma.
- Küçük işler: diğer PC'de ilk `git clone` denemesi "bir iki hata" verdi, ayrıntı gelmedi (USB paketiyle geçildi).
  Steam overlay sorun çıkarmadı (kullanıcı bir şey söylemedi).


---
## 2026-11-15 22:45–23:45 — Robot hasar modeli (RE ayrıntısı)

### Önceki oturumun "hit iletimi" ilk sürümü (HANDOFF'tan taşındı)
- Paket `SHT1` (`HITPKT` magic,seq,id,dmg). Client `hit_client_tick()`: `engine_gettaskinfo` ile yerel `pfcan_mod_LowAtak_*` görevinin başlamasını yakalar,
  150 birim içindeki en yakın `GBR_GOVDE` kuklasını seçer, 3 kez gönderir (dedupe `seq`). Host `hit_rx` → `hit_apply`.
- İlk sürüm skill'leri tahminle yazıyordu (s39 +dmg, s35/s36 −9·dmg, s37 1/3): sayaç birikiyordu ama animasyon/ölüm yoktu.
  `AIF_GBR_yaralan`'ı `engine_callname4(name, box,...)` ya da doğrudan adresle çağırmak ÇÖKTÜ (me global'i yanlış + wait'li gövde).

### Bulgular (bu oturum)
- `AIF_GBR_plazma(arg0, arg1)`: gerçek vuruşta arg0 = robot GBR_BOX, arg1 = 1.0 (0x400), me = host SUPERCAN_BOX (level ent, idx 04000154), you = robot GBR_BOX.
  Çağıran `pfcan_mod_LowAtak_A+0x41f4`. Yalnızca kıvılcım efekti + GBR_PLAZMA s39++; robot canına dokunmaz. me/you ayarlı `engine_callscript4` ile tekrar oynatma çöksüz.
- `AIF_GBR_yaralan()`: robotun KENDİ görevlerinden çağrılır: `AIF_GBR_atakA+0x8f9`, `AIF_GBR_bekleA+0x5b6`, `AIF_GBR_kos+0x77c`; me = robot GBR_BOX, you değişken
  (0, PLATFORM, SUPERCAN_BOX), arg1 değişken (0xfb9, 0xbb8, GBR_FIZIK ptr). `yaralan+0x3af` s35'i hasarlı tam sayıya yazar, `+0xd9f/+0xe2b` s34 (art arda sayaç), `+0x209e` s35'i tam sayıya yuvarlar.
- `watchhp` örnek (host vuruşları): `s35 -> 71.90 by pfcan_mod_LowAtak_A+0x3d07` → `s39 -> 1 by LowAtak_A+0x4131` → plazma → `yaralan` (atakA'dan) `s35 -> 68.00`.
- LowAtak_A..D farkı (kod okuma, `you` yazımları): s37 = 0/1/2/3; s35 −= 0.1 (dördünde de double 0.1); s39 = me.s39; s38 == 1 ise plazma, değilse 0x831bbeb (başka efekt);
  you.push(+132) == −1 ise fizik itmesi (`[ebx+0x28c974]`, ackphysX). D'de rastgele yerine sabit 15/20/100 değerleri var (efekt).
- Hedef seçimi LowAtak_A `081a2f63..081a30d2`: `vec_for_vertex` (silah ucu), `[ebx+0x2661d0](…, 0x200065)` = c_trace benzeri, `[ebx+0x26c678]` = hit yapısı,
  +80 alanı & 1536 kontrolü, you = +84 alanı; sonra `you.flags(+52)` & 16 kontrolü. → client'ta PASSABLE kuklalar asla `you` olmaz.
- `AIF_GBR_kos` saldırı kararı (`081f72c5..081f754f`): f(me.x, me.y) < 2500 VE global kilit `[ebx+0x26b180]` == 0 VE hedef `[ebx+0x26b184]`'e açı farkı < 6 → `AIF_GBR_atakA`
  (bir yerde de atakB). Canlı okuma: kilit 0, hedef = host SUPERCAN_BOX.
- Gözlenen hasarlar (yaralan): A −4, B −6, D −13; C client'tan gelince çoğunlukla 0 (stun penceresinde geldi). Host kendi kombosunda s34 1→2 artıyor.
- Robot ölünce motor entity belleğini/link.index'leri yeniden kullanıyor: yeni robotun GOVDE idx'i BOX'ınkinden küçük olabildi (050008fb < 0500090d) → gruplama konumla.
- `AIF_GBR_kos` her karede s34 = 0 yazar (veri kesme noktası taşması: yalnız değişimi logla).


---
## 2026-11-16 00:00 — Host'ta gerçek kombo ölçümü (watchhp)
- Host kendi `d d d d` yaparken: A: s35 72→71.9 (LowAtak_A+0x3d07) → plazma → yaralan (kos+0x77c / atakA+0x8f9) 68.0 (−4, +0x3af), s34=1 → yaralan+0x209e yuvarlar.
  B: s37=1 (LowAtak_B+0x36e0), s35−=0.1, s39++, plazma (B+0x3c66); yaralan **yaralan+0x1c29'dan (özyinelemeli)** → 28→22 (−6, +0x559), s34=2.
  C: s37=2 (C+0x346b), s35 21.9 (C+0x3504) → yaralan+0x703: 22→13 (−9). D: s37=3 (D+0x2ef6), s35 −0.1 (D+0x2f8f) → yaralan+0x8c8: →0 (ölüm), +0xc5f → −5.
- HeavyAtak_A: s37=4 (+0x40d8), s35−=0.1 (+0x4171), s39 (+0x44ff), plazma (+0x45c2); sonra `AIF_GBR_yuvarlan` s34=0 (+0x24f), s35 68→48 (+0x8e4, −20).
- Client'tan gelen vuruşlarda (SHT1, hit_delay 250 ms, pozitif s39): C'den sonra robot yaralan çağırmıyor (pz logunda B→C arasında yaralan yok); D'nin yaralan'ı D hasarını (−13) verir. s35 küsuratı C'de yuvarlanıp hasarsız kalıyor.
- Ağır vuruş client'tan: −20 çalışıyor. hit_delay'in B/D üzerinde etkisi ayrıca ölçülmedi.


---
## 2026-11-17 00:00–01:15 — Tam Oyun Lite-C Dökümü (Ghidra 857 Fonksiyon) ve Kesin Mimari Analiz

### 1. Döküm ve Decompile Boru Hattı (`local/dump/code/` & `local/tools/`)
- **`dumpall <klasör>` (`probe.c`):** Derlenmiş lite-C kod bölgesini (`region_07d40000.bin`, 10.6 MB), `functions.txt` (857 fonksiyon başlangıç adresi + adı) ve `acknex_exports.txt` dosyasını döker.
- **`local/tools/acknames.py`:** `acknex.dll` içindeki lite-C sembol tablosunu (475 motor fonksiyonu + 286 motor değişkeni: `me`, `you`, `player`, `camera`, `hit`, `c_trace`, `c_scan`, `c_move`, `vec_for_bone` vb.) `local/dump/code/acknex_names.txt`'ye çıkarır.
- **`local/tools/mkpe.py`:**
  1. Kendi inline hook'larımızı (`E9 rel32` + 4 NOP) orijinal prologa (`55 53 56 57 E8 00 00 00 00`) geri çevirir.
  2. Lite-C'nin konumdan bağımsız `EBX` kurulumunu (`call $+5; pop eax; mov ebx, eax; sub ebx, imm32`) sabit `mov ebx, 0x07d40045` + 9 NOP'a çevirir (857 fonksiyonun tamamı aynı `EBX = 0x07d40045` tabanını kullanır).
  3. Lite-C'nin her `var ↔ double` dönüşümünde kodun içine gömdüğü **5.035 adet** satır içi `call $+13; <8 bayt double 1024.0>; pop eax` (`E8 08 00 00 00 00 00 00 00 00 00 90 40 58`, 14 bayt) dizisini `0x07d40010` adresine tek bir `1024.0` sabiti koyup `mov eax, 0x07d40010` + 9 NOP'a çevirir. (Bu düzeltme olmadan Ghidra bunları `Possible PIC construction` sanıp blokları siliyordu.)
- **`local/tools/ghidra/DecompAll.java`:** 857 fonksiyon sınırında önceki fonksiyon taşmalarını kırpar (`prev.setBody(...)`), eksik fonksiyonları oluşturur ve tamamını `local/dump/code/decomp_raw.c` olarak döker (0 PIC uyarısı).
- **`local/tools/scnames.py`:** `decomp_raw.c` içindeki tüm `DAT_*` / `*Ram*` sembollerini motor adlarına (`E_c_trace`, `*E_me`, `*E_you`), sabit değerlerine (`1024.0`, `148`, `52` vb.), fonksiyon adlarına ve global değişkenlere (`g_<hex>`) çevirerek `local/dump/code/decomp.c` dosyasını üretir.
- **`local/tools/disfn.py <fonksiyon_adı> [max_ins]`:** İstenen fonksiyonu doğrudan Capstone ile sembol/sabit/string açıklamalı x86 assembly olarak döker.

### 2. Çalışma Anında `EBX` Tabanı ve Global Değişken Haritası (`main_variable_reset @ 082ecace`)
Her lite-C fonksiyonu `55 53 56 57 E8 00 00 00 00 58 8B D8 81 EB <imm32>` prologuyla başlar. Herhangi bir kancalanmamış lite-C fonksiyonundan (ör. `fn = engine_getscript("AIF_GBR_fizik")`) çalışma anı `EBX` tabanı:
```c
DWORD ebx_base = (DWORD)fn + 9 - *(DWORD *)((BYTE *)fn + 14);
```
Dökümde `ebx_base = 0x07d40045` idi. Dökümdeki her `g_<addr>` globalinin çalışma anı adresi `ebx_base + (0x<addr> - 0x07d40045)`:

| Döküm Adı | `EBX` Ofseti | Tipi | Açıklama / Kullanıldığı Yerler |
|---|---|---|---|
| `g_07fa466d` | `+0x264628` | `var` | Oyuncu canı (`can`, başlangıç `100 << 10`) |
| `g_07fa4671` | `+0x26462c` | `var` | **Oyuncu Nano barı** (`nano`, başlangıç/maks `100 << 10`; `HeavyAtak_A`: −20, `HeavyAtak_B`: −30) |
| `g_07fa00ad` | `+0x260068` | `var` | **Sahadaki canlı robot sayısı** (`N << 10`; `AIF_TSYC_kos` +1 artırır, `AIF_GBR_sil`/`parcalan` ve `AIF_ZBD_sil`/`parcalan` −1 azaltır) |
| `g_07fce699` | `+0x28e654` | `var` | **Taşıyıcının bırakacağı bekleyen `GBR` sayısı** (`N << 10`; `camera_path_loop` / `main_alarm_start` yazar, `AIF_TSYC_kos` bırakırken −1 azaltır) |
| `g_07fce69d` | `+0x28e658` | `var` | **Taşıyıcının bırakacağı bekleyen `ZBD` sayısı** (`N << 10`; `AIF_TSYC_kos` bırakırken −1 azaltır) |
| `g_07fce6a1` | `+0x28e65c` | `var` | **Aynı anda sahada olabilecek maksimum robot üst sınırı** (`AIF_TSYC_kos` içinde `g_07fa00ad < g_07fce6a1` kontrolü) |
| `g_07fa00b1` | `+0x26006c` | `var[]` | Sahadaki canlı robotların `handle` dizisi (`AIF_TSYC_kos` yazar, `AIF_*_sil`/`parcalan` sıfırlar) |
| `g_07fa4685` | `+0x264640` | `var` | Dalga/alarm toplam kalan robot sayacı (`AIF_Robot_olumctrl`, `AIF_GBR_parcalan`, `AIF_ZBD_parcalan` −1 azaltır) |
| `g_07fab1c9` | `+0x26b184` | `ENTITY*` | **Oyuncu kutusu (`SUPERCAN_BOX.MDL`)** — `vecAI_dist`, `AIF_ang_turn`, `AIF_GBR_kos`, `AIF_ZBD_kos`, `AIF_UCAN_patla` hedef olarak bunu okur |
| `g_07fab1c1` | `+0x26b17c` | `var` | Oyun/kontrol aktif bayrağı (`1 << 10` = aktif, `0` = ara sahne / geçiş) |
| `g_07fab1c5` | `+0x26b180` | `var` | Oyuncu yerde/ölü bayrağı (`0` = ayakta, `1 << 10` = düştü/öldü; `1` iken robotlar `bekleA` / `sevinA`'ya geçer) |
| `g_07fccb69` | `+0x28cb24` | `ENTITY*` | `supercan_govde.mdl` |
| `g_07fccb51` | `+0x28cb0c` | `ENTITY*` | `supercan_yuz.mdl` |
| `g_07fccb5d` | `+0x28cb18` | `ENTITY*` | `supercan_flora.mdl` (kılıç vertex 35 buradan alınır) |
| `g_07fccbb1` | `+0x28cb6c` | `ENTITY*` | `supercan_sword.mdl` |
| `g_07fccb9d` | `+0x28cb58` | `ENTITY*` | `supercan_ruzgar.mdl` |

### 3. Robot Skill Düzeni (`ENTITY + 148 + idx * 4`, 0-tabanlı `skill[idx]` = 1-tabanlı `s(idx+1)`)
`AIF_GBR_fizik` (`081e9b6c`), `AIF_ZBD_fizik` (`0821cf70`), `AIF_UCAN_fizik` (`082361d9`) incelemesinden:
- `skill[0]` (`s1`): Robotun `g_07fa00b1` dizisindeki slot indeksi (`0..N`).
- `skill[2]` (`s3`): Yüzey tipi (`-1 << 10` = robot kutusu).
- `skill[12]` (`s13`): Robotun baktığı/hareket ettiği yön açısı (`pan`).
- `skill[21]` (`s22`): `GBR_PLAZMA.MDL` / `ZBD_PLAZMA.MDL` handle'ı (`ptr_for_handle` ile çözülür).
- `skill[27]` (`s28`): `GBR_FIZIK.MDL` / `ZBD_FIZIK.MDL` handle'ı.
- `skill[28]` (`s29`): `GBR_GOZ.MDL` / `ZBD_LIGHT.MDL` handle'ı.
- `skill[29]` (`s30`): `GBR_LASTIK.MDL` / `ZBD_KAFA.MDL` handle'ı.
- `skill[30]` (`s31`): `GBR_GOVDE.MDL` / `ZBD_GOVDE.MDL` / `UCAN_GOVDE.MDL` handle'ı.
- `skill[31]` (`s32`): `GBR` ses handle'ı / `ZBD_SOPA0.MDL` handle'ı.
- `skill[33]` (`s34`): Art arda darbe sayacı (`AIF_GBR_kos` her kare `0` yazar; `yaralan` içinde artar).
- `skill[34]` (`s35`): **Tetikleyici can değeri** (`pfcan_mod_*` vuruşta `s35 -= 0.1` (`102`) yazar; robot döngüsü `s35 < s36` görünce `yaralan`/`yuvarlan` çağırır).
- `skill[35]` (`s36`): **Gerçek tam sayı can değeri** (`GBR` = `72 << 10`, `ZBD` = `96 << 10`, `UCAN` = `100 << 10`). Hasar yalnız `yaralan`/`yuvarlan` başında `s36`'dan düşülür ve hemen `s35 = s36` eşitlenir.
- `skill[36]` (`s37`): **Saldırı/kombo tipi kodu (`var`):**
  - `0 << 10` (`0`): `LowAtak_A` (1. vuruş) → `yaralan(0)` → **−4 HP**
  - `1 << 10` (`1024`): `LowAtak_B` (2. vuruş) → `yaralan(1)` → **−6 HP**
  - `2 << 10` (`2048`): `LowAtak_C` (3. vuruş) → `yaralan(2)` → **−9 HP**
  - `3 << 10` (`3072`): `LowAtak_D` (4. vuruş) → `yaralan(3)` → **−13 HP**
  - `4 << 10` (`4096`): `HeavyAtak_A` (`s` veya `ds`, nano ≥ 20 iken) → `yuvarlan(0)` → **−20 HP** (20 nano harcar)
  - `5 << 10` (`5120`): `HeavyAtak_B` (`dds`, nano ≥ 30 iken) → `yuvarlan(1)` → **−40 HP** (30 nano harcar)
  - `3482` (`3.4 * 1024`): `HeavyAtak_A` / `HeavyAtak_B` (nano yetersizken!) → `yuvarlan(3.4)` → **−10 HP** (0 nano harcar)
  - `6 << 10` (`6144`): `yuvarlan(2)`
  - `8 << 10` (`8192`): Yuvarlanan robotun başka robota çarpması → `yuvarlan(4)` → **−10 HP**
- `skill[37]` (`s38`): **Düşman sınıfı kimliği:**
  - `1 << 10`: `GBR` (`GBR_BOX.MDL` ve `GBR_FIZIK.MDL`)
  - `2 << 10`: `ZBD` (`ZBD_BOX.MDL` ve `ZBD_FIZIK.MDL`)
  - `3 << 10`: `UCAN` (`UCAN_BOX.MDL`)
  - **ÖNEMLİ (`AIF_GBR_plazma @ 081b5d75` & `AIF_ZBD_plazma @ 081ebcdb`):** Her iki fonksiyon da en başta `if (arg0->skill[37] == (1 << 10) || arg0->skill[37] == (2 << 10))` kontrolü yapar! Eğer `skill[37] == 0` ise **hiçbir handle (`skill[21]`, `skill[30]`) çözmeden derhal `return` eder!**
  - Önceki testte `pz_replay` hatasının (`FAULT at AIF_GBR_plazma+0x61c, access 0000012c`) sebebi: `GBR_FIZIK.MDL` de `skill[37] = 1 << 10` taşıyor ama `skill[21]` (plazma handle) yalnız `GBR_BOX.MDL` üzerinde dolu (`GBR_FIZIK.MDL` üzerinde `0`). Host vuruşu `GBR_FIZIK.MDL` yakaladığında `pz_replay` `GBR_FIZIK.MDL` ile `AIF_GBR_plazma` çağırdığı için `ptr_for_handle(0) -> NULL` çöktü. Çözüm: `AIF_GBR_plazma` / `AIF_ZBD_plazma` her zaman doğrudan `box` (`GBR_BOX.MDL` / `ZBD_BOX.MDL`) ile çağrılmalı.
- `skill[38]` (`s39`): Son vuran saldırının kimlik sayacı (oyuncunun `me->skill[38]` değeri her saldırı başında `+1 << 10` artar; aynı savurmada ikinci kez ses/plazma tetiklememek için `me->skill[38] != you->skill[38]` kontrol edilir).
- `skill[39]` (`s40`): `GOLGE_TOP.MDL` handle'ı (`AIF_GBR_sil`, `AIF_GBR_parcalan`, `AIF_GBR_yuvarlan` bunu `ptr_for_handle` ile çözer — **ASLA `skill[39]` üzerine sayı yazma!** Bizim `s39` dediğimiz 1-tabanlı `skill39` = `skill[38]`'dir).

### 4. C Vuruşunun (3. Kombo) Neden 0 Hasar Verdiğinin Kesin Kanıtı (`AIF_GBR_yaralan @ 081d769c..081d7dea`)
`AIF_GBR_yaralan` (ve `AIF_ZBD_yaralan`) başında hasar `skill[35]` (`s36`)'dan düşülür ve `skill[34] = skill[35]` (`s35 = s36`) yapılır. Sonra sarsılma animasyonu (`anim_progress`: `0` → `100`) döngüsünde **her karede** şu kod çalışır:
```c
if ((anim_progress > (50 << 10) || my->skill[36] /*s37*/ >= (4 << 10)) && (my->skill[34] /*s35*/ < my->skill[35] /*s36*/)) {
    // 081d784d: yeni vuruşu kabul et!
    if (my->skill[36] < (4 << 10)) AIF_GBR_yaralan(my->skill[36]);
    else if (my->skill[36] == (4 << 10)) AIF_GBR_yuvarlan(0);
    else if (my->skill[36] == (5 << 10)) AIF_GBR_yuvarlan(1 << 10);
    ...
    return;
} else {
    // 081d7d21..081d7dea:
    my->skill[34] /*s35*/ = my->skill[35] /*s36*/; // <--- HASAR BAYRAĞINI SİLİYOR!
}
```
- **Neden C yutuluyordu:** Client'tan B vuruşundan sonra gelen C vuruşu (`s37 = 2 << 10 < 4 << 10`), host'taki robotun B sarsılma animasyonu henüz **`%50`'yi geçmeden** (`anim_progress <= 50`) ulaştığında, `else` dalı (`081d7d21`) `my->s35 = my->s36` yazarak bizim `s35 -= 0.1` bayrağımızı **aynı karede siliyordu**!
- **Neden `s` (ağır vuruş) hep çalışıyordu:** Koşulda `|| s37 >= 4` olduğu için ağır vuruşlar `%50` sınırına takılmadan anında `if` dalına giriyordu.
- **Kesin Çözüm:** Host'ta vuruş geldiğinde robotun o anki gerçek canı (`orig_s36 = skill[35]`) kaydedilir; `skill[35]` gerçekten azalana kadar (yani `yaralan` veya `yuvarlan` vuruşu tüketene kadar) her karede `skill[36] = hit_s37` ve `skill[34] = orig_s36 - 102` yazılmaya devam edilir. Böylece `anim_progress` `50`'yi geçtiği ilk karede C vuruşu kesin olarak işlenir.

### 5. Client'ta Kukla Çarpışması, Hedef Kilidi, Nano ve Kılıç Tespiti (`pfcan_mod_LowAtak_A..D`, `HeavyAtak_A..B`)
- Oyunun saldırı fonksiyonları:
  1. Başlangıçta `c_scan(&me->x, pan_vec, vector(160, 175, 200), 0x240005 << 10)` çağırır; `you != 0 && (you->flags & 16 /*FLAG2*/)` ise oyuncuyu `you` varlığına döndürür (**hedef kilidi**).
  2. Saldırı sırasında `vec_for_vertex(temp, g_07fccb5d /*flora/kılıç*/, 35 << 10)` ile `c_trace(&me->x, temp, 0x200065 << 10)` çağırır (`0x200065` = `IGNORE_ME | IGNORE_PASSABLE | IGNORE_PASSENTS | IGNORE_SPRITES | SCAN_TEXTURE`).
  3. Eğer `(hit.flags & 1536) != 0` ve `you = hit.entity` üzerinde **`you->flags & 16` (`FLAG2`)** varsa:
     - `HeavyAtak_A` / `HeavyAtak_B` içinde **`g_07fa4671 = yeni_nano`** yazılır (**nano yalnız burada düşer!**).
     - `you->skill[36] = hit_s37` ve `you->skill[34] -= 102` yazılır.
     - `kivilcim_cik(...)` çağrılır.
     - `if (me->skill[38] != you->skill[38])` ise vuruş sesi (`entPlaySound`) çalınır, `you->skill[38] = me->skill[38]` yazılır ve `AIF_GBR_plazma` / `AIF_ZBD_plazma` çağrılır.
- **Kuklaları `c_trace` ve `c_scan`'e açmak:**
  - Client'ta `GBR_GOVDE.MDL`, `ZBD_GOVDE.MDL`, `UCAN_GOVDE.MDL` kuklalarına `PASSABLE_F` (`512`) yerine **`FLAG2` (`16`)** verilirse ve `skill[37] = 0` (`s38 = 0`), `skill[2] = -1 << 10` (`s3 = -1`) yazılırsa:
    - `c_scan` kuklayı bulur → **hedef kilidi** çalışır.
    - `c_trace` kuklayı vurur → **nano (`g_07fa4671`) düşer**, **kıvılcım** çıkar, **vuruş sesi** çalar, `AIF_ZBD_plazma` `skill[37] == 0` gördüğü için **hiçbir şeye dokunmadan çıkar**.
    - Kuklanın `skill[38]` (`s39`) alanı kılıç değdiği anda oyunun kendi kodu tarafından güncellenir → `hit_client_tick` bu değişimi yakalayıp kuklanın `skill[36]` (`s37`) değerini `0 ms` gecikmeyle host'a gönderir!

### 6. Bölüm Sonu Geçişi (`main_level_gecis @ 082e80db`) ve Tüm Dünya/Çöp Modelleri
- `main_level_gecis` başında:
  ```c
  if ((g_07fce699 + g_07fce69d + g_07fa00ad > 0) && (my->skill[0] % (2 << 10) == 0)) {
      main_info_uyari(g_07fa092d, 152 << 10, 0); // "Önce buradaki robotları halletmem gerek!"
      return;
  }
  ```
- Client'ta `g_world_client_active` iken:
  - `g_07fce699 = 0` ve `g_07fce69d = 0` tutulmalı (client'ın yerel `AIF_TSYC_kos` taşıyıcısı hiç robot bırakmaya çalışmamalı).
  - `g_07fa00ad` host'tan gelen canlı düşman sayısına eşitlenmeli (host'ta robotlar ölünce client'ta da `0` olur; bölüm kapısı, takla geçidi `helper_taklagecit`, tramplen `helper_tramplen` ve müzik senkron çalışır).
- Dünya senkronuna (`is_world_type`) eklenecek tüm modeller:
  - `GBR_*` (`GBR_BOX.MDL` ve `GBR_FIZIK.MDL` hariç)
  - `ZBD_*` (`ZBD_BOX.MDL` ve `ZBD_FIZIK.MDL` hariç)
  - `UCAN_*` (`UCAN_BOX.MDL` hariç)
  - `TASIYICI_*` (`TASIYICI_BOX.MDL` hariç)
  - Çöp parçaları: `COP_GBR_*` (`cop_GBR_govde0/1.mdl`, `cop_GBR_kafa0/1.mdl`, `cop_GBR_teker0/1.mdl`), `COP_ZBD_*` (`cop_ZBD_govde0/1.mdl`, `cop_ZBD_ayak0/1.mdl`, `cop_ZBD_kafa0/1.mdl`), `COP_ROBOT_PARCA.MDL`, `YANIK_COP.MDL`.
- Client'ta aksiyonu engellenecek (`is_blocked_spawn`) ana kutular:
  - `gbr_box.mdl`, `zbd_box.mdl`, `ucan_box.mdl`.

### 7. Bölünmüş Ekran Testi (`01:55` Logları) Bulguları, Kemik Kilidi ve Sıradaki Adım RE Analizi
- **Hedef kilidi (`c_scan` ve `ENABLE_SCAN`):**
  - `AIF_GBR_fizik` (`081e9b6c`), `AIF_ZBD_fizik` (`0821cf70`) ve `AIF_UCAN_fizik` (`082361d9`) içinde `*(DWORD *)(me + 52) |= 16` (`FLAG2`) yanında **`*(DWORD *)(me + 96) |= 256` (`emask |= ENABLE_SCAN`)** veriliyor. Acknex motorunda `c_scan` yalnız `emask & ENABLE_SCAN` (`1 << 8`) açık olan varlıkları tarar (`probe.c` içine eklendi).
- **Göz (`GBR_GOZ`) ve alt parçaların kemik kilidi (`AIF_GBR_boneupdate @ 081bfa15`, `AIF_ZBD_boneupdate @ 081ecdfb`):**
  - `GBR_LASTIK.MDL`: `vec_for_bone(&lastik->x, govde, "Bone06")` (`0x07fe0cc1`), `lastik->pan = govde->pan`, `lastik->roll = govde->roll`.
  - `GBR_GOZ.MDL`: `vec_for_bone(&goz->x, govde, "Bone01")` (`0x07fe0cc9`), `ang_for_bone(&goz->pan, govde, "Bone01")`.
  - `ZBD_KAFA.MDL` ve `ZBD_LIGHT.MDL`: `vec_for_bone` + `ang_for_bone` ile `"Bip01 Head"` (`0x07fe22d1`).
  - `ZBD_SOPA0.MDL`: `vec_for_bone` + `ang_for_bone` ile `"Bip01 R Hand"` (`0x07fe22dd`).
  - Client'ta `world_client_render` sonunda `wreplay` çalıştıktan hemen sonra bu kemik bağları doğrudan çalıştırılarak 20 Hz paket gecikmesinden kaynaklanan göz/kafa ayrılması sıfırlandı.
- **İki oyuncuyu hedefleme (`vecAI_dist @ 080179ff`, `AIF_ang_turn @ 0809d78e`):**
  - `vecAI_dist(var x, var y)` doğrudan `g_07fab1c9` (`ebx + 0x26b184`, `G_PLAYER_BOX`) varlığının `x` (`+16`) ve `y` (`+20`) koordinatlarına olan 2B uzaklık karesini (`float10`) döndürür.
  - `AIF_ang_turn(ENTITY *me, var mode, var *out_ang)` da hedef olarak `g_07fab1c9` (`G_PLAYER_BOX`) konumunu okur.
- **Robotların oyuncuya hasar vermesi (`AIF_GBR_atakA @ 081e32f6`, `AIF_GBR_atakB`, `AIF_ZBD_atakA..C`):**
  - `AIF_GBR_atakA`: `vec_for_vertex(temp, govde, 1219 << 10)` ve `c_trace(&me->x, temp, 613 << 10)` çağırır.
  - Çarptığı `you` varlığında `you->flags & 16` (`FLAG2`) varsa:
    - Saldıran robotun konumunu `vec_set(ebx + 0x261ed0 /* 0x07fa1f15 */, &me->x)` adresine yazar (oyuncunun darbe yönü),
    - `you->skill[36] = 0` (`s37 = saldırı tipi`) ve `you->skill[34] -= 102` (`s35 -= 0.1`) yazar.
    - Oyuncunun kendi `pfcan_mod_*` görev döngüsü `my->skill[34] < my->skill[35]` görünce `pfcan_mod_yaralan` / `pfcan_mod_yuvarlan` fonksiyonuna geçer ve canı (`g_07fa466d`, `ebx + 0x264628`) düşürür.



---

## 2026-11-17 (13:02) · Test Doğrulaması, Bölüm/Save Yapısı ve Senkron İhtiyaçları Haritası

### 1. `03:34` DLL Test Doğrulaması (`123953` / `124002` logları + Kullanıcı Geri Bildirimi)
- **Doğrulananlar:**
  - Taşıyıcı altındaki elektrik kubbesi (`PLAZMA_KURE.MDL`, `PLAZMA_KURE2.TGA`) client'ta oluştu ve göründü (`WMAX_PARTS` taşması yok, `trunc=0`).
  - `g_aim_pup` gövde/Z hizası (`dz=191.5`) çalıştı; robot vuruşları client'a isabet etti (`100 -> 95 -> 90 -> 85 -> 80 HP`).
  - Client önde ilerlediğinde dalga tetikleme (`seg=8/9`) ve doğru konumda spawn çalıştı.
- **Eksik / Dikkat Edilmesi Gerekenler:**
  - **Çöp vakumlama (`HIT_DMG_TRASH`):** Client çöpleri süpüremedi (`123953` logunda `TRASH` tetiklenmedi). `03:17`'de `skill[22] = 0` bırakıldığı için `pfcan_kutu_cek` / `pfcan_detect_event` içindeki vakum koşulunun tekrar incelenmesi gerekiyor.
  - Host logu sonunda (`123953`) `devicereset+0x135` noktasında bir `[crash-veh]` kaydı görüldü (pencere/cihaz sıfırlama veya çıkış sırasında).

### 2. Bölüm vs. Save Noktası (Alt-Bölge) Yapısı (`decomp.c`)
- Oyuncunun "1. / 2. / 3. bölüm" olarak oynadığı orman içi kısımlar (merdivenden inip başka bir orman kısmına çıkılan yerler) aslında **aynı harita (`orman.wmb`) içindeki farklı save noktaları / alt-bölgeler**.
- `main_level_gecis` (`082e80db`), `kapi_level_gecis` (`082ebebf`), `merd_level_gecis` (`082ec38a`) haritayı değiştirmeden bir sonraki bölgeye geçirip `main_check_save()` (`082f8729`) çağırıyor ve ilerleme değişkenini (`g_07fa2465`) güncelliyor.
- Ayrı `.wmb` harita yüklemeleri (`level_load`) ise `orman.wmb`, `tunel.wmb` ve diğer ana haritalar (kanal/alarm/hangar) arasında gerçekleşiyor.

### 3. Oyunun Tam Yapısı (`functions.txt` / `decomp.c`) — Senkron Edilmesi Gereken Mekanikler

#### A. Bölüm İçi Etkileşimli Nesneler & Bulmacalar
1. **Makara / Kol / Köprü / Kapı Sistemi (`main_mkr_*`, `nesne_mkr_*`):**
   - `nesne_mkr_kol` / `main_mkr_kolcek` (`082aacb2`), `event_nesne_kol`
   - `nesne_mkr_cark` / `main_mkr_cevir` (`082a8741`), `event_nesne_cark`
   - `nesne_mkr_tahta1` / `nesne_mkr_tahta2` (`g_07fccb8d`, `g_07fccb91` — 3. kısımdaki köprü tahtaları)
   - `nesne_mkr_kapi`, `nesne_mkr_blok1`, `nesne_mkr_blok2`
   - *Durum:* Bu nesneler `is_world_type` içinde yok; çarpışma (`PASSABLE` vb.) ve konum/animasyon durumları senkron değil.
2. **Alarm / Lazer / Silo / Terminal Sistemi (`main_alarm_*`, `nesne_alrm_*`):**
   - `main_alarm_alet`, `nesne_alrm_alet1..4`, `nesne_alrm_isin1..4`, `nesne_alrm_Silo`, `nesne_alrm_Ekran`, `alrm_dos_input`, `main_alarm_saldir`.
3. **Tramplen & Geçiş Yardımcıları:** `nesne_tramplen`, `helper_tramplen`, `helper_taklagecit`.
4. **Alt-Bölge / Harita Geçişleri:** `main_level_gecis`, `kapi_level_gecis`, `merd_level_gecis`, `nesne_knl_merd`.

#### B. Toplanabilirler, Çöp & Güçlendirmeler
5. **Çöp Vakumlama & Geri Dönüşüm Kutuları:** `pfcan_kutu_cek`, `pfcan_mod_CopBekle`, `main_geri_dnsm`, `doldur_geri_dnsm`, `nesne_geri_dnsm`.
6. **Kırılabilir Bonus / Meyve / Enerji Küreleri:** `Bonus_fizik_ekle`, `pfcan_bonus_parcalan`, `pfcan_meyve_parcalan`, `pfcan_rbtenrj_al`, `AIF_RbtEnrj_ekle`.

#### C. Diğer Düşmanlar, Özel Sekanslar & Boss
7. **Arı / Kuş Tehlikeleri:** `kus_ari_main`, `kus_ari_start0..2`, `orman_kus_*`.
8. **Taramalı Taretler:** `main_taramali`, `nesne_taramali1..3`, `topgit_taramali`.
9. **Tünel / Roket Bölümü (`tunel.wmb`):** `pfcan_main_roket`, `can_tunel_player`, `nesne_tunel_ZBD`, `nesne_tunel_TOP`, `nesne_tunel_engel`, `tunel_tas_main`.
10. **Deve / Hangar Boss Savaşı:** `AIF_DEVE_*`, `AIF_Hangar_Start`, `AIF_DEVE_asansor`, `AIF_DEVE_kapiblok`, `nesne_pil_box0..3`, `nesne_alt_laser`, `nesne_lsr_isin*`, `toportak_DEVE`, `DEVE_laser_atak`.

#### D. Oyun Akışı, Ara Sahneler & Ölüm
11. **Ölüm / Checkpoint Senkronu:** `pfcan_mod_olme`, `main_check_save`, `main_check_load` (karar: tek oyuncu ölünce yanına doğma vs. ortak checkpoint).
12. **Diyaloglar & Sinematik Kamera:** `diyalog_main1..4`, `camera_cinema_loop`, `Oyun_video_ac`.

## Geçmiş Handoff Kayıtları (Arşiv)
- 2026-11-15 19:35–20:11: `fps_max`, fare/pencere, paket kaybı/jitter dayanıklılığı, uyarlanır gecikme (`SCO6`).
- 2026-11-15 20:15–22:40: Düşman senkronu `SCW1`/`SCW2`, `ent_createlocal` engelleme, `watch`, otomasyon, `SHT1` ilk sürüm.
- 2026-11-15 22:45–00:25: Robot hasar modeli (s35/s37/s39), client vuruşu host'ta gerçek hasar/ölüm, `pz` hook, `watchhp`, ağır vuruş iletimi.
- 2026-11-17 00:00–01:15: Arkadaş Steam testi analizi, `SCW3` parçalı paket, `dumpall` + Ghidra ile 857 fonksiyonun tamamının decompile edilmesi (`decomp.c`).
- 2026-11-17 01:20–02:25: HANDOFF 1–5. adımlar, bölünmüş ekran testi (C vuruşu −9 HP, nano, `SCW3`), hedef kilidi (`emask |= 256`) ve göz kemik kilidi (`vec_for_bone`).
