#include "runtime.h"



struct ContainerArgs {
    std::filesystem::path rootDir;
    std::filesystem::path workDir;
    std::vector<std::string>* envVars;
    std::vector<std::string>* commands;
    int pipe_fd[2];
};

static int containerMain(void* arg) {
    auto* a = static_cast<ContainerArgs*>(arg);
    close(a->pipe_fd[1]);

    char buf;
    if (read(a->pipe_fd[0], &buf, 1) == -1) {
        perror("read failed");
        return 1;
    }
    close(a->pipe_fd[0]);

    // isolate mounts
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        perror("mount private failed");
        return 1;
    }

    // // bind mount rootfs so it's a mount point (required for chroot in new ns)
    // if (mount(a->rootDir.c_str(), a->rootDir.c_str(), NULL, MS_BIND | MS_REC, NULL) == -1) {
    //     perror("bind mount rootdir failed");
    //     return 1;
    // }

    // chroot — do this ONCE
    if (chdir(a->rootDir.c_str()) == -1) {
        perror("chdir to rootDir failed");
        return 1;
    }
    if (chroot(".") == -1) {
        perror("chroot failed");
        return 1;
    }
    if (chdir("/") == -1) {
        perror("chdir to / failed");
        return 1;
    }

    // mount /proc inside the chroot
    mkdir("/proc", 0755);
    // if (mount("proc", "/proc", "proc", 0, NULL) == -1) {
    //     perror("mount /proc failed");
    //     return 1;
    // }

    // workdir — we are inside chroot now, so wd must be relative to /
    std::string wd = a->workDir.empty() ? "/" : a->workDir.string();


    // std::cout << "wd passed to chdir : " << wd << "\n";
    if (chdir(wd.c_str()) == -1) {
        perror("chdir to workdir failed");
        return 1;
    }

    // env
    std::vector<const char*> env;
    for (auto& e : *(a->envVars))
        env.push_back(e.c_str());
    env.push_back(nullptr);

    // command
    std::string cmd;
    for (auto& c : *(a->commands))
        cmd += c + " ";

    
    // Debug: check what we can actually see
    // fprintf(stderr, "cwd = ");
    // char cwd[256];
    // if (getcwd(cwd, sizeof(cwd))) fprintf(stderr, "%s\n", cwd);
    
    // struct stat st;
    // fprintf(stderr, "stat /bin/sh: %s\n", stat("/bin/sh", &st) == 0 ? "OK" : strerror(errno));
    // fprintf(stderr, "stat /bin/busybox: %s\n", stat("/bin/busybox", &st) == 0 ? "OK" : strerror(errno));
    // fprintf(stderr, "access /bin/sh X_OK: %s\n", access("/bin/sh", X_OK) == 0 ? "OK" : strerror(errno));
    // fprintf(stderr, "access /bin/busybox X_OK: %s\n", access("/bin/busybox", X_OK) == 0 ? "OK" : strerror(errno));
    // fprintf(stderr, "uid=%d gid=%d\n", getuid(), getgid());
    
    // fprintf(stderr, "rootfs at: %s\n", a->rootDir.c_str());
    // sleep(100); // pause so you can inspect
    
    std::vector<const char*> argv = { "/bin/sh", "-c", cmd.c_str(), nullptr };

    // struct stat rst;
    // stat("/", &rst);
    // fprintf(stderr, "/ mode=%o uid=%d gid=%d\n", rst.st_mode & 0777, rst.st_uid, rst.st_gid);

    // fprintf(stderr, "rootfs at: %s\n", a->rootDir.c_str());
    // sleep(60);

    // for (auto& p : std::filesystem::directory_iterator(cwd))
    //     std::cerr << "  " << p.path() << "\n";


    execvpe("/bin/sh",
            const_cast<char* const*>(argv.data()),
            const_cast<char* const*>(env.data()));


    // workaround

    // std::vector<const char*> argv = { "/bin/busybox", "sh", "-c", cmd.c_str(), nullptr };

    // execvpe("/bin/busybox", 
    //     const_cast<char* const*>(argv.data()),
    //     const_cast<char* const*>(env.data()));
    
    


    perror("exec failed");


    _exit(1);
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
        if (write(fd, "deny", 4) == -1) {
            perror("write setgroups failed");
            return false;
        }   
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
        if (write(fd, map.c_str(), map.size()) == -1) {
            perror("write uid_map failed");
            return false;
        }
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
        if (write(fd, map.c_str(), map.size()) == -1) {
            perror("write uid_map failed");
            return false;
        }
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

    int pipefd[2];
    if(pipe(pipefd) == -1){
        perror("pipe failed");
        return false;
    }

    ContainerArgs args { rootDir, workDir, &envVars, &commands,{pipefd[0], pipefd[1]} };

    const int STACK_SIZE = 1024 * 1024;
    std::vector<char> stack(STACK_SIZE);
    char* stackTop = stack.data() + STACK_SIZE; // stack grows downward

    // Before clone(), add:
    // std::cerr << "=== rootDir contents ===\n";
    // for (auto& p : std::filesystem::directory_iterator(rootDir))
    //     std::cerr << "  " << p.path() << "\n";



    pid_t pid = clone(
        containerMain,
        stackTop,
        CLONE_NEWUSER |
        CLONE_NEWNS   |
        CLONE_NEWPID  |
        CLONE_NEWUTS  |
        CLONE_NEWIPC  |
        SIGCHLD,
        &args
    );

    close(pipefd[0]);

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

    if (write(pipefd[1], "x", 1) == -1) {
        perror("write failed");
    }
    close(pipefd[1]);   

    int status;
    waitpid(pid, &status, 0);

    // unmount /proc before temp dir is deleted
    

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