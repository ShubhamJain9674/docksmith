#pragma once

#include <iostream>
#include <vector>
#include <ranges>
#include <filesystem>
#include <fstream>
#include "crypto.h"


#ifdef _WIN32
    #include <windows.h>
#endif

#ifdef __linux__
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

inline std::filesystem::path getContextDir(){
    return std::filesystem::current_path();
}