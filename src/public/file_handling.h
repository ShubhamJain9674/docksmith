#pragma once

#include <iostream>
#include <vector>
#include <ranges>
#include <filesystem>
#include <fstream>
#include "crypto.h"
#include <unordered_map>



#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __linux__
    #include <cstdio>
    #include <unistd.h>
    #include <limits.h>
    #include <cstdlib>
    #include <cstring>
    #include <stdexcept>

#endif
    

std::filesystem::path getExePath();
std::filesystem::path getExecutableDir();


std::vector<std::string> getAllFilesUnderDir(const std::string& dir,
    const std::string& extension = "",
    bool strip_extension = false    
);

inline std::filesystem::path getCacheDir(){
    return getExecutableDir() / "cache";
}

inline std::filesystem::path getContextDir(){
    return std::filesystem::current_path();
}


inline std::filesystem::path getLayerDir(){
    return (getExecutableDir() / "layers");
}

inline bool checkPath(std::string path){
    std::filesystem::path contextPath = std::filesystem::absolute(path);
    if (!std::filesystem::exists(contextPath) || !std::filesystem::is_directory(contextPath))
        return false;
    
    return true;
}

inline bool checkForDocksmithFile(std::string path){

    std::filesystem::path docksmithfile = std::filesystem::absolute(path) / "Docksmithfile";

    if (!std::filesystem::exists(docksmithfile)) {
        return false;
    }
    return true;
}

void deleteJsonFile(const std::string& file);

// std::string CreateTempDir();

class TempDir{

    public:
        TempDir();
        
        const std::string& get() const {
            return path;
        }

        ~TempDir(){
            try{
                if(path.empty()) return;

                std::filesystem::path p = std::filesystem::weakly_canonical(path);

                if(p.string().find("/tmp/docksmith-") != 0){
                    std::cerr << "Refusing to delete unsafe path : " << path << "\n"; 
                    return;
                }
                if(std::filesystem::exists(p)){
                    std::filesystem::remove_all(p);
                }
            }
            catch(...){
                std::cerr << " error while removing temp path!\n";
            }
        }

    private:
        std::string path;
};



void extractTar(const std::filesystem::path& tarPath, const std::filesystem::path& dest);
void extractDeltaTar(const std::filesystem::path& tarPath, const std::filesystem::path& dest);
 
void handleWhiteouts(const std::filesystem::path& rootfs);

struct FileInfo {
    std::filesystem::file_time_type mtime;
    uintmax_t size;
    bool isSymlink = false;
    bool isDirectory = false;
    std::string symlinkTarget = "";
};


using Snapshot = std::unordered_map<std::string, FileInfo>;
Snapshot snapshotMtimes(const std::filesystem::path& rootfs);

void createTarFromDelta(
    const std::string& rootfs,
    const std::string& tarPath,
    const std::vector<std::string>& files
);