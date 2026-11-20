# CLAUDE.md — SüperCan Co-op

İşlemler bitince oyun dosyaları ve kişisel veriler olmadan GitHub'da **public** olarak yayınlanacak. **Kullanıcıyla Türkçe konuş.** Ortak kurallar `AGENTS.md`'de; bu dosya Claude'a özel.

## Oturum başında
1. `docs/HANDOFF.md` oku (durum + mimari + tuzaklar). Ayrıntı gerekirse `docs/NOTES.md`.
2. `AGENT_LOG.md`'de "İnceleme: bekliyor" varsa arkadaş Gemini ile çalışmış demektir → `docs/agents/model-gecisi.md` →
   "Claude için inceleme yöntemi"ni uygula, kullanıcıya özetle.

## Kurallar (kullanıcı kararları)
- Commit/paket/push **toplu**, arkadaşın PC'sinde git kurulunca; adım başına yapma (kullanıcı isteyince yap).
- Testler toplu: birkaç özelliği birleştir, tek testte dene. Otomasyon menüye kadar yeter; pencere sabitleme hileleri yok.
- Oyun dosyaları depoya girmez (`gamefiles/`, `testinstall/`, `*.exe`, `*.gen`). `gamefiles/`'a dokunma.
- Özel başlatıcı (Hata 22 / `ohc` jetonu + ayarlar + Co-op lobi): yasak değil, sonradan incelenip mümkün mü bakılacak.
- Kullanıcı "handoff'u güncelle" deyince `/handoff` skill'i (`.claude/skills/handoff/SKILL.md`).
- **Kaynak/araç deposu (`coop-anything/`):** Ayrı depo (commit'lenmez); `playbook/` (ağ, Steam Spacewar, desync, actor-ownership, animasyon, RE, hikâye/efekt senkronu belgeleri), `tools/` (`animcheck`, `bridge`, `chaos`, `fakeworld`, `flight`, `fuzz`, `hotreload`, `inspect`, `mcp`, `mutants`, `ownership`, `scenarios`), `templates/` ve `.claude/skills/` içerir — gerektiğinde doğrudan faydalanılabilir.
- Derleme: `src/build.bat` (VS2022 x86) → `local/build/dinput8.dll`. Release paketi üretmek için `tools/build_release.ps1`.
