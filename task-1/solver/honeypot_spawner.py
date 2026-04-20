#!/usr/bin/env python3
"""
honeypot_spawner.py
Program untuk memasukkan file berbahaya (.exe, .pcap) dan file noise (.sh, .bin, .txt)
ke dalam folder honeypot/ secara otomatis.
"""

import os
import random
import time
import argparse
import hashlib
from datetime import datetime

DEFAULT_TOTAL    = 15
DEFAULT_INTERVAL = 2
DEFAULT_DIR      = "honeypot"
DEFAULT_SEED     = 42

# ── Data file ─────────────────────────────────────────────────────────────────
DANGEROUS = [
    {"ext": ".exe", "names": [
        "backdoor", "keylogger", "rat_client", "reverse_shell",
        "mimikatz", "nc", "payload", "dropper", "exploit",
        "meterpreter", "shell32", "svchost_fake", "updater_fake",
    ]},
    {"ext": ".pcap", "names": [
        "capture_traffic", "network_dump", "session_hijack",
        "credentials_sniff", "dns_exfil", "c2_traffic",
        "lateral_movement", "data_exfil", "arp_poison",
    ]},
]

NOISE = [
    {"ext": ".sh",  "names": [
        "cleanup", "install", "setup", "backup", "cron_job",
        "monitor", "health_check", "restart",
    ]},
    {"ext": ".bin", "names": [
        "firmware_v1", "rom_dump", "memory_snapshot",
        "sensor_data", "calibration", "config_blob",
    ]},
    {"ext": ".txt", "names": [
        "readme", "notes", "todo", "config_draft", "log_summary",
    ]},
]

DANGER_RATIO = 0.6

# ── Konten file ───────────────────────────────────────────────────────────────
def make_exe_content(name, rng):
    header = b"MZ\x90\x00\x03\x00\x00\x00\x04\x00\x00\x00\xFF\xFF"
    filler = bytes([rng.randint(0, 255) for _ in range(32)])
    note   = f"\n[PRAKTIKUM OS - DO NOT MODIFY]\nFile: {name}.exe\n".encode()
    return header + filler + note

def make_pcap_content(name, rng):
    header = b"\xd4\xc3\xb2\xa1\x02\x00\x04\x00"
    header += b"\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\x00\x00\x01\x00\x00\x00"
    filler = bytes([rng.randint(0, 255) for _ in range(32)])
    note   = f"\n[PRAKTIKUM OS - DO NOT MODIFY]\nCapture: {name}\n".encode()
    return header + filler + note

def make_sh_content(name):
    return f"#!/bin/bash\n# [NOISE] Script: {name}.sh\necho 'Running {name}...'\n".encode()

def make_bin_content(name, rng):
    return bytes([rng.randint(0, 255) for _ in range(128)])

def make_txt_content(name):
    return f"[NOISE FILE]\nFilename: {name}.txt\nThis is a decoy file.\n".encode()

# ── Core ──────────────────────────────────────────────────────────────────────
def build_file_plan(total, seed):
    rng  = random.Random(seed)
    plan = []

    for i in range(total):
        is_dangerous = rng.random() < DANGER_RATIO
        if is_dangerous:
            category = rng.choice(DANGEROUS)
        else:
            category = rng.choice(NOISE)

        name   = rng.choice(category["names"])
        suffix = str(rng.randint(1000, 9999))
        ext    = category["ext"]
        fname  = f"{name}_{suffix}{ext}"

        plan.append({
            "index":        i + 1,
            "fname":        fname,
            "ext":          ext,
            "name":         name,
            "is_dangerous": is_dangerous,
        })

    return plan, rng

def write_file(target_dir, entry, rng):
    """Tulis satu file ke disk. Return (path, content_bytes)."""
    fpath = os.path.join(target_dir, entry["fname"])
    ext   = entry["ext"]
    name  = entry["name"]

    if   ext == ".exe":  content = make_exe_content(name, rng)
    elif ext == ".pcap": content = make_pcap_content(name, rng)
    elif ext == ".sh":   content = make_sh_content(name)
    elif ext == ".bin":  content = make_bin_content(name, rng)
    else:                content = make_txt_content(name)

    with open(fpath, "wb") as f:
        f.write(content)

    return fpath, content

def compute_session_hash(records):
    combined = "|".join(f"{r['fname']}:{r['filehash']}" for r in records)
    return hashlib.sha256(combined.encode()).hexdigest()

# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(
        description="Honeypot Spawner"
    )
    parser.add_argument("--total",    type=int,   default=DEFAULT_TOTAL,
                        help=f"Jumlah file yang di-spawn (default: {DEFAULT_TOTAL})")
    parser.add_argument("--interval", type=float, default=DEFAULT_INTERVAL,
                        help=f"Jeda antar spawn dalam detik (default: {DEFAULT_INTERVAL})")
    parser.add_argument("--dir",      type=str,   default=DEFAULT_DIR,
                        help=f"Direktori target (default: {DEFAULT_DIR})")
    parser.add_argument("--seed",     type=int,   default=DEFAULT_SEED,
                        help=f"Seed untuk hasil deterministik (default: {DEFAULT_SEED})")
    args = parser.parse_args()

    os.makedirs(args.dir, exist_ok=True)

    print(f"{'='*60}")
    print(f"  Honeypot Spawner ")
    print(f"{'='*60}")
    print(f"  Target dir : {args.dir}/")
    print(f"  Total file : {args.total}")
    print(f"  Interval   : {args.interval} detik")
    print(f"  Seed       : {args.seed}")
    print(f"{'='*60}\n")

    plan, rng = build_file_plan(args.total, args.seed)

    records   = []
    spawned   = 0
    dangerous = 0
    noise_cnt = 0

    try:
        for entry in plan:
            fpath, content = write_file(args.dir, entry, rng)
            filehash       = hashlib.sha256(content).hexdigest()[:16]
            tag            = "BERBAHAYA" if entry["is_dangerous"] else "NOISE    "
            ts             = datetime.now().strftime("%H:%M:%S")

            print(f"[{ts}] ({entry['index']:>3}/{args.total}) "
                  f"{entry['fname']:<42} hash:{filehash}")

            records.append({"fname": entry["fname"], "filehash": filehash})
            spawned += 1
            if entry["is_dangerous"]:
                dangerous += 1
            else:
                noise_cnt += 1

            if entry["index"] < args.total:
                time.sleep(args.interval)

    except KeyboardInterrupt:
        print("\n[!] Dihentikan oleh pengguna.")

    # ── Ringkasan ─────────────────────────────────────────────────────────────
    session_hash    = compute_session_hash(records)
    dangerous_files = [
        r["fname"] for r in records
        if r["fname"].endswith(".exe") or r["fname"].endswith(".pcap")
    ]

    print(f"\n{'='*60}")
    print(f"  Selesai. {spawned} file di-spawn ke '{args.dir}/'")
    print(f"  Berbahaya (.exe / .pcap) : {dangerous}")
    print(f"  Noise (.sh / .bin / .txt): {noise_cnt}")
    print(f"\n  SESSION HASH : {session_hash}")
    print(f"  Tunjukkan SESSION HASH ini ke asisten.")
    print(f"{'='*60}")

if __name__ == "__main__":
    main()
