# AGENT_LOG — modeller arası iş kaydı

Kurallar ve şablon: `docs/agents/model-gecisi.md`. Yeni kayıtlar **en alta** eklenir; eski kayıtlar silinmez.

**Son incelenen commit:** 28c0adc (Claude, 2026-11-15) — bu commit'e kadar her şey Claude'un kendi işi.

---

## 2026-11-15 · Claude Sonnet 5.5 (Claude Code) · Çok modelli çalışma kurulumu
İnceleme: tamam (Claude'un kendi işi)
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `CLAUDE.md`, `AGENTS.md`, `GEMINI.md`, `AGENT_LOG.md`, `docs/agents/model-gecisi.md`, `.claude/skills/handoff/SKILL.md` (Aqualine'den uyarlandı)
- `local/HANDOFF.md` şablona göre yeniden yazıldı; eski günlükler `local/NOTES.md` sonuna taşındı
Doğrulama: yalnızca belge, derleme gerekmedi
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- yok
Kalan sorunlar / yarım kalanlar:
- yok


## 2026-11-15 · Claude Sonnet 5.5 (Claude Code) · Client vuruşu → host hasarı (SHT1)
İnceleme: tamam (Claude'un kendi işi; commit yok)
Commit'ler: yok
Yapılan:
- `probe.c`: `HITPKT`/`SHT1`, `hit_client_tick`, `hit_rx`, `hit_apply`, `sk_add/sk_get`, host `hit` komutu, `hit_call` denemeleri
- `local/tools/autotest.ps1`: `-MenuOnly`
Doğrulama: derlendi; iki pencerede denendi — hasar sayacı host'ta birikiyor, robot ölmüyor/animasyon yok; `AIF_GBR_yaralan` DLL'den çağrısı çöküyor
Kullanıcı/arkadaş denedi mi: kullanıcı denedi (yukarıdaki sonuç)
Varsayımlar:
- Hasar puanı A/B=1, C/D=2; menzil 150 birim (ayarlanmadı)
Kalan sorunlar / yarım kalanlar:
- Oyunun gerçek `AIF_GBR_plazma` çağrısını yakalayıp host'ta tekrar oynatmak (HANDOFF "Sıradaki adım")


## 2026-11-15 · Claude Opus 5.5 (Claude Code) · Robot hasar modeli çözüldü, client vuruşu gerçek hasar veriyor
İnceleme: tamam (Claude'un kendi işi; commit yok)
Commit'ler: yok
Yapılan:
- `probe.c`: `AIF_GBR_plazma`/`AIF_GBR_yaralan` satır içi hook (`pz_*`), `engine_callscript4` ile me/you ayarlı tekrar oynatma, `watchhp` veri kesme noktası, `script_where`
- `hit_apply` oyunun kendi vuruşunu taklit eder (s37 = kombo basamağı, s35 −= 0.1 bayrağı, s39 negatif damga, kıvılcım); robot konuma göre bulunur; SHT1 dmg = basamak+1
- `local/tools/rpm.ps1`, `local/tools/flaghit.ps1`
Doğrulama: derlendi (DLL 23:24); iki pencerede denendi — client vuruşları robotu stun'a sokup öldürüyor, A/B hasarı host'unkiyle aynı
Kullanıcı/arkadaş denedi mi: kullanıcı denedi
Varsayımlar:
- Negatif s39 damgasını başka kod okumuyor (sorun görülmedi)
Kalan sorunlar / yarım kalanlar:
- C vuruşu zamanlama yüzünden yutuluyor; client'ta hedef kilidi yok → HANDOFF "Sıradaki adım" (kuklaları c_trace'e görünür yapmak)


## 2026-11-17 · Claude Opus 5.5 + Gemini 4 Argon · SCW3 parçalı dünya paketi + 857 fonksiyon tam Ghidra decompile ve kaynak kod analizi
İnceleme: bekliyor
Commit'ler: yok
Yapılan:
- `local/src/probe/probe.c`: `dumpall` komutu (`dump_all_code`, `dump_region_set`) ve `SCW3` parçalı dünya paketi (`wpkt_encode`, `wpkt_decode`, `world_rx`, `world_commit`, `world_tick`; `WMAX_PARTS=64`, `WMAX_CALLS=128`) (Claude tarafından yazıldı ve derlendi).
- `local/tools/mkpe.py`: 5.035 adet satır içi `1024.0` double sabitini (`call $+13; 1024.0; pop eax`) düz `mov eax, B+0x10` komutuna çevirerek Ghidra'nın `Possible PIC construction` / `Removing unreachable block` hatalarını sıfırladı.
- `local/tools/ghidra/DecompAll.java`: Fonksiyon sınırlarında önceki fonksiyon taşmalarını kırparak (`setBody`) 857 fonksiyonun tamamının (%100) decompile edilmesini sağladı.
- `local/tools/scnames.py`: `*Ram*` regex düzeltmesi ve `B+0x10 -> 1024.0` eşlemesiyle `local/dump/code/decomp.c` üretildi.
- `local/tools/disfn.py`: İstenen lite-C fonksiyonunu doğrudan Capstone ile sembol/sabit/string açıklamalı x86 assembly olarak döken araç eklendi.
- `local/NOTES.md` ve `local/HANDOFF.md`: Decompile edilen oyun kodundan çıkarılan tüm `EBX` global ofsetleri (`g_07fa4671` nano, `g_07fa00ad` canlı robot, `g_07fce699`/`g_07fce69d` taşıyıcı kuyrukları, `g_07fce6a1` maks robot, `g_07fab1c9` oyuncu kutusu), robot skill tablosu (`skill[0..39]`), C vuruşunun `AIF_GBR_yaralan+0x1fd3` (`081d7d21`) adresinde `anim_progress <= 50` iken `s35 = s36` ile silindiğinin kanıtı, kukla `FLAG2` (`16`) + `skill[37]=0` mimarisi ve `/clear` sonrası uygulama planı kaydedildi.
Doğrulama: `probe.c` derlendi (`local/build/dinput8.dll`); `decomp.c` 857 fonksiyonla üretildi. Yeni mimari henüz `probe.c`'ye kodlanmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki 00:05 DLL'i Steam'de denendi (`coop_probe_000921_9244.log`); yeni yapılacaklar `/clear` sonrası kodlanıp denenecek.
Varsayımlar:
- yok (tüm adres, ofset ve koşullar doğrudan `decomp.c` ve `region_07d40000.bin` içinden doğrulandı)
Kalan sorunlar / yarım kalanlar:
- `local/HANDOFF.md` "Sıradaki adım" altındaki 1–5. maddelerin `/clear` sonrası `probe.c`'ye kodlanıp derlenmesi ve oyunda test edilmesi.


## 2026-11-17 · Gemini 4 Argon · HANDOFF 1–5. adımların probe.c'ye kodlanması (EBX tabanı, tüm düşman/çöp tipleri, sayaç senkronu, FLAG2 kukla çarpışması, host vuruş kuyruğu)
İnceleme: bekliyor
Commit'ler: yok
Yapılan:
- `local/src/probe/probe.c`:
  - `ebx_base()` ve `G_NANO`, `G_ROBOT_ALIVE`, `G_GBR_QUEUE`, `G_ZBD_QUEUE`, `G_ROBOT_MAX`, `G_PLAYER_BOX` makroları eklendi (`AIF_GBR_fizik` 14 bayt prolog doğrulamalı).
  - `is_blocked_spawn`: `"gbr_box.mdl"`, `"zbd_box.mdl"`, `"ucan_box.mdl"` olarak genişletildi.
  - `is_world_type`, `is_enemy_body_type`, `is_enemy_box_type`, `mtl_source`: `GBR_*`, `ZBD_*`, `UCAN_*`, `TASIYICI_*`, `COP_GBR_*`, `COP_ZBD_*`, `COP_ROBOT_PARCA.MDL`, `YANIK_COP.MDL` eklendi.
  - `world_client_sync_counters`: Client aktifken her karede `G_GBR_QUEUE = 0`, `G_ZBD_QUEUE = 0` yazar ve `G_ROBOT_ALIVE` (`g_07fa00ad`) sayacını sahadaki kukla robot (`is_enemy_body_type`) sayısıyla senkronlar.
  - `world_client_render`, `world_apply_look`, `hit_client_tick`: Client'ta `GBR_GOVDE.MDL`, `ZBD_GOVDE.MDL`, `UCAN_GOVDE.MDL` kuklalarına `PASSABLE_F` yerine `FLAG2_F` (`16`), `skill[2] = -1 << 10`, `skill[37] = 0` verildi; `world_apply_look` içinde `skill[2]` ve `skill[34..39]` korundu; `hit_client_tick` kılıç `c_trace`'i kuklanın `skill[38]` (`s39`) değerini değiştirdiği anda `skill[36]` (`s37`) değerini `0 ms` gecikmeyle `HITPKT` olarak gönderir.
  - `pz_spark`, `hit_host_tick`, `hit_apply`, `hit_rx`, `remote_cmd`: `pz_replay` şablonu yerine `box->skill[37]` (`1` ise `AIF_GBR_plazma`, `2` ise `AIF_ZBD_plazma`) doğrudan `box` ile çağrıldı (`skill[21]` ve `skill[30]` handle kontrolüyle); host'ta robot başına FIFO vuruş kuyruğu (`g_hq`, `hit_host_tick`) eklendi (`skill[35] == orig_s36` kaldığı sürece her karede `s37` ve `s35 = orig_s36 - 102` yenilenir).
  - Bölünmüş ekran testi (`coop_probe_015506_10072.log` & `coop_probe_015520_14140.log`) sonrası ek düzeltmeler:
    1. **Hedef kilidi (`c_scan`):** `AIF_GBR/ZBD/UCAN_fizik` içinde olduğu gibi kuklalara `*(DWORD *)((BYTE *)e + 96) |= 256` (`e->emask |= ENABLE_SCAN`) eklendi (`c_scan` yalnız `ENABLE_SCAN` bayrağı açık varlıkları tarar).
    2. **Robot gözleri (`GBR_GOZ.MDL`) ve alt parça kemik kilidi:** `world_client_render` içinde animasyon eşleşmesi parça bazlı (`(part_id, op, scene)`) yapıldı ve `wreplay` sonrasında `vec_for_bone` / `ang_for_bone` ile `GBR_GOZ` (`"Bone01"`), `GBR_LASTIK` (`"Bone06"`), `ZBD_KAFA`/`ZBD_LIGHT` (`"Bip01 Head"`), `ZBD_SOPA0` (`"Bip01 R Hand"`) doğrudan client'taki gövde kuklasının kemiğine kilitlendi.
    3. **Dalga arası sayaç koruması (`WFLAG_WAVE_BUSY`):** Host'ta `G_GBR_QUEUE + G_ZBD_QUEUE + G_ROBOT_ALIVE > 0` iken `part[0].flags` bit 30 üzerinden client'a iletilir; sahadaki robot ölse bile taşıyıcıda bekleyen robot varken client'ta `alive` `0`'a düşmez.
Doğrulama: `local/src/probe/build.bat` ile derlendi. İlk bölünmüş ekran testinde (`015506` / `015520`) C vuruşu (−9 HP, 4–18 kare kuyruk bekletmesiyle), A/B/D (−4/−6/−13 HP), ağır vuruşlar (−20/−40 HP), client nano düşüşü (100→80→60→40→10), `SCW3` parçalı paket (`chunks<=3`, `drop=0`, 24 parça), çöp parçaları (`cop_gbr_*`, `cop_robot_parca`, `yanik_cop`), sıfır `pz` hatası ve bölüm sonu geçişi (`alive=1→2→3→0`) doğrulandı. Hedef kilidi (`emask |= 256`) ve kemik kilidi (`vec_for_bone`) düzeltmeleri derlenip paketlere kopyalandı (ikinci düzeltmeler henüz oyunda test edilmedi).
Kullanıcı/arkadaş denedi mi: evet — bölünmüş ekranda denedi: kıvılcım, C hasarı, nano, çöp parçaları ve 2. bölüme geçiş çalıştı; hedef kilidinin dönmediğini ve gözlerin hâlâ oynadığını bildirdi (yukarıdaki 1. ve 2. maddelerle düzeltildi).
Varsayımlar:
- yok
Kalan sorunlar / yarım kalanlar:
- Yeni derlenen `emask |= 256` (hedef kilidi) ve `vec_for_bone` (göz/kemik kilidi) düzeltmelerinin oyunda doğrulanması; ileride çöp süpürme senkronu ve düşmanların iki oyuncuyu da hedeflemesi.



## 2026-11-17 · Claude Opus 5.5 (Antigravity) · Düşman hedefleme + oyuncu hasarı RE (kod yok)
İnceleme: bekliyor
Commit'ler: yok
Yapılan:
- Yalnız araştırma: `decomp.c` + `acknex.dll` disassembly. `probe.c` DEĞİŞMEDİ.
- Bulgular ve uygulama planı `local/HANDOFF.md` "02:26–02:35 oturumu" bölümünde (zamanlayıcı `me` yazma kancası `0x10018d3d`/`0x10018cc9`/`0x10018ce9`,
  `G_PLAYER_BOX` robot başına swap, kukla kutuyu yalnız robot görevinde katı+FLAG2 yapma, `g_07fa1f15/1f25/1f31` + skill[34]/[36] ile client'a hasar paketi).
Doğrulama: derleme yok (kod yazılmadı). Test edilmedi.
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- acknex.dll sürümü sabit (adresler testinstall kopyasından); kurulumda bayt doğrulaması şart.
Kalan sorunlar / yarım kalanlar:
- Planın `probe.c`'ye kodlanması (coop_tick yürüyüşü, coop_tick 4. adım kukla bayrakları, yeni sched_me kancası, yeni hasar paketi), derleme, toplu test.

## 2026-11-17 · Gemini 4 Argon · İki oyuncuyu hedefleme (scheduler me kancası) + düşmanların client'a hasar vermesi (SPH1) + client çöp vakumlama + client dalga tetikleme
İnceleme: bekliyor
Commit'ler: yok
Yapılan:
- `local/src/probe/probe.c`:
  - `is_trash_type`: `"cop_gbr_*"`, `"cop_zbd_*"`, `"cop_robot_parca.mdl"`, `"yanik_cop.mdl"` yardımcısı eklendi.
  - `world_client_render`, `world_apply_look`, `hit_client_tick`, `hit_rx`, `hit_apply`: Client'ta çöp kuklalarına `skill[25] == 0` iken `emask |= 256` (`ENABLE_SCAN`), `skill[7] = 1<<10` (+15 nano / +7 puan) ve `skill[22] = 1<<10` verildi; vakum sırasında (`skill[25] != 0`) host konumunun kukla konumunu ezmesi engellendi; vakum bittiğinde (`skill[25] == 2<<10`) `HIT_DMG_TRASH` (`99<<10`) ile host'a bildirilir ve host gerçek çöp varlığına `skill[25] = 2<<10`, `emask &= ~256` yazarak kendi `cop_robot_fizik` döngüsünün çöpü ve `metal_icon.tga` simgesini silmesini sağlar.
  - `G_PATH_SEG`, `G_LVL_SLOT`, `HIT_DMG_WAVE`, `host_trigger_wave_seg`, `world_client_sync_counters`: Client haritada ilerleyip kendi yerel `camera_path_loop` döngüsünde yeni bir dalga bölgesini (`G_GBR_QUEUE` / `G_ZBD_QUEUE` > 0) tetiklediğinde, sıfırlamadan hemen önce `G_PATH_SEG` (`g_07fae8f5`) değerini `HIT_DMG_WAVE` (`100<<10`) ile host'a gönderir; host o segmentin dalga tablosundaki (`ebx + 0x26c970`) robot sayılarını negatife çevirip kendi `G_GBR_QUEUE`, `G_ZBD_QUEUE`, `G_ROBOT_MAX` kuyruklarına ekler ve taşıyıcı dronun o bölgeye robot bırakması için `G_PATH_SEG(ebx) = seg` yazar.
  - `install_sched_hooks`, `stub_sched_me_eax`, `stub_sched_me_edx`, `sched_on_me`, `aim_host_tick`, `aim_remove_pup`: `acknex.dll` görev zamanlayıcısının `me` (`0x1020c4e8`) değişkenini güncellediği 3 nokta (`0x10018d3d`, `0x10018cc9`, `0x10018ce9`) bayt doğrulamalı kancalandı; host'ta client için görünmez `SUPERCAN_BOX.MDL` kuklası (`g_aim_pup`) oluşturuldu; her karede her robot kutusu (`GBR_BOX`, `ZBD_BOX`, `UCAN_BOX`) için %20 histerezis ile daha yakın oyuncu seçilir ve yalnız o robotun görevi çalışırken `G_PLAYER_BOX(ebx)` yakın oyuncuya çevrilip `g_aim_pup` katı + `FLAG2` yapılır (diğer tüm görevlerde gerçek kutu seçilidir ve `g_aim_pup` `PASSABLE` olup `FLAG2` kapalıdır).
  - `PHITPKT` (`'SPH1'`), `phit_rx`, `coop_tick`, `player_box`: Host'ta bir robot `g_aim_pup` kutusuna vurduğunda (`skill[34] < skill[35]`) vuruş kodu (`skill[36]`), vuruş sayısı (`n`), saldırgan konumları (`ebx + 0x261ed0` ve `ebx + 0x261ee0`) ve saldırgan açısı (`ebx + 0x261eec`) `'SPH1'` paketiyle (3x) client'a gönderilir; client kendi `ebx` ofsetlerine ve `player_box()` varlığına aynı değerleri yazarak oyunun doğal yaralanma/yuvarlanma/çarpılma animasyonunu ve can düşüşünü çalıştırır.
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`, `02:53`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Test edilmedi.
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- yok (tüm adres, bayt ve skill ofsetleri doğrudan `decomp.c` ve `acknex.dll` içinden doğrulandı)
Kalan sorunlar / yarım kalanlar:
- `02:53` DLL'inin bölünmüş ekranda (veya Steam'de) topluca test edilmesi.

## 2026-11-17 · Gemini 4 Hydrogen · Client dalga konumu düzeltmesi + phit_rx/crash teşhis logları + çöp emask koruması
İnceleme: bekliyor
Commit'ler: yok
Yapılan:
- `local/src/probe/probe.c`:
  - `g_wave_seg_override`, `g_tsyc_box`, `host_trigger_wave_seg`, `sched_on_me`, `aim_host_tick`: `decomp.c` içindeki `camera_path_loop` her karede `G_PATH_SEG` (`g_07fae8f5`) değerini host'un segmentiyle ezdiği için `g_wave_seg_override` eklendi; taşıyıcı dron (`TASIYICI_BOX.MDL`) görevi çalışırken `sched_on_me` içinde `G_PATH_SEG(ebx)` client'ın tetiklediği segmente sabitlenir ve kuyruk boşalınca (`G_GBR_QUEUE + G_ZBD_QUEUE <= 0`) sıfırlanır.
  - `phit_rx`: Host logunda (`coop_probe_025709_12100.log`) `[aim] host enemy hit client puppet` görünmesine karşın client logunda (`coop_probe_025718_7048.log`) `[aim] client took enemy hit` görünmediği için `len/is_host`, `magic/n`, `ebx/pb` ve `s36 <= (1<<10)` erken çıkışlarına seyrek `[phit]` teşhis logları eklendi.
  - `world_client_render`: Kukla silinirken `p_ent_remove` öncesinde `*(DWORD *)((BYTE *)g_wp[i].e + 96) &= ~256u` (`ENABLE_SCAN` kapatma) eklendi.
  - `crash_filter`, `init`: Yakalanmamış ölümcül istisnalar için `SetUnhandledExceptionFilter(crash_filter)` eklendi (`[crash] EXC code=... at eip=...` + modül ofseti ve `script_where` loglanır).
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`, `03:06`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `02:53` DLL'ini denedi (`025709` / `025718` logları): robot dalgasının hostun yanında spawn olduğunu, D-S (`HeavyAtak_A`) sırasında client'ın çöktüğünü ve client'ın hasar almadığını bildirdi. Yeni `03:06` DLL henüz denenmedi.
Varsayımlar:
- `TASIYICI_BOX.MDL` entity pointer'ının seviye boyunca değişmediği (değişirse `!is_entity` kontrolü yeniden bulur).
Kalan sorunlar / yarım kalanlar:
- `03:06` DLL'inin bölünmüş ekranda tekrar denenip `[phit]` ve (tekrarlarsa) `[crash]` loglarının incelenmesi.

## 2026-11-17 · Gemini 4 Hydrogen · 03:06 DLL test doğrulaması + VEH crash loglayıcı (crash_veh)
İnceleme: bekliyor
Commit'ler: yok
Yapılan:
- `local/src/probe/probe.c`: `crash_filter` (`SetUnhandledExceptionFilter`) `acknex.dll` tarafından ezildiği/yakalandığı için `crash_veh` (`AddVectoredExceptionHandler(1, crash_veh)`) ile değiştirildi; `dinput8.dll` (`g_self_mod`) dışındaki ölümcül istisnaların EIP, modül+ofset, `script_where` ve yığın dönüş adreslerini `[crash-veh]` olarak loglar.
Doğrulama: `03:06` DLL (`030854` / `030903` logları) oyunda denendi:
- Client dalga tetikleme ve doğru yerde robot spawnı doğrulandı.
- Client'ın düşmandan hasar alması doğrulandı (`[aim] client took enemy hit: s37=0.0 n=1 hp=100.0 -> 95.0`).
- Client `pfcan_mod_HeavyAtak_A` (`s37=4.00`) sırasında yine çöktü; `03:12` DLL (`crash_veh`) derlenip paketlere/test kurulumlarına kopyalandı (henüz denenmedi).
Kullanıcı/arkadaş denedi mi: evet (`03:06` DLL'ini denedi; `03:12` DLL'i henüz denenmedi).
Varsayımlar:
- yok
Kalan sorunlar / yarım kalanlar:
- `03:12` DLL'i ile `HeavyAtak_A` çökmesini tekrarlatıp `[crash-veh]` çıktısıyla kök nedeni kesinleştirmek.

## 2026-11-17 · Gemini 4 Hydrogen · pfcan_copicon_goster NULL+0x10 çökmesi düzeltmesi (skill[22]/skill[31])
İnceleme: bekliyor
Commit'ler: yok
Yapılan:
- `local/src/probe/probe.c`:
  - `world_client_render`, `world_apply_look`: `031345` logunda `[crash-veh] EXC code=c0000005 at eip=08127711 script='pfcan_copicon_goster+0x27a' access=00000010` yakalandı. `decomp.c` (`pfcan_copicon_goster` 103889–103902, `pfcan_detect_event` 105590–105596) incelenerek `skill[22] == 1<<10` olan çöp kuklalarında `pfcan_copicon_goster`'ın `ptr_for_handle(skill[31])` (`metal_icon.tga` handle'ı) aradığı ve client'ta `0` döndüğü için `0x10` ofsetinde çöktüğü doğrulandı; `pfcan_kutu_cek` vakumunun `skill[22]` kullanmadığı doğrulandı.
  - Çöp kuklalarında `skill[22] = 0` bırakıldı ve `world_apply_look` içinde `idx == 31` (`skill[31]`) de atlandı.
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`, `03:17`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: `03:12` DLL'ini denedi (`031331` / `031345` logları) ve `[crash-veh]` ile çökme adresi yakalandı. Yeni `03:17` DLL'i henüz denenmedi.
Varsayımlar:
- yok (tüm skill ofsetleri ve çağrılar `decomp.c` içinden doğrulandı)
Kalan sorunlar / yarım kalanlar:
- `03:17` DLL'i ile robot öldürme, çöp vakumlama ve client'ın ardışık hasar alma davranışının bölünmüş ekranda doğrulanması.

## 2026-11-17 · Gemini 4 Hydrogen · Taşıyıcı elektrik kubbesi + g_aim_pup gövde/Z hizası + client G_PLAYER_BOX
İnceleme: bekliyor
Commit'ler: yok
Yapılan:
- `local/src/probe/probe.c`:
  - `is_world_type`: Taşıyıcı altındaki elektrik kubbesi/ışınları (`PLAZMA_KURE*`, `PLAZMA_YILD*`) eklendi.
  - `aim_host_tick`: `g_puppet[0]` (ör. `SUPERCAN_YUZ.MDL`) yerine `SUPERCAN_GOVDE.MDL` kuklası seçildi; host'taki `SUPERCAN_GOVDE.MDL -> g_aim_real` Z farkı (`dz`) ölçülerek `g_aim_pup` konumu `pc.z + dz` olarak hizalandı; `c_updatehull` varsa kutu sınırından sonra çağrıldı ve `[aim] created client target box ... dz=...` loguna eklendi.
  - `player_box`: Client tarafında (`!g_aim_active`) `G_PLAYER_BOX(ebx)` öncelikli döndürüldü (2. bölüm geçişinde eski `SUPERCAN_BOX` seçilmesini önler).
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`, `03:34`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır (`03:34` DLL yeni derlendi).
Varsayımlar:
- `PLAZMA_KURE*` ve `PLAZMA_YILD*` varlıklarının `WMAX_PARTS` (64) sınırını aşmadığı.
Kalan sorunlar / yarım kalanlar:
- `03:34` DLL'inin bölünmüş ekranda denenip elektrik kubbesinin göründüğünün, `dz` hizasının ve robotun ilk vuruş isabetinin loglardan doğrulanması.


## 2026-11-17 · Gemini 4 Hydrogen · 03:34 DLL test analizi + save/alt-bölge yapısı + tüm oyun senkron haritası (NOTES.md)
İnceleme: bekliyor
Commit'ler: yok
Yapılan:
- `local/testinstall/SuperCan1/Game/coop_probe_123953_16252.log` ve `SuperCan1b/Game/coop_probe_124002_18140.log` incelendi.
- `local/dump/code/functions.txt` ve `local/dump/code/decomp.c` üzerinden save/alt-bölge geçişleri (`main_level_gecis`, `merd_level_gecis`, `main_check_save`), 3. kısımdaki kol/köprü (`main_mkr_kolcek`, `nesne_mkr_tahta1/2`) ve oyunun geri kalan senkron ihtiyaçları analiz edildi.
- `local/NOTES.md`: Test doğrulama durumu, save/alt-bölge yapısı ve tüm oyun senkron haritası eklendi. (`probe.c` değiştirilmedi.)
Doğrulama: kod değişikliği yok, yalnızca log ve `decomp.c` analizi (`local/NOTES.md` güncellendi).
Kullanıcı/arkadaş denedi mi: evet (`03:34` DLL'ini 3. kısıma kadar denedi: kubbe ve düşman vuruşları çalıştı, client çöp vakumlama çalışmadı, 3. kısımdaki kol/köprü senkron değil).
Varsayımlar:
- yok
Kalan sorunlar / yarım kalanlar:
- Client çöp vakumlama (`pfcan_kutu_cek`) nedeninin tekrar incelenmesi ve 3. kısımdaki kol/köprü (`nesne_mkr_*`) senkronu.


## 2026-11-17 · Gemini 4 Hydrogen · 3. kısım kol / köprü / kapı senkronu (SMK1)
İnceleme: bekliyor
Commit'ler: yok
Yapılan:
- `local/src/probe/probe.c`:
  - `MKR_MAGIC` (`'SMK1'`), `MKRITEM`, `MKRPKT`, `mkr_collect_ent`, `mkr_apply_item`, `mkr_rx`, `mkr_tick` eklendi.
  - `d_ent_animate` içinde makara/kol/köprü/kapı varlıklarının (`skill[19] == 100..106`) animasyon yüzdesi (`g_mkr_pct`) kaydedildi.
  - `coop_tick` içinde `0x04` harita varlıkları dahil `mkr_collect_ent` ile kol (`100`), çark (`101`), kapı (`102`), tahta1 (`103`), tahta2 (`104`), blok1 (`105`), blok2 (`106`) toplandı; iki yönlü `MKRPKT` senkronuyla `flags`, `emask`, `ent_animate` (`kol_cek`/`cevir_loop`/`frame`) ve `c_updatehull` güncellemesi eklendi.
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`, `239104 B`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Bölünmüş ekranda (`coop_probe_131946_3676.log`) kolun client tarafından çekilip her iki tarafta da köprünün görsel (`pct`) ve fiziksel (`flags=0x4000123`, `PASSABLE` kalktı) olarak açıldığı doğrulandı.
Kullanıcı/arkadaş denedi mi: evet — client ile kolu çekti, iki tarafta da köprünün açıldığını doğruladı.
Varsayımlar:
- `skill[19]` (`s20`) değerleri `100..106` olan makara/köprü varlıklarının haritada tekil olduğu (iki tarafta da mevcut `0x04` harita varlıkları olarak eşleştiği).
Kalan sorunlar / yarım kalanlar:
- 3. kısımda kol çekilince köprünün iki tarafta da görsel ve fiziksel (`PASSABLE` + `c_updatehull`) olarak açıldığının oyunda test edilmesi; ardından sıradaki adım olarak client çöp vakumlama + ortak puan/elmas senkronu.


## 2026-11-17 · Gemini 4 Hydrogen · Client çöp vakumlama (c_setminmax) + çevre çöp/elmas senkronu (SIT1) + ortak puan/elmas/ceza (SST1)
İnceleme: bekliyor
Commit'ler: yok
Yapılan:
- `local/src/probe/probe.c`:
  - `G_SCORE`, `G_DIAMONDS`, `G_MISSED_TRASH`, `G_TRASH_RBT`, `G_TRASH_ENV`, `G_TRASH_LVL` makroları eklendi (`G_NANO` ayrı bırakıldı).
  - `world_client_render`: `is_trash_type` kuklaları için `c_setminmax` çağrısı eklendi (client `c_scan` / `vec_for_max` çöp vakumlama düzeltmesi).
  - `ITEM_MAGIC` (`'SIT1'`), `itm_collect_ent`, `itm_rx`, `itm_tick`: Haritadaki çevre çöpleri (`cop_model_fizik`: `s23=1, s7=5, s4=3`) ve elmas/bonus kutuları (`s23=3`) koordinat+tip hash'i ile iki tarafta eşleştirildi; bir oyuncu topladığında/kırdığında diğer tarafta gizlenir.
  - `STAT_MAGIC` (`'SST1'`), `stat_rx`, `stat_tick`: Host otoriteli ortak puan, elmas, kaçan çöp cezası ve çöp sayaçları senkronu + client'tan puan/elmas artış deltalarının host'a aktarımı eklendi (`G_NANO` her oyuncuda ayrı kalır).
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- Çevre çöpleri ve elmas/bonus kutularının harita başlangıcındaki `(x>>10, y>>10, z>>10)` koordinatlarının iki tarafta aynı olduğu.
Kalan sorunlar / yarım kalanlar:
- Bölünmüş ekranda client'ın robot çöplerini ve çevre çöplerini süpürebildiğinin, puan/elmasların ortak artıp nano barlarının ayrı kaldığının ve meyvelerin bağımsız toplanabildiğinin test edilmesi.

## 2026-11-17 · Gemini 4 Hydrogen · Steam testi log analizi + orijinal FPS sınırı + SMK1/SIT1/SST1/çöp/düşman hull düzeltmeleri
İnceleme: bekliyor
Commit'ler: cc9d6c8 (öncesi); bu değişiklikler henüz commit'lenmedi
Yapılan:
- Steam testi logları (`local/testinstall/SuperCan1/Game/coop_probe_215838_6168.log` ve arkadaş logu `coop_probe_215956_30196.log`) ve `local/dump/code/decomp.c` incelendi.
- `local/src/probe/probe.c`:
  - `fps_tick`: Varsayılan FPS sınırı `144` yerine `0` (oyunun orijinal sınırı; `coop.ini` `fps_max=` yoksa dokunmaz) yapıldı — trambolin zıplama bozulmasını düzeltir.
  - `G_MKR_STATE` (`g_07fa256d`, `0x262528`), `mkr_apply_state`, `mkr_rx`, `mkr_tick`: `SMK1` artık 7 varlığı ping-pong ile göndermek yerine `G_MKR_STATE` (`0->1->2`) ilerlediğinde `main_mkr_reset` + `c_updatehull` çalıştırır (çift kol çekme, köprüden düşme, kapı görüntü/takla bug'ı ve 3443 satır log taşması düzeltildi).
  - `itm_collect_ent`, `itm_tick`: `SIT1` içine `!is_entity` ve `(emask & 1) == 0` (`pfcan_bonus_parcalan` elmas/kutu toplama bayrağı) ile host/client `seq` bit ayrımı eklendi.
  - `stat_rx`, `stat_tick`: `SST1` paketine `client_ack` ve `g_stat_pend_s/d` eklendi (client puan/elmas artışlarının host onayından önce kaybolması veya çift sayılması önlendi).
  - `world_client_render`: Çöp kuklalarında `c_setminmax` + `vec_for_min`/`vec_for_max` + `vec_mul(scale)` eklendi (`q->posed` bozulmadan); düşman kuklalarında animasyon replay sonrası `c_updatehull` eklendi.
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `13:28` DLL'ini Steam'de denedi (`215838` / `215956` logları); bu yeni `22:27` DLL'i henüz denenmedi.
Varsayımlar:
- `main_mkr_reset()` `G_MKR_STATE` güncellendikten sonra tek sefer çağrıldığında kamera/oyuncu durumunu bozmaz (`decomp.c:216118` incelendi, `wait` yok).
Kalan sorunlar / yarım kalanlar:
- `22:27` DLL'inin (arkadaşa `coop_paket/dinput8.dll` atılıp veya push'lanıp) Steam'de veya bölünmüş ekranda test edilmesi.

## 2026-11-17 · Gemini 4 Hydrogen · 22:27 Steam testi analizi + trash_init + MAX_SYNC_ITEMS=2048 + enemy c_updatehull geri alma
İnceleme: bekliyor
Commit'ler: cc9d6c8 (öncesi); henüz commit'lenmedi
Yapılan:
- `22:27` DLL Steam testi (`coop_probe_224214_18140.log` / `coop_probe_224341_28588.log`) incelendi: `SMK1` (`G_MKR_STATE` + `main_mkr_reset`) 4 satırda temiz çalıştı; client `85/89` vuruş işledi.
- `local/src/probe/probe.c`:
  - `WPUP.trash_init` eklendi: `ent_create` sonrası `min_x/max_x` 0 olmadığı için hiç çalışmayan `c_setminmax` + `vec_for_min/max` + `vec_mul` bloğu `if (!q->trash_init)` ile kesinleştirildi ve `[trash] init` logu eklendi.
  - `MAX_SYNC_ITEMS` `256` -> `2048` çıkarıldı (`orman.wmb`'deki 256 sonrası elmas/çevre çöplerinin atlanması giderildi) ve `itm_hash` `>> 12` ızgarasına alındı (`[itm] tracking N items` logu eklendi).
  - `world_client_render`: her karede düşman kuklalarına çağrılan `c_updatehull` kaldırıldı (animasyon takılması ve yerdeki düşman kutusunun küçülüp kılıç `c_trace`'ini kaçırması giderildi).
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`, `22:54`, `242688 B`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `22:27` DLL'ini Steam'de denedi (köprü/kapı ve `SST1` çalıştı; çöp, elmas sınırı ve animasyon/yerde vuruş sorunları bu `22:54` DLL'inde düzeltildi).
Varsayımlar:
- yok
Kalan sorunlar / yarım kalanlar:
- `22:54` DLL'inin arkadaşa da verilip Steam'de veya bölünmüş ekranda tekrar denenmesi.

## 2026-11-19 · Gemini 4 Neon · Hydrogen değişiklikleri incelemesi ve kritik/orta hata düzeltmeleri (TRASH_FLAG2, O(1) itm_tick, SST2, SMK2)
İnceleme: bekliyor
Commit'ler: yok (kullanıcı isteğiyle commit edilmedi)
Yapılan:
- `local/src/probe/probe.c`:
  - `TRASH_FLAG2_F` (`1 << 1 = 2`), `world_client_render`, `hit_apply`: Çöp kuklalarına `decomp.c` (`cop_robot_fizik:132672`, `pfcan_mod_CopBekle` `c_scan 0x140001 = SCAN_FLAG2`) ile uyumlu gerçek `FLAG2` (`2`) eklendi, `FLAG1` (`1`) temizlendi, vakum sonrası (`skill[25] != 0`) kukla `flags`'inin host tarafından geri görünür yapılması engellendi; host `HIT_DMG_TRASH` işlerken `G_TRASH_RBT += 1<<10` eklendi.
  - `SYNCITM.seen_frame`, `ITM_Q_MAX=32`, `itm_collect_ent`, `itm_rx`, `itm_tick`: Her karede 2048 eşya için tüm sahneyi tarayan $O(N \times M)$ `is_entity()` çağrısı kaldırılıp `seen_frame != g_frame` ile $O(1)$ yapıldı; erken karede pasif görülen eşyalar `done=2` ile izlenip `main_check_save` sonrası `done=0`'a geçer; gönderim kuyruğu `8 -> 32` slota çıkarıldı.
  - `STAT_MAGIC` (`'SST1' -> 'SST2'`), `stat_client_capture`, `stat_rx`, `stat_tick`: Çift sayma ve aynı karedeki kayıp delta hatalarını önlemek için kümülatif client kazancı (`client_tot_score`, `client_tot_diamonds`) tabanlı idempotent senkronizasyona geçildi.
  - `MKR_MAGIC` (`'SMK1' -> 'SMK2'`), `mkr_apply_state`, `mkr_rx`, `mkr_tick`: Protokol sürümü güncellendi; `g_mkr_ent[0]` kontrolüyle başka bölümlerde `main_mkr_reset()` çağrılması önlendi.
  - `aim_remove_pup`, `aim_host_tick`: Harita çıkışında `g_wave_seg_override = -1` / `g_tsyc_box = 0` sıfırlaması, `g_puppet_type[i]` kullanımı ve `G_GBR_QUEUE + G_ZBD_QUEUE` için `__try` koruması eklendi.
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`, `243200 B`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- İki tarafın da yeni `'SMK2'` / `'SST2'` DLL'ini kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Yeni DLL'in (`243200 B`) iki kopyada / Steam'de test edilerek client çöp vakumlama, FPS/akıcılık, ortak puan/elmas ve 3. kısım köprü/kapı senkronunun doğrulanması.

## 2026-11-19 · Gemini 4 Neon · Sıra 3: Taramalı taret vuruşu + Alarm/Lazer/Silo senkronu (SAL1) + kapsamlı teşhis logları
İnceleme: bekliyor
Commit'ler: yok (kullanıcı isteğiyle commit edilmedi)
Yapılan:
- `local/dump/code/decomp.c` analizi:
  - `kus_ari_hesap` / `kus_ari_main`: yalnız dekoratif çevre animasyonu (hasar/hedefleme yok, senkron gerekmez).
  - `topgit_taramali`: `c_trace` ile düşmana çarpınca `skill[36] = 1<<10` ve `skill[34] -= 0.1` yazar (`skill[38]`'e dokunmaz).
  - `main_alarm_reset` / `main_alarm_saldir`: `G_ALRM_STATE` (`g_07fa2569`, `ebx + 0x262524`, `0..3`) + `G_ALRM_SILO` (`g_07fccb3d`, `ebx + 0x28caf8`) üzerinden çalışır; `main_alarm_reset` `wait` içermez.
- `local/src/probe/probe.c`:
  - `WPUP.last_s35`, `world_client_render`, `hit_client_tick`, `hit_host_tick`: Client düşman kuklalarına `skill[34] = 100<<10` tabanı verildi; `topgit_taramali` mermisi `s35`'i düşürdüğünde `HITPKT` ile host'a iletilir (`[hit] client turret hit puppet`); `UCAN_BOX.MDL` (`cls == 3<<10`) için kuyruk 2 kare tutularak hasarın kesin işlenmesi sağlandı.
  - `G_ALRM_STATE`, `G_ALRM_SILO`, `ALRM_MAGIC` (`'SAL1'`), `alrm_has_silo`, `alrm_apply_state`, `alrm_rx`, `alrm_tick`, `coop_tick`: Alarm/lazer/silo aşaması (`0->1->2->3`) iki yönlü senkronlandı; `0 -> >=1` geçişinde `main_alarm_reset()` çağrılır (`G_ALRM_SILO` doğrulamalı).
  - Teşhis logları: `[itm] tracking ... (active/passive)`, `[itm] local gone`, `[stat] client send tot`, `[alrm] ...`, `[hit] client turret hit ...` ve her 600 karede bir tek satırlık `[sync]` özet satırı (`hit`, `phit`, `mkr`, `alrm`, `itm`, `score/diam/rbt/env/nano`) eklendi.
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- İki tarafın da yeni DLL'i (`SMK2`, `SST2`, `SAL1`) kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Arkadaşla Steam testinde çöp vakumlama (`TRASH_FLAG2`), FPS/akıcılık (`SIT1` $O(1)$), puan/elmas (`SST2`), köprü (`SMK2`), taramalı taret vuruşları ve alarm/lazer/silo (`SAL1`) loglarının incelenmesi.

## 2026-11-19 · Gemini 4 Neon · Sıra 4: .wmb harita geçişi + Deve/Hangar Boss & 4 Pil ortak can senkronu (SBO1)
İnceleme: bekliyor
Commit'ler: yok (kullanıcı isteğiyle commit edilmedi)
Yapılan:
- `local/dump/code/decomp.c` analizi:
  - Oyundaki iki ana `.wmb` haritası (`orman.wmb` -> `tunel.wmb`) `main_alarm_load` ile geçilir; `SUPERCAN_TUNELBOX` zaten `is_player_part` dışında tutuluyor.
  - Deve/Hangar Boss kutusu `G_DEVE_BOX` (`g_07d5a26d`, `ebx + 0x01a228`), 4 pil `G_PIL_BOX` (`g_07faa39d[0..3]`, `ebx + 0x26a358`) ve kırılan pil sayacı `G_PIL_DEAD` (`g_07faa399`, `ebx + 0x26a354`) doğrulandı; tümü `skill[34]`/`skill[35]` (`s35`/`s36`) hasar modelini kullanır.
- `local/src/probe/probe.c`:
  - `G_DEVE_BOX`, `G_PIL_DEAD`, `G_PIL_BOX`, `BOSS_MAGIC` (`'SBO1'`), `boss_rx`, `boss_tick`: Deve Boss ve 4 pilin can düşüşleri iki yönlü senkronlanır; pil `0`'a indiğinde `s35 = -102` ile doğal `nesne_pil_patla` tetiklenir (`[boss]` teşhis logları).
  - `aim_host_tick`: `G_DEVE_BOX(ebx)` de `g_ebox_cand` içine alınarak Deve Boss'un iki oyuncudan daha yakın olanı hedeflemesi sağlandı.
  - `level_sync_tick`: Bir oyuncu `orman.wmb`'den `tunel.wmb`'ye geçtiğinde (2 sn kararlı kalınca) diğer oyuncuda da `main_alarm_load` (`engine_callscript4`) otomatik tetiklenir (`[lvl]` logları).
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- İki tarafın da bu yeni DLL'i (`SMK2`, `SST2`, `SAL1`, `SBO1`) kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Arkadaşla Steam testinde tüm sistemlerin (çöp vakumlama, FPS, `SST2`, `SMK2`, taret, `SAL1`, `tunel.wmb` geçişi, `SBO1` Deve/Piller) loglarının incelenmesi; Sıra 5 (ölüm/checkpoint kuralı ve diyalog/sinematik kamera) kullanıcı kararı gerektirir.

## 2026-11-19 · Gemini 4 Neon · Sıra 5 (Ölüm/Canlanma Seçenek A) + coop-anything araç/belge referansları
İnceleme: bekliyor
Commit'ler: yok (kullanıcı isteğiyle commit edilmedi)
Yapılan:
- `local/src/probe/probe.c`:
  - `G_PLAYER_HP` (`0x264628`), `g_revive_invuln_until`, `revive_tick`, `phit_rx`, `coop_tick`: Co-op aktifken canı `<= 1` olan oyuncu `Game Over` / checkpoint yüklemesi olmadan diğer oyuncunun yanında `100 HP` + `3 sn` dokunulmazlıkla canlanır (`pXent_setposition`, `[revive]` logu).
- `AGENTS.md`, `CLAUDE.md`, `local/HANDOFF.md`: `coop-anything/` içindeki co-op geliştirme rehberleri (`playbook/`), araçlar (`tools/`), şablonlar (`templates/`) ve skill'lerin konumları ve içerikleri referans olarak eklendi.
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- İki tarafın da bu son DLL'i (`SMK2`, `SST2`, `SAL1`, `SBO1`, `revive_tick`) kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Arkadaşla Steam testinde yeni DLL'in tüm sistemlerinin ve loglarının (`[trash]`, `[hit]`, `[itm]`, `[stat]`, `[mkr]`, `[alrm]`, `[boss]`, `[lvl]`, `[revive]`, `[sync]`) incelenmesi.

## 2026-11-19 · Gemini 4 Neon · Deve/Hangar tam senkronu (SBO2) + Diyalog/Sinematik dokunulmazlığı + devicereset/geri dönüşüm/tünel doğrulaması
İnceleme: bekliyor
Commit'ler: yok (kullanıcı isteğiyle commit edilmedi)
Yapılan:
- `local/dump/code/decomp.c` incelemesi:
  - `devicereset`: Kapanışta harici gamma/shader callback (`0xcf711f0`) çağrısından kaynaklanan oyunun kendi kapanış olayı (DLL kaynaklı değil).
  - `tunel_bonus_parcalan` / `can_tunel_event`: Tünel elmasları zaten `skill[22] == 3<<10` (`SIT1`) ve `G_SCORE`/`G_DIAMONDS` (`SST2`) ile uyumlu; Hangar (`AIF_Hangar_Start`) `tunel.wmb` içindeki `AIF_Hangar_dummy1` ile başlar.
  - `nesne_geri_dnsm` / `doldur_geri_dnsm`: Geri dönüşüm kutuları kalıcı; `G_CTRL_ENABLED` (`g_07fab1a5`, `ebx + 0x26b160`) diyalog/sinematik/terminal/geri dönüşüm sırasında `0` olur.
- `local/src/probe/probe.c`:
  - `G_DEVE_KAPI` (`0x26a320`), `G_CTRL_ENABLED` (`0x26b160`) eklendi.
  - `BOSS_MAGIC` (`'SBO1' -> 'SBO2'`), `boss_rx`, `boss_tick`: Host Deve aktifken ~15 Hz konum (`deve_x/y/z`), açı (`deve_pan`) ve Hangar kapı bloğu (`G_DEVE_KAPI` `INVISIBLE|PASSABLE` + `c_updatehull`) durumunu client'a gönderir; client'ta Deve konumu >25 birim saparsa `pXent_setposition` ile hizalanır, 4 pil + Deve canı iki yönlü ortak erir.
  - `phit_rx`, `revive_tick`: `G_CTRL_ENABLED(ebx) == 0` iken (diyalog/sinematik/terminal/geri dönüşüm) düşman hasarı ve canlanma ışınlaması bastırıldı.
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- İki tarafın da bu son DLL'i (`SMK2`, `SST2`, `SAL1`, `SBO2`) kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Arkadaşla Steam testinde yeni DLL'in tüm sistemlerinin ve loglarının incelenmesi; ardından toplu commit/push ve (istenirse) SüperCan 2'ye geçiş.

## 2026-11-19 · Gemini 4 Neon · probe.c zayıf nokta denetimi ve 5 cerrahi düzeltme (SST2 lag koruması, kuyruk/tekrar aralığı, WMAX_PARTS önceliği, api_log, copy_look)
İnceleme: bekliyor
Commit'ler: yok (kullanıcı isteğiyle commit edilmedi)
Yapılan:
- `local/src/probe/probe.c`:
  - `stat_tick`, `g_stat_level`, `coop_tick`: Kümülatif puan/elmas sayaçları (`g_stat_tot_*`, `g_stat_host_seen_*`) artık kısa süreli lag'da (`peer_active == 0`) sıfırlanmıyor; yalnızca harita değişiminde veya menüye çıkışta (`!in_level`) sıfırlanıyor (2 sn lag sonrası puan/elmas çift sayma bug'ı giderildi).
  - `HIT_Q_MAX` (`8 -> 32`), `hit_enqueue`, `PHIT_Q_MAX` (`8 -> 16`), `g_phit_due`, `g_itm_due`, `hit_client_tick`, `aim_host_tick`, `itm_tick`: Vuruş (`SHT1`), oyuncu hasarı (`SPH1`) ve eşya (`SIT1`) 3x tekrarları ardışık 3 kare yerine `0 / +50 / +120 ms` aralıklı gönderime alındı; `g_hit_q` taşması için log eklendi.
  - `world_tick`: `g_nwcand > WMAX_PARTS` (64) olduğunda önce canlı düşman/taşıyıcı parçaları (`!is_trash_type`), kalan slotlara çöpler (`is_trash_type`) alınacak şekilde 2 geçişli önceliklendirme eklendi.
  - `api_log_on`, `h_CreateFileA/W`, `h_Reg*`, `h_FindWindowA`, `h_Open/CreateMutexA`, `h_GetEnvironmentVariableA`, `h_OpenFileMappingA`, `h_CreateToolhelp32Snapshot`, `h_Process32Next`, `h_GetPrivateProfileStringA`, `h_GetProcAddress`: `[api]` ve `[gpa]` izleme logları varsayılan kapalı (`api_log=0`, `coop.ini` `api_log=1` veya `COOP_API_LOG=1` ile açılabilir) yapıldı; `[ga]`, `[ohc]` ve `MessageBoxA` her zaman loglanır.
  - `g_puppet_looked`, `coop_tick`: Oyuncu kuklalarına her karede 432 B `copy_look` yerine ilk oluşumda + 30 karede bir kopyalama yapıldı.
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`, `251392 B`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokoller değişmedi. Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- yok
Kalan sorunlar / yarım kalanlar:
- Sıradaki adım: Faz 1 (D3D9 Co-op HUD / Overlay + baş üstü isim + F5 ışınlanma + ping).

## 2026-11-19 · Gemini 4 Neon · Faz 1: Saf D3D9 Co-op HUD/Overlay (SHD1) + baş üstü isim + yön/mesafe + ping/RTT + F5 ışınlanma + F9 aç/kapa
İnceleme: bekliyor
Commit'ler: yok (kullanıcı isteğiyle commit edilmedi)
Yapılan:
- `local/src/probe/probe.c`:
  - `HUD_MAGIC` (`'SHD1'`), `HUDPKT`, `HUDSTATE`, `hud_ascii_copy`, `hud_toast`, `hud_local_name`, `hud_rx`, `hud_tick`, `coop_tick`: ~4 Hz HUD durum/ping paketi (arkadaş adı, Can, Nano, RTT/ping, ara sahne/dokunulmazlık bayrakları), `vec_to_screen` ile baş üstü ve ekran dışı konum/mesafe hesabı eklendi.
  - `F5` kısayolu (`hud_tick`): Aynı haritada ve `G_CTRL_ENABLED != 0` iken `pXent_setposition` ile arkadaşın yanına ışınlanma (`1.5 sn` dokunulmazlık, `2 sn` cooldown, toast bildirimi); `revive_tick` canlanmasında da toast bildirimi eklendi.
  - `hook_engine_frame`: `F9` tuşu Co-op HUD aç/kapa (`g_hud.enabled`, `coop.ini` `hud=0` ile kapalı başlatılabilir) yapıldı; eski ağır debug dökümü `Ctrl+F9`'a taşındı.
  - `hud_draw_d3d9`, `g_font5x7`, `hud_push_rect/border/text`, `h_Present`, `h_SCPresent`: Harici DLL / texture / `D3DPOOL_DEFAULT` kaynağı olmadan (`Device::Reset` güvenli) saf D3D9 `DrawPrimitiveUP` ile sağ üst köşe paneli, baş üstü isim + can barı, ekran dışı yön/mesafe göstergesi ve alt-orta toast bildirimi çizimi eklendi.
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`, `260608 B`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Yeni protokol paketi `'SHD1'` eklendi (iki taraf aynı DLL olmalı). Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- İki tarafın da yeni `'SHD1'` destekli DLL'i kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Oyunda D3D9 HUD görünümünün/konumunun ve F5/F9 kısayollarının doğrulanması; sıradaki adım Faz 2 (`run_id` + late-join / reconnect snapshot).

## 2026-11-19 · Gemini 4 Neon · Bölünmüş ekran testi analizi + Acknex native HUD (draw_quad/draw_text) + SIT1 çevre çöpü LOD düzeltmesi + F5/F9 odak koruması
İnceleme: bekliyor
Commit'ler: yok (kullanıcı isteğiyle commit edilmedi)
Yapılan:
- `205819_7956.log` / `205828_8464.log` ve ekran görüntüsü analizi:
  - `SST2` ortak puan (`784 -> 790 -> 796`) ve ayrı nano (`90` vs `80`) doğrulandı.
  - D3D9 `Present` kancası oyun içinde yalnız ilk 3 karede (`Present=3 SCPresent=0 EndScene=0`) çağrıldığı için D3D9 overlay hiç çizilmedi; ayrıca oyunun kendi Can/Nano HUD'u sağ üst köşede.
  - `SIT1` çevre çöpleri (`kind == 1`, `cop_model_fizik`) uzaktayken motor tarafından `INVISIBLE_F` yapıldığı için 144 çöp harita başında yanlışlıkla `done=1` işaretlenip senkrondan düşmüştü.
- `local/src/probe/probe.c`:
  - `hud_tick`, `hud_focused`, `hook_engine_frame`: HUD çizimi D3D9 `Present` yerine Acknex'in kendi yerel `draw_quad` + `draw_textmode` + `draw_text` API'lerine taşındı (her karede kesin çizilir); panel oyunun kendi HUD'uyla çakışmaması için **sağ alt köşeye** alındı (`360x132`, `1920x1080` backbuffer ölçeği); `F5`/`F9` tuşlarına `hud_focused()` odak koruması eklendi.
  - `itm_collect_ent`, `itm_tick`: `kind == 1` (çevre çöpü) için `INVISIBLE_F` / `emask` yanlış pozitif kontrolü kaldırılıp yalnızca `skill[25] == 2<<10` (gerçek vakum) veya varlık silinmesi (`seen_frame != g_frame`) esas alındı.
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`, `261120 B`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Yeni DLL henüz oyunda test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `260608 B` DLL'i tek PC bölünmüş ekranda denedi (`SST2` çalıştı, HUD görünmedi, çevre çöpleri senkron olmadı); bu yeni `261120 B` DLL henüz denenmedi.
Varsayımlar:
- yok (`draw_quad`, `draw_textmode`, `draw_text` imzaları `acknex.dll` ve `afuncs.h` üzerinden doğrulandı)
Kalan sorunlar / yarım kalanlar:
- Yeni `261120 B` DLL'in `autotest.ps1 -MenuOnly` ile tek PC bölünmüş ekranda tekrar denenip Acknex HUD'unun, F5/F9'un ve çevre çöpü senkronunun doğrulanması.

## 2026-11-19 · Gemini 4 Neon · 21:06 test analizi: 3x kılıç/taret animasyon takılması + 360° kamera ok yönü + çevre çöpü ikonu & s23=4 elmas
İnceleme: bekliyor
Commit'ler: yok (kullanıcı isteğiyle commit edilmedi)
Yapılan:
- `210622_11180.log` / `210636_5944.log` analizi:
  - Acknex native HUD ekranda göründü.
  - Client kılıç vuruşlarında `turret hit` algılayıcısı aynı vuruşu 3x (`c_trace` + `turret` + `turret`) gönderip host'ta robotu sürekli `yaralan` stun döngüsünde kilitliyordu.
  - Ekran dışı ok, `vec_to_screen == NULL` durumunda hep alt kenara (`off_dir = 3`) sabitlenmişti.
  - `cop_model_fizik` (`kind=1`) `cop_icon.tga` ikonunu `skill[31]` handle'ında tutuyor ve gizlenmiyordu; ayrıca `skill[22] == 4<<10` elmas/bonus tipi `itm_collect_ent`'te eksikti.
- `local/src/probe/probe.c`:
  - `WPUP.last_hit_frame`, `hit_client_tick`: Normal kılıç vuruşunda `s35 = 100<<10` ve `last_hit_frame = g_frame` yazıldı; `turret hit` yalnız `(g_frame - last_hit_frame) > 6 && s37 == (1 << 10)` iken tetiklenir.
  - `hud_tick`: `camera` (`VIEW*` @40 x/y, @52 pan) üzerinden `cos`/`sin` 360° projeksiyonla gerçek sol/sağ/üst/alt ekran kenarı yön oku hesaplandı + tek seferlik `[hud] v2s` logu eklendi.
  - `itm_collect_ent`, `itm_rx`: `s23 == (4 << 10)` elmas/bonus eklendi; `itm_rx` içinde `TRASH_FLAG2_F` kapatılıp `ptr_for_handle(skill[31])` ile `cop_icon.tga` ikonu da gizlendi.
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Yeni DLL henüz oyunda test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `261120 B` DLL'i denedi (HUD göründü; ok hep aşağıdaydı, robot animasyonu takılıyordu, çöp/elmas senkronu eksikti); bu yeni DLL henüz denenmedi.
Varsayımlar:
- yok (`VIEW` ofsetleri `atypes.h`'den, `skill[31]` ve `s23=4` `decomp.c`'den doğrulandı)
Kalan sorunlar / yarım kalanlar:
- Yeni DLL'in `autotest.ps1 -MenuOnly` ile tekrar denenip 4 düzeltmenin doğrulanması.

## 2026-11-19 · Gemini 4 Neon · Robot yaralanma animasyonu donma kök nedeni (ent_animate STRING*) + RTT bekleme süresi düzeltmesi + SIT2 koordinat yedekli eşya senkronu
İnceleme: bekliyor
Commit'ler: yok (kullanıcı isteğiyle commit edilmedi)
Yapılan:
- `decomp.c` (`AIF_GBR_yaralan`) ve `211525_12868.log` / `211534_10432.log` analizi:
  - Client'ta robota vurduktan sonra animasyonun donmasının gerçek kök nedeni: `AIF_GBR_yaralan` / `AIF_ZBD_yaralan` `ent_animate`'e `char*` yerine `STRING*` (sınıf 10) geçiyormuş; `d_ent_animate` bunu düz `safe_str` ile okuduğu için animasyon adı boş/bozuk gidiyordu.
  - Host'ta 200 ms / Client'ta 30 ms ping görünmesinin nedeni: `HUDPKT` 250 ms'de bir gönderildiği için karşı tarafın `echo_ms` cevabına 250 ms bekleme süresi (holding delay) ekleniyordu.
  - `SIT1` harita başı ilk 3 sn'de `main_check_save` / LOD kaynaklı 40+ sahte `local gone` ile kuyruğu dolduruyordu.
- `local/src/probe/probe.c`:
  - `str_or_cstr`, `d_ent_animate`, `d_ent_animatefrom`: `STRING*` (class tag 10 -> `st->chars`) ve `char*` desteği eklendi.
  - `hud_tick`, `hud_rx`: `echo_ms` gönderilirken `hold = now - g_hud_last_rx_ms` eklenerek 250 ms bekleme süresi RTT'den düşüldü.
  - `ITEM_MAGIC` (`'SIT1' -> 'SIT2'`), `ITEMPKT`, `itm_collect_ent`, `itm_rx`, `itm_tick`: Harita başı ilk 180 kare (3 sn) `settle` koruması + `(x, y, z, kind)` koordinat yedekli (`< 48` birim) eşya eşleşmesi eklendi.
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol `'SIT1' -> 'SIT2'` güncellendi. Yeni DLL henüz oyunda test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki DLL'i denedi (ok yönü düzeldi, 3x taret hatası bitti; robot animasyonu hâlâ donuyordu, ping 200/30ms gösteriyordu, çöp/elmas senkronu eksikti); bu yeni DLL henüz denenmedi.
Varsayımlar:
- İki tarafın da yeni `'SIT2'` DLL'ini kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Yeni DLL'in `autotest.ps1 -MenuOnly` ile denenip robot yaralanma animasyonunun, gerçek pingin ve `SIT2` çöp/elmas senkronunun doğrulanması.

## 2026-11-19 · Gemini 4 Neon · A8 STRING* tag=2 / chars@+20 düzeltmesi + çevre çöpü emask&=~1 algılama + [wanim] teşhis logu
İnceleme: bekliyor
Commit'ler: yok (kullanıcı isteğiyle commit edilmedi)
Yapılan:
- `acknex.dll` (`draw_text`) ve `decomp.c` (`pfcan_kutu_cek:121957`, `pfcan_detect_event:105662`) incelemesi:
  - A6 SDK'da `HBYTE_STRING = 10` (`chars@+12`) iken A8 `acknex.dll`'de `STRING` class tag'inin **`2`** (`0x02000000`) ve `chars` ofsetinin **`+20` (`0x14`)** olduğu doğrulandı; önceki `str_or_cstr` yalnız tag `10` baktığı için `AIF_GBR_yaralan` `STRING*` sahne adları hâlâ çözülememişti.
  - `pfcan_kutu_cek` çevre çöpünü (`kind=1`) vakumlayınca `skill[25]=2` değil `emask &= ~1` yazıyor; önceki testte `(emask & 1) == 0` kaldırıldığı için 0 çevre çöpü `local gone` olmuştu.
  - `skill[22] == 4<<10` elmas değil trambolin (`vecTramp_distratio`) olduğu için `itm_collect_ent`'ten çıkarıldı.
- `local/src/probe/probe.c`:
  - `str_or_cstr`: A8 `tag == 2` (`*(char **)(nm + 20)`) eklendi.
  - `wrecord`: İlk 16 benzersiz `OP_ANIMATE` sahne adı için `[wanim]` teşhis logu eklendi.
  - `itm_collect_ent`, `itm_tick`: `kind == 1` için `(emask & 1) == 0` algılaması geri eklendi (`settle` 180 kare koruması aktif); `s23 == 4<<10` çıkarıldı.
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Yeni DLL henüz oyunda test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki DLL'de pingin normalleştiği doğrulandı; robot animasyonu ve çöp/elmas senkronu bu yeni DLL'de tekrar denenecek.
Varsayımlar:
- yok (tüm ofsetler `acknex.dll` ve `decomp.c` içinden doğrulandı)
Kalan sorunlar / yarım kalanlar:
- Yeni DLL'in `autotest.ps1 -MenuOnly` ile denenip `[wanim]` satırlarının, robot yaralanma animasyonunun ve çevre çöpü/elmas senkronunun doğrulanması.

## 2026-11-19 · Gemini 4 Neon · Bit-exact acknex 0x10004260 STRING* çözümü (+12) + G_ITM_TBL (g_07faa41d) tabanlı SIT3 çöp/elmas senkronu
İnceleme: bekliyor
Commit'ler: yok (kullanıcı isteğiyle commit edilmedi)
Yapılan:
- `acknex.dll` (`ent_animate` -> `ent_animatefrom` -> `0x10004260` disassembly) ve `decomp.c` (`g_07faa41d`, `g_07faa73d`) kesin doğrulaması:
  - `acknex.dll` `0x10004260`: `(hdr & 0xFF700000) == 0x02000000 || hdr == -1` olduğunda `*(char **)((BYTE *)nm + 12)` (`+0x0C`) döndürüyor (`+20` yanlıştı).
  - Oyunun kendi kayıtlı eşya dizisi `G_ITM_TBL(ebx, idx)` (`ebx + 0x26a3d8`, `0..199`) ve `G_ITM_CNT(ebx)` (`ebx + 0x26a6f8`) bulundu; hem `pfcan_kutu_cek` (çevre çöpü) hem `pfcan_bonus_parcalan` (elmas) toplama anında `G_ITM_TBL[e->skill[5] >> 10] = 0` yazıyor.
- `local/src/probe/probe.c`:
  - `str_or_cstr`: `acknex.dll` `0x10004260` ile bit-exact uyumlu hale getirildi (`+12`).
  - `G_ITM_TBL`, `G_ITM_CNT`, `ITEM_MAGIC` (`'SIT2' -> 'SIT3'`), `itm_collect_ent`, `itm_rx`, `itm_tick`, `coop_tick`: Koordinat hash'i / LOD / `INVISIBLE` / `emask` tahminleri tamamen kaldırılıp doğrudan oyunun kendi `G_ITM_TBL[0..cnt-1]` (`1 -> 0`) tablosu ve `skill[5] >> 10` sabit ID'si kullanıldı (meyveler `s23 != 1, 3` olduğu için bağımsız kalır; çevre çöpünün `skill[31]` `cop_icon.tga` ikonu da gizlenir).
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol `'SIT2' -> 'SIT3'` güncellendi. Yeni DLL henüz oyunda test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır (önceki hatalı tahminler giderilip assembly ve `decomp.c` ile kesinleştirildi).
Varsayımlar:
- yok (tüm ofsetler doğrudan `acknex.dll` `0x10004260` ve `decomp.c` içinden doğrulandı)
Kalan sorunlar / yarım kalanlar:
- Yeni DLL'in oyunda tek seferlik doğrulaması (`[wanim]`, robot yaralanma animasyonu, `SIT3` çöp/elmas senkronu).

## 2026-11-19 · Gemini 4 Neon · 21:43 testi doğrulaması (robot yaralanma + robot/çevre çöpü tamam) + elmas skill[8]=0 & LOD koruması
İnceleme: bekliyor
Commit'ler: yok (kullanıcı isteğiyle commit edilmedi)
Yapılan:
- `214345_13744.log` / `214354_20900.log` doğrulaması:
  - Robot yaralanma animasyonları (`Yaralan_A/B/C/D`, `Toparlan_A`, `Atak_A/B`, `Titre`) tamamen çözüldü ve takılma bitti.
  - Client robot çöpü vakumlama (`+7/+7/+7`) ve çevre çöpü senkronu (`idx=101 kind=1`, `+6`) doğrulandı.
  - Elmas (`idx=135 kind=3`) host'a ulaşıp gizlenmesine rağmen `pfcan_bonus_parcalan:105045`'teki `skill[8] = 0` yazılmadığı için oyunun görünürlük/LOD yöneticisi elması tekrar görünür yapıyordu.
- `local/src/probe/probe.c`:
  - `itm_rx`, `itm_tick`: `kind == 3` (elmas) gizlenirken `skill[8] = 0` eklendi; ayrıca `G_ITM_TBL[i] == 0` olan (toplanmış) eşyaları motor tekrar görünür yapmaya kalkarsa `itm_tick` tarafından bastırılması eklendi.
Doğrulama: `local/src/probe/build.bat` ile hatasız derlendi (`local/build/dinput8.dll`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol değişmedi (`SIT3`). Elmas düzeltmesi henüz oyunda test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `SIT3` DLL'ini denedi (robotlar takılmıyor, çöpler senkron, ping normal; elmas hâlâ görünür kalıyordu); yeni elmas `skill[8]=0` düzeltmesi henüz denenmedi.
Varsayımlar:
- yok (`skill[8] = 0` doğrudan `decomp.c:105045` `pfcan_bonus_parcalan` içinden doğrulandı)
Kalan sorunlar / yarım kalanlar:
- Sıradaki adım: Faz 2 (`run_id` + late-join / reconnect snapshot) veya önce elmasın son bir kontrolü.


## 2026-11-19 · Gemini 4 Neon · Faz 2: Oturum kimliği (run_id / SHD2) + Late-Join/Reconnect Snapshot (SSN1) + bağlantı kopma/yeniden bağlanma HUD'u
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `local/src/probe/probe.c`:
  - `g_run_id`, `g_peer_run_id`, `coop_init`: Her oturum başında `GetTickCount() ^ (PID * 2654435761u) ^ QPC` ile rastgele 32-bit `g_run_id` üretimi eklendi.
  - `HUD_MAGIC` (`'SHD1' -> 'SHD2'`), `HUDPKT`, `HUDSTATE`, `hud_rx`, `hud_tick`: `HUDPKT` içine `run_id` eklendi; peer'ın `run_id`'si değiştiğinde `net_on_peer_run_id` ile tüm RX sequence/dedup sayaçları (`g_last_rx_seq`, `g_nsnap`, `g_clk_init`, `g_last_wseq`, `g_nwsnap`, `g_boss_rx_seq`, `g_stat_rx_seq`, `g_mkr_rx_state`, `g_alrm_rx_state`, `g_hit_seen`, `g_phit_seen`, `g_itm_rx_seen`, `g_ssn_rx_id`) ve `g_stat_host_seen_*`/`g_stat_tot_*` sıfırlanır (erken restart'ta paket düşmesi giderildi).
  - `SSN_MAGIC` (`'SSN1'`), `SSNPKT`, `ssn_rx`, `ssn_tick`, `itm_hide_ent`, `itm_tick`, `coop_tick`: Yeni `peer_run_id` veya >5 sn kopma sonrası yeniden bağlanmada host iki taraf aynı haritadayken tek seferlik (3x aralıklı: `0 / +80 / +200 ms`) `'SSN1'` dünya snapshot'ı gönderir (`G_MKR_STATE`, `G_ALRM_STATE`, puan/elmas/çöp sayaçları, `G_ITM_TBL[0..199]` için 200 bit / 25 B `itm_mask`, Deve HP, 4 Pil HP, Hangar kapısı); `itm_hide_ent` yardımcısı hem `itm_rx`, hem `ssn_rx`, hem `itm_tick` LOD bastırmasında (çevre çöpü ikonu ve elmas `skill[8]=0` dahil) ortak kullanıldı.
  - `hud_tick`: Bağlantı >5 sn kesilirse sağ alt köşe HUD panelinde kırmızı şerit + `"BAGLANTI KESILDI (Nsn)"` uyarısı, geri dönünce `"YENIDEN BAGLANDI"` toast bildirimi eklendi.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata / sıfır yeni uyarı ile derlendi (`local/build/dinput8.dll`, `271360 B`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol `'SHD1' -> 'SHD2'` güncellendi ve `'SSN1'` eklendi (iki taraf aynı DLL olmalı). Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- İki tarafın da yeni `'SHD2'` / `'SSN1'` destekli `271360 B` DLL'ini kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Yeni DLL'in oyunda (özellikle client restart / late-join + elmas `skill[8]=0` kontrolü) test edilmesi; ardından HANDOFF sırasına göre grafik/ışık bozulması (`oyuncu_ga.gen` / `defG*`) incelemesi veya Faz 3 (3D ses senkronu `'SND1'` + ara sahne).


## 2026-11-19 · Gemini 4 Neon · 22:07 testi doğrulaması (elmas SIT3 tamam) + G_CTRL_ENABLED ofset düzeltmesi (0x26b17c) + SSN1 menu.wmb koruması + [gfx] teşhis logu
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `220736_8408.log` / `220745_8692.log` ve ekran görüntüsü analizi + `decomp.c` doğrulaması:
  - Elmas senkronu (`skill[8]=0` + LOD bastırma) oyunda doğrulandı (iki tarafta da puan `0828`, elmas geri gelmedi).
  - HUD'da sürekli `"ARA SAHNE"` yazmasının ve F5 ışınlanmanın çalışmamasının kök nedeni: `G_CTRL_ENABLED` için yazılan `0x26b160` (`g_07fab1a5`) gerçekte P tuşu kamera moduymuş (`on_p_tus:94442`, normalde `0`). Gerçek kontrol açık bayrağı `pfcan_mod_pasif:102104` ve `pfcan_mod_olme:107881` kanıtıyla **`g_07fab1c1` (`ebx + 0x26b17c`)**.
  - `SSN1` snapshot'ının host/client `menu.wmb`'de buluşunca harcanıp `orman.wmb`'de tekrar gitmediği logda görüldü.
  - Grafik/ışık farkında iki kopyanın `oyuncu_ga.gen` MD5'inin (`DBC5EDD5...`) ve komut satırlarının birebir aynı olduğu, HUD parlaklıklarının aynı olup yalnız 3D sahnenin sağda gölgede/karanlık kaldığı doğrulandı.
- `local/src/probe/probe.c`:
  - `G_CTRL_ENABLED`: `0x26b160` -> `0x26b17c` (`g_07fab1c1`).
  - `ssn_tick`: `menu.wmb` iken `g_ssn_pending` harcanmaz; gerçek oyun haritasına girilince gönderilir.
  - `coop_tick`: Harita değişiminden 120 kare sonra tek satır `[gfx]` teşhis logu (`g_07fa0c11`, `g_07fa0c25`, `g_07fa0b39`, `g_07fa0b45`, `g_07fcd1d9` sun view + flags, `sun_light`, `sun_color`, `ambient_color`, `ctrl`) eklendi.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata ile derlendi (`local/build/dinput8.dll`, `271872 B`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol değişmedi (`SHD2` + `SSN1`). Yeni düzeltme henüz oyunda test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `271360 B` DLL'ini denedi (elmas senkronu doğrulandı; `"ARA SAHNE"` / F5 hatası ve sağ pencerede grafik/ışık farkı görüldü); yeni `271872 B` DLL henüz denenmedi.
Varsayımlar:
- yok (`g_07fab1c1` doğrudan `decomp.c:102104` içinden doğrulandı)
Kalan sorunlar / yarım kalanlar:
- Yeni `271872 B` DLL ile F5 ışınlanma, `ARA SAHNE` yazısının kalkması, `orman.wmb`'de `SSN1` ve `[gfx]` log çıktısının incelenmesi.


## 2026-11-19 · Gemini 4 Neon · 22:18 testi doğrulaması (SSN1 + G_CTRL_ENABLED tamam) + F5/revive ışınlanma düzeltmesi (pb->x/y/z) + [gfx] log tetik düzeltmesi
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `221801_18544.log` / `221810_7908.log` ve `decomp.c:78774-78780` doğrulaması:
  - `SSN1` host->client `orman.wmb` üzerinde doğrulandı (`[ssn] host built SSN1 id=1` -> `[ssn] client applied SSN1 id=1`).
  - `G_CTRL_ENABLED` (`0x26b17c`) doğrulandı (`ARA SAHNE` yazısı gitti, F5 tetiklendi).
  - F5 ışınlanmasının aynı yerde kalmasının kök nedeni: `SUPERCAN_BOX.MDL` PhysX rigid body değil Acknex `c_move` karakter kutusu (`decomp.c:78780` `vec_set(g_07fab1c9 + 16, ...)`). `if (pX_setpos) pX_setpos(...) else { pb->x=... }` yazıldığı için `pXent_setposition` bulunup no-op dönüyor, `pb->x/y/z` hiç yazılmıyordu.
- `local/src/probe/probe.c`:
  - `hud_tick` (F5) ve `revive_tick`: Önce doğrudan `pb->x = pos[0]; pb->y = pos[1]; pb->z = pos[2];` yazılır, ardından varsa `pX_setpos(pb, pos)` çağrılır.
  - `coop_tick`: `[gfx]` teşhis logunun harita yükleme sırasında `out.count == 0` iken atlanmasını önlemek için tetikleme `out.count > 0 && _stricmp(out.level, "menu.wmb")` koşuluna bağlandı.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata ile derlendi (`local/build/dinput8.dll`, `271872 B`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol değişmedi. F5 ışınlanma düzeltmesi henüz oyunda test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `22:16` DLL'ini denedi (`ARA SAHNE` gitti, F5 toast çıktı ama oyuncu aynı yerde kaldı); bu yeni `22:21` DLL henüz denenmedi.
Varsayımlar:
- yok (`pb->x/y/z` doğrudan `decomp.c:78774-78780` içinden doğrulandı)
Kalan sorunlar / yarım kalanlar:
- F5 ışınlanmasının oyunda doğrulanması ve sıradaki adım olarak Faz 3 (3D ses senkronu `'SND1'` + ara sahne geçme bildirimi/senkronu).


## 2026-11-19 · Gemini 4 Neon · Faz 3: 3D Ses Senkronu (SND1, entPlaySound kancası) + Ara Sahne Geçme Senkronu (SCT1) + HUD UI yapılacaklar notu
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `decomp.c` (`entPlaySound` @ `08001d30`, `g_07fcd2a9[0..320]`, `Oyun_video_ac:62313`) ve `disfn.py entPlaySound` analizi:
  - Oyunun tüm 3D efekt sesleri `entPlaySound(var *pos, var handle, var vol)` üzerinden ve `G_SND_TBL(ebx, idx)` (`ebx + 0x28d264`, `0..320`) sabit FMOD handle tablosuyla çalınıyor.
  - `entPlaySound` prologu `55 53 56 57 E8 00 00 00 00` (`pz_install` ile birebir aynı).
- `local/src/probe/probe.c`:
  - `G_SND_TBL`, `SND_TBL_MAX`, `SND_MAGIC` (`'SND1'`), `SNDPKT`, `snd_on_ent_play`, `stub_ent_playsound`, `snd_hook_install`, `snd_rx`, `snd_tick`: `entPlaySound` prolog doğrulamalı kancalandı; `G_SND_TBL` indeksi + 3D konum + ses seviyesi `400 ms` aynı-ses filtresi ve `12/sn` flood korumasıyla aktarılır; karşı tarafta `g_snd_in_replay` yankı koruması ve `2500` birim mesafe filtresiyle orijinal `entPlaySound` çağrılır (`coop.ini` `sound_sync=0` ile kapatılabilir).
  - `CUT_MAGIC` (`'SCT1'`), `CUTPKT`, `cut_rx`, `cut_tick`, `cut_inject_esc`: Ara sahnede (`G_CTRL_ENABLED == 0`) bir oyuncu `ESC` ile geçtiğinde 3x aralıklı `'SCT1'` paketi gider; karşı taraf da ara sahnedeyse 6 kare `DIK_ESCAPE` (`0x01`) enjekte edilip `"ARKADAS ARA SAHNEYI GECTI"` toast'ı gösterilir.
  - `net_on_peer_run_id`, `coop_tick`: `SND_MAGIC` / `CUT_MAGIC` Steam+UDP alım yönlendirmesi, `snd_tick` / `cut_tick` çağrıları ve `run_id` değişiminde dedup sıfırlamaları eklendi.
- `local/HANDOFF.md`: Yapılacaklar listesine "Genel Co-op HUD UI tipi/tasarımı elden geçirme" notu eklendi.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata ile derlendi (`local/build/dinput8.dll`, `275456 B`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Yeni protokol paketleri `'SND1'` ve `'SCT1'` eklendi (iki taraf aynı DLL olmalı). Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- İki tarafın da yeni `'SND1'` / `'SCT1'` destekli `275456 B` DLL'ini kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Yeni `275456 B` DLL'in oyunda denenmesi (F5 ışınlanma + `[snd]` 3D ses + `[cut]` ara sahne geçme + `[gfx]` teşhis logu); ardından Faz 4 (`diff_logs.py` + save güvenliği doğrulaması) ve HUD UI yenilemesi.


## 2026-11-19 · Gemini 4 Neon · 2. Müzik kanalı senkronu (SMU1) + Faz 4: Host/Client Log Diff aracı (diff_logs.py) + Save güvenliği doğrulaması
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `local/src/probe/probe.c`:
  - `G_MUS2_TRK` (`0x28c4f0`, `g_07fcc535`), `G_MUS2_ON` (`0x28c4f4`, `g_07fcc539`), `G_MUS2_VOL` (`0x28c4f8`, `g_07fcc53d`) makroları eklendi (`decomp.c:93940-94015` `music_channel2_loop` doğrulamalı).
  - `MUS_MAGIC` (`'SMU1'`), `MUSPKT`, `mus_rx`, `mus_tick`, `net_on_peer_run_id`, `coop_tick`: Özel bölge / boss 2. müzik kanalı host otoriteli senkronlandı (değişimde + 2 sn periyodik güncelleme; savaş müziği zaten `G_ROBOT_ALIVE` üzerinden senkronlu).
- `local/dump/code/decomp.c` (`main_check_save:219047-219235`, `main_genel_save:219345-219534`) save güvenliği doğrulaması:
  - Oyun `game_save` (sahne entity dump) kullanmıyor; yalnızca sabit skaler globalleri ve `G_ITM_TBL[]` dizisini şifreleyip `.gen`'e yazıyor. Kuklaların `.gen` kayıtlarına bulaşma riski sıfır.
- `local/tools/diff_logs.py`:
  - İki `coop_probe_*.log` dosyasını (veya argümansız `SuperCan1` / `SuperCan1b` son loglarını) okuyup `run_id`, `SSN1`, `[gfx]`, crash, F5/revive, ses/müzik, zaman hizalı `[sync]` kıyas tablosu ve `SIT3`/makara/alarm/boss özetini çıkaran Python 3 aracı eklendi.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata ile derlendi (`local/build/dinput8.dll`, `276480 B`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. `diff_logs.py` gerçek test logları (`220736_8408.log` / `220745_8692.log`) üzerinde çalıştırılarak doğrulandı. Yeni `'SMU1'` protokolü eklendi (iki taraf aynı DLL olmalı); `SMU1`, `SND1`, `SCT1` ve F5 ışınlanma henüz oyunda test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- İki tarafın da yeni `276480 B` DLL'ini kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Yeni `276480 B` DLL'in oyunda (mümkünse arkadaşla Steam'de veya tek PC bölünmüş ekranda) denenip `python -I local\tools\diff_logs.py` ile doğrulanması; ardından genel Co-op HUD UI yenilemesi veya Faz 5 (modülerleştirme + SüperCan 2).


## 2026-11-19 · Gemini 4 Neon · Co-op HUD UI yenilemesi (Modern Kompakt Takım Kartı — L4D2 / Deep Rock Galactic tarzı)
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `local/src/probe/probe.c`:
  - `hud_rect`, `hud_frame`, `hud_text_s`, `hud_bar`, `hud_ping_bars` yerel Acknex 2D çizim yardımcıları eklendi.
  - `hud_tick`: Sağ alt takım kartı (`392x138`) yenilendi — yumuşak dış gölge, koyu başlık şeridi, sol dikey rol/durum renk şeridi, metalik ince çerçeve, `HOST`/`P2` rozeti, 4 çubuklu canlı ping sinyali + `ms`, cam parlama efektli 4-segmentli Can/Nano barları, kalın `Tahoma` gölgeli net font (`17..22 pt` — küçük pencerede de kırılmadan okunur), alt bilgi/mesafe şeridi.
  - Baş üstü etiket: sol cyan çizgili + ince çerçeveli + ok uçlu kart; ekran dışı gösterge: kenar yönü vurgulu cyan şeritli kart; toast bildirimi: üst/alt altın çizgili modern banner.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata ile derlendi (`local/build/dinput8.dll`, `279552 B`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol değişmedi. Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- yok
Kalan sorunlar / yarım kalanlar:
- Yeni `279552 B` DLL'in oyunda topluca test edilip (yeni HUD, F5 ışınlanma, `SND1` 3D ses, `SMU1` 2. müzik kanalı, `SCT1` ara sahne, `[gfx]`) `python -I local\tools\diff_logs.py` ile doğrulanması.


## 2026-11-19 · Gemini 4 Neon · 22:55 toplu testi doğrulaması (yeni HUD, F5, SND1, SSN1 tamam) + SCT1 harita başı sahte tetikleme düzeltmesi
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `225529_14348.log` / `225538_17884.log` (`diff_logs.py` + ekran görüntüsü) doğrulaması:
  - 0 crash, 0 desync (`784 -> 865`, `9/2`, `SIT3` `12/12`, `SSN1` `hid_itm=19`), yeni HUD, F5 ışınlanma ve `SND1` 3D ses oyunda doğrulandı.
  - Sağ pencerede grafik/zemin shader bozulması tekrar görüldü; `[gfx]` satırları iki tarafta birebir aynı çıktı (`shd=2 q25=9 gam=50 b45=3 sunView(f=25704041)`). Kullanıcı bu sorunun arkadaşın PC'sinde hiç olmadığını, yalnız ana PC'de (çoğunlukla tek PC bölünmüş ekranda, 1-2 kez tek oyun açılışında) olduğunu bildirdi.
  - 2. bölüm girişinde gerçek ara sahne olmamasına rağmen harita yükleme fade-in'inde `G_CTRL_ENABLED` `0 -> 1` geçişinin `g_cut_prev_in && !in_cut` dalını yanlışlıkla tetikleyip client'a `ESC` enjekte ettiği tespit edildi.
- `local/src/probe/probe.c`:
  - `cut_tick`, `cut_rx`: `g_cut_prev_in && !in_cut` sahte tetikleme dalı kaldırıldı; `g_cut_in_frames >= 30` (~0.5 sn) eşiği eklendi — yalnızca oyuncu gerçekten ara sahnedeyken `ESC` tuşuna bastığında `'SCT1'` gönderilir ve karşı taraf da `>=30` karedir ara sahnedeyse `ESC` enjekte edilir.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata ile derlendi (`local/build/dinput8.dll`, `279552 B`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol değişmedi. `SCT1` gerçek bir ara sahnede henüz test EDİLMEDİ; `SMU1` özel bölge/boss müziğinde henüz test EDİLMEDİ.
Kullanıcı/arkadaş denedi mi: evet (`22:55` testinde yeni HUD, F5 ışınlanma ve 3D ses doğrulandı; `23:03` `SCT1` düzeltmesi henüz denenmedi).
Varsayımlar:
- yok
Kalan sorunlar / yarım kalanlar:
- `/clear` sonrası yeni sohbette bu oturumda yapılan tüm değişikliklerin (`probe.c` Faz 2, Faz 3, `SMU1`, `SCT1`, yeni HUD ve `local/tools/diff_logs.py`) kapsamlı kod incelemesi (review) yapılacak; ardından ana PC'deki ara sıra olan grafik/gölge bozulması, Steam testi ve Faz 5 (modülerleştirme + SüperCan 2).


## 2026-11-19 · Gemini 3.1 Pro (High) · Sıradaki Adım #1: Kapsamlı Kod İncelemesi (Code Review) + Edge-Case Düzeltmeleri (revive_tick, SMU1, SSN1, diff_logs.py)
İnceleme: geçti
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `local/src/probe/probe.c` ve `local/tools/diff_logs.py` üzerinde önceki oturumda yazılan tüm değişiklikler (`SHD2`/`g_run_id`, `net_on_peer_run_id`, `SSN1`, `G_CTRL_ENABLED`, F5/`revive_tick`, `SND1` `entPlaySound` kancası, `SCT1`, `SMU1`, yeni HUD yardımcıları) `local/dump/code/decomp.c` ile satır satır çapraz kontrol edildi:
  - **`revive_tick` (`pfcan_mod_olme` / `Oyun_gameover_ac` yarış durumu düzeltmesi)**: `decomp.c:107803` (`pfcan_mod_yuvarlan`), `109171` (`pfcan_mod_yaralan`) ve `106100` (`pfcan_mod_olme`) incelendiğinde, oyuncunun canı (`skill[35]`) `<= 1<<10`'a düştüğü karede `wait(1)` olmadan doğrudan `pfcan_mod_olme()` ölüm döngüsüne girdiği ve döngü `skill[35]`'e hiç bakmadığı için ~1.5 sn sonra `Oyun_gameover_ac()` (Game Over) çağırdığı tespit edildi. Ayrıca su/uçurum ölümlerinde (`pfcan_hareket_kont:101950-101978`) `g_07fa4675` (`ebx + 0x264630`) `!=-1` yapıldığı için `pfcan_mod_olme`'nin ilk karede `iStack_4b0 = 1<<10` ve kararma paneli (`0x7d4ccad`) açtığı görüldü. `revive_tick` içinde canlanma tetiklendiğinde `G_CTRL_ENABLED(ebx) = 0` (`g_revive_break_olme = 1`), `g_07fa4675 = -1<<10`, `g_07fa0991 = 1<<10`, `skill[36] = 0` ve panel `0x7d4ccad` temizliği yapılarak `pfcan_mod_olme` döngüsünün ilk `wait(1)`'de `iStack_4b0 == 0` ile kırılıp `pfcan_mod_pasif()`'e düşmesi, ertesi karede `G_CTRL_ENABLED(ebx) = 1<<10` geri açılarak `pfcan_mod_bekle()`'ye temiz geçiş yapması sağlandı.
  - **`mus_rx` (`SMU1` — `music_channel2_loop` görev yaşam döngüsü düzeltmesi)**: `decomp.c:93873` incelendiğinde `music_channel2_loop`'un harita başında kendiliğinden çalışan bir görev olmadığı (yalnız `main_alarm_music`, `AIF_Hangar_Start`, `AIF_Hangar_Load` içinden çağrıldığı) doğrulandı. Client'ta `p.on != 0 && cur_on == 0` geldiğinde, `g_07fcbf61` (`ebx + 0x28bf1c`) `0` ise ve görev listesinde (`safe_task` + `safe_scriptinfo`) `music_channel2_loop` yoksa `engine_callscript4` ile tek seferlik `music_channel2_loop` başlatılması eklendi (çift görev / FMOD stream sızıntısı korumalı).
  - **`ssn_tick` (`SSN1` — `main_check_load` zamanlama penceresi genişletmesi)**: `decomp.c:80453` ve `217486` (`main_check_load`) incelendiğinde, harita yüklenirken `main_check_load`'un 6 `wait(1)` (~100-180 ms) boyunca çalışıp sonunda `gamedata_load_locfx()` ile `.gen` dosyasını okuyarak `G_SCORE`, `G_DIAMONDS` ve `G_ITM_TBL[]` dizisini ezdiği görüldü. `SSN1` 3x tekrar aralığı `0 / +80 / +200 ms` yerine `0 / +150 / +800 ms` yapılarak 3. paketin `main_check_load` bittikten sonra da kesin uygulanması garantilendi.
  - **`SND1` (`entPlaySound` @ `08001d30`, `decomp.c:7138`) ve `SCT1` (`cut_tick`/`cut_rx`)**: `entPlaySound`'un içinde `wait()` veya `E_me`/`E_you` bağımlılığı olmadığı, `stub_ent_playsound` ve `snd_rx` doğrudan çağrısının güvenli olduğu doğrulandı. `SCT1` `g_cut_in_frames >= 30` ve `g_cut_esc_prev` kenar tetiklemesinin çok satırlı diyaloglarla (`diyalog_wait`) uyumlu olduğu doğrulandı.
  - **`local/tools/diff_logs.py` (`menu.wmb` / boş seviye hizalama düzeltmesi)**: Bir taraf `menu.wmb`'de veya yükleme sırasında fazladan `[sync]` satırı yazdığında tüm oyun içi `[sync]` satırlarının kaymasını önlemek için `level not in ("", "?", "menu.wmb")` filtreli hizalama eklendi.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata ile derlendi (`local/build/dinput8.dll`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. `python -I -X utf8 local\tools\diff_logs.py` ile son test logları (`225529_14348.log` / `225538_17884.log`) üzerinde 16/16 `orman.wmb` satırının kaymasız `OK` hizalandığı doğrulandı. `revive_tick` (`pfcan_mod_olme` kırma), `SMU1` görev başlatma ve `SSN1` `+800 ms` tekrarı oyunda henüz test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- İki tarafın da yeni derlenen `dinput8.dll`'i kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Oyunda gerçek bir ara sahnede `SCT1` (ESC ile geçme), özel bölge/boss müziğinde `SMU1` ve oyuncu ölümünde `revive_tick` (`pfcan_mod_olme` kırma) davranışı test edilecek.
- Ana PC'deki ara sıra olan grafik/zemin shader bozulması için teşhis (`sc_setup` / `sc_loop` D3D9 render-target / `tex4` incelemesi), arkadaşla Steam testi ve Faz 5 (`probe.c` modülerleştirme + SüperCan 2 hazırlığı).
- **23:35 Ek Güncelleme (Steam Paketi & Handoff):** `local/steamcoop_paket/` ve `local/coop_paket/` içine son `dinput8.dll` (`280.064 B`, SHA-256 `9ADF00E0...1F66`), güncellenmiş `KURULUM.txt` ve sonunda boş `"Ek Notlar:"` bölümü bulunan `TEST_LISTESI.txt` eklendi; `local/testinstall/SuperCan1/Game/coop.ini` Steam ayarına (`net=steam`, `listen=27015`, `peer=`, `window=full`) alındı ve `local/HANDOFF.md` `/clear` öncesi güncellendi.


## 2026-11-20 · Gemini 4 Hydrogen (Medium) · Steam testi log analizi ve Handoff güncellemesi
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `local/testinstall/SuperCan1/Game/coop_probe_234247_15096.log` (host) ve `local/coop_probe_002618_28184.zip` (`local/tmp_friend_logs/`: `234529`, `002255`, `002424`, `002618` client logları) `local/tools/diff_logs.py` ve satır analiziyle incelendi; `is_host()` Steam başlangıç `P2` davranışı doğrulandı.
- `local/HANDOFF.md`: Tüm Steam testi bulguları (Hangar `tunel.wmb` `devicereset+0x135` crash + harita altı spawn + sürekli ara sahne, çıkış `fmod.dll` crash, Alt-Tab parça kaybı, su ölümü `revive_tick`, baygın/bazı robotlara vuramama, çöp ikonu/animasyonu, spawn müziği, sağlık paketi, hack terminali & ileri bölüm mekanikleri, HUD/name tag konumu, `[interp]` ping/jitter) ve `/clear` sonrası Neon planı eklendi.
Doğrulama: Yalnızca log/kod incelemesi ve `local/HANDOFF.md` güncellemesi (`probe.c` değiştirilmedi, derleme gerekmedi).
Kullanıcı/arkadaş denedi mi: evet — arkadaşla gerçek Steam testi yapıldı; bulgular kullanıcı notları ve loglarla doğrulandı.
Varsayımlar:
- yok
Kalan sorunlar / yarım kalanlar:
- `/clear` sonrası Neon modeli ile `local/HANDOFF.md` altındaki 11 maddelik bulgu listesinin `decomp.c` + `probe.c` üzerinden incelenip çözülmesi.


## 2026-11-20 · Gemini 4 Neon (Medium) · Steam testi 10 maddelik toplu düzeltme (kapanış crash, Hangar/alrm, Alt-Tab, baygın robot, sağlık paketi, su ölümü, çöp ikonu, müzik, sol alt HUD, interp)
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `local/src/probe/probe.c`:
  - **Kapanış crash koruması (`fmod.dll` + `devicereset+0x135`)** (`g_closing`, `G_GAMMA_MODE`, `h_wndproc`, `DllMain`, `snd_on_ent_play`, `snd_rx`): `WM_CLOSE`/`WM_DESTROY`/`DLL_PROCESS_DETACH` sırasında `g_closing = 1` ve `G_GAMMA_MODE(ebx) = 2 << 10` (`devicereset` ölü harici DLL çağrısını atlar) eklendi; kapanışta veya `menu.wmb`/boş haritada `snd_rx`/`snd_on_ent_play` durduruldu.
  - **Hangar (`tunel.wmb`) düzeltmeleri** (`G_SUB_LVL`, `alrm_has_silo`, `cut_tick`, `coop_tick` `[gfx]`): `alrm_has_silo()` yalnız `orman.wmb` ile sınırlandı (`tunel.wmb`'de eski pointer ile `main_alarm_reset()` çalışması önlendi); oyuncu `(0,0,0)` konumunda takılıyken `cut_tick` sahte ara sahne `ESC` spam'i bastırıldı; `[gfx]` loguna `sub=%ld` (`G_SUB_LVL >> 10`) eklendi.
  - **Alt-Tab kukla görünürlük/materyal yenileme** (`g_d3d_reset_gen`, `h_Reset`, `world_client_render`): Başarılı D3D9 `Reset` sonrası tüm aktif `g_wp[i].looked = 0` sıfırlanarak materyal/skin/görünürlük yeniden uygulanır.
  - **Baygın/yerdeki robot vuruşları** (`hit_host_tick`): `AIF_GBR_yuvarlan` / `AIF_ZBD_yuvarlan` (`decomp.c`) yerde `ptr_for_handle(box->skill[27])` üzerinde hasar dinlediği için `hit_host_tick` artık hem `box` hem de geçerli `skill[27]` gövdesine `skill[36]`, `skill[34]`, `skill[38]` yazar ve can düşüşünü ikisinde de takip eder.
  - **Sağlık paketi (`enrj_paket.mdl`) senkronu** (`is_medkit_type`, `is_world_type`, `world_client_render`, `HIT_DMG_MEDKIT`, `hit_client_tick`, `hit_rx`, `hit_apply`): Robot/pil sağlık paketleri client'ta görünür ve `AIF_RbtEnrj_ekle` ile uyumlu (`skill[22]=11<<10`, `skill[7]=7<<10`, `FLAG1`, `ENABLE_SCAN`) hale getirildi; client aldığında `HIT_DMG_MEDKIT` (`101<<10`) ile host'taki gerçek paketi gizler.
  - **Su/uçurum ölümü** (`G_DEATH_TYPE`, `revive_tick`): Kullanıcı tercihiyle `G_DEATH_TYPE(ebx) != (-1 << 10)` (su/uçurum ölümü) iken `revive_tick` müdahale etmez, oyunun kendi su ölümü işler; yalnız normal düşman ölümünde canlanma çalışır.
  - **Robot çöpü kırmızı logosu (`metal_icon.tga`)** (`WPUP.icon`, `world_drop_puppets`, `world_client_render`): Çöp kuklasına yerel `metal_icon.tga` oluşturulup `skill[31] = handle(icon)` bağlandı ve `skill[22] = 1 << 10` açıldı; kukla silinirken veya vakumlanınca ikon da sızıntısız kaldırılır.
  - **Düşman spawn / savaş müziği senkronu** (`G_MUSIC_ALLOW`, `WFLAG_MUS_ALLOW`, `world_tick`, `world_client_sync_counters`): `g_07f9f7c9` müzik izin bayrağı `part[0].flags` bit 29 ile host'tan client'a senkronlandı.
  - **HUD & Name Tag** (`hud_tick`): Co-op takım kartı kullanıcı tercihiyle **sol alt köşeye** (`px = 22.0f`) taşındı; baş üstü Name Tag Z ofseti `72 -> 54`, ekran Y ofseti `-50 -> -34` ve üst kenar kırpması (`>= 10px`) eklendi.
  - **Yüksek ping / `[interp]` akıcılığı** (`g_interp_min/max`, `snap_push`): `g_interp_min = 0.08`, `g_interp_max = 0.25` (250 ms üst sınır), `clk_resets` sıçrama eşiği `1.0s -> 1.5s` ve drift oranı `0.002 -> 0.004` yapıldı.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata / sıfır yeni uyarı ile derlendi (`local/build/dinput8.dll`, `282.624 B`, SHA-256 `3DED7D5E...7E8`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol paket boyutları değişmedi (geriye uyumlu ek bayraklar; iki taraf aynı DLL kullanmalı). Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- İki tarafın da yeni `282.624 B` DLL'i kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Yeni DLL'in arkadaşla Steam'de (veya bölünmüş ekranda) test edilmesi; özellikle `tunel.wmb` Hangar `[gfx] sub=...` çıktısı, baygın robot vuruşu, `enrj_paket.mdl` sağlık paketi, çöp `metal_icon.tga` logosu ve sol alt HUD görünümünün doğrulanması.
- İleri bölüm mekanikleri (hack terminali / `main_alarm_alet` — `G_ALRM_STATE` ile ilişkisi teyit edildi, ek özel senkron ihtiyacı test sonrası değerlendirilecek).


## 2026-11-20 · Gemini 4 Neon (Medium) · Hangar siyah ekran kesin çözümü (G_GAME_ACTIVE) + kukla ışıklandırma (copy_look) + Hack Terminalleri & Alarm Süre Sayacı (SAL2)
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `local/src/probe/probe.c`:
  - **Hangar siyah ekran / `(0,0,0)` / `ctrl=0` kesin çözümü (`G_GAME_ACTIVE`, `world_client_sync_counters`):** Tek PC testinde canlı `dump` ile client'ın `main_check_load` (`decomp.c:217523`, `while (G_ROBOT_ALIVE > 0) wait(1)`) içinde kilitlendiği doğrulandı (host'ta robotlar varken `world_client_sync_counters` yükleme bitmeden `G_ROBOT_ALIVE = 6` yazıyordu). `G_GAME_ACTIVE(ebx) == 0` (`g_07fa2461`) iken `G_ROBOT_ALIVE = 0` tutularak kilit tamamen çözüldü.
  - **Kukla ışıklandırma farkı (`copy_look`):** Oyuncu kuklasına (`local0` `0x220` vs `puppet0` `0x200`) çarpışma/görünürlük dışındaki tüm ışık/gölge bayrakları (`~(INVISIBLE_F | PASSABLE_F | 1 | 2 | 16)`) kopyalanmaya başlandı.
  - **İleri bölüm mekanikleri — 4 Hack Terminali + Lazerler + Alarm Geri Sayım Sayacı (`SAL1 -> SAL2`, `G_ALRM_ALET_ST`, `G_ALRM_ALET_ENT`, `G_ALRM_ISIN_ENT`, `G_ALRM_TIMER`, `alrm_rx`, `alrm_tick`):** `g_07fa4dc9[0..3]` (4 hack terminali) iki yönlü senkronlandı (bir oyuncu hack'leyince diğerinde de terminal `emask &= ~256` ve ilgili lazer `INVISIBLE|PASSABLE` kapanır, 4/4 olunca alarm biter); `g_07fa432d` alarm geri sayım sayacı host otoriteli ~2 Hz eşitlenir.
- `local/testinstall/SuperCan1/Game/coop.ini`: Tek PC testi sonrası `coop.ini.steam`'den tekrar Steam moduna (`net=steam`, `window=full`) döndürüldü.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata ile derlendi (`local/build/dinput8.dll`, `283.648 B`, SHA-256 `246424A8...E5C8`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol `'SAL1' -> 'SAL2'` güncellendi (iki taraf aynı DLL olmalı).
- **Oyunda doğrulananlar (tek PC bölünmüş ekran):** Hangar siyah ekran/doğma düzeltmesi (`G_GAME_ACTIVE`) ve sol alt Co-op HUD kullanıcı tarafından doğrulandı.
- **Test edilmedi:** `copy_look` kukla ışıklandırma düzeltmesi ve `SAL2` (hack terminalleri + alarm süre sayacı) henüz oyunda test edilmedi (Steam testinde denenecek).
Kullanıcı/arkadaş denedi mi: evet (Hangar doğma ve sol alt HUD tek PC'de doğrulandı; yeni `283.648 B` DLL Steam testine hazır).
Varsayımlar:
- İki tarafın da yeni `'SAL2'` destekli `283.648 B` DLL'ini kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Arkadaşla Steam testinde yeni `283.648 B` DLL'in topluca doğrulanması (Hangar, kukla ışıklandırması, baygın robotlar, sağlık paketi, çöp `metal_icon.tga`, su ölümü, `SAL2` hack terminalleri + alarm sayacı, kapanış crash koruması).


## 2026-11-20 · Gemini 4 Neon (Medium) · Deve Boss Fight tam co-op koruması (SBO3: Arena 2P lazer kafes eşiği, QTE üstüne çıkma kilidi, lazer saldırı senkronu)
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `local/src/probe/probe.c`:
  - `BOSS_MAGIC` (`'SBO2' -> 'SBO3'`), `BOSSPKT`, `boss_proc_active`, `boss_rx`, `boss_tick`:
    - **Hangar Arenası 2-Oyuncu Lazer Kafes Eşiği (`AIF_Hangar_Start` `y >= -600`):** `tunel.wmb` `sub==8` iken tek oyuncu `-625` sınırına gelip diğer oyuncu hâlâ `< -635` gerideyse ilk oyuncuyu `-625`'te bekletip `"ARKADASIN ARENAYA GIRMESI BEKLENIYOR"` toast'ı gösterir; iki oyuncu da gelince eşik kalkar. Ek güvenlik ağı olarak kapı/kafes kapandığında (`(flags & PASSABLE_F) == 0`) dışarıda kalan client otomatik arena içine (`y = -500`) çekilir.
    - **Boss Üstüne Çıkma / QTE Kilidi (`AIF_DEVE_combo_kap`):** Bir oyuncu QTE'deyken `combo_busy = 1` gönderir; diğer oyuncuda `G_DEVE_BOX` `emask &= ~256` ve `skill[34] = skill[35]` ile ikinci QTE engellenir (`"ARKADAS BOSSUN USTUNDE!"`).
    - **Lazer Saldırısı Senkronu (`DEVE_laser_atak`):** Host `DEVE_laser_atak` başladığında `laser_seq` artırır; client aynı anda `DEVE_laser_atak` başlatır.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata / sıfır yeni uyarı ile derlendi (`local/build/dinput8.dll`, `285.696 B`, SHA-256 `FD9C8185...7DB4`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol `'SBO2' -> 'SBO3'` güncellendi (iki taraf aynı DLL olmalı). Oyun içi test henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- İki tarafın da yeni `'SAL2'` + `'SBO3'` destekli `285.696 B` DLL'ini kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Arkadaşla Steam testinde yeni `285.696 B` DLL'in tüm özelliklerinin doğrulanması ve test loglarının incelenmesi.


## 2026-11-20 · Gemini 4 Neon (Medium) · Açılış intro videosu/logolarını atlama (skip_intro=1) + Public yayın/Steam Lobi belge güncellemeleri
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `local/src/probe/probe.c`:
  - `intro_skip_install`, `hook_engine_frame`: `Oyun_intro_start` (`decomp.c:92599`, Sobee videosu + NVIDIA/splash logoları; hiçbir oyun globali değiştirmez) prolog doğrulamalı (`55 53 56 57 E8 00 00 00 00`) `0xC3` (`ret`) ile atlandı (`skip_intro=1` varsayılan açık, `coop.ini` `skip_intro=0` ile kapatılabilir; `Oyun_video_ac` hikâye ara sahnelerine dokunmaz).
- `AGENTS.md`, `CLAUDE.md`, `local/HANDOFF.md`: Public GitHub yayın hedefi (oyun dosyası ve kişisel veri olmadan) ve Steam Lobi + oyun içi arkadaş davet/katıl sistemi yapılacaklara işlendi.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata ile derlendi (`local/build/dinput8.dll`, `286.720 B`, SHA-256 `F4DF2C43...A2AC`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol değişmedi. `skip_intro` oyunda henüz test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- yok (`Oyun_intro_start` gövdesi `decomp.c:92570-92754` incelenerek yan etkisiz olduğu doğrulandı)
Kalan sorunlar / yarım kalanlar:
- Arkadaşla Steam testinde son `286.720 B` DLL'in doğrulanması.


## 2026-11-20 · Gemini 4 Neon (Medium) · 02:13 Steam testi düzeltmeleri (metal_icon ölçeği 0.05, SBO4 Hangar giriş lazeri & Deve gövdesi & arena spam kaldırma, silo kapağı/terminal kapalı animasyonu, devicereset ret)
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `local/testinstall/SuperCan1/Game/coop_probe_020219_9668.log` ve `local/coop_probe_021345_8840.log` incelendi:
  - **Doğrulananlar:** 0 client crash, `skip_intro=1`, `enrj_paket.mdl` sağlık paketi (`init` + `client took`), `DEVE_laser_atak` (`laser_seq=1..11`), `SAL2` 4/4 hack terminali, sol alt HUD + Name Tag + `59–75ms` ping.
  - **Bulunan ve düzeltilen 4 hata (`local/src/probe/probe.c`):**
    1. **Kırmızı çöp ikonu (`metal_icon.tga`) devasa çıkması (`world_client_render`):** `cop_robot_fizik` (`decomp.c:132571`, `disfn.py`) ile birebir `scale_x/y/z = 52` (`0.05`) ve `flags |= PASSABLE_F | INVISIBLE_F | 1 | 128` eklendi (önceden default `1.0` olduğu için 20x büyüktü).
    2. **Hangar boss fight (`BOSS_MAGIC` `'SBO3' -> 'SBO4'`, `G_DEVE_GOVDE`, `G_HANGAR_LSR1`, `G_HANGAR_LSR2`, `boss_rx`, `boss_tick`):** 450 kez spam'leyen `pulled client inside arena (was y=... -> -500)` ve `-625` bekleme bloğu kaldırıldı; host'taki Hangar giriş lazerleri (`g_07faa38d`/`91`) ve Deve Boss gövde görünürlüğü/konumu (`g_07faa355`) client'a birebir senkronlandı.
    3. **Silo kapağı + hack terminali kapalı animasyonu (`G_ALRM_KAPAK`, `alrm_apply_state`, `alrm_rx`):** `state == 3` olduğunda client'ta `G_ALRM_SILO` `emask |= 256` ve silo kapağı (`g_07fccb15`) `INVISIBLE|PASSABLE` açılır; hack'lenen terminalde `t_ent_animate(ae, "kapali", 100<<10, 0)` oynatılır.
    4. **Kapanış `devicereset+0x37` crash'i (`h_wndproc`):** `WM_CLOSE`/`WM_DESTROY` anında `devicereset` ilk baytına `0xC3` (`ret`) yazılarak hem `+0x37` (`bmap_zbuffer`) hem `+0x135` kapanış crash'leri susturuldu.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata ile derlendi (`local/build/dinput8.dll`, `287.232 B`, SHA-256 `5407723C...C478`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol `'SBO3' -> 'SBO4'` güncellendi (iki taraf aynı DLL olmalı). Yeni düzeltmeler oyunda henüz test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `286.720 B` DLL Steam'de denendi (yukarıdaki doğrulamalar + 4 hata yakalandı); yeni `287.232 B` DLL henüz denenmedi.
Varsayımlar:
- İki tarafın da yeni `'SBO4'` destekli `287.232 B` DLL'ini kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Yeni `287.232 B` DLL'in Steam'de test edilmesi (kırmızı çöp ikonu boyutu, Hangar giriş lazeri + Deve Boss görünürlüğü, silo atlama + terminal kapalı modeli, 0 kapanış crash'i).


## 2026-11-20 · Gemini 4 Neon (Medium) · 02:29 Steam testi crash çözümü (pfcan_copicon_goster deferred icon remove) + Hangar merdiven giriş lazeri/duvarı (SBO5)
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `local/testinstall/SuperCan1/Game/coop_probe_022708_19720.log` (host: **0 crash**, kapanış `devicereset` `ret` doğrulandı!) ve `local/coop_probe_022908_8552.log` (client: `L4538 pfcan_copicon_goster+0x27a access=00000010`) incelendi.
- `local/src/probe/probe.c`:
  - **`pfcan_copicon_goster` NULL+0x10 crash çözümü (`icon_defer_remove`, `icon_defer_flush`, `world_drop_puppets`, `world_client_render`):** Çöp kuklası silinirken anında `p_ent_remove(q->icon)` yapıldığı için o karede çalışan `pfcan_copicon_goster` `ptr_for_handle(skill[31]) == NULL` alıp çöküyordu. Artık silme sırasında önce `skill[25] = 2<<10`, `skill[22] = 0`, `emask &= ~256` yazılır, `q->icon` gizlenip 20 kare gecikmeli silme kuyruğuna (`g_icon_del_e`) alınır; ayrıca `p_handle(ic) != 0` doğrulanmadan `skill[22]` açılmaz.
  - **Hangar merdiven arası kırmızı giriş lazeri + duvar bloğu (`BOSS_MAGIC` `'SBO4' -> 'SBO5'`, `G_ALT_BLOK`, `G_ALT_LSR`, `boss_rx`, `boss_tick`):** `nesne_alt_blok` (`g_07faa30d`, `+0x26a2c8`) ve `nesne_alt_laser` (`g_07faa311`, `+0x26a2cc`) host'tan client'a senkronlandı (`c_updatehull` dahil).
Doğrulama: `local/src/probe/build.bat` ile sıfır hata / sıfır yeni uyarı ile derlendi (`local/build/dinput8.dll`, `288.256 B`, SHA-256 `11271857...B5BA`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol `'SBO4' -> 'SBO5'` güncellendi (iki taraf aynı DLL olmalı). Host kapanış crash düzeltmesi (`022708` logu `0 crash`) doğrulandı; `icon_defer_remove` ve `SBO5` merdiven lazeri henüz oyunda test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `287.232 B` DLL Steam'de denendi (host 0 crash kapandı; client'ta `pfcan_copicon_goster` çökmesi ve merdiven giriş lazeri yakalandı); yeni `288.256 B` DLL henüz denenmedi.
Varsayımlar:
- İki tarafın da yeni `'SBO5'` destekli `288.256 B` DLL'ini kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Yeni `288.256 B` DLL ile Steam'de Hangar + Deve Boss fight testi.


## 2026-11-20 · Gemini 4 Neon (Medium) · Yalnız toplanabilir robot çöpüne (YANIK_COP.MDL) kırmızı ikon ve vakum bağlama + 02:27/02:29 tam log doğrulaması
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `local/dump/code/decomp.c` (`cop_robot_fizik` @ L133848) doğrulaması: toplanabilir robot çöpü ve `metal_icon.tga` yalnız `YANIK_COP.MDL` içindir; `COP_GBR_*`, `COP_ZBD_*` ve `COP_ROBOT_PARCA.MDL` toplanamayan dekoratif patlama parçalarıdır.
- `local/src/probe/probe.c` (`is_trash_type`, `is_debris_type`, `is_world_type`): `is_trash_type` yalnız `YANIK_COP.MDL` ile sınırlandı; `COP_GBR_*`/`COP_ZBD_*`/`COP_ROBOT_PARCA.MDL` `is_debris_type` olarak dekoratif çizilir ama `metal_icon.tga`/vakum almaz.
- `022708` / `022908` tam log sağlık kontrolü: `0 fault`, `0 failed`, `0 simdrop`, `0 late_old`, `0` host crash; `[interp]` `delay=80–109ms` ve `clk_resets=1–2` ile çok stabil.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata / sıfır yeni uyarı ile derlendi (`local/build/dinput8.dll`, `288.256 B`, SHA-256 `69A93DE3...0122`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol değişmedi (`SBO5`). Oyunda henüz test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır
Varsayımlar:
- İki tarafın da yeni `288.256 B` (`69A93DE3...0122`) DLL'ini kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Yeni `288.256 B` DLL ile Steam'de Hangar + Deve Boss fight testi.


## 2026-11-20 · Gemini 4 Neon (Medium) · 02:45/02:51 Steam testi analizi (0 crash doğrulandı) + SBO6 (AIF_Hangar_Load, G_HANGAR_STAGE lazer hasarı) + Client Ana Menüye Dön düzeltmesi
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `024343` (host), `024519` (client `sub=8`), `025133 (1)` (client direkt `sub=9`) logları incelendi:
  - **Doğrulananlar:** Hem host hem client **`0 crash`** (`pfcan_copicon_goster` deferred remove ve `devicereset` `ret` doğrulandı); direkt `sub=9` girişinde 4 pil ortak hasar (`160 -> 156 -> 150 -> 146 -> 140`, `160 -> 1`), `YANIK_COP.MDL` temiz ikon, `enrj_paket.mdl` sağlık paketi doğrulandı.
- `local/src/probe/probe.c`:
  - **Client "Ana Menüye Dön" düzeltmesi (`world_client_sync_counters`):** `G_MUSIC_ALLOW` (`g_07f9f7c9`) oyunun bölümden çıkış/menüye dön bayrağı olduğu için client'ta her karede `1` ile ezilmesi kaldırıldı.
  - **Görünmez lazer hasarı + Client'ta Deve Boss & 4 Pil başlatma (`BOSS_MAGIC` `'SBO5' -> 'SBO6'`, `G_HANGAR_STAGE`, `boss_rx`, `boss_tick`):** `g_07faa36d` (`+0x26a328`) host'tan client'a senkronlandı (lazer hasarını kapatır); host `sub_lvl >= 9` / `pil_hp[0] > 0` olduğunda client'ta piller hâlâ `0` ise tek seferlik `AIF_Hangar_Load` (`engine_callscript4`) tetiklenir (4 pil + Deve AI/animasyon/çarpışma doğal başlar).
Doğrulama: `local/src/probe/build.bat` ile sıfır hata / sıfır yeni uyarı ile derlendi (`local/build/dinput8.dll`, `289.792 B`, SHA-256 `ED7DE9D7...5559`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol `'SBO5' -> 'SBO6'` güncellendi (iki taraf aynı DLL olmalı). Yeni `SBO6` ve menüye dönüş düzeltmesi oyunda henüz test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `288.256 B` DLL Steam'de denendi (0 crash doğrulandı; görünmez lazer hasarı, `sub=8->9` boss başlamaması ve Ana Menüye Dön sorunu yakalandı); yeni `289.792 B` DLL henüz denenmedi.
Varsayımlar:
- İki tarafın da yeni `'SBO6'` destekli `289.792 B` DLL'ini kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Yeni `289.792 B` DLL ile Steam'de Hangar `sub=8 -> 9` geçişi, 4 batarya, Deve Boss fight ve Ana Menüye Dön doğrulaması.


## 2026-11-20 · Gemini 4 Neon (Medium) · 02:58/03:00 Steam testi analizi + SBO7 (gerçek Deve HP G_DEVE_HP, batarya 160->0 & çift boss koruması, D.E.V.E barında HUD kaydırma)
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `local/testinstall/SuperCan1/Game/coop_probe_025830_6120.log` (host) ve `local/coop_probe_030004_19960.log` (client) + ekran görüntüleri incelendi:
  - **Doğrulananlar:** 0 crash, `G_HANGAR_STAGE` görünmez lazer hasarı kalktı, `sub=8 -> 9` geçişinde client'ta boss ve piller başladı, QTE kilidi (`combo_busy`) ve `DEVE_laser_atak` (`laser_seq`) çalıştı.
  - **Bulunan 4 sorun ve kök nedenleri:**
    1. **Bataryaların `160 -> 0` sıfırlanması:** `AIF_Hangar_Start` `sub=9` yapınca `nesne_pil_loop` pilleri `160.0` yapmadan ~2 sn önce client `AIF_Hangar_Load` tetikliyor, host'tan gelen başlatılmamış `pils=(0,0,0,0)` paketi `0 < 160` diyerek tüm pilleri iki tarafta sıfırlıyordu.
    2. **Çift / görünmez Deve Boss:** Client'ta zaten `AIF_DEVE_*` görevi varken `AIF_Hangar_Load` tekrar tetiklenebiliyordu.
    3. **Deve Boss canının (`500`) senkronlanmaması:** `G_DEVE_BOX->skill[35]` sabit `500.0` kalıyor; `Oyun_panel_update` ve `AIF_DEVE_combo_kap` (`decomp.c:176635`, `-18<<10`) gerçek boss canı olarak **`g_07fa467d`** (`ebx + 0x264638`, `G_DEVE_HP`) kullanıyor.
    4. **Sol alt Co-op HUD'ın `D.E.V.E` boss can barının solunu kapatması.**
- `local/src/probe/probe.c`:
  - `BOSS_MAGIC` `'SBO6' -> 'SBO7'`, `G_DEVE_HP(ebx)` (`+0x264638`):
  - `boss_rx` & `boss_tick`: Gerçek boss canı `G_DEVE_HP(ebx)` üzerinden iki yönlü senkronlandı (`cur_deve > 0` korumalı); başlatılmamış `p.pil_hp[i] == 0 && p.pil_dead <= 0` paketlerinin `160.0` pilleri sıfırlaması engellendi; `AIF_Hangar_Load` yalnız `p.pil_hp[0] > 0` iken ve aktif `AIF_Hangar_Load`/`AIF_DEVE_kos`/`AIF_DEVE_bekle`/`AIF_DEVE_sevin` yokken tetiklenir; QTE sırasında (`!g_boss_last_combo`) client konumu zorla çekilmez.
  - `hud_tick`: `tunel.wmb` `sub >= 9` iken sol alt takım kartı `py = bh - ph - 92.0f` ile `D.E.V.E` boss barının üstüne kaydırıldı.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata / sıfır yeni uyarı ile derlendi (`local/build/dinput8.dll`, `290.304 B`, SHA-256 `1BACEF77E4F17CC0C7EEEDA31DF2D54B65B41F1D2D320C679497723039A0838C`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol `'SBO6' -> 'SBO7'` güncellendi (iki taraf aynı DLL olmalı). Yeni `SBO7` ve HUD kaydırma henüz oyunda test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `289.792 B` (`SBO6`) DLL Steam'de denendi; yeni `290.304 B` (`SBO7`) DLL henüz denenmedi.
Varsayımlar:
- İki tarafın da yeni `'SBO7'` destekli `290.304 B` DLL'ini kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Yeni `290.304 B` (`SBO7`) DLL ile Steam'de Deve Boss fight (4 batarya `160 -> 0` olmadan, tek boss, ortak `G_DEVE_HP` barı, yukarı kaymış HUD) testi.


## 2026-11-20 · Gemini 4 Neon (Medium) · D.E.V.E. Boss Fight tam decompile analizi ve Tek Otoriteli (Host-Driven) mimari (SBO8)
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `local/testinstall/SuperCan1/Game/coop_probe_031803_14708.log` ve `local/coop_probe_030004_19960.log` incelendi; ardından `local/dump/code/decomp.c` içindeki tüm Hangar/Deve/Pil/QTE/Lazer fonksiyonları (`AIF_Hangar_Start`, `AIF_Hangar_Load`, `AIF_DEVE_fizik`, `AIF_DEVE_govde..nesne_alt_laser`, `nesne_pil_box0/loop/yaralan/patla`, `AIF_DEVE_sevin/kos/bekle/geri/doldur/ziplageri/ziplasevin/ziplasaldir/ziplabekle`, `AIF_DEVE_combo_kap`, `DEVE_laser_atak`, `EP_deve_panel_ac/kapa/upd`) çözümlendi.
- **Kök nedenler ("2 farklı deve" + senkron kopuklukları):**
  1. Client'ta `AIF_Hangar_Load` / `AIF_Hangar_Start` bağımsız bir `AIF_DEVE_*` AI zinciri başlatıp host'tan farklı yere koşuyor/zıplıyor, `boss_rx` 15 Hz'de `G_DEVE_GOVDE`'yi çekince 1 kare sonra client AI geri çekiyordu.
  2. `G_DEVE_BOX` (`g_07d5a26d`) PhysX aktörü değil (düz Acknex entity); `pXent_setposition` sessizce başarısız olup konum güncellemiyordu.
  3. Deve'nin yön açısı `de->pan` değil `g_07d5a275` (`G_DEVE_PAN`) globali.
  4. `boss_tick`'te pil canı `0 -> 160` geçişinde `g_boss_last_pil[i]` güncellenmediği için pil hasarları `chg = 1` tetiklemiyordu.
  5. 1. düşüş sonrası `G_DEVE_PHASE2` (`g_07d5a271`) `0 -> 1` ve `G_DEVE_HP` `1 -> 201` geçişi + `G_DEVE_GOVDE` animasyonu senkronlanmıyordu.
- `local/src/probe/probe.c`:
  - `d_ent_animate`: Host'ta `G_DEVE_GOVDE` üzerinde çağrılan son animasyon (`g_boss_anim`, `pct`, `mode`) kaydedildi.
  - `G_DEVE_PHASE2`, `G_DEVE_PAN`, `G_DEVE_ISARET`, `G_LSR_ISIN1`, `G_LSR_SAP1`, `G_LSR_SAP2`, `G_PIL_CAM` makroları eklendi.
  - `BOSS_MAGIC` `'SBO7' -> 'SBO8'`, `stub_deve_sevin`, `stub_deve_ziplageri`, `boss_ai_hooks_install` (`hook_engine_frame` kare 100): Client'ta `AIF_DEVE_sevin` ve `AIF_DEVE_ziplageri` PIC dönüş adresini koruyan naked kancalarla erken döndürülür (2. bağımsız Deve AI imkânsız; tek oyunculu ve host normal çalışır).
  - `boss_client_apply_mirror`, `boss_rx`, `boss_tick`: Host 30 Hz Deve kutusu/modeli konumu + `G_DEVE_PAN` + `G_DEVE_GOVDE` animasyonu + `G_DEVE_ISARET` + üst lazer rayı + `G_DEVE_PHASE2`/`G_DEVE_HP` gönderir; client her kare doğrudan struct alanlarına (`pXent_setposition` olmadan) ve `t_ent_animate` ile aynalar; `AIF_Hangar_Load` yerine yalnız 4 pil için `nesne_pil_loop(box, cam)` başlatılır; 4 pil kırılınca client'ta `EP_deve_panel_ac()`, boss ölünce (`stage=3`) `EP_deve_panel_kapa()` çağrılır; Faz 2'de `G_DEVE_ISARET` görünürken client da yaklaşıp QTE tuşu (`ack+0x225864`) ile `AIF_DEVE_combo_kap()` başlatabilir.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata / sıfır yeni uyarı ile derlendi (`local/build/dinput8.dll`, `292.864 B`, SHA-256 `F8F0E93D...1CE2`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol `'SBO7' -> 'SBO8'` güncellendi (iki taraf aynı DLL olmalı). Oyun içi Steam testi henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `SBO7` DLL Steam'de denendi ("2 farklı deve" bildirildi); yeni `SBO8` (`292.864 B`) DLL henüz denenmedi.
Varsayımlar:
- İki tarafın da yeni `'SBO8'` destekli `292.864 B` DLL'ini kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Yeni `SBO8` (`292.864 B`) DLL ile Steam'de Deve Boss fight (tek senkron Deve + animasyon + 4 pil + Faz 2 `EP_deve_panel_ac` + QTE + 1. düşüş `1 -> 201` + ölüm) testi.


## 2026-11-20 · Gemini 4 Neon (Medium) · SBO8 bölünmüş ekran testi analizi ve 4 kritik düzeltme (SBO9: ANM_ADD ayrımı, pil anında patlatma s39, Deve client hedefleme FISH.MDL+BOSS_FIRE, Hangar karakter ışığı)
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `121839_15348` (host) ve `121849_3692` (client) logları + ekran görüntüsü incelendi: tek Deve ve 0 crash doğrulandı, 4 kök neden bulundu ve `local/src/probe/probe.c`'de düzeltildi:
  1. **Client Deve modelinin katlanması (`d_ent_animate`, `BOSS_MAGIC` `'SBO8' -> 'SBO9'`, `boss_client_apply_mirror`):** `AIF_DEVE_bekle/kos` temel tam gövde animasyonu üstüne `ANM_ADD` (`2<<10`) oynattığı için sadece son `ANM_ADD` çağrısını oynatmak kemikleri her kare katlıyordu. Artık temel (`!(m & (2<<10))`) ve `ANM_ADD` katmanı ayrı gönderilip sırayla oynatılıyor (`ge->tilt = 0; ge->roll = 0;`). Ayrıca `is_debris_type`'a `BOSS_FIRE_GLOW.TGA` ve `BOSS_FIRE_ALEV+32.TGA` eklenerek ateş toplarının client'ta da görünmesi sağlandı.
  2. **Client pil patlatınca hostta son 1 vuruş gerekmesi (`boss_rx`):** `nesne_pil_loop` (`decomp.c:170418`) `skill[38]` (`s39`) değişmeden hasarı yok sayıyordu; `p.pil_hp[i] <= 0` iken `sk[36] = 0; sk[38] += (1 << 10);` eklendi (host vurmadan anında patlar).
  3. **Deve'nin client'a saldırmaması (`aim_host_tick`, `coop_tick`):** `AIF_DEVE_*` AI zincirinin `me` varlığı olan `FISH.MDL` (`AIF_Hangar_dummy1/2`, `x < -1500`, `G_HANGAR_STAGE >= 2`) ve ateş topu `BOSS_FIRE_GLOW.TGA` `g_aim[]` tablosuna eklendi → Deve client'a da döner, ateş eder, zıplar ve ateş topları `g_aim_pup` üstünden client'a hasar verir.
  4. **Client Hangar karakter ışıklandırması (`G_LIGHT_POS`, `G_LIGHT_RNG`, `boss_rx`):** `AIF_Hangar_Start` (`decomp.c:184959`) ile birebir `0x7f9f751 = (-506, -1840, 1019)<<10` ve `g_07f9f771 = 300<<10` client'a yazıldı.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata / sıfır yeni uyarı ile derlendi (`local/build/dinput8.dll`) ve `coop_paket/`, `steamcoop_paket/`, `testinstall/SuperCan1/Game`, `testinstall/SuperCan1b/Game` içine kopyalandı. Protokol `'SBO8' -> 'SBO9'` güncellendi (iki taraf aynı DLL olmalı). Yeni `SBO9` DLL oyunda henüz test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `SBO8` DLL bölünmüş ekranda denendi (tek Deve doğrulandı; yukarıdaki 4 sorun yakalandı); yeni `SBO9` DLL henüz denenmedi.
Varsayımlar:
- İki tarafın da yeni `'SBO9'` DLL'ini kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Yeni `SBO9` DLL ile bölünmüş ekranda 4 düzeltmenin (Deve modeli/ateş topları, pil anında patlama, Deve'nin client'a saldırması, client karakter ışığı) doğrulanması.


## 2026-11-20 · Gemini 4 Neon (Medium) · SBO9 bölünmüş ekran testi analizi ve 5 kesin düzeltme (cubemap skin1 ışıklandırma, S simgesi, host QTE kilidi+animasyon, FISH.MDL kesin konumu, BRIGHT/TRANSLUCENT ateş topları)
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `123751_1416` (host) ve `123800_15808` (client) logları incelendi:
  - **Doğrulananlar:** 4 pil anında patlatma (`dead=4`, hostta son vuruş gerekmedi!), Deve modeli/animasyonu düzeldi, client QTE + ortak `G_DEVE_HP` (`500 -> 212`) çalıştı, 0 crash.
  - **Bulunan ve düzeltilen 5 kök neden (`local/src/probe/probe.c`):**
    1. **Tüm bölümlerde client karakter ışıklandırmasının koyu olması (`coop_tick`):** `[gfx-char]` logu `SUPERCAN_GOVDE.MDL` üzerinde host'ta `sk1=07D1A059`, client'ta `sk1=00000000` (NULL cubemap!) olduğunu kesinleştirdi. Artık `SUPERCAN_GOVDE/CANTA/SWORD` materyallerinde `skin1 == NULL` görülünce `0x7fcd231[0]` (`ebx + 0x28d1ec`) env cubemap'i atanıp `bmap_to_cubemap` çağrılıyor (`[gfx-fix]` logu).
    2. **Client'ta QTE "S" tuş simgesi (`boss_tick`):** Faz 2'de `G_DEVE_ISARET` görünürken ve client mesafedeyken oyunun `S` simge paneli (`ebx + 0x0105e8 + 28`, bit `16384` VISIBLE) açılıp kapanır.
    3. **Client QTE oynarken host'ta Deve'nin zıplamaya devam etmesi (`stub_deve_ziplageri`, `boss_rx`, `boss_tick`):** Host'ta `g_boss_peer_combo == 1` iken `AIF_DEVE_ziplageri` bekletilir (`g_boss_host_deferred_zgeri = 1`), client QTE sırasında 30 Hz Deve animasyonu gönderir ve host bunu oynatır; client QTE bitince host `AIF_DEVE_ziplageri(0)` ile savaşa devam eder.
    4. **Deve'nin hâlâ sadece host'a odaklanması (`coop_tick`):** Haritadaki `alev_bas` `FISH.MDL` varlıkları `g_boss_dummy_ent`'i eziyordu; tam `AIF_Hangar_dummy1/2` konumu `(-2000, 0, 500)` filtresi eklendi.
    5. **Client'ta Deve ateş toplarının koyu görünmesi (`world_client_render`):** `else` koluna `TRANSLUCENT | BRIGHT | OVERLAY | UNLIT` bayrak kopyalaması eklendi.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata / sıfır yeni uyarı ile derlendi (`local/build/dinput8.dll`). Protokol değişmedi (`SBO9`). Yeni düzeltmeler oyunda henüz test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `SBO9` DLL bölünmüş ekranda denendi (4 pil anında patlatma + Deve modeli + QTE HP senkronu doğrulandı; yukarıdaki 5 sorun yakalandı); bu yeni derleme henüz denenmedi.
Varsayımlar:
- yok (tüm ofsetler `decomp.c` ve `[gfx-char]` loguyla doğrulandı)
Kalan sorunlar / yarım kalanlar:
- Bu yeni `SBO9` DLL ile bölünmüş ekranda 5 düzeltmenin (özellikle `[gfx-fix]` cubemap ışıklandırma + client QTE + Deve'nin client'a saldırması) doğrulanması.


## 2026-11-20 · Gemini 4 Neon (Medium) · SBO9 boss ölümü + 1->201 koruması + Faz 2 zıplama hedefleme + ateş topu bayrakları + supercan_tunel_mtrl yenileme
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `133624_6448` (host) ve `133633_17080` (client) logları incelendi:
  - **Doğrulananlar:** Deve artık client'a da ateş etti, havada kalan client QTE animasyonu çözüldü, 4 pil + ortak QTE canı + boss ölüm animasyonuna kadar gelindi, 0 crash.
  - **Düzeltilen 4 sorun (`local/src/probe/probe.c`):**
    1. **Boss öldükten sonra savaşmaya devam etmesi & 1->201 geri ezilmesi (`boss_rx`, `boss_tick`):** QTE yapan tarafın (`g_boss_last_combo == 1`) `G_DEVE_HP`'si dışarıdan gelen eski paketle geri ezilmez; izleyen taraf aktif QTE paketindeki `deve_hp`'yi (`1 -> 201` dahil) doğrudan alır; `p.hangar_stage >= 3` (boss ölümü) artık **iki yönlü** işlenir (client bossu öldürünce host da `G_HANGAR_STAGE = 3`, `PASSABLE`, `EP_deve_panel_kapa`, `AIF_DEVE_combo_bns` uygular ve bekletilen `AIF_DEVE_ziplageri`'yi iptal eder).
    2. **Faz 2 zıplamanın sadece host'a zıplaması (`boss_call_script_me`, `boss_rx`):** Host client QTE sonrası `AIF_DEVE_ziplageri`'yi `me = g_boss_dummy_ent` (`FISH.MDL`) ile devam ettirir → `ziplasaldir` da en yakın oyuncuya (client dahil) zıplar.
    3. **Deve ateş toplarının koyu kalması (`world_client_render`):** `toportak_DEVE` (`decomp.c:171595`) ile birebir `1 | 131072 | 1048576 | 2097152` bayrakları `vmask`'a eklendi.
    4. **Client karakter ışıklandırması (`coop_tick`):** `[gfx-fix]` `skin1` onarımı sonrası `tunel.wmb`'de `g_07faa159 = 1<<10` + `supercan_tunel_mtrl(0)` çağrısı eklendi.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata / sıfır yeni uyarı ile derlendi (`local/build/dinput8.dll`, `296.448 B`, SHA-256 `74208A8E...15CB`) ve **4 klasöre de** (`coop_paket`, `steamcoop_paket`, `SuperCan1/Game`, `SuperCan1b/Game`) kopyalandı. Protokol değişmedi (`SBO9`). Yeni derleme oyunda henüz test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `FB43AE56` DLL denendi (client'a ateş + havada kalan QTE çözüldü; yukarıdaki 4 nokta yakalandı); bu yeni `74208A8E` DLL henüz denenmedi.
Varsayımlar:
- yok
Kalan sorunlar / yarım kalanlar:
- Yeni `74208A8E` DLL ile boss ölümünün kesinleşmesi, Faz 2 client zıplaması ve ateş topu/karakter ışıklandırmasının doğrulanması.

## 2026-11-20 — [gemini] Client karakter ışıklandırma kök sebep çözümü + akıcı Deve animasyonu + opsiyonel pencere modu
- **Kök sebep analizi (`local/dump/code/decomp.c` + loglar):**
  - Tüm bölümlerde (`orman.wmb`, `tunel.wmb` vb.) bölüm açılışında çalışan `level_metarials_apply()` oyundaki tek `ent_for_file()` çağrıcısıdır (satır 34021, 34238). `File_mat_<level>` dosyasından her modelin `skill[48..93]` değerlerini okuyup `BR_material_load()` çağırır.
  - Client haritaya girdiğinde host zaten aynı haritada olduğu için `probe.c`, `level_metarials_apply()` henüz bitmeden `g_puppet[0..7]` (`supercan_govde/yuz/sac/canta/sword/flora/saat/boru.mdl`) ve dünya kuklalarını `ent_create` ile oluşturuyordu.
  - `ent_for_file("supercan_*.mdl")` gerçek yerel karakter yerine kuklayı döndürdüğünden, client'ın kendi karakter parçalarına `BR_material_load()` hiç uygulanmıyor (koyu/gölgeli karakter, kılıç parlaması ve cubemap yokluğu).
- **Yapılan değişiklikler (`local/src/probe/probe.c`, `local/coop_paket/coop.ini`, `local/steamcoop_paket/coop.ini`):**
  1. `level_materials_ready()` eklendi: `proc_status(level_metarials_apply) == 0` ve `SUPERCAN_GOVDE.MDL` materyal/skill (`skill[48/64/93]`) yüklenene kadar (en fazla 90 kare güvenlik sınırı) oyuncu (`g_puppet`) ve dünya (`world_client_render`) kukla oluşturması ertelenir; `world_tick` imzası `mats_ready` parametresi alacak şekilde güncellendi (client düşman spawn engeli anında aktif kalır).
  2. `boss_rx` + `boss_client_apply_mirror` (`boss_lerp_var`, `boss_lerp_ang`, `boss_lerp_pct`): Client'ta 30 Hz gelen `BOSSPKT` paketleri arasında Deve kutusu/gövde konumu, en kısa açıdan yön (`deve_pan`), temel ve `ANM_ADD` animasyon yüzdeleri (döngü sarımı dahil) ve üst lazer rayı (`lsr_sap1_y`) yumuşakça interpole edilir. Protokol `SBO9` değişmedi.
  3. `force_windowed` + `COOP_WINDOW`/`window=` ayrıştırması: Zorunlu pencere modu varsayılan olarak kapalı (`g_force_windowed = 0`); sadece `window=full` veya `window=x,y,w,h` verilirse D3D9 `Windowed = TRUE` yapılır. `window=off` / `0` / `fullscreen` veya boş bırakılınca oyunun kendi tam ekran ve çözünürlük ayarına hiç dokunulmaz. Paket `coop.ini` dosyaları `window=off` yapıldı.
- **Derleme ve dağıtım:** `local/src/probe/build.bat` 0 hata ile derlendi → `local/build/dinput8.dll` (`299.520 B`, SHA-256 `99624B66...4986`), 4 hedefe (`coop_paket`, `steamcoop_paket`, `SuperCan1/Game`, `SuperCan1b/Game`) kopyalandı.
- **Durum:** Tek PC iki kopya testinde doğrulandı (`coop_probe_140620_5152.log` — `[gfx] level 'tunel.wmb' materials ready at frame +5`, host ve client karakter/kılıç/çanta ışıklandırması birebir aynı, Deve akıcı).


## 2026-11-20 · Gemini 4 Neon (Medium) · Orman gece/karanlık analizi ve Steam paketleri window=full güncellemesi
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `SuperCan 2026-11-20 14-16-55.bmp` + `coop_probe_141613_3248.log` + `local/dump/code/decomp.c` incelendi:
  - `oyuncu_ga.gen` hash'inin orijinalle birebir aynı olduğu (`6EE53C94...`) doğrulandı.
  - `decomp.c` içinde orman bölümü yüklenirken oyunun 10 elemanlı gün zamanı tablosu (`0x7fa4629 = [0, 0, 1, 0, 0, 1, 0, 2, 0, 1]`, sayaç `g_07fa4655`, tema `g_07fa2439`: `0=gündüz, 1=akşam, 2=gece`) kullandığı tespit edildi.
- `local/testinstall/SuperCan1/Game/coop.ini`, `local/testinstall/SuperCan1/Game/coop.ini.steam`, `local/steamcoop_paket/coop.ini`, `local/coop_paket/coop.ini`: `window=off` → `window=full` olarak güncellendi. `dinput8.dll` (`299.520 B`, SHA-256 `99624B66...4986`) tüm paketlerde birebir aynı doğrulandı.
Doğrulama: `probe.c` değiştirilmedi (derleme gerekmedi); kullanıcı `SuperCan1`'de `window=full` ile grafiklerin düzeldiğini doğruladı.
Kullanıcı/arkadaş denedi mi: evet — kullanıcı `window=full` ile düzeldiğini bildirdi; arkadaşla Steam testi şimdi yapılacak.
Varsayımlar:
- yok
Kalan sorunlar / yarım kalanlar:
- İleride `g_07fa2439` (gün zamanı) host→client senkronu ve `window=off` modunda da `h_wndproc` kapanış korumasının aktif tutulması düşünülebilir.


## 2026-11-20 · Gemini 4 Neon (Medium) · Steam testi 3 bulgu çözümü (SB10) + Steam Lobi/F8 Davet Paneli + Özel Co-op Başlatıcı (ohc) + Modülerleştirme & SC2 Hazırlığı
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor; önceki toplu commit `1836d6e`)
Yapılan:
- **1) Steam testi (`144551` host / `144524` client) 3 bulgunun kesin çözümü (`SB10`):**
  - **Client "Ana Menü" tuşu kilidi (`sc1/sc1_world_combat.inc`, `world_client_sync_counters`):** ESC → "Ana Menü"ye basınca oyun `G_MUSIC_ALLOW = 0` yapıp `while (G_ROBOT_ALIVE > 0) wait(1)` (`decomp.c:51412..51420`) bekliyordu; artık `G_GAME_ACTIVE == 0 || G_MUSIC_ALLOW == 0` iken `G_ROBOT_ALIVE = 0` tutuluyor.
  - **Client Deve lazer saldırısı görünmez hasar (`sc1/sc1_boss.inc`, `BOSS_MAGIC` `'SBO9' -> 'SB10'`, `G_LSR_TIMER` `+0x26a334`):** `G_LSR_SAP2->y` (`lsr_sap2_y`) pakete eklendi; client'ta `G_LSR_TIMER = 0` tutularak bağımsız/hayalet lazer engellendi; `DEVE_laser_atak` başlamadan hemen önce `G_LSR_SAP2/SAP1/ISIN1` host'un gerçek konumuna hizalanıyor ve lazer aktifken `boss_client_apply_mirror` ışın Y'sini ezmiyor.
  - **Boss ölümü (`stage=3`) sonrası ölü Deve'nin ayağa kalkması & son ara sahne (`sc1/sc1_boss.inc`, `boss_client_apply_mirror`, `boss_rx`, `boss_tick`):** `stage >= 3` iken `boss_client_apply_mirror` durur; `p.hangar_stage >= 3` alınınca `"olum"` animasyonu son karesi oynatılır; client'ta `nesne_lsr_ustengel` başlatılarak çıkış kapısından (`y > 650`) geçildiğinde son ara sahne (`Oyun_video_ac` + `Oyun_sonmenu_ac`) doğal tetiklenir.
  - **Kapanış koruması (`core/core_d3d_input.inc`, `place_window`):** `h_wndproc` subclass'ı artık `window=off` modunda da aktif.
- **2) Steam Lobi + Oyun İçi Arkadaş Davet Paneli (`core/core_net.inc`, `core/core_hud.inc`):**
  - `ISteamMatchmaking` (14 flat API export'u `steam_api.dll`'de doğrulandı) + `SetRichPresence` + ManualDispatch callback'leri (`703`, `513`, `504`, `506`, `333`, `337`) eklendi; `+connect_lobby <id>` ve `coop.ini` `steam_lobby=` desteği eklendi.
  - Oyun içi `F8` Steam Lobi & Davet Paneli (Acknex native): otomatik Friends-Only lobi kurulumu, çevrimiçi arkadaş listesi (`1..8` ile `InviteUserToLobby`), `[R]` yenile, `[K]` Lobi ID panoya kopyala, `[C]` kur, `[L]` ayrıl.
- **3) Özel Co-op Başlatıcı + Hata 22 (`ohc`) Çözümü:**
  - `decomp.c:220962..221005` içinden `ohc` formülü çıkarılıp gerçek log değeri (`18370` @ `2026-11-20 14:16`) ile birebir doğrulandı.
  - `local/coop_paket/SuperCan_Coop_Baslatici.ps1` + `SuperCan_Coop_Oyna.bat` oluşturuldu (`steamcoop_paket/` ve `testinstall/SuperCan1/Game/` içine kopyalandı; PS1 sözdizimi doğrulandı).
- **4) Modülerleştirme & SüperCan 2 Hazırlığı:**
  - `local/src/probe/probe.c` unity-build yapısıyla `core/` (`core_acknex.inc`, `core_net.inc`, `core_hud.inc`, `core_d3d_input.inc`) ve `sc1/` (`sc1_offsets.h`, `sc1_world_combat.inc`, `sc1_boss.inc`, `sc1_revive.inc`, `sc1_sync_extra.inc`) modüllerine bölündü.
  - `gamefiles/SuperCan2/Game/` salt-okunur incelendi (15/15 Acknex export'u SC1 ile aynı); `docs/supercan2-hazirlik.md` yazıldı.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata / sıfır yeni uyarı ile derlendi (`local/build/dinput8.dll`, `309.760 B`, SHA-256 `0BCA103B...F212`) ve 4 hedefe (`coop_paket`, `steamcoop_paket`, `SuperCan1/Game`, `SuperCan1b/Game`) kopyalandı. Protokol `'SBO9' -> 'SB10'` güncellendi (iki taraf aynı DLL olmalı). Yeni DLL, `F8` Steam Lobi paneli ve Özel Co-op Başlatıcı henüz oyunda test edilmedi (Test edilmedi).
Kullanıcı/arkadaş denedi mi: hayır (kullanıcı 1 saatlik aradayken hazırlandı).
Varsayımlar:
- İki tarafın da yeni `'SB10'` destekli `309.760 B` DLL'ini kullanacağı.
Kalan sorunlar / yarım kalanlar:
- Yeni `309.760 B` (`SB10` + `F8` Steam Lobi) DLL'inin ve `SuperCan_Coop_Oyna.bat` başlatıcısının iki PC'de Steam üzerinden test edilmesi.


## 2026-11-20 · Gemini 4 Neon (Medium) · Steam P2P SessionFailed -> EResult 35 (bozuk oturum kilidi) düzeltmesi
İnceleme: bekliyor
Commit'ler: yok (kullanıcı toplu commit istiyor)
Yapılan:
- `local/testinstall/SuperCan1/Game/coop_probe_171707_8012.log` incelendi:
  - Steam Lobi kurma, `InviteUserToLobby`, `GameLobbyJoinRequested` ve `LobbyChatUpdate` (`members=2`, `locked peer`) **sorunsuz çalıştı**.
  - **Kök neden:** Lobiye girmeden önce arkadaş henüz hazır değilken gönderilen ilk keşif paketinde Steam `SessionFailed` (`cb=1252`) döndürdü; ardından `CloseSessionWithUser` çağrılmadığı ve `AutoRestartBrokenSession` bayrağı verilmediği için tüm sonraki `SendMessageToUser` çağrıları kalıcı olarak `EResult 35` (`k_EResultConnectFailed`) döndürüp paketleri reddetti (`rx=0`).
- `local/src/probe/core/core_net.inc`:
  - `ST_SEND_UNRELIABLE_NONAGLE` bayrağı `(1 | 32)` (`NoNagle | AutoRestartBrokenSession`) yapıldı.
  - `st_Close` (`SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser`) yüklendi (`steam_api.dll` içinde doğrulandı).
  - `steam_handle_cb` (`cb == 1252`, `SessionFailed`): bozuk oturum `st_Close` ile anında kapatılır.
  - `st_send_to`: `r == 35` dönerse `st_Close` çağırıp hemen yeniden dener; hata loglaması ilk 20 hata + her 300. hata olarak güncellendi.
  - `steam_lobby_sync_peer`: lobi üyesi `g_st_peer` olarak kilitlendiğinde `st_Accept` ile oturum önceden kabul edilir.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata ile derlendi (`local/build/dinput8.dll`, `310.272 B`, SHA-256 `F3E01061...ADC5`), 4 hedefe (`coop_paket`, `steamcoop_paket`, `SuperCan1/Game`, `SuperCan1b/Game`) kopyalandı ve `local/SuperCan_Coop_Steam_Paket.zip` (`309.174 B`) yenilendi. Protokol değişmedi. İki PC Steam testi henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `309.760 B` DLL Steam'de denendi (lobi/davet çalıştı, `EResult 35` nedeniyle "arkadaş bekleniyor"da kaldı); yeni `310.272 B` DLL henüz denenmedi.
Varsayımlar:
- İki tarafın da bu yeni `310.272 B` `dinput8.dll`'ini kullanması gerekir (iki tarafta da bozuk oturum kilidini engellemek için).
Kalan sorunlar / yarım kalanlar:
- Yeni `310.272 B` DLL ile iki PC'de Steam lobi + davet testi.


## 2026-11-20 · Gemini 4 Neon (Medium) · Client "Ana Menü" kilidi kesin çözümü (host farklı bölümdeyken G_ROBOT_ALIVE) + Steam Lobi davetsiz bağlanma engeli
İnceleme: bekliyor
Commit'ler: yok (önceki commit `5a9b29c`)
Yapılan:
- `local/testinstall/SuperCan1/Game/coop_probe_172333_13412.log` + `local/dump/code/decomp.c` (`E_smart_btn_yes2`, `59046..59064`, `disfn.py` ile assembly doğrulandı) incelendi:
  - **Doğrulananlar (`5a9b29c`):** `closing broken session` ile `EResult 35` kilidi çözüldü; iki PC Steam'de Deve Boss baştan sona öldürülüp (`st=3`) 0 crash ile son ara sahneden ana menüye geçildi.
  - **Client "Ana Menü" takılma kök sebebi:** Host ana menüye (`menu.wmb`) döndüğünde client'ta `active = !strcmp(g_remote.level, out.level)` (`probe.c:164`) `0` oldu → `world_tick` içinde `if (peer_active && in_level)` bloğundaki `world_client_sync_counters()` artık çağrılmadı → client'ta önceden yazılmış `G_ROBOT_ALIVE > 0` kalıcı kaldı ve `E_smart_btn_yes2` `while (0 < G_ROBOT_ALIVE) wait(1)` döngüsünde kilitlendi.
- `local/src/probe/sc1/sc1_sync_extra.inc` (`world_tick`):
  - Client'ta `g_world_client_active` `1 -> 0` düştüğünde (host başka bölüme/menüye geçtiğinde veya bağlantı koptuğunda) eski `G_ROBOT_ALIVE` anında `0`'a sıfırlanır.
  - Ek olarak `else` kolunda (`!peer_active`) `G_GAME_ACTIVE == 0 || G_MUSIC_ALLOW == 0` görülürse `G_ROBOT_ALIVE = 0` tutulur.
- `local/src/probe/core/core_net.inc` (`steam_send`, `steam_handle_cb`) ve `local/src/probe/probe.c` (`coop_tick`):
  - Steam Lobisi kuruluyken (`g_st_lobby != 0`) eski AppID 480 arkadaş taraması kapatıldı; `SessionRequest` (`cb=1251`) ve paket kabulü yalnızca lobi üyesi (`g_st_peer`) ile sınırlandı (davet kabul edilmeden otomatik bağlanma engellendi).
- Arkadaş logları (`local/coop_probe_172508_13304.log` + `local/coop_probe_172818_32072.log`) doğrulaması & ek düzeltme (`local/src/probe/sc1/sc1_boss.inc`):
  - `172508`: `alive=4` -> `client enemy spawns allowed (solo)` -> `puppets removed (no host)` satırları Ana Menü kilidi teşhisini %100 doğruladı.
  - `172818`: Client'ta `rx boss DEFEATED (stage=3)`, `nesne_lsr_ustengel` son ara sahne ve `menu.wmb` dönüşü 0 crash ile doğrulandı; `menu.wmb`'de eski global değerlerle `boss_tick`/`boss_rx`'in tekrar tetiklenmemesi için `boss_rx` ve `boss_tick` yalnız `tunel.wmb` ile sınırlandı.
Doğrulama: `local/src/probe/build.bat` ile sıfır hata ile derlendi (`local/build/dinput8.dll`, `310.272 B`, SHA-256 `05C9B32F...06C6`), 4 hedefe (`coop_paket`, `steamcoop_paket`, `SuperCan1/Game`, `SuperCan1b/Game`) kopyalandı ve `local/SuperCan_Coop_Steam_Paket.zip` yenilendi. Protokol değişmedi (`SB10`). İki PC Steam testi henüz yapılmadı (Test edilmedi).
Kullanıcı/arkadaş denedi mi: önceki `5a9b29c` DLL Steam'de denendi (bağlantı ve boss tamam, client Ana Menü takılması ve davetsiz bağlanma bildirildi); yeni `7C57B96E` DLL henüz denenmedi.
Varsayımlar:
- yok (tüm ofsetler `disfn.py E_smart_btn_yes2` ile doğrulandı).
Kalan sorunlar / yarım kalanlar:
- Yeni `7C57B96E` DLL'in iki PC Steam'de (host önce ana menüye döndükten sonra client'ın ESC → Ana Menü yapması + `F8` davet kabul edilmeden bağlanmaması) doğrulanması.


## 2026-11-20 · Gemini 4 Neon (Medium) · GitHub Public Yayın Planı (onaylandı) ve HANDOFF güncellemesi
İnceleme: bekliyor
Commit'ler: `5a9b29c..0d522d0` (önceki kod değişiklikleri push'landı; HANDOFF güncellemesi commit'siz)
Yapılan:
- GitHub Public Yayın Planı (`v1.0.0`) hazırlandı ve kullanıcı tarafından onaylandı: **tek temiz başlangıç commit'i (`orphan` branch)** + **MIT Lisansı** + PII/yol temizliği + `LICENSE`/`THIRD_PARTY_LICENSES.md`/`CHANGELOG.md`/`.github/` + profesyonel `README.md` + `KALDIR.bat`/`LOG_TOPLA.bat`/`build_release.ps1`.
- `local/HANDOFF.md`: Son oturum durumu (`0d522d0`), çözülen 2 yeni kalıcı tuzak (`SessionFailed -> EResult 35`, host farklı bölümdeyken client `G_ROBOT_ALIVE` kilidi) ve `/clear` sonrası doğrudan uygulanacak GitHub Public Yayın adımları eksiksiz güncellendi.
Doğrulama: yalnızca belge/handoff güncellemesi (derleme gerekmedi).
Kullanıcı/arkadaş denedi mi: hayır (yalnızca plan onayı ve handoff hazırlığı).
Varsayımlar:
- yok
Kalan sorunlar / yarım kalanlar:
- `/clear` sonrası yeni oturumda `local/HANDOFF.md` "Sıradaki adım" 1. maddesindeki GitHub Public Yayın (`v1.0.0`) adımlarının uygulanması.


## 2026-11-20 · Gemini 4 Neon (Medium) · Klasör yeniden düzenleme + GitHub Public Yayın Hazırlığı (v1.0.0, 1.1–1.4 + sandbox doğrulama)
İnceleme: bekliyor
Commit'ler: yok (kullanıcıdan `orphan` branch + push onayı bekleniyor)
Yapılan:
- **Klasör yeniden düzenleme:**
  - `local/src/probe/*` → `src/` (`src/build.bat` çıktı yolu `..\local\build\dinput8.dll` güncellendi)
  - `local/coop_paket/*` → `package/` (`dinput8.dll` ve `steam_api.dll` git takibinden çıkarıldı; `steam_api.dll` yerel `local/vendor/` altında; `TEST_LISTESI.txt` → `docs/TEST_LISTESI.txt`)
  - `local/tools/*` → `tools/` ve RE araçları → `tools/re/`
  - `local/HANDOFF.md` → `docs/HANDOFF.md`, `local/NOTES.md` → `docs/NOTES.md`; `local/` tamamen `.gitignore` kapsamına alındı
  - `AGENTS.md`, `CLAUDE.md`, `GEMINI.md`, `docs/agents/model-gecisi.md`, `.claude/skills/handoff/SKILL.md` yeni yollara göre güncellendi
- **1.1 PII & Yol Temizliği:** `docs/HANDOFF.md`, `docs/TEST_LISTESI.txt`, `package/coop.ini`, `tools/autotest.ps1`, `tools/launch.ps1`, `tools/start_second.ps1`, `tools/re/ohc_test.ps1` içindeki SteamID/nick/yerel IP ve sabit `C:\Users\...` yolları temizlendi; `.gitignore` güncellendi.
- **1.2 Lisans & Standartlar:** `LICENSE` (MIT), `THIRD_PARTY_LICENSES.md`, `CHANGELOG.md` (`v1.0.0`), `.github/ISSUE_TEMPLATE/bug_report.yml`, `.github/ISSUE_TEMPLATE/feature_request.yml`, `.github/PULL_REQUEST_TEMPLATE.md` oluşturuldu.
- **1.3 Profesyonel `README.md`:** Rozetler, özellikler tablosu, 3 adımda kurulum/oynama, kısayollar (`F8`/`F9`/`F5`/`ESC`), dürüst doğrulama durumu, kaynak koddan derleme, yol haritası ve telif uyarısıyla yeniden yazıldı.
- **1.4 Kur/Kaldır + Anonim Log + Release Üretici:** `package/kur.ps1` (`-Uninstall` + masaüstü kısayolu), `package/KALDIR.bat`, `package/LOG_TOPLA.bat`, `package/log_topla.ps1`, `package/KUR.bat`, `package/KURULUM.txt` ve `tools/build_release.ps1` oluşturuldu/güncellendi.
Doğrulama:
- `src/build.bat` sıfır hata ile derlendi (`local/build/dinput8.dll`, `310.272 B`).
- `tools/build_release.ps1 -Version 1.0.0` ile `local/release/SuperCan-Coop-v1.0.0.zip` (`313.653 B`, SHA-256 `34B90730...76EF`) ve `local/release/SHA256SUMS.txt` üretildi; otomatik yasaklı dosya/PII taraması temiz geçti.
- Temiz sandbox testinde (`kur.ps1` → `log_topla.ps1` → `kur.ps1 -Uninstall`): orijinal `supercan.exe` (`AA91BD3B...`) → LAA (`0AB2D5FF...`) → SteamID/IP/kullanıcı adı maskeleme (`127.0.0.1` korunarak) → orijinal `supercan.exe` (`AA91BD3B...`) geri yükleme ve tüm mod dosyalarının temizlendiği doğrulandı.
Kullanıcı/arkadaş denedi mi: hayır (yayın hazırlığı + sandbox testi).
Varsayımlar:
- yok
Kalan sorunlar / yarım kalanlar:
- Kullanıcı onayıyla `orphan` branch üzerinden tek temiz `v1.0.0` commit'i ile push, GitHub repo Public açılışı, Topics ve `v1.0.0` Release yüklemesi.
