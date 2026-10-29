# Phase 2 implementation notes

## Added

- Per-core ready queues; each core dispatches from its own queue.
- Optional periodic load balancing. Only queued tasks are moved, and pinned tasks are never moved away from their pinned core.
- Preferred core as a soft hint (used to break ties between equally loaded cores) and pinned core as a hard restriction.
- Configurable migration penalty paid when a process is dispatched on a core different from the core on which it last executed.
- Explicit `MIGRATION` timeline segments, task migration count, migration overhead time, useful CPU utilization, total CPU utilization, and maximum observed task-count imbalance (queued plus running tasks; not an estimate of remaining CPU work).
- Optional affinity columns in the input file: `PID Arrival Burst Priority [PreferredCore] [PinnedCore]`.
- Automated tests for pinning correctness, balancing-induced migrations, overhead accounting, and invalid affinity settings.

## Verification

Build the application:

```bash
g++ src/main.cpp src/scheduler.cpp src/utils.cpp -Iinclude -std=c++17 -Wall -Wextra -Wpedantic -O2 -o scheduler
```

Build and run tests:

```bash
g++ tests/scheduler_tests.cpp src/scheduler.cpp -Iinclude -std=c++17 -Wall -Wextra -Wpedantic -O2 -o scheduler_tests
./scheduler_tests
```

## Modeling caveat

The load balancer is a deterministic educational policy based on queued-plus-running task counts. Migration overhead is an explicit configurable simulation cost, not an empirical claim about real CPU migration latency. The separate Linux experiment is needed to measure actual OS behavior.

## Policy and load-balancing validation improvements

- Added a same-arrival, single-core test that verifies FCFS selects the earliest queued task, SJF selects the shortest queued task, and Priority selects the numerically lowest priority value.
- Added a soft-affinity test that verifies preferred-core hints do not overload one core when another is less loaded.
- Added a load-balancing test that verifies pinned processes never leave their pinned core.
- Kept a deterministic Round Robin workload that exercises cross-core migrations and verifies that each migration's configured overhead is represented in the timeline and metrics.
- The reported imbalance is a task-count difference (`queued + running`), not a prediction of remaining execution time. It should be interpreted as a simple educational queue-pressure metric.
