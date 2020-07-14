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
        std::map<std::string, long> contents; 
        float cpuUsage, cpuTotal, cpuIdle;
        float sysLoad;
        float memUsage, memTotal, memAvailable;
    };
    private:
        Data current;
        Data priviuse;
        const std::vector<std::string> contents_dir = { 
            "Video", "Audio", "TimeShift", "Image", "userContents" };
    public:
        SysUsage() { calcCurrentUsage(); };
        void calcCurrentUsage();
        const std::string getUsageJson(int systemId);
    private:
        void calcCurrentPartitions();
        void calcCurrentInterfaces();
        void calcCurrentCpu();
        void calcCurrentMem();
        void calcCurrentLoad();
        void calcCurrentContents();
};
