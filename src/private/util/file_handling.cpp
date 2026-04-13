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




void handleWhiteouts(const fs::path& rootfs) {
    std::vector<fs::path> whiteouts;

    // Collect first
    for (auto& p : fs::recursive_directory_iterator(rootfs)) {
        std::string name = p.path().filename().string();
        if (name.rfind(".wh.", 0) == 0) {
            whiteouts.push_back(p.path());
        }
    }

    // Process after
    for (auto& p : whiteouts) {
        std::string name = p.filename().string();
        std::string originalName = name.substr(4);

        fs::path target = p.parent_path() / originalName;

        if (fs::exists(target)) {
            fs::remove_all(target);
        }

        fs::remove(p);
    }
}


void extractTar(const std::filesystem::path& tarPath, const std::filesystem::path& dest){
    std::string cmd =
        "tar -xf " + tarPath.string() +
        " --strip-components=1 -C " + dest.string();

    int res = system(cmd.c_str());
    if (res != 0) {
        throw std::runtime_error("tar extraction failed");
    }
}

Snapshot snapshotMtimes(const fs::path& rootfs) {
    Snapshot snap;

    for (auto& entry : fs::recursive_directory_iterator(rootfs,
        fs::directory_options::skip_permission_denied
    )) {

        if (!entry.is_regular_file() && !entry.is_symlink())
            continue;

        // Get path relative to rootfs
        fs::path relPath = fs::relative(entry.path(), rootfs);

        FileInfo info;
        if(entry.is_symlink()){
            // symlinks have no meaningful mtime/size for change detection
            // store the target as a marker instead

            info.mtime = fs::file_time_type{};
            info.size = 0;
            info.symlinkTarget = fs::read_symlink(entry.path().string());
            info.isSymlink = true;
        }
        else{
            info.mtime = fs::last_write_time(entry.path());
            info.size = fs::file_size(entry.path());
            info.isSymlink = false;
        }

        snap[relPath.string()] = info;
    }

    return snap;
}

void createTarFromDelta(
    const std::string& rootfs,
    const std::string& tarPath,
    const std::vector<std::string>& files
) {
    std::string cmd =
        "tar --sort=name "
        "--mtime='UTC 1970-01-01' "
        "--owner=0 --group=0 --numeric-owner "
        "-cf " + tarPath +
        " -C " + rootfs +
        " -T -";

    FILE* pipe = popen(cmd.c_str(), "w");
    if (!pipe) {
        throw std::runtime_error("popen failed");
    }

    // Write file list to tar via stdin
    for (const auto& f : files) {
        fprintf(pipe, "%s\n", f.c_str());
    }

    int status = pclose(pipe);

    if (status != 0) {
        throw std::runtime_error("tar failed");
    }
}