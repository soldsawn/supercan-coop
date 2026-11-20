# SüperCan 2 Co-op Taşıma Hazırlığı

Bu belge, `local/src/probe/` modüler mimarisinin SüperCan 2'ye (`supercan2.exe`) nasıl taşınacağını özetler.

## 1. Motor ve İkili Uyumluluk (Doğrulanan Bulgular)
`gamefiles/SuperCan2/Game/` üzerinde yapılan salt-okunur inceleme sonucu:
- **Motor:** SüperCan 1 ile aynı Acknex/Gamestudio 32-bit (x86) motoru (`acknex.dll`, `ackphysX.dll`, `fmod.dll`, `d3dx9_30.dll`, `color.dll`).
- **Enjeksiyon:** `supercan2.exe` de `acknex.dll!engine_frame` import ediyor ve `acknex.dll` çalışma anında `dinput8.dll` yüklüyor → mevcut **`dinput8.dll` vekili enjeksiyonu** birebir geçerli.
- **Doğrulanan 15 Acknex export'u (SC1 & SC2 %100 aynı):**
  `engine_frame`, `ent_next`, `ent_create`, `ent_remove`, `ent_animate`, `ent_blendpose`, `ent_bonereset_all`, `ent_bonerotate`, `draw_quad`, `draw_text`, `draw_textmode`, `vec_to_screen`, `engine_getscript`, `engine_getvar`, `proc_status`.
- **Kaynak yapısı (`supercan2.wdl`):** `defGSQ_1..3`, `defGPQ_0..1`, `defGTQ_0..2` derleme bayrakları ve `Data\*.gen` paket yapısı SC1 ile aynı kalıpta.

## 2. Modüler Mimari (`local/src/probe/`)
`probe.c` artık iki katmanlı unity-build yapısındadır:

### Ortak Çekirdek (`core/`) — SC2'de doğrudan yeniden kullanılır
| Dosya | Sorumluluk |
|---|---|
| `core/core_acknex.inc` | Acknex `ENTITY`/`STRING`/`var`, animasyon kancaları (`ent_animate`, `ent_bonerotate` vb.), `SCO6` oyuncu paketi ve interpolasyon |
| `core/core_net.inc` | Steam Spacewar P2P + `ISteamMatchmaking` Lobi & Davet (`F8`) + UDP/LAN keşif + ağ simülasyonu |
| `core/core_hud.inc` | Acknex native Co-op HUD (`SHD2`), `F5` ışınlanma, `F8` Steam Lobi & Davet paneli, `F9` gizle |
| `core/core_d3d_input.inc` | D3D9 pencere/tam ekran yönetimi, `h_wndproc` kapanış koruması, uzaktan kumanda, `dinput8` vekili, IAT kancaları |

### Oyuna Özgü Katman (`sc1/` → SC2 için `sc2/` eklenecek)
| Dosya | SC2 Karşılığı İçin Yapılacaklar |
|---|---|
| `sc1/sc1_offsets.h` | `dumpall` + `mkpe.py` + `DecompAll.java` ile SC2 `decomp.c` çıkarılıp `sc2/sc2_offsets.h` (`ebx_base` ve SC2 global ofsetleri) yazılır |
| `sc1/sc1_world_combat.inc` | SC2 düşman model/box adları, hasar skill ofsetleri ve dünya mekanikleri (bulmaca/kapı) |
| `sc1/sc1_boss.inc` | SC2 boss savaşları (host otoriteli ayna şablonu hazır) |
| `sc1/sc1_revive.inc` | SC2 ölüm/canlanma ve `.wmb` bölüm geçiş eşlemesi |
| `sc1/sc1_sync_extra.inc` | SC2 eşya/puan/elmas tablosu, snapshot (`SSN1`), ses/müzik/ara sahne |

## 3. SC2'ye Geçiş İçin İlk 3 Adım
1. `gamefiles/SuperCan2`'nin test kopyasına (`local/testinstall/SuperCan2`) LAA + `dinput8.dll` (yalnız `core/` ile derlenmiş temel SC2 profili) koyup ilk açılışta `dumpall` ile SC2 lite-C dökümünü al.
2. `local/tools/mkpe.py` → Ghidra `DecompAll.java` → `scnames.py` boru hattıyla SC2 `decomp.c`'yi üret.
3. `SuperCan2_Oyna.exe` / `ohc` kontrolünü doğrulayıp `SuperCan_Coop_Baslatici.ps1`'e SC1/SC2 oyun seçimi ekle.
