#ifndef CORE_H
#define CORE_H

#include <vector>

struct GanttEntry {
    int pid;       // -1 denotes idle; -2 denotes modeled migration overhead
    int start;
    int end;
};

struct Core {
    int coreId = 0;
    std::vector<GanttEntry> gantt;
};

#endif
