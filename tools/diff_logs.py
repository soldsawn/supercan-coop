#!/usr/bin/env python3
"""
SuperCan Co-op — Host / Client Log Diff Aracı (Faz 4)

Kullanım:
  python local/tools/diff_logs.py                         # testinstall/SuperCan1 ve SuperCan1b son loglarını otomatik kıyaslar
  python local/tools/diff_logs.py <host.log> <client.log> # belirtilen iki log dosyasını kıyaslar
"""

import glob
import os
import re
import sys
from collections import Counter

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEF_HOST_DIR = os.path.join(REPO_ROOT, "local", "testinstall", "SuperCan1", "Game")
DEF_CLIENT_DIR = os.path.join(REPO_ROOT, "local", "testinstall", "SuperCan1b", "Game")

RE_SYNC = re.compile(
    r"\[sync\] hit=(\d+)/(\d+) phit=(\d+)/(\d+) mkr=(-?\d+) alrm=(-?\d+) "
    r"itm=(\d+)/(\d+)\(tot (\d+)\) \| score=(-?\d+) diam=(-?\d+) rbt=(-?\d+) env=(-?\d+) nano=(-?\d+)"
)
RE_COOP = re.compile(
    r"\[coop\] f(\d+) level='([^']*)' local_parts=(\d+) rx=(\d+) remote_level='([^']*)' remote_parts=(\d+)"
)
RE_RUN_ID = re.compile(r"\[coop\] session run_id=([0-9a-fA-F]+)")
RE_PEER_RID = re.compile(r"\[net\] new peer run_id=([0-9a-fA-F]+) \(was ([0-9a-fA-F]+)\)")
RE_TAG = re.compile(r"^\[([a-z0-9_-]+)\]", re.I)


def latest_log(directory: str) -> str:
    files = glob.glob(os.path.join(directory, "coop_probe_*.log"))
    if not files:
        return ""
    files.sort(key=os.path.getmtime, reverse=True)
    return files[0]


def parse_log(path: str) -> dict:
    info = {
        "path": path,
        "name": os.path.basename(path),
        "run_id": "-",
        "peer_run_ids": [],
        "levels": [],
        "syncs": [],
        "events": Counter(),
        "ssn": [],
        "gfx": [],
        "crashes": [],
        "mkr": [],
        "alrm": [],
        "boss": [],
        "itm_local": [],
        "itm_rx": [],
        "snd_tx": 0,
        "snd_rx": 0,
        "cut": [],
        "mus": [],
        "f5": 0,
        "revive": 0,
    }
    last_coop = None
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for raw in f:
            line = raw.strip()
            if not line:
                continue
            m = RE_TAG.match(line)
            if m:
                info["events"][m.group(1).lower()] += 1
            m = RE_RUN_ID.search(line)
            if m:
                info["run_id"] = m.group(1)
            m = RE_PEER_RID.search(line)
            if m:
                info["peer_run_ids"].append(f"{m.group(2)}->{m.group(1)}")
            m = RE_COOP.search(line)
            if m:
                last_coop = {
                    "frame": int(m.group(1)),
                    "level": m.group(2),
                    "rx": int(m.group(4)),
                }
                if m.group(2) and (not info["levels"] or info["levels"][-1] != m.group(2)):
                    info["levels"].append(m.group(2))
            m = RE_SYNC.search(line)
            if m:
                info["syncs"].append({
                    "frame": last_coop["frame"] if last_coop else 0,
                    "level": last_coop["level"] if last_coop else "?",
                    "hit_tx": int(m.group(1)), "hit_rx": int(m.group(2)),
                    "phit_tx": int(m.group(3)), "phit_rx": int(m.group(4)),
                    "mkr": int(m.group(5)), "alrm": int(m.group(6)),
                    "itm_act": int(m.group(7)), "itm_done": int(m.group(8)), "itm_tot": int(m.group(9)),
                    "score": int(m.group(10)), "diam": int(m.group(11)),
                    "rbt": int(m.group(12)), "env": int(m.group(13)), "nano": int(m.group(14)),
                })
            if line.startswith("[ssn]"):
                info["ssn"].append(line)
            elif line.startswith("[gfx]"):
                info["gfx"].append(line)
            elif line.startswith("[crash"):
                info["crashes"].append(line)
            elif line.startswith("[mkr]"):
                info["mkr"].append(line)
            elif line.startswith("[alrm]"):
                info["alrm"].append(line)
            elif line.startswith("[boss]"):
                info["boss"].append(line)
            elif line.startswith("[itm] local taken"):
                info["itm_local"].append(line)
            elif line.startswith("[itm] rx hid"):
                info["itm_rx"].append(line)
            elif line.startswith("[snd] tx"):
                info["snd_tx"] += 1
            elif line.startswith("[snd] rx"):
                info["snd_rx"] += 1
            elif line.startswith("[cut]"):
                info["cut"].append(line)
            elif line.startswith("[mus]"):
                info["mus"].append(line)
            elif "F5 teleported" in line:
                info["f5"] += 1
            elif line.startswith("[revive]"):
                info["revive"] += 1
    return info


def main() -> int:
    if len(sys.argv) >= 3:
        h_path, c_path = sys.argv[1], sys.argv[2]
    else:
        h_path = latest_log(DEF_HOST_DIR)
        c_path = latest_log(DEF_CLIENT_DIR)

    if not h_path or not os.path.isfile(h_path):
        print(f"HATA: Host logu bulunamadı: {h_path}")
        return 1
    if not c_path or not os.path.isfile(c_path):
        print(f"HATA: Client logu bulunamadı: {c_path}")
        return 1

    h = parse_log(h_path)
    c = parse_log(c_path)

    print("=" * 78)
    print("SUPERCAN CO-OP LOG DIFF RAPORU")
    print("=" * 78)
    print(f"HOST   : {h['name']}  (run_id={h['run_id']}, peer={','.join(h['peer_run_ids']) or '-'})")
    print(f"CLIENT : {c['name']}  (run_id={c['run_id']}, peer={','.join(c['peer_run_ids']) or '-'})")
    print(f"Haritalar -> HOST: {' -> '.join(h['levels']) or '-'} | CLIENT: {' -> '.join(c['levels']) or '-'}")
    print(f"Crash     -> HOST: {len(h['crashes'])} | CLIENT: {len(c['crashes'])}")
    print(f"F5 / Rev  -> HOST: F5={h['f5']} rev={h['revive']} | CLIENT: F5={c['f5']} rev={c['revive']}")
    print(f"Ses/Müzik -> HOST: snd={h['snd_tx']}/{h['snd_rx']} mus={len(h['mus'])} cut={len(h['cut'])} | "
          f"CLIENT: snd={c['snd_tx']}/{c['snd_rx']} mus={len(c['mus'])} cut={len(c['cut'])}")

    if h["crashes"] or c["crashes"]:
        print("\n[!] CRASH SATIRLARI:")
        for line in h["crashes"]:
            print("  HOST  :", line)
        for line in c["crashes"]:
            print("  CLIENT:", line)

    if h["ssn"] or c["ssn"]:
        print("\n--- SSN1 Snapshot ---")
        for line in h["ssn"]:
            print("  HOST  :", line)
        for line in c["ssn"]:
            print("  CLIENT:", line)

    if h["gfx"] or c["gfx"]:
        print("\n--- Grafik / Işık ([gfx]) ---")
        for line in h["gfx"]:
            print("  HOST  :", line)
        for line in c["gfx"]:
            print("  CLIENT:", line)

    # [sync] satırlarını yan yana kıyasla (oyun içi haritalarda menu.wmb ve boş seviye bekleme kaymasını gidererek hizala)
    print("\n--- [sync] Zaman Hizalı Kıyas (Host vs Client) ---")
    h_game = [s for s in h["syncs"] if s["level"] not in ("", "?", "menu.wmb")]
    c_game = [s for s in c["syncs"] if s["level"] not in ("", "?", "menu.wmb")]
    if h_game and c_game:
        h_syncs, c_syncs = h_game, c_game
        h_menu_n = len(h["syncs"]) - len(h_game)
        c_menu_n = len(c["syncs"]) - len(c_game)
        if h_menu_n or c_menu_n:
            print(f"  (Not: menü/yükleme satırları hizalama dışı bırakıldı: HOST={h_menu_n}, CLIENT={c_menu_n})")
    else:
        h_syncs, c_syncs = h["syncs"], c["syncs"]
    n = max(len(h_syncs), len(c_syncs))
    desyncs = []
    if n == 0:
        print("  ([sync] satırı yok — oturum <600 kare sürmüş)")
    else:
        print(f"  {'#':>2} {'Seviye':<10} | {'H.Scr/Dm':>9} {'C.Scr/Dm':>9} | {'H.Rbt/Env':>9} {'C.Rbt/Env':>9} | {'H.M/A':>5} {'C.M/A':>5} | {'Durum'}")
        print("  " + "-" * 74)
        for i in range(n):
            hs = h_syncs[i] if i < len(h_syncs) else None
            cs = c_syncs[i] if i < len(c_syncs) else None
            lvl = (hs["level"] if hs else cs["level"])[:10]
            if hs and cs:
                flags = []
                if hs["level"] != cs["level"]:
                    flags.append(f"LVL({hs['level']}!={cs['level']})")
                elif hs["level"] != "menu.wmb":
                    if abs(hs["score"] - cs["score"]) > 20:
                        flags.append(f"SCORE({hs['score']}!={cs['score']})")
                    if hs["diam"] != cs["diam"]:
                        flags.append(f"DIAM({hs['diam']}!={cs['diam']})")
                    if hs["mkr"] != cs["mkr"]:
                        flags.append(f"MKR({hs['mkr']}!={cs['mkr']})")
                    if hs["alrm"] != cs["alrm"]:
                        flags.append(f"ALRM({hs['alrm']}!={cs['alrm']})")
                status = "OK" if not flags else "FARK: " + ", ".join(flags)
                if flags:
                    desyncs.append((i + 1, status))
                print(
                    f"  {i+1:>2} {lvl:<10} | "
                    f"{hs['score']:>5}/{hs['diam']:<3} {cs['score']:>5}/{cs['diam']:<3} | "
                    f"{hs['rbt']:>4}/{hs['env']:<4} {cs['rbt']:>4}/{cs['env']:<4} | "
                    f"{hs['mkr']:>2}/{hs['alrm']:<2} {cs['mkr']:>2}/{cs['alrm']:<2} | {status}"
                )
            else:
                side = "HOST" if hs else "CLIENT"
                s = hs or cs
                print(f"  {i+1:>2} {lvl:<10} | ({side} tek başına: score={s['score']} diam={s['diam']})")

    print("\n--- Eşya (SIT3) Özeti ---")
    print(f"  HOST   : local_taken={len(h['itm_local'])}, rx_hid={len(h['itm_rx'])}")
    print(f"  CLIENT : local_taken={len(c['itm_local'])}, rx_hid={len(c['itm_rx'])}")
    if h["mkr"] or c["mkr"] or h["alrm"] or c["alrm"] or h["boss"] or c["boss"]:
        print("\n--- Makara / Alarm / Boss Olayları ---")
        for tag, key in [("MKR", "mkr"), ("ALRM", "alrm"), ("BOSS", "boss")]:
            if h[key] or c[key]:
                print(f"  {tag}: HOST={len(h[key])}, CLIENT={len(c[key])}")

    print("\n--- Sonuç ---")
    if h["crashes"] or c["crashes"]:
        print("  [UYARI] Oturumda crash tespit edildi!")
    elif desyncs:
        print(f"  [BILGI] {len(desyncs)} satırda geçici/kalıcı senkron farkı görüldü (son satır: {desyncs[-1][1]}).")
    else:
        print("  [TAMAM] Kritik senkron farkı veya crash yok.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
