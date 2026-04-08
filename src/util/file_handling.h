#pragma once

#include <iostream>
#include <vector>
#include <ranges>
#include <filesystem>

#ifdef _WIN32
    #include <windows.h>
#endif

#ifdef __linux__
    #include <unistd.h>
    #include <limits.h>
#endif
    



std::filesystem::path getExePath();
std::filesystem::path getExecutableDir();

void initDocksmithDir();

std::vector<std::string> getAllFilesUnderDir(const std::string& dir,
    const std::string& extension = "",
    bool strip_extension = false    
);
