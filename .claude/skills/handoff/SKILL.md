---
name: handoff
description: Update docs/HANDOFF.md so a fresh session (after /clear) can continue the work. Use when the user says "handoff", "handoff'u güncelle", "clear çekelim", or a feature step is finished.
---

# Handoff

Kullanıcı bir adım bitince bunu çalıştırıp `/clear` yapar. Sonraki oturumun elinde yalnızca repo dosyaları olacak
(konuşma kalmayacak). Dosya: `docs/HANDOFF.md`.

## Yapı
`docs/HANDOFF.md` iki parçadır:
1. **Durum kısmı** (`---` çizgisinin üstü): her seferinde **yeniden yazılır**.
2. **Kalıcı bölümler** (`# Kalıcı bölümler` altı): Kurulum/dağıtım, Mimari, Çözülmüş tuzaklar, Test, Otomatik test altyapısı.
   Yalnızca yeni kalıcı bilgi çıktıysa ilgili bölüme eklenir (yeni tuzak, yeni ayar, yeni protokol). Durum günlüğü buraya girmez.

## Adımlar
1. Bu oturumda ne yapıldığını topla: değişen dosyalar, kararlar, doğrulananlar, doğrulanamayanlar (`git status`, `git log`).
2. Durum kısmını aşağıdaki şablonla yeniden yaz. Eski "Son oturumda yapılanlar" **Geçmiş**'e tek satır olarak iner (en fazla 10 satır;
   fazlası ve uzun ayrıntı `docs/NOTES.md` sonuna taşınır, silinmez).
3. Kalıcı bölümleri gözden geçir: yeni tuzak/ayar/mimari değişikliği varsa ekle, eskimiş olanı düzelt.
4. `AGENT_LOG.md`'de "İnceleme: bekliyor" kayıt varsa (başka model çalışmış) durumu "Şu an nerede"ye yansıt.
5. Kullanıcıya 2–3 cümlelik özet ver; `/clear` yapabileceğini söyle. Commit/paket/push YAPMA (kullanıcı kararı: toplu, arkadaşın git kurulumundan sonra).

## Şablon (durum kısmı)
```markdown
# HANDOFF — <YYYY-MM-DD>

<proje bağlamı 3–4 satır: özel proje, Türkçe, depo, NOTES.md, çok modelli çalışma>

## Şu an nerede
## Son oturumda yapılanlar
## Doğrulama durumu        (çalıştığı doğrulanan / test EDİLMEDİ)
## Sıradaki adım
## Açık sorular / kullanıcıdan beklenen
## Dikkat                  (commit'siz dosyalar, paket DLL sürümü, tuzaklar)
## Geçmiş                  (tek satırlar, en fazla 10)
```

Kısa ve somut yaz; dosya yolu ve saat ver; "muhtemelen" yerine ne doğrulandıysa onu yaz. Testte olmayanı "çalışıyor" diye yazma.
