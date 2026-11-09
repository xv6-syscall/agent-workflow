#!/bin/bash
python3 -c '
import pexpect, subprocess

print("====================================================")
print(" Starting 15-Point WAL Crash Consistency Matrix Sweep")
print("====================================================")

table_lines = [
    "| Crash Point | Status | Observed Data Count | Result Guarantee |",
    "|-------------|--------|---------------------|------------------|"
]

for i in range(1, 16):
    print(f"[+] Testing Crash Point #{i} / 15... ", end="", flush=True)

    subprocess.run(["make", "clean"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["make", "fs.img"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    child = pexpect.spawn("make TOOLPREFIX=riscv64-linux-gnu- qemu CPUS=1", encoding="utf-8", timeout=10)
    try:
        child.expect(r"\$ ")
        child.sendline(f"test_crash {i}")
        child.expect([r"CRASHAT:", r"panic", pexpect.TIMEOUT], timeout=5)
    except Exception:
        pass
    finally:
        child.close(force=True)

    child2 = pexpect.spawn("make TOOLPREFIX=riscv64-linux-gnu- qemu CPUS=1", encoding="utf-8", timeout=10)
    count = 0
    try:
        child2.expect(r"\$ ")
        child2.sendline("test_crash 0")
        child2.expect(r"\$ ")
        child2.sendline("cat /crash_test.txt")
        child2.expect(r"\$ ")
        count = child2.before.count("CrashData")
    except Exception:
        pass
    finally:
        child2.sendline("\x01x")
        child2.close(force=True)

    if count == 1:
        print("PASS (Exactly Once)")
        table_lines.append(f"| {i:^11} | PASS   | {count:^19} | Exactly Once     |")
    else:
        print(f"FAIL (Data Count: {count})")
        table_lines.append(f"| {i:^11} | FAIL   | {count:^19} | Violation        |")
        break

print("\n====================================================")
table_content = "\n".join(table_lines)
print(table_content)
with open("crash_summary.txt", "w") as f:
    f.write(table_content + "\n")
'
