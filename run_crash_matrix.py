import pexpect
import subprocess

def main():
    print("Running crash matrix test (15 points)...\n")

    for i in range(1, 16):
        print(f"Testing crashpoint {i}/15...", end=" ", flush=True)

        # Clean build before each run
        subprocess.run(["make", "clean"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(["make", "fs.img"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        # Boot QEMU and inject crash
        child = pexpect.spawn("make TOOLPREFIX=riscv64-linux-gnu- qemu CPUS=1", encoding="utf-8", timeout=10)
        try:
            child.expect(r'\$ ')
            child.sendline(f"test_crash {i}")
            child.expect([r'CRASHAT:', r'panic', pexpect.TIMEOUT], timeout=5)
        except Exception:
            pass
        finally:
            child.close(force=True)

        # Reboot QEMU, replay, and check file
        child2 = pexpect.spawn("make TOOLPREFIX=riscv64-linux-gnu- qemu CPUS=1", encoding="utf-8", timeout=10)
        count = 0
        try:
            child2.expect(r'\$ ')
            child2.sendline("test_crash 0")
            child2.expect(r'\$ ')
            child2.sendline("cat /crash_test.txt")
            child2.expect(r'\$ ')
            output = child2.before
            count = output.count("CrashData")
        except Exception as e:
            print(f"error on reboot: {e}")
        finally:
            child2.sendline("\x01x")
            child2.close(force=True)

        if count == 1:
            print("PASS")
        else:
            print(f"FAIL (found {count} copies of data)")
            break

    print("\nSweep finished.")

if __name__ == "__main__":
    main()
