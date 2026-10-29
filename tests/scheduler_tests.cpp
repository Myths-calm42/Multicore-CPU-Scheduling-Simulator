#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "scheduler.h"

static bool closeTo(double a, double b) { return std::fabs(a - b) < 1e-9; }

static void checkMetrics(const SimulationResult& r,
                         const std::vector<Process>& input) {
    assert(r.processes.size() == input.size());
    std::unordered_map<int, int> executed;
    std::unordered_map<int, int> latestEnd;
    for (const auto& p : input) executed[p.pid] = 0;

    for (const auto& core : r.cores) {
        int previousEnd = 0;
        bool first = true;
        for (const auto& e : core.gantt) {
            assert(e.end > e.start);
            if (!first) assert(e.start == previousEnd);
            else assert(e.start == 0);
            previousEnd = e.end;
            first = false;
            if (e.pid >= 0) {
                assert(executed.count(e.pid) == 1);
                executed[e.pid] += e.end - e.start;
                latestEnd[e.pid] = std::max(latestEnd[e.pid], e.end);
            }
        }
    }

    for (std::size_t i = 0; i < input.size(); ++i) {
        const auto& p = r.processes[i];
        assert(executed[p.pid] == p.burstTime);
        assert(p.completionTime == latestEnd[p.pid]);
        assert(p.turnaroundTime == p.completionTime - p.arrivalTime);
        assert(p.waitingTime == p.turnaroundTime - p.burstTime);
        assert(p.responseTime >= 0);
        assert(p.responseTime <= p.waitingTime);
    }
}

int main() {
    {
        const std::vector<Process> p{{1, 0, 3, 2}, {2, 1, 2, 1}};
        auto r = Simulate(p, 1, Policy::FCFS);
        assert(r.processes[0].completionTime == 3);
        assert(r.processes[1].completionTime == 5);
        assert(r.processes[1].waitingTime == 2);
        assert(r.processes[1].turnaroundTime == 4);
        assert(r.processes[1].responseTime == 2);
        assert(closeTo(r.averageWaitingTime, 1.0));
        checkMetrics(r, p);
    }
    {
        const std::vector<Process> p{{1, 0, 3, 2}, {2, 1, 2, 1}};
        auto r = Simulate(p, 1, Policy::RoundRobin, 2);
        assert(r.processes[0].completionTime == 5);
        assert(r.processes[1].completionTime == 4);
        assert(r.processes[0].waitingTime == 2);
        assert(r.processes[1].waitingTime == 1);
        checkMetrics(r, p);
    }
    {
        const std::vector<Process> p{{1, 2, 3, 1}, {2, 2, 2, 2}};
        auto r = Simulate(p, 2, Policy::FCFS);
        assert(r.processes[0].completionTime == 5);
        assert(r.processes[1].completionTime == 4);
        assert(r.makespan == 5);
        assert(closeTo(r.cpuUtilizationPercent, 50.0));
        checkMetrics(r, p);
    }
    {
        const std::vector<Process> p{{1, 0, 4, 2}, {2, 0, 2, 1}, {3, 1, 1, 3}};
        auto r = Simulate(p, 2, Policy::SJF);
        assert(r.totalBusyTime == 7);
        checkMetrics(r, p);
    }
    {
        // All policies should preserve input order and compute independently.
        const std::vector<Process> p{
            {1, 0, 5, 3}, {2, 1, 2, 1}, {3, 1, 1, 2}, {4, 2, 3, 1}
        };
        auto fcfs = Simulate(p, 1, Policy::FCFS);
        auto sjf = Simulate(p, 1, Policy::SJF);
        auto priority = Simulate(p, 1, Policy::Priority);
        auto rr = Simulate(p, 1, Policy::RoundRobin, 2);
        assert(fcfs.processes[0].pid == 1);
        assert(sjf.processes[0].pid == 1);
        assert(priority.processes[0].pid == 1);
        assert(rr.processes[0].pid == 1);
        assert(sjf.processes[1].completionTime != fcfs.processes[1].completionTime);
        checkMetrics(fcfs, p);
        checkMetrics(sjf, p);
        checkMetrics(priority, p);
        checkMetrics(rr, p);
    }
    {
        // No process may execute on two cores at the same time.
        const std::vector<Process> p{
            {1, 0, 5, 2}, {2, 0, 4, 1}, {3, 1, 3, 3}, {4, 3, 2, 1}
        };
        for (Policy policy : {Policy::FCFS, Policy::SJF, Policy::Priority,
                              Policy::RoundRobin}) {
            auto r = Simulate(p, 3, policy, 2);
            for (const auto& a : r.cores) {
                for (const auto& ea : a.gantt) {
                    if (ea.pid < 0) continue;
                    for (const auto& b : r.cores) {
                        if (a.coreId == b.coreId) continue;
                        for (const auto& eb : b.gantt) {
                            if (eb.pid != ea.pid || eb.pid < 0) continue;
                            assert(ea.end <= eb.start || eb.end <= ea.start);
                        }
                    }
                }
            }
            checkMetrics(r, p);
        }
    }
    {
        // Pinned processes must execute only on their selected core.
        const std::vector<Process> p{
            {1, 0, 4, 1, -1, 0},
            {2, 0, 3, 2, -1, 1},
            {3, 1, 2, 3, -1, 0}
        };
        auto r = Simulate(p, 2, Policy::FCFS);
        for (const auto& core : r.cores) {
            for (const auto& e : core.gantt) {
                if (e.pid < 0) continue;
                for (const auto& proc : p) {
                    if (proc.pid == e.pid && proc.pinnedCore >= 0)
                        assert(core.coreId == proc.pinnedCore);
                }
            }
        }
        checkMetrics(r, p);
    }
    {
        // Balancing may migrate an unpinned task; modeled migration cost must
        // appear explicitly in timelines and utilization accounting.
        const std::vector<Process> p{
            {1, 2, 5, 0, 0, -1},
            {2, 3, 6, 0, 0, -1},
            {3, 3, 4, 2, 0, -1},
            {4, 1, 3, 2, 0, -1}
        };
        SchedulingOptions options;
        options.loadBalancing = true;
        options.balanceInterval = 1;
        options.migrationCost = 2;
        auto r = Simulate(p, 2, Policy::RoundRobin, 1, options);
        // This deterministic workload should trigger exactly two cross-core
        // redispatches, each paying two simulated time units.
        assert(r.taskMigrations == 2);
        assert(r.migrationOverheadTime == 4);
        assert(r.makespan == 13);
        assert(r.cpuUtilizationPercent >= r.usefulCpuUtilizationPercent);
        int migrationSegments = 0;
        int migrationSegmentTime = 0;
        for (const auto& core : r.cores) {
            for (const auto& e : core.gantt) {
                if (e.pid == -2) {
                    ++migrationSegments;
                    migrationSegmentTime += e.end - e.start;
                }
            }
        }
        assert(migrationSegments == 2);
        assert(migrationSegmentTime == 4);
        checkMetrics(r, p);
    }
    {
        // Policy validation: with all jobs ready at t=0 on one core, each
        // policy must select its intended first task.
        const std::vector<Process> p{
            {1, 0, 8, 3}, {2, 0, 2, 1}, {3, 0, 1, 2}
        };
        const auto fcfs = Simulate(p, 1, Policy::FCFS);
        const auto sjf = Simulate(p, 1, Policy::SJF);
        const auto priority = Simulate(p, 1, Policy::Priority);
        auto executedOrder = [](const Core& core) {
            std::vector<int> order;
            for (const auto& e : core.gantt)
                if (e.pid >= 0) order.push_back(e.pid);
            return order;
        };
        assert((executedOrder(fcfs.cores[0]) == std::vector<int>{1, 2, 3}));
        assert((executedOrder(sjf.cores[0]) == std::vector<int>{3, 2, 1}));
        assert((executedOrder(priority.cores[0]) == std::vector<int>{2, 3, 1}));
        checkMetrics(fcfs, p);
        checkMetrics(sjf, p);
        checkMetrics(priority, p);
    }
    {
        // Preferred-core affinity is a soft hint: it should not overload a
        // preferred core when another core has less queued work.
        const std::vector<Process> p{
            {1, 0, 4, 0, 0, -1}, {2, 0, 4, 0, 0, -1},
            {3, 0, 4, 0, 0, -1}, {4, 0, 4, 0, 0, -1}
        };
        const auto r = Simulate(p, 2, Policy::FCFS);
        int onCore0 = 0, onCore1 = 0;
        for (const auto& core : r.cores) {
            for (const auto& e : core.gantt) {
                if (e.pid < 0) continue;
                if (core.coreId == 0) onCore0 += e.end - e.start;
                else if (core.coreId == 1) onCore1 += e.end - e.start;
            }
        }
        assert(onCore0 == 8);
        assert(onCore1 == 8);
        checkMetrics(r, p);
    }
    {
        // A preferred core is honored when cores are equally loaded. This is
        // a soft preference: the process is not permanently pinned there.
        const std::vector<Process> p{{1, 0, 3, 0, 1, -1}};
        const auto r = Simulate(p, 2, Policy::FCFS);
        bool ranOnPreferredCore = false;
        for (const auto& core : r.cores) {
            for (const auto& e : core.gantt) {
                if (e.pid == 1) {
                    assert(core.coreId == 1);
                    ranOnPreferredCore = true;
                }
            }
        }
        assert(ranOnPreferredCore);
        checkMetrics(r, p);
    }
    {
        // Pinned work must stay on its assigned core even when that creates
        // a queue and the other core remains idle with spare capacity.
        const std::vector<Process> p{
            {1, 0, 5, 0, -1, 0}, {2, 0, 4, 0, -1, 0}
        };
        SchedulingOptions options;
        options.loadBalancing = true;
        options.balanceInterval = 1;
        options.migrationCost = 1;
        const auto r = Simulate(p, 2, Policy::RoundRobin, 1, options);
        for (const auto& e : r.cores[0].gantt)
            if (e.pid >= 0) assert(e.pid == 1 || e.pid == 2);
        for (const auto& e : r.cores[1].gantt) assert(e.pid == -1);
        assert(r.taskMigrations == 0);
        assert(r.migrationOverheadTime == 0);
        checkMetrics(r, p);
    }
    {
        // Deterministic load-balancing regression: balancing moves one queued
        // task to the less-loaded core, reducing both observed task-count
        // imbalance and makespan for this workload.
        const std::vector<Process> p{
            {1, 0, 2, 0}, {2, 0, 1, 0}, {3, 0, 2, 0},
            {4, 0, 1, 0}, {5, 0, 1, 0}
        };
        SchedulingOptions noBalance;
        noBalance.loadBalancing = false;
        const auto baseline = Simulate(p, 2, Policy::RoundRobin, 1, noBalance);
        SchedulingOptions balancing;
        balancing.loadBalancing = true;
        balancing.balanceInterval = 1;
        const auto balanced = Simulate(p, 2, Policy::RoundRobin, 1, balancing);
        assert(baseline.maxLoadImbalance == 3);
        assert(balanced.maxLoadImbalance == 2);
        assert(baseline.makespan == 5);
        assert(balanced.makespan == 4);
        assert(balanced.taskMigrations == 1);
        checkMetrics(baseline, p);
        checkMetrics(balanced, p);
    }
    {
        // Balancing must not move pinned tasks away from their assigned core.
        const std::vector<Process> p{
            {1, 0, 5, 0, -1, 0}, {2, 0, 4, 0, -1, 0},
            {3, 0, 3, 0, -1, 1}, {4, 0, 2, 0, -1, 1}
        };
        SchedulingOptions options;
        options.loadBalancing = true;
        options.balanceInterval = 1;
        options.migrationCost = 1;
        const auto r = Simulate(p, 2, Policy::RoundRobin, 1, options);
        for (const auto& core : r.cores) {
            for (const auto& e : core.gantt) {
                if (e.pid < 0) continue;
                for (const auto& proc : p) {
                    if (proc.pid == e.pid) assert(core.coreId == proc.pinnedCore);
                }
            }
        }
        checkMetrics(r, p);
    }
    {
        // Invalid affinity core indexes must be rejected.
        const std::vector<Process> p{{1, 0, 2, 1, -1, 2}};
        bool threw = false;
        try { (void)Simulate(p, 2, Policy::FCFS); }
        catch (const std::invalid_argument&) { threw = true; }
        assert(threw);
    }
    {
        const std::vector<Process> empty;
        auto r = Simulate(empty, 2, Policy::FCFS);
        assert(r.processes.empty());
        assert(r.makespan == 0);
        assert(closeTo(r.cpuUtilizationPercent, 0.0));
    }
    {
        const std::vector<Process> p{{1, 0, 1, 1}};
        bool threw = false;
        try { (void)Simulate(p, 0, Policy::FCFS); }
        catch (const std::invalid_argument&) { threw = true; }
        assert(threw);
        threw = false;
        try { (void)Simulate(p, 1, Policy::RoundRobin, 0); }
        catch (const std::invalid_argument&) { threw = true; }
        assert(threw);
    }

    std::cout << "All scheduler tests passed.\n";
}
