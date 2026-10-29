# Multicore SMP CPU Scheduling Simulator

A C++17 command-line simulator for comparing CPU scheduling policies across multiple simulated CPU cores. The project models per-core ready queues, optional load balancing, preferred/pinned CPU affinity, and configurable task migration overhead.

## Implemented Features

- FCFS, non-preemptive SJF, non-preemptive Priority, and Round Robin scheduling.
- Independent per-core ready queues and concurrent simulated core timelines.
- Arrival-aware dispatching and Round Robin time slices.
- Optional periodic load balancing between per-core ready queues.
- Preferred-core affinity (soft hint) and pinned-core affinity (hard restriction).
- Configurable migration overhead when a task resumes on a different core from its previous execution.
- Explicit `MIGRATION` Gantt segments and counts for task migrations and migration overhead.
- Per-process completion, turnaround, waiting, and response times.
- Average metrics, CPU utilization including overhead, useful CPU utilization, throughput, makespan, estimated context switches, and maximum observed task-count imbalance.
- Input validation and separate simulation state for each scheduling policy.
- Automated invariant and regression tests covering scheduling order, affinity, load balancing, and migration-overhead accounting.

## Model Assumptions and Limitations

- Time is represented in discrete integer units. Each process has one CPU burst and no I/O blocking.
- Lower numeric priority means higher priority.
- SJF and Priority are non-preemptive; Round Robin uses time slices.
- Each process resides in one core's ready queue at a time. Load balancing moves eligible queued, unpinned tasks to a less-loaded queue; it does not forcibly preempt a running process.
- `preferredCore` is a soft hint. `pinnedCore` is a hard restriction. `-1` means unrestricted or unspecified.
- Migration cost is a configurable simulated core-time penalty paid when a previously executed process is dispatched on a different core. It is a model parameter, not a measured hardware cost.
- CPU utilization including overhead counts useful execution and modeled migration overhead. Useful CPU utilization excludes migration overhead.
- Context-switch count is an estimate derived from adjacent task segments, not a hardware measurement.
- This is an educational simulator, not a replacement for the Linux kernel scheduler. Real Linux behavior will be investigated separately in the planned Linux experiment.

## Project Structure

```text
include/
  process.h
  core.h
  scheduler.h
  utils.h
src/
  main.cpp
  scheduler.cpp
  utils.cpp
tests/
  scheduler_tests.cpp
workloads/
  affinity_load_balance_demo.txt
input.txt
README.md
PHASE1_VERIFICATION.md
PHASE2_IMPLEMENTATION.md
PHASE2_RUN_INSTRUCTIONS.md
PHASE2_TEST_REPORT.md
```

## Input Format

Each process is specified on one line:

```text
PID ArrivalTime BurstTime Priority [PreferredCore] [PinnedCore]
```

The last two columns are optional and default to `-1` when omitted. Core indexes start at `0`.

Example workload for four simulated cores:

```text
# PID Arrival Burst Priority PreferredCore PinnedCore
1 0 5 2 -1 -1
2 1 3 1 0 -1
3 2 8 3 -1 2
4 3 6 2 2 2
```

Process 2 prefers core 0 but may run elsewhere. Processes 3 and 4 are pinned to core 2. A pinned process cannot be moved by load balancing.

## Build and Run

### Requirements

- A C++17-compatible compiler, such as GCC or MinGW-w64.
- A terminal such as PowerShell on Windows or a Linux shell.

### Compile

Run from the project root:

```bash
g++ src/main.cpp src/scheduler.cpp src/utils.cpp -Iinclude -std=c++17 -Wall -Wextra -Wpedantic -O2 -o scheduler
```

On Windows PowerShell with MinGW-w64:

```powershell
g++ src/main.cpp src/scheduler.cpp src/utils.cpp -Iinclude -std=c++17 -Wall -Wextra -Wpedantic -O2 -o scheduler.exe
```

### Execute

Linux or a compatible shell:

```bash
./scheduler
```

Windows PowerShell:

```powershell
.\scheduler.exe
```

Run the program from the project root so it can find `input.txt`.

When prompted, choose the number of cores, Round Robin quantum, whether to enable load balancing, the balancing interval, and migration overhead. Use migration overhead `0` to disable the modeled migration cost.

## Run Automated Tests

Compile the test suite:

```bash
g++ tests/scheduler_tests.cpp src/scheduler.cpp -Iinclude -std=c++17 -Wall -Wextra -Wpedantic -O2 -o scheduler_tests
```

Run on Linux or a compatible shell:

```bash
./scheduler_tests
```

Run in Windows PowerShell:

```powershell
.\scheduler_tests.exe
```

Expected output:

```text
All scheduler tests passed.
```

See `PHASE2_TEST_REPORT.md` for the recorded validation details.

## Phase 2 Validation

The Phase 2 regression tests cover the following behaviors:

- Preferred-core affinity is a soft hint: it is honored when the preferred core is tied for least-loaded; otherwise, a least-loaded eligible core is selected.
- Pinned-core affinity remains a hard restriction.
- FCFS, SJF, and Priority execution order is verified for a single-core workload in which all jobs are ready at time zero.
- A tie-breaking test verifies preferred-core selection when both cores have equal load.
- A separate test verifies that a soft preference does not override least-load placement.
- Pinned tasks are tested when multiple pinned jobs share one core while another core remains idle, and during load balancing.
- A deterministic load-balancing regression compares enabled and disabled balancing, checking the expected change in maximum observed task-count imbalance from 3 to 2 and makespan from 5 to 4.
- A deterministic migration test asserts exactly two migrations, four migration-overhead core-time units, two visible migration segments, and a makespan of 13 for its fixed workload.

**Metric definition:** Maximum observed task-count imbalance is the difference between the most- and least-loaded cores by queued-plus-running task count. It is a simple simulator metric, not an estimate of remaining work or real-hardware performance.

## NVIDIA-Oriented Development Roadmap

1. **Phase 1 — Simulator baseline:** Implement scheduling policies and verify baseline metrics.
2. **Phase 2 — Multicore scheduling:** Add per-core queues, load balancing, CPU affinity, migration-cost modeling, and automated tests.
3. **Phase 3 — Linux experiments:** Build a separate C++ experiment using real threads, CPU affinity, and repeatable measurements.
4. **Phase 4 — Benchmarking and analysis:** Add CSV export, workload generation, benchmark analysis, and expanded documentation.

## Scope

This project is intended for learning and experimentation with operating-system scheduling concepts, multicore workload distribution, and CPU affinity. Its simulated timings and migration costs are not measurements of NVIDIA hardware or the Linux kernel scheduler.