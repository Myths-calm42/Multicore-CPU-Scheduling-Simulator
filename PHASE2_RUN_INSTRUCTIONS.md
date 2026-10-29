# Phase 2 — Build and Run (Windows PowerShell)

## 1. Extract the ZIP
Extract the complete `Multicore-SMP-CPU-Scheduling-updated` folder. Open PowerShell in that folder (the project root, where `input.txt` is located).

## 2. Compile the program
Make sure you compile from the extracted Phase 2 folder. This command overwrites an older executable with the Phase 2 build:

```powershell
g++ src/main.cpp src/scheduler.cpp src/utils.cpp -Iinclude -std=c++17 -Wall -Wextra -Wpedantic -O2 -o scheduler.exe
```

## 3. Run the built-in tests

```powershell
g++ tests/scheduler_tests.cpp src/scheduler.cpp -Iinclude -std=c++17 -Wall -Wextra -Wpedantic -O2 -o scheduler_tests.exe
.\scheduler_tests.exe
```

Expected: `All scheduler tests passed.`

The regression tests include exact policy-order checks, soft-affinity tie-breaking, pinned tasks with an idle alternate core, a balancing on/off comparison, and exact migration/overhead accounting. See `PHASE2_TEST_REPORT.md` for the cases and expected values.

## 4. Run with the standard workload

```powershell
.\scheduler.exe
```
Choose `2` for file input. The included root `input.txt` uses the four required columns: `PID ArrivalTime BurstTime Priority`.

## 5. Run the affinity/load-balancing demonstration

First back up the standard workload, then copy the demo workload into the root `input.txt` file:

```powershell
Copy-Item .\input.txt .\input.backup.txt
Copy-Item .\workloads\affinity_load_balance_demo.txt .\input.txt -Force
```

Run the program:

```powershell
.\scheduler.exe
```

Enter these choices when prompted:

- Input choice: `2`
- CPU cores: `2`
- Round Robin quantum: `1`
- Enable load balancing: `1`
- Load-balancing interval: `1`
- Migration overhead: `2`

**Important:** The Phase 2 executable must be recompiled using Step 2. If you still see `Malformed input file. Expected: PID ArrivalTime BurstTime Priority`, you are running an older executable or are not in the extracted Phase 2 project folder.

Restore the standard input after the demo:

```powershell
Copy-Item .\input.backup.txt .\input.txt -Force
```

The demo file belongs in `workloads/`, not `src/`. Do not copy the demo into `input.txt` until you are ready to run this specific test.
