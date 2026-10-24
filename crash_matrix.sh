#!/bin/bash

for i in {1..15}; do
    echo "Testing crash point #$i..."
    
    # 1. Reset filesystem (Compile and build fs.img)
    make clean > /dev/null
    make fs.img > /dev/null 2>&1
    
    # 2. Inject the crash at write 'i'
    # Wait 2 seconds for boot, then send the command
    (sleep 2; echo "test_crash $i") | timeout 4 make qemu CPUS=1 > /dev/null 2>&1
    
    # 3. Reboot, replay the request (n=0), and read the file
    # Wait 2s for boot, run test_crash 0, wait 1s for it to finish, run cat
    (sleep 2; echo "test_crash 0"; sleep 1; echo "cat /crash_test.txt") | timeout 5 make qemu CPUS=1 > result.log 2>&1
    
    # 4. Assert Exactly-Once Semantics
    COUNT=$(grep -o "CrashData" result.log | wc -l)
    
    if [ "$COUNT" -eq 1 ]; then
        echo "  [PASS] Recovery successful. Exactly-once guaranteed."
    else
        echo "  [FAIL] Data found $COUNT times!"
        break
    fi
done

echo "Matrix sweep complete."
