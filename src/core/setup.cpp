#include "setup.h"

namespace fs = std::filesystem;

fs::path getExePath(){
    //not sure if we support windows later or not.
    #ifdef _WIN32
        char buffer[MAX_PATH];
        DWORD len = GetModuleFileNameA(NULL, buffer, MAX_PATH);
        if (len == 0) {
            throw std::runtime_error("Failed to get executable path (Windows)");
        }
        return fs::path(std::string(buffer, len));
    #elif __APPLE__
        std::cout <<"who uses apple!" << std::endl;
        exit(1);
    #elif __linux__

        char buffer[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
        if (len == -1) {
            throw std::runtime_error("Failed to get executable path (Linux)");
        }

        buffer[len] = '\0';
        return fs::path(buffer);
    #else
        #error Unsupported platform
    #endif

}

fs::path getExecutableDir() {
    return getExePath().parent_path();
}


void initDocksmithDir(){

    fs::path exe_dir = getExecutableDir();
    const fs::path images = exe_dir / fs::path("images");
    const fs::path layers = exe_dir / fs::path("layers");
    const fs::path cache = exe_dir / fs::path("cache");

    if(!fs::exists(images)){
        std::cout << "creating images dir!\n";
        fs::create_directory(images);
    }
    
    if(!fs::exists(layers)){
        std::cout << "creating layers dir!\n";
        fs::create_directory(layers);
    }

    if(!fs::exists(cache)){
        std::cout << "creating cache dir!\n"; 
        fs::create_directory(cache);
    }

}