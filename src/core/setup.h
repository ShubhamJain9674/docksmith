#pragma once

#include <iostream>
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