| Point | Where the machine died | Log blocks replayed at boot | Replay returned | Copies of data | Verdict |
|------:|------------------------|----------------------------:|----------------:|---------------:|---------|
| 1 | log_write: staging | 0 | 10 | 1 | PASS |
| 2 | log_write: staging | 0 | 10 | 1 | PASS |
| 3 | log_write: staging | 0 | 10 | 1 | PASS |
| 4 | log_write: staging | 0 | 10 | 1 | PASS |
| 5 | log_write: staging | 0 | 10 | 1 | PASS |
| 6 | log_write: staging | 0 | 10 | 1 | PASS |
| 7 | log_write: staging | 0 | 10 | 1 | PASS |
| 8 | log_write: staging | 0 | 10 | 1 | PASS |
| 9 | log_write: staging | 0 | 10 | 1 | PASS |
| 10 | log_write: staging | 0 | 10 | 1 | PASS |
| 11 | commit: log blocks written, header not landed | 0 | 10 | 1 | PASS |
| 12 | commit: header landed, nothing installed | 6 | 10 | 1 | PASS |
| 13 | install_trans: one block installed, header still set | 6 | 10 | 1 | PASS |
| 14 | install_trans: one block installed, header still set | 6 | 10 | 1 | PASS |
| 15 | install_trans: one block installed, header still set | 6 | 10 | 1 | PASS |
| 16 | install_trans: one block installed, header still set | 6 | 10 | 1 | PASS |
| 17 | install_trans: one block installed, header still set | 6 | 10 | 1 | PASS |
| 18 | install_trans: one block installed, header still set | 6 | 10 | 1 | PASS |
| 19 | commit: all blocks installed, header not cleared | 6 | 10 | 1 | PASS |
| 20 | commit: header cleared, transaction complete | 0 | 10 | 1 | PASS |

Blocks in the effect transaction (from install_trans events): B = 6
Disk writes per effect = B (log) + 1 (header) + B (install) + 1 (clear) = 14
Extra writes caused by the effect table = 2 (the table block is logged once and installed once)

20/20 crash points PASS (exactly once, replay returned 10).
