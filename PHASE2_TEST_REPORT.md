# Phase 2 — Targeted Regression Test Report

This report describes deterministic assertions in `tests/scheduler_tests.cpp`. The simulator is a discrete-time model; migration overhead is simulated time, not a measurement of hardware migration latency.

## Policy selection

A single core receives three jobs at time zero:

| PID | Burst | Priority |
|---:|---:|---:|
| 1 | 8 | 3 |
| 2 | 2 | 1 |
| 3 | 1 | 2 |

Expected execution order:

- FCFS: `1, 2, 3`
- SJF: `3, 2, 1`
- Priority (lower number is higher): `2, 3, 1`

## Preferred-core affinity

- When core loads tie, a process preferring core 1 is initially placed on core 1.
- With several processes all preferring core 0, soft affinity must not cause all CPU work to remain on core 0 when core 1 is less loaded. The test checks equal useful execution time across the two cores for its fixed workload.

## Pinned-core affinity

A Round Robin workload has two processes pinned to core 0 and enables load balancing while core 1 has spare capacity. The assertions verify that all process segments remain on core 0, core 1 remains idle, and no migrations or migration overhead are recorded. Another test checks pinning when tasks are assigned to both cores.

## Load-balancing regression

For the fixed Round Robin workload with burst lengths `2, 1, 2, 1, 1`, two cores, quantum 1, and no migration cost:

| Metric | Balancing off | Balancing on |
|---|---:|---:|
| Maximum observed task-count imbalance | 3 | 2 |
| Makespan | 5 | 4 |

The balancing-enabled run is expected to record one task migration. This is a regression for this particular workload, not a claim that balancing always improves every workload.

## Migration-overhead regression

For the fixed migration test workload with two cores, Round Robin quantum 1, balancing interval 1, and migration cost 2:

- Task migrations: `2`
- Migration overhead: `4` core-time units
- Visible migration segments: `2`
- Total duration of migration segments: `4`
- Makespan: `13`

## Build and run

```powershell
g++ src/main.cpp src/scheduler.cpp src/utils.cpp -Iinclude -std=c++17 -Wall -Wextra -Wpedantic -O2 -o scheduler.exe
g++ tests/scheduler_tests.cpp src/scheduler.cpp -Iinclude -std=c++17 -Wall -Wextra -Wpedantic -O2 -o scheduler_tests.exe
.\scheduler_tests.exe
```

Expected test output: `All scheduler tests passed.`
