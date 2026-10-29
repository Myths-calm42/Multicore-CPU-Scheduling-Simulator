# Phase 1 verification report

## Scope

This revision completes the Phase 1 cleanup and baseline verification for the Multicore SMP CPU Scheduling Simulator.

## Changes in this update

- Replaced tab-separated process metrics with fixed-width columns using `std::setw`, fixing the concatenated-looking output in PowerShell.
- Expanded unit and invariant tests to cover:
  - FCFS process timing and metric identities.
  - Round Robin completion/waiting metrics.
  - Initial idle intervals and utilization.
  - SJF policy differences on a workload with competing ready processes.
  - Metric consistency across all four policies.
  - Per-core timeline continuity and exact total executed burst time.
  - No simultaneous execution of the same process on two cores.
  - Empty workloads and invalid core/quantum configuration.

## Verification performed

Commands run from the project root:

```bash
g++ src/main.cpp src/scheduler.cpp src/utils.cpp -Iinclude -std=c++17 -Wall -Wextra -Wpedantic -O2 -o scheduler
g++ tests/scheduler_tests.cpp src/scheduler.cpp -Iinclude -std=c++17 -Wall -Wextra -Wpedantic -O2 -o scheduler_tests
./scheduler_tests
```

Result:
- Application compilation: passed with no compiler warnings.
- Test compilation: passed with no compiler warnings.
- Automated test suite: `All scheduler tests passed.`
- Sample execution: checked with `input.txt`, one core and Round Robin quantum 2; process metric columns align correctly.

## Remaining limitations

This is still a discrete-time educational simulator. It does not yet model per-core ready queues, load balancing, task affinity, migration costs, I/O blocking, cache effects, or the real Linux kernel scheduler. Those belong to later milestones.
