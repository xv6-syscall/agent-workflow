pls note: the Agent part assumes u are working in a lima instance terminal (because thats where the project was built natively)

make clean && make qemu
make CPUS=1 qemu
make CPUS=3 qemu

### in xv6 kernel

hello 1 2 3 4
test_effect
test_conc
bench_effect
effect 42 0 /orders.txt "ItemA"
cat /orders.txt


### crash sweep

pip install pexpect
make clean
python3 crash_sweep2.py --toolprefix riscv64-linux-gnu- --cpus 1
cat crash_sweep_results.md
ls sweep_logs/

### agent xv6 bridge ollama
ollama pull llama3.1
ollama serve
python3 agent.py "Store record with key 9001 and text ItemA to /orders.txt"

### debug in a commit

make CPUS=1 qemu-gdb
riscv64-unknown-elf-gdb
target remote :26000
