#pragma once

#include <iostream>
#include <filesystem>
#include <vector>
#include <string>



#ifdef __linux__

    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <sys/mount.h>
    #include <sys/stat.h>
    #include <sched.h>
    #include <errno.h>
    #include <string.h>
    #include <fcntl.h>
    #include <signal.h>
#endif


bool runInRootLinux(
    std::filesystem::path rootDir,
    std::filesystem::path workDir,
    std::vector<std::string>& envVars,
    std::vector<std::string>& commands,
    bool execDirect = false
);