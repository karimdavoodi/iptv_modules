#include <iostream>
#include <vector>
#include <map>
class SysUsage {
    struct transfer {
        float read;
        float write;
    };
    struct Data {
        std::map<std::string, transfer> partitions;   
        std::map<std::string, transfer> interfaces; 
        float cpuUsage, cpuTotal, cpuIdle;
        float sysLoad;
        float memUsage, memTotal, memAvailable;
    };
    private:
        Data current;
        Data priviuse;
    public:
        SysUsage() { calcCurrentUsage(); };
        void calcCurrentUsage();
        const std::string getUsageJson();
    private:
        void calcCurrentPartitions();
        void calcCurrentInterfaces();
        void calcCurrentCpu();
        void calcCurrentMem();
        void calcCurrentLoad();
};
