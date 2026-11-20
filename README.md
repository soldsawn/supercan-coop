# SüperCan 1 Co-op Mod

[![Lisans: MIT](https://img.shields.io/badge/Lisans-MIT-green.svg)](LICENSE)
[![Platform: Windows x86](https://img.shields.io/badge/Platform-Windows%20(32--bit)-blue.svg)](#kaynak-koddan-derleme)
[![Bağlantı: Steam P2P & LAN](https://img.shields.io/badge/A%C4%9F-Steam%20P2P%20%26%20LAN-purple.svg)](#3-ad%C4%B1mda-kurulum-ve-oynama)
[![Sürüm: v1.0.0](https://img.shields.io/badge/S%C3%BCr%C3%BCm-v1.0.0-orange.svg)](CHANGELOG.md)

**SüperCan 1** (Acknex/Gamestudio motoru) için geliştirilmiş, açık kaynaklı **iki oyunculu çevrimiçi (Steam P2P / Steam Lobi) ve yerel ağ (LAN) co-op modu**.

> [!IMPORTANT]
> Bu depo ve yayın paketleri **hiçbir orijinal oyun dosyasını içermez**. Oynamak için her iki oyuncunun da kendi SüperCan 1 kurulumuna sahip olması gerekir. Ayrıntılar için [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) dosyasına bakın.

---

## Özellikler

| Kategori | Desteklenen Özellikler |
|---|---|
| **Ağ & Lobi** | **Steam P2P (`net=steam`)** ile port yönlendirme/IP gerektirmeden internetten oynama, oyun içi **`F8` Steam Lobi & Davet Paneli**, **Yerel Ağ (`net=udp`)** otomatik keşif ve sabit IP desteği |
| **Oyuncu & Animasyon** | 30 Hz konum/yön/ölçek senkronizasyonu, 8 parçalı karakter modeli, tam kemik & poz animasyonları (`ent_animate`, `ent_blendpose`, `ent_bonerotate`) + istemci yumuşatma |
| **Savaş & Yapay Zekâ** | Tüm düşmanların (`GBR`, `ZBD`, `UCAN`, `D.E.V.E.`) iki oyuncuyu da hedeflemesi, ortak düşman hasarı, kıvılcım efektleri, iki oyuncunun da hasar alması ve arkadaşın yanında canlanma |
| **Dünya & İlerleme** | Çöp vakumlama, çevre çöpleri & elmaslar, ortak puan/elmas (bağımsız nano barları), köprü/makara/kapı, taramalı taret, alarm/lazer/hack terminalleri ve `orman.wmb → tunel.wmb` bölüm geçişi |
| **D.E.V.E. Boss Fight** | Tek otoriteli senkron Boss savaşı: 4 batarya, iki yönlü QTE, ortak Boss canı, lazer rayı senkronizasyonu, Boss ölümü ve son ara sahne geçişi |
| **HUD & Ses** | Acknex native Co-op HUD (`F9`), canlı ping (ms), can/nano barları, baş üstü isim, 360° ekran dışı yön/mesafe göstergesi, 3D ses, müzik ve ortak ara sahne geçme (`ESC`) |
| **Başlatıcı & Araçlar** | Orijinal başlatıcıya gerek kalmadan açan **Özel Co-op Başlatıcı**, tek tıkla **Kur (`KUR.bat`) / Kaldır (`KALDIR.bat`)** ve anonim **Log Toplayıcı (`LOG_TOPLA.bat`)** |

---

## 3 Adımda Kurulum ve Oynama

> [!TIP]
> İki oyuncunun da **birebir aynı sürümdeki** paketi kurmuş olması gerekir.

### 1. Oyununuzu kopyalayın
Mevcut SüperCan 1 klasörünüzü ayrı bir yere kopyalayın (ör. `C:\SuperCanCoop`). Orijinal kurulumunuza dokunmayın.

### 2. Modu kurun
1. [Releases](https://github.com/soldsawn/supercan-coop/releases) sayfasından en son `SuperCan-Coop-v*.zip` dosyasını indirip bir klasöre çıkarın.
2. **`KUR.bat`** dosyasına çift tıklayın ve kopyaladığınız oyunun **`Game`** klasörünü (ör. `C:\SuperCanCoop\Game`) girin ya da pencereye sürükleyip bırakıp `Enter`'a basın.
   - Kurulum `supercan.exe`'yi otomatik yedekler (`supercan.exe.orig`), 4 GB bellek yamasını (LAA — `E2005` doku hatasını önler) yerinde uygular, DLL'leri, başlatıcıyı ve `coop.ini` dosyasını yerleştirir.

### 3. Oyunu başlatın ve bağlanın
- **Steam Modu (varsayılan, internet üzerinden):**
  1. İki oyuncu da **Steam'i açıp giriş yapmalıdır** (Steam'de arkadaş olmalısınız).
  2. Oyun klasöründeki **`SuperCan_Coop_Oyna.bat`** (veya masaüstü kısayolu) ile oyunu başlatın.
     *(Steam profilinizde Valve'ın ücretsiz test uygulaması olan **"Spacewar oynuyor"** görünmesi normaldir.)*
  3. Oyun içinde **`F8`** tuşuna basıp **Steam Lobi & Davet Paneli**'ni açın, arkadaşınızı **`1..8`** tuşlarıyla davet edin (veya Steam üzerinden daveti kabul edin).
  4. İki oyuncu da aynı bölüme girdiğinde otomatik olarak buluşursunuz.
- **Yerel Ağ / LAN Modu (`net=udp`):**
  - Kurulumu PowerShell üzerinden `-Net udp` ile yapın veya `Game\coop.ini` içinde `net=udp` yazın; gerekiyorsa `guvenlik_duvari.bat`'ı yönetici olarak çalıştırın.

---

## Oyun İçi Kısayollar

| Tuş | İşlev |
|---|---|
| **`F8`** | Steam Lobi & Arkadaş Davet Panelini aç / kapat (`1..8` davet et, `C` kur, `L` ayrıl, `K` Lobi ID kopyala, `R` yenile) |
| **`F9`** | Co-op HUD panelini gizle / göster |
| **`F5`** | Arkadaşının yanına ışınlan (`1.5 sn` dokunulmazlık, `2 sn` bekleme; ara sahnelerde kapalı) |
| **`ESC`** | Ara sahnede veya diyalogda (`>0.5 sn`) iki oyuncuda birden ara sahneyi geç |
| **`Ctrl + F9`** | Geliştirici teşhis dökümü (log dosyasına yazar) |

---

## Kaldırma ve Hata Bildirimi

- **Modu tamamen kaldırmak için:** Paket içindeki **`KALDIR.bat`** dosyasına çift tıklayın (veya `powershell -ExecutionPolicy Bypass -File package\kur.ps1 -Game "<Game klasörü>" -Uninstall`). Orijinal `supercan.exe.orig` yedeği geri yüklenir ve tüm mod dosyaları temizlenir.
- **Hata bildirmek için:** **`LOG_TOPLA.bat`** dosyasına çift tıklayın. Masaüstü'nde son 3 oyun logunu, `coop.ini` dosyasını ve DLL/EXE hash bilgilerini içeren **anonimleştirilmiş** bir `.zip` arşivi (SteamID, IP adresleri ve Windows kullanıcı adınız otomatik maskelenir) oluşturulur. Bu arşivi yeni bir [Hata Bildirimi (Issue)](https://github.com/soldsawn/supercan-coop/issues/new/choose) içine sürükleyip bırakabilirsiniz.

---

## Doğrulama Durumu

- **Gerçek iki PC Steam testinde doğrulananlar:**
  - `0 crash` ile kararlı Steam P2P bağlantısı ve `SessionFailed` otomatik kurtarma.
  - Başlangıçtan sona **D.E.V.E. Boss Fight** (4 batarya, iki yönlü QTE, ortak boss canı, ölüm, son ara sahne ve `menu.wmb` dönüşü).
  - İstemci karakter ışıklandırma, kılıç parlaması, Co-op HUD, ses/müzik ve ortak bölüm senkronları.
- **Derlenmiş fakat henüz iki PC Steam testinde tekrar doğrulanmamış son düzeltmeler:**
  - Host önce ana menüye döndükten sonra client'ın `ESC → Ana Menü` yapabilmesi (`G_ROBOT_ALIVE` sıfırlama).
  - `F8` Steam Lobisi kuruluyken davet kabul edilmeden otomatik bağlanmanın engellenmesi.
  - `menu.wmb`'de boss kontrolünün tamamen devre dışı kalması.

---

## Proje Yapısı ve Kaynak Koddan Derleme

```text
supercan-coop/
├── src/                  # C kaynak kodu (dinput8.dll vekili, x86 unity build)
│   ├── probe.c           # Ana giriş noktası ve tick döngüsü
│   ├── probe.def         # DirectInput8Create export tanımı
│   ├── build.bat         # Visual Studio 2022 x86 derleme betiği
│   ├── core/             # Motor, ağ (Steam/UDP), HUD ve D3D9/DInput kancaları
│   └── sc1/              # SüperCan 1'e özgü ofsetler, savaş, boss, canlanma ve senkron
├── package/              # Oyuncu kurulum/kaldırma/log/başlatıcı betikleri ve coop.ini
├── tools/                # Release üretici, log karşılaştırıcı ve test yardımcıları
│   └── re/               # Tersine mühendislik ve decompile yardımcıları
└── docs/                 # Teknik dokümantasyon ve SüperCan 2 taşıma rehberi
```

### Derleme Gereksinimleri
- Windows + **Visual Studio 2022** (**C++ ile Masaüstü Geliştirme**, x86 / 32-bit araç takımı)

```bat
src\build.bat
```
Çıktı `local\build\dinput8.dll` olarak üretilir.

### Release Paketi Üretme
`local\vendor\steam_api.dll` (Steamworks.NET `2025.164.1` x86) yerleştirildikten sonra:
```powershell
powershell -ExecutionPolicy Bypass -File tools\build_release.ps1 -Version 1.0.0
```
Bu komut `src\build.bat`'ı çalıştırır, yasaklı dosya/PII taraması yapar ve `local\release\SuperCan-Coop-v1.0.0.zip` + `SHA256SUMS.txt` dosyalarını üretir.

---

## Yol Haritası

- [x] SüperCan 1 tam hikâye ve D.E.V.E. Boss Co-op senkronizasyonu (`v1.0.0`)
- [x] Steam P2P + Oyun içi `F8` Steam Lobi & Davet Paneli + Özel Co-op Başlatıcı
- [ ] Gelişmiş Co-op Başlatıcı arayüzü (orijinal grafik/gölge/doku ayarları)
- [ ] Oyuncu 2 kıyafet rengi farklılaştırma (`coop.ini` üzerinden)
- [ ] Orman gün zamanı senkronizasyonu ve Alt-Tab gamma koruması
- [ ] **SüperCan 2 Co-op** entegrasyonu (bkz. [docs/supercan2-hazirlik.md](docs/supercan2-hazirlik.md))

---

## Lisans

Bu projenin kaynak kodu [MIT Lisansı](LICENSE) ile lisanslanmıştır. Üçüncü parti bileşenler ve telif bildirimleri için [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) dosyasına bakın.
