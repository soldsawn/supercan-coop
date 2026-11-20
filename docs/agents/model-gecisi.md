# Modeller arası çalışma (Claude ↔ Gemini)

İki asistan sırayla/paralel çalışır: **Claude Code** (kullanıcı) ve **Gemini / Antigravity** (arkadaşın tarafı). Amaç: iş el değiştirirken
hiçbir şey kaybolmasın, her değişiklik incelensin, aynı kod bölgesi çakışmasın.

| Dosya | Ne için | Kim yazar |
|---|---|---|
| `docs/HANDOFF.md` | Anlık durum, sıradaki adım, mimari, tuzaklar | Kullanıcı isteyince her iki model |
| `AGENT_LOG.md` | Kim, ne zaman, neyi değiştirdi, nasıl doğruladı (birikir) | Her iki model, her iş sonunda |
| `docs/NOTES.md` | Ayrıntılı gözlemler, eski günlükler | Gerektikçe |

## Gemini (veya Claude dışı her model) için kurallar
1. **Başlarken oku:** `AGENTS.md` → `docs/HANDOFF.md` → `AGENT_LOG.md` (son 3 kayıt). "Çözülmüş tuzaklar"ı atlama.
2. **Bir seferde bir iş.** Sırayı HANDOFF "Sıradaki adım" belirler; kullanıcı başka şey isterse onu yap ama kayda yaz.
3. **Çakışma:** `src/probe.c` + `src/core/` + `src/sc1/`. Başlamadan `git pull`, `AGENT_LOG.md`'de başkasının dokunduğu fonksiyonlara bak; küçük ve cerrahi değiştir,
   ilgisiz kodu biçimlendirme/taşıma.
4. **Doğrulama:** değişiklik sonrası `src/build.bat` hatasız derlenmeli. Oyun içi davranışı sen test edemiyorsan
   kayda "Test edilmedi" yaz — "çalışıyor" yazma. Protokol değişirse sürüm etiketini artır ve kayda yaz.
5. **Commit:** `[gemini] <ne yapıldı>`. Amend/force push yok. Yalnızca kullanıcı "commit et" derse. Oyun dosyası, `.gen`, exe, log, kişisel veri ASLA.
6. **Kayıt:** iş bitince (yarım kalsa bile) `AGENT_LOG.md` sonuna aşağıdaki şablonla kayıt ekle.
7. Kod C, yorumlar Türkçe. Oyun dosyası / kişisel veri (SteamID, IP, `.gen`, exe) depoya ASLA girmez.

## Kayıt şablonu (`AGENT_LOG.md` sonuna)
```markdown
## <YYYY-MM-DD> · <model adı> · <iş kısa adı>
İnceleme: bekliyor
Commit'ler: <ilk_hash>..<son_hash> (yoksa "yok")
Yapılan:
- <madde> (`dosya`, fonksiyon)
Doğrulama: <derlendi mi; oyunda denendi mi, sonuç>
Kullanıcı/arkadaş denedi mi: <evet + ne dedi / hayır>
Varsayımlar:
- <yoksa "yok">
Kalan sorunlar / yarım kalanlar:
- <yoksa "yok">
```

## Claude için inceleme yöntemi (oturum başında "İnceleme: bekliyor" görürse)
1. `AGENT_LOG.md` başındaki **Son incelenen commit**'ten bu yana: `git log --oneline <hash>..HEAD`, `git diff <hash>..HEAD`.
2. Her bekleyen kaydı HANDOFF tuzakları ile karşılaştır: protokol uyumu, kancaların prolog doğrulaması, ilgisiz değişiklik, kişisel veri/oyun dosyası sızıntısı.
3. `build.bat` ile derle; mümkünse iki kopyada kısa deneme.
4. Düzeltmeleri ayrı commit: `[claude-review] <ne düzeltildi>`.
5. Kaydı güncelle: `İnceleme: tamam (<hash>) — <bulgu>` / `düzeltildi (<hash>)`; **Son incelenen commit**'i HEAD'e çek.
6. Kullanıcıya kısa özet.
