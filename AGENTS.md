# AGENTS.md — SüperCan Co-op (tüm yapay zekâ asistanları için)

Araçtan bağımsız rehber (Gemini/Antigravity, Codex vb.). Claude Code `CLAUDE.md` + `.claude/` kullanır; kural değişince ikisini birlikte güncelle.

## Proje
**SüperCan 1 için iki oyunculu (co-op) mod.** Oyun (Acknex/Gamestudio tabanlı `supercan.exe`) kapalı kaynak; biz `dinput8.dll` vekili
enjekte edip her karede oyuncu konumu/animasyonunu ağdan eşleştiriyoruz. İşlemler tamamlanınca **oyun dosyaları ve kişisel veriler olmadan GitHub'da public olarak yayınlanacak**.
Hedefler: SC1 önce (Steam Lobi + oyun içi davet/katıl dahil) → GitHub public yayın → SüperCan 2'ye taşıma.

## Oturuma başlarken oku (sırayla)
1. `docs/HANDOFF.md` — nerede kaldık, sıradaki adım, mimari, çözülmüş tuzaklar (**tuzakları tekrar yaşama**).
   Claude dışı modelsen önce `docs/agents/model-gecisi.md`'yi oku ve ona uy.
2. `AGENT_LOG.md` — son kayıtlar.
3. Gerekirse `docs/NOTES.md` (ayrıntılı gözlemler/eski günlükler), `README.md` (kurulum).

## Klasörler
```
src/                      TÜM mod kodu (C, x86: probe.c, core/, sc1/) · src/build.bat derler
package/                  oyuncuya dağıtılan paket şablonu (KUR.bat, KALDIR.bat, kur.ps1, LOG_TOPLA.bat, coop.ini, başlatıcı, KURULUM.txt)
tools/                    derleme, release ve test araçları (build_release.ps1, diff_logs.py, autotest.ps1, start_second.ps1, winctl.ps1, shot.ps1)
tools/re/                 RE / decompile yardımcıları (acknames.py, mkpe.py, scnames.py, disfn.py, ghidra/ ...)
docs/                     dokümantasyon (HANDOFF.md, NOTES.md, supercan2-hazirlik.md, agents/model-gecisi.md)
local/                    YALNIZCA git-dışı yerel çalışma alanı (build/, release/, vendor/, testinstall/, dump/, ref/, steamcoop_paket/)
gamefiles/                orijinal oyun kurulumu (git dışı, DOKUNMA)
coop-anything/            ayrı depo (buraya commit'lenmez) — co-op mod geliştirme rehberleri (`playbook/`: networking, steam-spacewar,
                          desync-hunting, actor-ownership, animation-sync-checks, reverse-engineering, decomp-and-recomp,
                          effect-capture-story-sync vb.), araçlar (`tools/`: animcheck, bridge, chaos, fakeworld, flight, fuzz,
                          hotreload, inspect, mcp, mutants, ownership, scenarios), şablonlar (`templates/`) ve skill'ler (`.agents/skills/`).
                          İhtiyaç duyulduğunda doğrudan referans/araç olarak kullanılabilir.
```

## Derleme ve test
- Derleme: Visual Studio 2022 (x86 araçları) → `src/build.bat`. Hatasız `local/build/dinput8.dll` oluşmalı.
- Oyunu çalıştırmak için kendi (kopya) SüperCan kurulumun gerekir: `package/kur.ps1 -Game "<kopya>\Game"`. İki taraf aynı DLL olmalı.
- Otomatik test altyapısı ve bilinen tuzaklar `docs/HANDOFF.md` içinde. Gerçek doğrulama oyunda iki kopyayla yapılır
  (kullanıcı oynar, `Game\coop_probe_*.log` analiz edilir). Derlendi ≠ çalışıyor: testte görmediğini "çalışıyor" diye yazma.

## Kurallar
- Kod C; yorumlar ve dokümanlar **Türkçe**; kullanıcıyla Türkçe konuş.
- Oyun dosyası, exe, `.gen`, kayıt defteri jetonu, SteamID/IP gibi kişisel veri depoya GİRMEZ.
- Protokol (SCO6 oyuncu, SCW3 dünya, SAL2 alarm/terminal, SBO3 boss vb.) değişince sürüm etiketini artır; iki taraf aynı DLL.
- Sadece isteneni yap; ilgisiz kodu "iyileştirme". `probe.c` tek dosya: küçük, cerrahi değişiklikler; çakışmayı önlemek için
  `AGENT_LOG.md`'ye hangi fonksiyonlara dokunduğunu yaz.
- Commit sadece kullanıcı isteyince (kullanıcı toplu yapmak istiyor).

## Handoff
`docs/HANDOFF.md` kullanıcı "handoff'u güncelle" deyince güncellenir (durum kısmı yeniden yazılır, kalıcı bölümler korunur;
yöntem `.claude/skills/handoff/SKILL.md`). Her iş sonunda `AGENT_LOG.md`'ye kayıt.
