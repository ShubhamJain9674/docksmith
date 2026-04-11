#include "file_handling.h"

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
        throw std::runtime_error("Unsupported platform");
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
        throw std::runtime_error("Unsupported platform");
    #endif

}

fs::path getExecutableDir() {
    return getExePath().parent_path();
}




std::vector<std::string> getAllFilesUnderDir(const std::string& dir,
    const std::string& extension,
    bool strip_extension    
){

    std::vector<std::string> file_list;
    const fs::path exe_dir = getExecutableDir();
    const fs::path p = exe_dir / fs::path(dir);


    for (const auto& entry : fs::directory_iterator(p)) {
        if (!entry.is_regular_file()) continue;

        if (extension.empty() || entry.path().extension() == extension) {
            // std::cout << entry.path().filename().string() << std::endl;
            std::string required = (strip_extension) 
                                    ? entry.path().stem().string() 
                                    : entry.path().filename().string(); 
            file_list.push_back(required);
        }
    }

    return file_list;

} 

void deleteJsonFile(const std::string& file){

    fs::path path = getExecutableDir() / "images" / (file + ".json");
    fs::remove(path);
}


std::string CreateTempDir(){
    std::string base = "/tmp/docksmith-XXXXXX";

    //mkdtemp modifies the string in-place.
    //it needs a mutable buffer

    char* buffer = new char[base.size() + 1];
    std::strcpy(buffer,base.c_str());

    char* result = mkdtemp(buffer);

    if(!result){
        delete[] buffer;
        throw std::runtime_error("mkdtemp failed");
    }
    std::string path(result);
    delete[] buffer;

    return path;
}

TempDir::TempDir(){
    path = CreateTempDir();
}







