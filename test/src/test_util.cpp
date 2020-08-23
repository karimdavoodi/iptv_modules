#include "test_util.hpp"
#include <thread>
#include <filesystem>


using namespace std;

void wait(int millisecond)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(millisecond));    
}
int file_size(const std::string path)
{
    if(std::filesystem::exists(path)){
        return std::filesystem::file_size(path);
    }
    return 0;
}
