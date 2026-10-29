#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <string>
#include <vector>
#include "process.h"
#include "core.h"

enum class Policy { FCFS, SJF, Priority, RoundRobin };

struct SchedulingOptions {
    bool loadBalancing = false;
    int balanceInterval = 1;  // simulated time units
    int migrationCost = 0;    // simulated time units; paid on cross-core redispatch
};

struct SimulationResult {
    std::string algorithm;
    std::vector<Process> processes;
    std::vector<Core> cores;
    int makespan = 0;
    long long totalBusyTime = 0;          // useful process execution
    long long migrationOverheadTime = 0;  // modeled core time spent migrating
    int contextSwitches = 0;
    int taskMigrations = 0;
    int maxLoadImbalance = 0;
    double averageWaitingTime = 0.0;
    double averageTurnaroundTime = 0.0;
    double averageResponseTime = 0.0;
    double cpuUtilizationPercent = 0.0;       // includes modeled migration overhead
    double usefulCpuUtilizationPercent = 0.0; // excludes migration overhead
    double throughput = 0.0;
};

SimulationResult Simulate(const std::vector<Process>& processes,
                          int coreCount, Policy policy, int quantum = 1,
                          const SchedulingOptions& options = {});
void PrintResult(const SimulationResult& result);

#endif
