#ifndef PROCESS_H
#define PROCESS_H

struct Process {
    int pid = 0;
    int arrivalTime = 0;
    int burstTime = 0;
    int priority = 0;

    // Optional affinity: -1 means unrestricted/not preferred.
    // A pinned process may execute only on pinnedCore. A preferredCore is a hint.
    int preferredCore = -1;
    int pinnedCore = -1;

    // Derived values; recalculated independently for each simulation.
    int waitingTime = 0;
    int turnaroundTime = 0;
    int responseTime = 0;
    int completionTime = 0;
};

#endif
