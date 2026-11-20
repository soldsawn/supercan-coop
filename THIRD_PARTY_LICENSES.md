# Üçüncü Parti Lisanslar ve Telif Bildirimleri

Bu proje [MIT Lisansı](LICENSE) altında açık kaynaklı bir hayran modudur. Aşağıda dağıtım paketinde kullanılan üçüncü parti bileşenler ve telif uyarıları yer almaktadır.

---

## 1. Steamworks SDK / Steamworks.NET (`steam_api.dll`)

Release `.zip` paketinde Steam P2P ve Steam Lobi desteği için dağıtılan `steam_api.dll` dosyası, Steamworks.NET Standalone dağıtımından alınmıştır:

- **Kaynak:** [Steamworks.NET](https://github.com/rlabrecque/Steamworks.NET) — Standalone `2025.164.1` (Windows x86 / 32-bit)
- **SHA-256:** `FDAFB7C65EC7D4182B42BF92CC8671C68E4DC547CC6E613899D2618A47EB65DA`
- **Lisans:** Steamworks.NET MIT Lisansı; `steam_api.dll` Valve Corporation'ın Steamworks SDK yeniden dağıtılabilir ikili dosyasıdır.
- **AppID 480 (Spacewar) Notu:** Mod, internet üzerinden port yönlendirme gerektirmeden Steam arkadaş listesi ve Steam Lobisi üzerinden bağlanabilmek için Valve'ın herkese açık ücretsiz test uygulaması olan `Spacewar` (`AppID 480`) kimliğini kullanır (`steam_appid.txt`). Bu nedenle oynarken Steam profilinizde *"Spacewar oynuyor"* görünmesi normaldir.

---

## 2. Oyun Dosyaları ve Telif Uyarısı

- **SüperCan** ve ilgili tüm karakterler, varlıklar, sesler, grafikler ve ticari markalar ilgili hak sahiplerine (Sobee Studios / Türk Telekom) aittir.
- Bu proje **resmî olmayan, kâr amacı gütmeyen bir topluluk / hayran modudur** ve hak sahipleriyle herhangi bir resmî bağlantısı yoktur.
- Bu depo ve Release paketleri **hiçbir orijinal oyun dosyasını (`.exe`, `.wmb`, `.wrs`, `.mdl`, `.gen` vb.) içermez**. Modu kullanabilmek için kendi yasal SüperCan 1 kurulumunuza sahip olmanız gerekir.
- Kurulum betiği (`package/kur.ps1`) yalnızca sizin bilgisayarınızdaki kopyada `supercan.exe` yedeğini (`supercan.exe.orig`) alarak 4 GB bellek adres alanı (`IMAGE_FILE_LARGE_ADDRESS_AWARE`) bitini yerinde etkinleştirir; `KALDIR.bat` ile orijinal `.exe` birebir geri yüklenir.
