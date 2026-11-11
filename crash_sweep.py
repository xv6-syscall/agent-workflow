#!/usr/bin/env python3
"""
crash_sweep.py -- crash-point sweep for sys_effect (Problem 4).

Every event inside one effect() call gets a number: each log_write() and each
stage inside commit()/install_trans() (see crash_point() in kernel/log.c).
For crash point n = 1, 2, 3, ... this script:

  1. restores a pristine fs.img (so every run starts from the same disk)
  2. boots QEMU, runs `test_crash n`, which arms crashat(n) and calls effect();
     the kernel panics at event n, which stands in for power loss
  3. reboots QEMU on the SAME fs.img (never rebuilt between steps 2 and 3,
     otherwise recovery has nothing to recover)
  4. replays the identical request with `test_crash 0`
  5. checks the file holds the data exactly once and the replay returned the
     stored byte count

It stops at the first n where effect() finishes without crashing, since that
means n-1 was the last crash point. Raw QEMU output for every boot goes to
sweep_logs/, and the table goes to crash_sweep_results.md.

Usage:  python3 crash_sweep.py [--toolprefix riscv64-linux-gnu-] [--cpus 1]
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import time

import pexpect

PAYLOAD = "CrashData\n"   # must match user/test_crash.c
EXPECT_RESULT = len(PAYLOAD)
LOGDIR = "sweep_logs"


def qemu_cmd(args):
    return f"make TOOLPREFIX={args.toolprefix} qemu CPUS={args.cpus}"


def kill_qemu(child):
    """Quit QEMU (Ctrl-A x works even after a guest panic), then make sure
    no orphan keeps the write lock on fs.img."""
    try:
        child.send("\x01x")
        child.expect(pexpect.EOF, timeout=3)
    except Exception:
        pass
    child.close(force=True)
    subprocess.run(["pkill", "-f", "qemu-system-riscv64"],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(0.5)


def boot(args, logname):
    logf = open(os.path.join(LOGDIR, logname), "w")
    child = pexpect.spawn(qemu_cmd(args), encoding="utf-8", timeout=30)
    child.logfile_read = logf
    return child, logf


def label(stage):
    if "Simulated crash point" in stage:
        return "log_write (staging)"
    return stage.replace("CRASHAT: ", "")


def run_point(args, n):
    shutil.copyfile(os.path.join(LOGDIR, "fs.img.pristine"), "fs.img")

    # ---- boot 1: crash at event n -------------------------------------
    child, logf = boot(args, f"point{n:02d}_boot1_crash.log")
    stage = None
    completed = False
    try:
        child.expect(r"\$ ")
        child.sendline(f"test_crash {n}")
        i = child.expect([r"CRASHAT: ([^\r\n]*)",
                          r"Effect returned: (-?\d+)",
                          pexpect.TIMEOUT], timeout=15)
        if i == 0:
            stage = "CRASHAT: " + child.match.group(1).strip()
        elif i == 1:
            completed = True
    finally:
        kill_qemu(child)
        logf.close()

    if completed:
        return None  # no crash at n: sweep is over
    if stage is None:
        return dict(n=n, stage="(no panic seen)", recovered=-1, result=None,
                    copies=-1, ok=False)

    # ---- boot 2: reboot on the same image, replay, inspect -------------
    child, logf = boot(args, f"point{n:02d}_boot2_replay.log")
    recovered = result = None
    copies = -1
    try:
        child.expect(r"\$ ")
        recovered = child.before.count("recovering tail")
        child.sendline("test_crash 0")
        child.expect(r"Effect returned: (-?\d+)")
        result = int(child.match.group(1))
        child.expect(r"\$ ")
        child.sendline("cat /crash_test.txt")
        child.expect(r"\$ ")
        copies = child.before.count("CrashData")
    except Exception as e:  # keep sweeping; the raw log shows what happened
        print(f"  (error on reboot: {e})", end="")
    finally:
        kill_qemu(child)
        logf.close()

    ok = (copies == 1 and result == EXPECT_RESULT)
    return dict(n=n, stage=label(stage), recovered=recovered, result=result,
                copies=copies, ok=ok)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--toolprefix", default="riscv64-linux-gnu-")
    ap.add_argument("--cpus", type=int, default=1)
    ap.add_argument("--max", type=int, default=64,
                    help="safety cap on crash points to try")
    args = ap.parse_args()

    os.makedirs(LOGDIR, exist_ok=True)
    print("Building kernel and fs.img once...")
    subprocess.run(["make", f"TOOLPREFIX={args.toolprefix}",
                    "kernel/kernel", "fs.img"], check=True,
                   stdout=subprocess.DEVNULL)
    shutil.copyfile("fs.img", os.path.join(LOGDIR, "fs.img.pristine"))

    rows = []
    for n in range(1, args.max + 1):
        print(f"[+] crash point {n}: ", end="", flush=True)
        row = run_point(args, n)
        if row is None:
            print("effect() completed without crashing -> sweep complete")
            break
        rows.append(row)
        print(f"{row['stage']}  copies={row['copies']} "
              f"result={row['result']}  {'PASS' if row['ok'] else 'FAIL'}")

    # ---- results table ------------------------------------------------
    lines = [
        "| Point | Where the machine died | Log blocks replayed at boot "
        "| Replay returned | Copies of data | Verdict |",
        "|------:|------------------------|----------------------------:"
        "|----------------:|---------------:|---------|",
    ]
    for r in rows:
        lines.append(
            f"| {r['n']} | {r['stage']} | {r['recovered']} | {r['result']} "
            f"| {r['copies']} | {'PASS' if r['ok'] else 'FAIL'} |")

    # Disk-write cost, derived from what the sweep observed:
    # B = blocks installed (one install_trans event per block).
    b = sum(1 for r in rows if r["stage"].startswith("install_trans"))
    summary = []
    if b:
        summary.append(
            f"\nBlocks in the effect transaction (from install_trans events): "
            f"B = {b}")
        summary.append(
            f"Disk writes per effect = B (log) + 1 (header) + B (install) "
            f"+ 1 (clear) = {2 * b + 2}")
        summary.append(
            "Extra writes caused by the effect table = 2 "
            "(the table block is logged once and installed once)")
    passed = sum(1 for r in rows if r["ok"])
    summary.append(f"\n{passed}/{len(rows)} crash points PASS "
                   f"(exactly once, replay returned {EXPECT_RESULT}).")

    out = "\n".join(lines + summary) + "\n"
    with open("crash_sweep_results.md", "w") as f:
        f.write(out)
    print("\n" + out)
    sys.exit(0 if rows and passed == len(rows) else 1)


if __name__ == "__main__":
    main()
