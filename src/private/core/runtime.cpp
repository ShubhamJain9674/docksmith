#include "runtime.h"



struct ContainerArgs {
    std::filesystem::path rootDir;
    std::filesystem::path workDir;
    std::vector<std::string>* envVars;
    std::vector<std::string>* commands;
};

static int containerMain(void* arg) {
    auto* a = static_cast<ContainerArgs*>(arg);

    // mount /proc inside container
    std::filesystem::path procPath = a->rootDir / "proc";
    std::filesystem::create_directories(procPath);

    if (mount("proc", procPath.c_str(), "proc",
              MS_NOSUID | MS_NOEXEC | MS_NODEV, nullptr) == -1)
    {
        std::cerr << "failed to mount /proc: " << strerror(errno) << "\n";
        return 1;
    }

    // chroot into rootDir
    if (chroot(a->rootDir.c_str()) == -1) {
        std::cerr << "chroot failed: " << strerror(errno) << "\n";
        return 1;
    }

    // chdir to workDir after chroot
    std::string wd = a->workDir.empty() ? "/" : a->workDir.string();
    if (chdir(wd.c_str()) == -1) {
        std::cerr << "chdir to " << wd
                  << " failed: " << strerror(errno) << "\n";
        return 1;
    }

    // build env array
    std::vector<const char*> env;
    for (auto& e : *(a->envVars))
        env.push_back(e.c_str());
    env.push_back(nullptr);

    // build argv array
    std::vector<const char*> argv;
    for (auto& c : *(a->commands))
        argv.push_back(c.c_str());
    argv.push_back(nullptr);

    // exec
    execvpe(a->commands->at(0).c_str(),
            const_cast<char* const*>(argv.data()),
            const_cast<char* const*>(env.data()));

    std::cerr << "exec failed: " << strerror(errno) << "\n";
    return 1;
}

static bool writeUserMappings(pid_t pid) {
    // deny setgroups first — kernel requires this before writing gid_map
    {
        std::string path = "/proc/" + std::to_string(pid) + "/setgroups";
        int fd = open(path.c_str(), O_WRONLY);
        if (fd == -1) {
            std::cerr << "failed to open setgroups: " << strerror(errno) << "\n";
            return false;
        }
        write(fd, "deny", 4);
        close(fd);
    }

    // uid_map: container uid 0 -> host uid
    {
        std::string path = "/proc/" + std::to_string(pid) + "/uid_map";
        std::string map  = "0 " + std::to_string(getuid()) + " 1\n";
        int fd = open(path.c_str(), O_WRONLY);
        if (fd == -1) {
            std::cerr << "failed to open uid_map: " << strerror(errno) << "\n";
            return false;
        }
        write(fd, map.c_str(), map.size());
        close(fd);
    }

    // gid_map: container gid 0 -> host gid
    {
        std::string path = "/proc/" + std::to_string(pid) + "/gid_map";
        std::string map  = "0 " + std::to_string(getgid()) + " 1\n";
        int fd = open(path.c_str(), O_WRONLY);
        if (fd == -1) {
            std::cerr << "failed to open gid_map: " << strerror(errno) << "\n";
            return false;
        }
        write(fd, map.c_str(), map.size());
        close(fd);
    }

    return true;
}

bool runInRootLinux(
    std::filesystem::path rootDir,
    std::filesystem::path workDir,
    std::vector<std::string>& envVars,
    std::vector<std::string>& commands
)
{
    ContainerArgs args { rootDir, workDir, &envVars, &commands };

    const int STACK_SIZE = 1024 * 1024;
    std::vector<char> stack(STACK_SIZE);
    char* stackTop = stack.data() + STACK_SIZE; // stack grows downward

    pid_t pid = clone(
        containerMain,
        stackTop,
        CLONE_NEWNS   |
        CLONE_NEWPID  |
        CLONE_NEWUTS  |
        CLONE_NEWIPC  |
        CLONE_NEWUSER |
        SIGCHLD,
        &args
    );

    if (pid == -1) {
        std::cerr << "clone failed: " << strerror(errno) << "\n";
        return false;
    }

    // parent writes uid/gid maps for the child
    if (!writeUserMappings(pid)) {
        std::cerr << "failed to write user mappings\n";
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        return false;
    }

    int status;
    waitpid(pid, &status, 0);

    // unmount /proc before temp dir is deleted
    std::filesystem::path procPath = rootDir / "proc";
    umount2(procPath.c_str(), MNT_DETACH);

    if (WIFEXITED(status)) {
        std::cout << "container exited with code "
                  << WEXITSTATUS(status) << "\n";
        return WEXITSTATUS(status) == 0;
    } else if (WIFSIGNALED(status)) {
        std::cout << "container killed by signal "
                  << WTERMSIG(status) << "\n";
        return false;
    }

    return true;
}