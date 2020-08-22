/*
 * Copyright (c) 2020 Karim, karimdavoodi@gmail.com
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
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
