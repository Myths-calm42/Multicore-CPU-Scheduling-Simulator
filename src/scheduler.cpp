#include "scheduler.h"

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct RunningTask {
    bool active = false;
    std::size_t processIndex = 0;
    int sliceRemaining = 0;
    int migrationRemaining = 0;
};

const char* PolicyName(Policy policy) {
    switch (policy) {
        case Policy::FCFS: return "FCFS";
        case Policy::SJF: return "Shortest Job First (non-preemptive)";
        case Policy::Priority: return "Priority (non-preemptive; lower number = higher priority)";
        case Policy::RoundRobin: return "Round Robin";
    }
    return "Unknown";
}

void AppendSegment(Core& core, int pid, int start, int end) {
    if (end <= start) return;
    if (!core.gantt.empty() && core.gantt.back().pid == pid &&
        core.gantt.back().end == start) {
        core.gantt.back().end = end;
    } else {
        core.gantt.push_back({pid, start, end});
    }
}
}

SimulationResult Simulate(const std::vector<Process>& input,
                          int coreCount, Policy policy, int quantum,
                          const SchedulingOptions& options) {
    if (coreCount <= 0) throw std::invalid_argument("Core count must be positive.");
    if (policy == Policy::RoundRobin && quantum <= 0)
        throw std::invalid_argument("Round Robin quantum must be positive.");
    if (options.balanceInterval <= 0)
        throw std::invalid_argument("Load-balancing interval must be positive.");
    if (options.migrationCost < 0)
        throw std::invalid_argument("Migration cost cannot be negative.");
    for (const Process& p : input) {
        if (p.pinnedCore < -1 || p.pinnedCore >= coreCount)
            throw std::invalid_argument("Pinned core must be -1 or a valid core index.");
        if (p.preferredCore < -1 || p.preferredCore >= coreCount)
            throw std::invalid_argument("Preferred core must be -1 or a valid core index.");
        if (p.pinnedCore >= 0 && p.preferredCore >= 0 && p.pinnedCore != p.preferredCore)
            throw std::invalid_argument("A process cannot prefer a different core from its pinned core.");
    }

    SimulationResult result;
    result.algorithm = PolicyName(policy);
    result.processes = input;
    result.cores.resize(static_cast<std::size_t>(coreCount));
    for (int c = 0; c < coreCount; ++c)
        result.cores[static_cast<std::size_t>(c)].coreId = c;

    const std::size_t n = input.size();
    if (n == 0) return result;

    std::vector<int> remaining(n);
    std::vector<int> firstStart(n, -1);
    std::vector<int> completion(n, -1);
    std::vector<int> lastExecutedCore(n, -1);
    std::vector<std::size_t> arrivalOrder(n);
    std::iota(arrivalOrder.begin(), arrivalOrder.end(), 0);
    for (std::size_t i = 0; i < n; ++i) remaining[i] = input[i].burstTime;
    std::stable_sort(arrivalOrder.begin(), arrivalOrder.end(), [&](std::size_t a, std::size_t b) {
        if (input[a].arrivalTime != input[b].arrivalTime)
            return input[a].arrivalTime < input[b].arrivalTime;
        return a < b;
    });

    std::vector<std::deque<std::size_t>> queues(static_cast<std::size_t>(coreCount));
    std::vector<RunningTask> running(static_cast<std::size_t>(coreCount));
    std::size_t nextArrival = 0;
    std::size_t completedCount = 0;
    int time = 0;

    auto loadOf = [&](int core) -> int {
        return static_cast<int>(queues[static_cast<std::size_t>(core)].size()) +
               (running[static_cast<std::size_t>(core)].active ? 1 : 0);
    };
    auto recordImbalance = [&]() {
        if (coreCount < 2) return;
        int minLoad = std::numeric_limits<int>::max();
        int maxLoad = 0;
        for (int c = 0; c < coreCount; ++c) {
            const int load = loadOf(c);
            minLoad = std::min(minLoad, load);
            maxLoad = std::max(maxLoad, load);
        }
        result.maxLoadImbalance = std::max(result.maxLoadImbalance, maxLoad - minLoad);
    };

    auto chooseTargetCore = [&](std::size_t idx) -> int {
        const Process& p = input[idx];
        if (p.pinnedCore >= 0) return p.pinnedCore;

        // A preferred core is a soft hint, not an implicit pin. Honor it when
        // it is tied for least-loaded; otherwise use the least-loaded core to
        // avoid creating a queue hotspot. Ties remain deterministic by core ID.
        int leastLoadedCore = 0;
        for (int c = 1; c < coreCount; ++c) {
            if (loadOf(c) < loadOf(leastLoadedCore)) leastLoadedCore = c;
        }
        if (p.preferredCore >= 0 &&
            loadOf(p.preferredCore) == loadOf(leastLoadedCore)) {
            return p.preferredCore;
        }
        return leastLoadedCore;
    };

    auto admitArrivals = [&]() {
        while (nextArrival < n && input[arrivalOrder[nextArrival]].arrivalTime <= time) {
            const std::size_t idx = arrivalOrder[nextArrival++];
            const int target = chooseTargetCore(idx);
            queues[static_cast<std::size_t>(target)].push_back(idx);
        }
    };

    auto chooseReadyPosition = [&](const std::deque<std::size_t>& ready) -> std::size_t {
        if (policy == Policy::FCFS || policy == Policy::RoundRobin) return 0;
        std::size_t bestPos = 0;
        for (std::size_t pos = 1; pos < ready.size(); ++pos) {
            const Process& candidate = input[ready[pos]];
            const Process& best = input[ready[bestPos]];
            bool better = false;
            if (policy == Policy::SJF) {
                better = candidate.burstTime < best.burstTime ||
                    (candidate.burstTime == best.burstTime && candidate.arrivalTime < best.arrivalTime);
            } else if (policy == Policy::Priority) {
                better = candidate.priority < best.priority ||
                    (candidate.priority == best.priority && candidate.arrivalTime < best.arrivalTime);
            }
            if (better) bestPos = pos;
        }
        return bestPos;
    };

    auto balanceQueues = [&]() {
        if (!options.loadBalancing || coreCount < 2) return;
        // Move queued tasks from overloaded to underloaded cores. Pinned tasks
        // can only remain on their assigned core; preferred-core affinity is a hint.
        bool moved = true;
        std::size_t moveGuard = 0;
        while (moved && moveGuard++ < n * static_cast<std::size_t>(coreCount) + 1) {
            moved = false;
            int source = -1, destination = -1;
            for (int s = 0; s < coreCount; ++s) {
                for (int d = 0; d < coreCount; ++d) {
                    if (s == d || loadOf(s) <= loadOf(d) + 1) continue;
                    const auto& sourceQueue = queues[static_cast<std::size_t>(s)];
                    const bool hasMovable = std::any_of(sourceQueue.begin(), sourceQueue.end(), [&](std::size_t idx) {
                        return input[idx].pinnedCore < 0 || input[idx].pinnedCore == d;
                    });
                    if (hasMovable && (source < 0 || loadOf(s) - loadOf(d) > loadOf(source) - loadOf(destination))) {
                        source = s;
                        destination = d;
                    }
                }
            }
            if (source >= 0 && destination >= 0) {
                auto& sourceQueue = queues[static_cast<std::size_t>(source)];
                auto it = std::find_if(sourceQueue.rbegin(), sourceQueue.rend(), [&](std::size_t idx) {
                    return input[idx].pinnedCore < 0 || input[idx].pinnedCore == destination;
                });
                if (it != sourceQueue.rend()) {
                    const std::size_t idx = *it;
                    sourceQueue.erase(std::next(it).base());
                    queues[static_cast<std::size_t>(destination)].push_back(idx);
                    moved = true;
                }
            }
        }
    };

    admitArrivals();
    while (completedCount < n) {
        if (options.loadBalancing && time % options.balanceInterval == 0) balanceQueues();
        recordImbalance();

        // Each core dispatches only from its own ready queue.
        for (int c = 0; c < coreCount; ++c) {
            RunningTask& slot = running[static_cast<std::size_t>(c)];
            auto& ready = queues[static_cast<std::size_t>(c)];
            if (slot.active || ready.empty()) continue;
            const std::size_t pos = chooseReadyPosition(ready);
            auto it = ready.begin() + static_cast<std::ptrdiff_t>(pos);
            const std::size_t idx = *it;
            ready.erase(it);
            slot.active = true;
            slot.processIndex = idx;
            slot.sliceRemaining = policy == Policy::RoundRobin ? std::min(quantum, remaining[idx]) : std::numeric_limits<int>::max();
            slot.migrationRemaining = (lastExecutedCore[idx] >= 0 && lastExecutedCore[idx] != c)
                ? options.migrationCost : 0;
            if (lastExecutedCore[idx] >= 0 && lastExecutedCore[idx] != c) ++result.taskMigrations;
        }

        std::vector<std::size_t> quantumExpired;
        for (int c = 0; c < coreCount; ++c) {
            RunningTask& slot = running[static_cast<std::size_t>(c)];
            Core& core = result.cores[static_cast<std::size_t>(c)];
            if (!slot.active) {
                AppendSegment(core, -1, time, time + 1);
                continue;
            }
            const std::size_t idx = slot.processIndex;
            if (slot.migrationRemaining > 0) {
                AppendSegment(core, -2, time, time + 1);
                --slot.migrationRemaining;
                ++result.migrationOverheadTime;
                continue;
            }
            AppendSegment(core, input[idx].pid, time, time + 1);
            if (firstStart[idx] == -1) firstStart[idx] = time;
            lastExecutedCore[idx] = c;
            --remaining[idx];
            ++result.totalBusyTime;
            if (policy == Policy::RoundRobin) --slot.sliceRemaining;
        }

        ++time;
        // Completion first, then admit arrivals, then requeue expired RR tasks.
        for (int c = 0; c < coreCount; ++c) {
            RunningTask& slot = running[static_cast<std::size_t>(c)];
            if (!slot.active || slot.migrationRemaining > 0) continue;
            const std::size_t idx = slot.processIndex;
            if (remaining[idx] == 0) {
                completion[idx] = time;
                slot.active = false;
                ++completedCount;
            } else if (policy == Policy::RoundRobin && slot.sliceRemaining == 0) {
                quantumExpired.push_back(idx);
                slot.active = false;
            }
        }
        admitArrivals();
        // Expired processes return to the queue of the core where their slice ended.
        for (std::size_t idx : quantumExpired) {
            int target = lastExecutedCore[idx];
            if (target < 0) target = chooseTargetCore(idx);
            queues[static_cast<std::size_t>(target)].push_back(idx);
        }
        recordImbalance();
    }

    result.makespan = time;
    double totalWait = 0.0, totalTurnaround = 0.0, totalResponse = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        Process& p = result.processes[i];
        p.completionTime = completion[i];
        p.turnaroundTime = completion[i] - p.arrivalTime;
        p.waitingTime = p.turnaroundTime - p.burstTime;
        p.responseTime = firstStart[i] - p.arrivalTime;
        totalWait += p.waitingTime;
        totalTurnaround += p.turnaroundTime;
        totalResponse += p.responseTime;
    }
    result.averageWaitingTime = totalWait / n;
    result.averageTurnaroundTime = totalTurnaround / n;
    result.averageResponseTime = totalResponse / n;
    const long long occupiedTime = result.totalBusyTime + result.migrationOverheadTime;
    if (result.makespan > 0) {
        const double capacity = static_cast<double>(result.makespan) * coreCount;
        result.cpuUtilizationPercent = 100.0 * occupiedTime / capacity;
        result.usefulCpuUtilizationPercent = 100.0 * result.totalBusyTime / capacity;
        result.throughput = static_cast<double>(n) / result.makespan;
    }

    for (const auto& core : result.cores) {
        int previousPid = -1;
        for (const auto& entry : core.gantt) {
            if (entry.pid < 0) { previousPid = -1; continue; }
            if (previousPid >= 0 && previousPid != entry.pid) ++result.contextSwitches;
            previousPid = entry.pid;
        }
    }
    return result;
}

void PrintResult(const SimulationResult& result) {
    std::cout << "\n=== " << result.algorithm << " ===\n";
    for (const auto& core : result.cores) {
        std::cout << "Core " << core.coreId << ": ";
        if (core.gantt.empty()) std::cout << "(no activity)";
        else {
            for (const auto& entry : core.gantt) {
                if (entry.pid == -1) std::cout << "| IDLE [" << entry.start << "," << entry.end << ") ";
                else if (entry.pid == -2) std::cout << "| MIGRATION [" << entry.start << "," << entry.end << ") ";
                else std::cout << "| P" << entry.pid << " [" << entry.start << "," << entry.end << ") ";
            }
            std::cout << "|";
        }
        std::cout << '\n';
    }

    std::cout << "\nPer-process metrics:\n" << std::left
              << std::setw(6) << "PID" << std::setw(10) << "Arrival" << std::setw(8) << "Burst"
              << std::setw(10) << "Priority" << std::setw(12) << "Completion"
              << std::setw(12) << "Turnaround" << std::setw(10) << "Waiting"
              << std::setw(10) << "Response" << '\n';
    for (const auto& p : result.processes) {
        std::cout << std::left << std::setw(6) << p.pid << std::setw(10) << p.arrivalTime
                  << std::setw(8) << p.burstTime << std::setw(10) << p.priority
                  << std::setw(12) << p.completionTime << std::setw(12) << p.turnaroundTime
                  << std::setw(10) << p.waitingTime << std::setw(10) << p.responseTime << '\n';
    }
    std::cout << std::fixed << std::setprecision(2)
              << "\nAverage waiting time: " << result.averageWaitingTime
              << "\nAverage turnaround time: " << result.averageTurnaroundTime
              << "\nAverage response time: " << result.averageResponseTime
              << "\nCPU utilization (including migration overhead): " << result.cpuUtilizationPercent << "%"
              << "\nUseful CPU utilization: " << result.usefulCpuUtilizationPercent << "%"
              << "\nThroughput: " << result.throughput << " processes/time unit"
              << "\nContext-switch estimate: " << result.contextSwitches
              << "\nTask migrations: " << result.taskMigrations
              << "\nMigration overhead: " << result.migrationOverheadTime << " core-time units"
              << "\nMaximum observed task-count imbalance: " << result.maxLoadImbalance
              << "\nMakespan: " << result.makespan << '\n';
}
