# Değişiklik Günlüğü (Changelog)

Bu projenin önemli sürümleri ve değişiklikleri burada belgelenir.

## [v1.0.0] — 2026-11-20

İlk herkese açık (Public) SüperCan 1 Co-op sürümü.

### Eklenenler
- **Ağ ve Bağlantı:**
  - **Steam P2P + Steam Lobi (`net=steam`):** Port yönlendirme veya IP gerekmeden Steam arkadaşlarıyla internet üzerinden oynama (`ISteamNetworkingMessages` + `ISteamMatchmaking`, AppID 480).
  - **Oyun İçi `F8` Steam Lobi & Davet Paneli:** Arkadaşa özel lobi kurma, çevrimiçi arkadaş listesini görme, tek tuşla (`1..8`) davet gönderme ve Lobi ID kopyalama.
  - **Yerel Ağ / LAN Modu (`net=udp`):** Aynı ağda otomatik yayınla eş bulma veya sabit `ip:port` bağlantısı.
  - **Bozuk Oturum Kurtarma:** Steam P2P `SessionFailed` / `EResult 35` durumunda otomatik oturum yenileme (`AutoRestartBrokenSession` + `CloseSessionWithUser`).
- **Oynanış ve Dünya Senkronizasyonu (`SCO6`, `SCW3`, `SAL2`, `SB10`):**
  - Oyuncu konumu, yönü, 8 parçalı karakter modeli ve kemik/iskelet animasyonları (`ent_animate`, `ent_blendpose`, `ent_bonerotate`) + istemci yumuşatma (`[interp]`).
  - İki oyuncuyu da hedefleyen düşman yapay zekâsı (`GBR`, `ZBD`, `UCAN`, `D.E.V.E. Boss`), ortak düşman hasarı, kıvılcım efektleri ve client hasar alma (`SPH1`).
  - Çöp vakumlama (`TRASH_FLAG2`), çevre çöpleri/elmaslar (`SIT1`), ortak puan/elmas ve bağımsız nano barları (`SST2`).
  - Köprü/makara/kapı (`SMK2`), taramalı taret, alarm/lazer/hack terminalleri (`SAL2`), `orman.wmb → tunel.wmb` harita geçişi.
  - **D.E.V.E. Boss Fight (`SB10`):** 4 batarya, iki yönlü QTE, ortak boss canı (`G_DEVE_HP`), lazer rayı senkronu, boss ölümü ve son ara sahne geçişi.
  - 3D ses (`SND1`), müzik (`SMU1`), ortak ara sahne geçme (`SCT1`), late-join/reconnect snapshot (`SSN1`) ve arkadaşın yanında canlanma (`revive_tick`).
- **Arayüz ve Kısayollar:**
  - Acknex native Co-op HUD (`F9` ile aç/kapa): arkadaşın adı, HOST/P2 rozeti, canlı ping (ms), can/nano barları, baş üstü isim ve ekran dışı 360° yön/mesafe göstergesi.
  - `F5` ile arkadaşın yanına ışınlanma (`1.5 sn` dokunulmazlık, `2 sn` bekleme).
- **Kurulum ve Araçlar:**
  - **Özel Co-op Başlatıcı (`SuperCan_Coop_Oyna.bat`):** Orijinal başlatıcıya gerek kalmadan Hata 22 (`ohc`) uyumlu doğrudan başlatma.
  - **Tek Tıkla Kurulum & Kaldırma:** `KUR.bat`, `KALDIR.bat` (orijinal `supercan.exe` geri yükleme) ve anonimleştirilmiş hata raporu arşivi oluşturan `LOG_TOPLA.bat`.
  - **Kapanış ve Grafik Koruması:** `devicereset` kapanış çökmesi koruması, `keep_ga=1` grafik ayarı koruması ve istemci karakter ışıklandırma (`level_materials_ready`) düzeltmesi.

### Bilinen Sınırlamalar / Henüz Doğrulanmamış Son Düzeltmeler
- Son sürümde eklenen 3 küçük düzeltme (host menüye döndükten sonra client'ın Ana Menüye dönebilmesi, `F8` lobi kuruluyken davetsiz bağlanma engeli, `menu.wmb`'de boss kontrolünün kapanması) derlenmiş ancak henüz iki PC Steam testinde tekrar doğrulanmamıştır.
- Orman bölümünde gün zamanı (`gündüz/akşam/gece`) iki oyuncuda farklı gelebilir.
