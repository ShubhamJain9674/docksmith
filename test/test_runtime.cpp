// test_runtime.cpp
// compile: g++ -std=c++17 test_runtime.cpp runtime.cpp -o test_runtime
// run:     ./test_runtime

#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sched.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

// forward declare your function
bool runInRootLinux(
    std::filesystem::path rootDir,
    std::filesystem::path workDir,
    std::vector<std::string>& envVars,
    std::vector<std::string>& commands
);

// ================================================================
// tiny test framework
// ================================================================
int passed = 0;
int failed = 0;

#define TEST(name, expr) \
    do { \
        std::cout << "  " << name << " ... "; \
        if (expr) { std::cout << "PASS\n"; passed++; } \
        else      { std::cout << "FAIL\n"; failed++; } \
    } while(0)

void printSummary() {
    std::cout << "\n--- results: "
              << passed << " passed, "
              << failed << " failed ---\n";
}

// ================================================================
// preflight checks
// ================================================================
bool preflightCheck() {
    bool ok = true;
    std::cout << "\n[preflight checks]\n";

    // check 1: unprivileged user namespaces
    {
        std::ifstream f("/proc/sys/kernel/unprivileged_userns_clone");
        if (f.good()) {
            int val = -1;
            f >> val;
            if (val == 0) {
                std::cerr << "  FAIL: unprivileged user namespaces disabled\n"
                          << "  fix:  sudo sysctl -w kernel.unprivileged_userns_clone=1\n";
                ok = false;
            } else {
                std::cout << "  OK: unprivileged_userns_clone = " << val << "\n";
            }
        } else {
            std::cout << "  INFO: unprivileged_userns_clone not found (may be fine)\n";
        }
    }

    // check 2: max_user_namespaces
    {
        std::ifstream f("/proc/sys/user/max_user_namespaces");
        if (f.good()) {
            int val = 0;
            f >> val;
            if (val == 0) {
                std::cerr << "  FAIL: max_user_namespaces = 0\n"
                          << "  fix:  sudo sysctl -w user.max_user_namespaces=10000\n";
                ok = false;
            } else {
                std::cout << "  OK: max_user_namespaces = " << val << "\n";
            }
        }
    }

    // check 3: /bin/sh exists
    if (!std::filesystem::exists("/bin/sh")) {
        std::cerr << "  FAIL: /bin/sh not found\n";
        ok = false;
    } else {
        std::cout << "  OK: /bin/sh exists\n";
    }

    // NOTE: no unshare() call here — calling it in the parent
    // prevents child processes from unsharing again later

    return ok;
}

// ================================================================
// copy all shared libs needed by a binary using ldd
// ================================================================
void copyLibsForBinary(const std::string& binary,
                       const std::filesystem::path& root)
{
    std::string cmd = "ldd " + binary + " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return;

    char line[512];
    while (fgets(line, sizeof(line), pipe)) {
        std::string s(line);
        std::string path;

        auto arrow = s.find("=>");
        if (arrow != std::string::npos) {
            auto start = s.find('/', arrow);
            auto end   = s.find(' ', start);
            if (start != std::string::npos)
                path = s.substr(start, end - start);
        } else if (s.find('/') != std::string::npos) {
            auto start = s.find('/');
            auto end   = s.find(' ', start);
            path = s.substr(start, end - start);
        }

        // trim trailing whitespace and newlines
        while (!path.empty() &&
               (path.back() == '\n' || path.back() == ' ' || path.back() == '\r'))
            path.pop_back();

        if (path.empty() || !std::filesystem::exists(path)) continue;

        std::filesystem::path dest = root / path.substr(1); // strip leading /
        std::filesystem::create_directories(dest.parent_path());

        if (!std::filesystem::exists(dest)) {
            std::filesystem::copy_file(
                path, dest,
                std::filesystem::copy_options::overwrite_existing
            );
            std::cout << "  copied lib: " << path << "\n";
        }
    }
    pclose(pipe);
}

// ================================================================
// build minimal rootfs
// ================================================================
bool buildMinimalRootfs(const std::filesystem::path& root) {
    try {
        std::cout << "\nbuilding rootfs at " << root << "\n";

        std::filesystem::remove_all(root);

        // create base dirs
        for (auto& dir : {
            "usr/bin", "usr/lib", "usr/lib64",
            "usr/lib/x86_64-linux-gnu",
            "proc", "tmp", "etc", "app"
        }) {
            std::filesystem::create_directories(root / dir);
        }

        // on modern Linux /bin -> usr/bin and /lib -> usr/lib are symlinks.
        // recreate those symlinks inside the rootfs so paths resolve correctly.
        auto makeSymlink = [&](const char* target, const char* linkName) {
            std::filesystem::path linkPath = root / linkName;
            if (!std::filesystem::exists(linkPath))
                std::filesystem::create_symlink(target, linkPath);
        };

        makeSymlink("usr/bin",    "bin");
        makeSymlink("usr/lib",    "lib");
        makeSymlink("usr/lib64",  "lib64");

        // now copy /bin/sh — resolves to rootfs/usr/bin/sh via symlink
        // use the real path to avoid following host symlinks
        std::string realSh = std::filesystem::canonical("/bin/sh").string();
        std::filesystem::copy_file(
            realSh,
            root / "usr/bin/sh",
            std::filesystem::copy_options::overwrite_existing
        );
        std::cout << "  copied sh from " << realSh << "\n";

        // copy libs using the real canonical path
        copyLibsForBinary(realSh, root);

        // copy other tools
        for (auto& bin : {"/bin/echo", "/bin/ls",
                          "/bin/cat",  "/bin/pwd"}) {
            if (!std::filesystem::exists(bin)) continue;

            std::string realBin = std::filesystem::canonical(bin).string();
            // destination mirrors real path inside rootfs
            std::filesystem::path dest = root / realBin.substr(1);
            std::filesystem::create_directories(dest.parent_path());

            if (!std::filesystem::exists(dest)) {
                std::filesystem::copy_file(
                    realBin, dest,
                    std::filesystem::copy_options::overwrite_existing
                );
                copyLibsForBinary(realBin, root);
            }
        }

        // test file inside /app
        {
            std::ofstream f(root / "app/hello.txt");
            f << "hello from inside the container\n";
        }

        // test script
        {
            std::ofstream f(root / "app/test.sh");
            f << "#!/bin/sh\n"
              << "echo \"workdir: $(pwd)\"\n"
              << "echo \"TEST_VAR=$TEST_VAR\"\n";
            chmod((root / "app/test.sh").c_str(), 0755);
        }

        // verify sh is reachable via /bin/sh inside rootfs
        std::filesystem::path shCheck = root / "bin/sh";
        if (!std::filesystem::exists(shCheck)) {
            std::cerr << "  ERROR: " << shCheck
                      << " not reachable — symlink or copy failed\n";
            return false;
        }
        std::cout << "  verified: bin/sh reachable inside rootfs\n";

        std::cout << "rootfs ready.\n";
        return true;
    }
    catch (std::exception& e) {
        std::cerr << "buildMinimalRootfs failed: " << e.what() << "\n";
        return false;
    }
}
// ================================================================
// tests
// ================================================================
void test_basic_echo(const std::filesystem::path& root) {
    std::cout << "\n[test 1] basic echo\n";
    std::vector<std::string> env = {"PATH=/usr/bin:/bin"};
    std::vector<std::string> cmd = {"/bin/sh", "-c", "echo hello from container"};
    TEST("exits 0", runInRootLinux(root, "/", env, cmd));
}

void test_workdir(const std::filesystem::path& root) {
    std::cout << "\n[test 2] workdir set to /app\n";
    std::vector<std::string> env = {"PATH=/usr/bin:/bin"};
    std::vector<std::string> cmd = {"/bin/sh", "-c", "pwd"};
    // visual check: output must print /app
    TEST("exits 0", runInRootLinux(root, "/app", env, cmd));
}

void test_env_injection(const std::filesystem::path& root) {
    std::cout << "\n[test 3] env var injection\n";
    std::vector<std::string> env = {
        "PATH=/usr/bin:/bin",
        "TEST_VAR=hello_from_docksmith"
    };
    std::vector<std::string> cmd = {"/bin/sh", "-c", "echo $TEST_VAR"};
    // visual check: output must print hello_from_docksmith
    TEST("exits 0", runInRootLinux(root, "/", env, cmd));
}

void test_isolation(const std::filesystem::path& root) {
    std::cout << "\n[test 4] filesystem isolation (key test)\n";

    std::filesystem::path hostSentinel = "/tmp/docksmith_escape.txt";
    std::filesystem::remove(hostSentinel);

    std::vector<std::string> env = {"PATH=/usr/bin:/bin"};
    std::vector<std::string> cmd = {
        "/bin/sh", "-c",
        "echo escaped > /tmp/docksmith_escape.txt"
    };
    runInRootLinux(root, "/", env, cmd);

    bool escapedToHost  = std::filesystem::exists(hostSentinel);
    bool existsInRootfs = std::filesystem::exists(
                              root / "tmp/docksmith_escape.txt");

    TEST("file did NOT appear on host /tmp", !escapedToHost);
    TEST("file exists inside container rootfs", existsInRootfs);
}

void test_nonzero_exit(const std::filesystem::path& root) {
    std::cout << "\n[test 5] non-zero exit\n";
    std::vector<std::string> env = {"PATH=/usr/bin:/bin"};
    std::vector<std::string> cmd = {"/bin/sh", "-c", "exit 42"};
    TEST("returns false", runInRootLinux(root, "/", env, cmd) == false);
}

void test_invalid_command(const std::filesystem::path& root) {
    std::cout << "\n[test 6] invalid binary\n";
    std::vector<std::string> env = {"PATH=/usr/bin:/bin"};
    std::vector<std::string> cmd = {"/bin/sh", "-c", "doesnotexist123"};
    TEST("returns false", runInRootLinux(root, "/", env, cmd) == false);
}

// ================================================================
// main
// ================================================================
int main() {
    std::cout << "=== docksmith runtime tests ===\n";

    if (!preflightCheck()) {
        std::cerr << "\npreflight failed — fix the above and rerun.\n";
        return 1;
    }

    std::filesystem::path root = "/tmp/docksmith-test-rootfs";

    if (!buildMinimalRootfs(root)) return 1;
    // debug: show what's in bin/ inside rootfs
    std::cout << "\n[rootfs structure check]\n";
    std::cout << "  bin/ -> "
              << std::filesystem::read_symlink(root / "bin").string()
              << " (symlink)\n";
    std::cout << "  usr/bin/sh exists: "
              << std::filesystem::exists(root / "usr/bin/sh") << "\n";
    std::cout << "  bin/sh reachable:  "
              << std::filesystem::exists(root / "bin/sh") << "\n";


    test_basic_echo(root);
    test_workdir(root);
    test_env_injection(root);
    test_isolation(root);
    test_nonzero_exit(root);
    test_invalid_command(root);

    std::cout << "\ncleaning up...\n";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    printSummary();
    return failed > 0 ? 1 : 0;
}