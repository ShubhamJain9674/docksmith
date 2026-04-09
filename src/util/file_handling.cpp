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



bool checkLayersCache(){
    const fs::path cache = getExecutableDir() / "cache" / "layers.json";
    return fs::exists(cache);
}

void createLayersCache(){
    const fs::path cache = getExecutableDir() / "cache" / "layers.json";
    std::ofstream f(cache);

    if(!f.is_open()){
        std::cerr << "failed to create image cache file\n";
    }

    f.close();
}


void initDocksmithDir(){

    fs::path exe_dir = getExecutableDir();
    const fs::path images = exe_dir / fs::path("images");
    const fs::path layers = exe_dir / fs::path("layers");
    const fs::path cache = exe_dir / fs::path("cache");
    const fs::path base_image = exe_dir / fs::path("base_image");


    if(!fs::exists(images)){
        std::cout << "creating images dir!\n";
        fs::create_directory(images);
    }
    
    if(!fs::exists(layers)){
        std::cout << "creating layers dir!\n";
        fs::create_directory(layers);
    }

    if(!fs::exists(cache)){
        std::cout << "creating cache dir!\n"; 
        fs::create_directory(cache);
        createLayersCache();
    }
    else{
        if(!checkLayersCache()){
            createLayersCache();
        }

    }

    if(!fs::exists(base_image)){
        std::cout << "creating base_image dir!\n";
        std::cout << "you need to add Alpine linux in .tar format here.\n";
        std::cout << ".tar.gz format wont work on this implementation\n";
        fs::create_directory(base_image);
    }
    else{
        if(!isBaseImageAvailable()){
            createBaseLinuxLayer();
        }
    }


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


void storeAlpineLayer(Layer l,std::string name){

    const auto exe_dir = getExecutableDir();
    std::cout << name;
    const auto source_alpine_path = exe_dir / "base_image" / name ;
    const auto dest_path = exe_dir / "layers" / (l.digest + ".tar");

    
    fs::copy_file(source_alpine_path,dest_path);

    try{
        std::cout << "layer created and stored\n";
    }catch(const fs::filesystem_error& e){
        std::cout << "error : " << e.what() << std::endl;
    }

    // req storing in cache file.
}




void createBaseLinuxLayer(){


    auto exe_dir = getExecutableDir();
    auto path  = exe_dir / "base_image";
    std::string req_file;
    
    {
        std::vector<std::string> files = getAllFilesUnderDir(path,".tar");
        if(files.size() == 0){
            std::cerr << "base linux file not found.Look into readme for more info\n";
            return;
        }
        req_file = files[0];
    }

    
    std::string digest = encryptSHA256((path / req_file)); 
    std::cout << "digest : " << digest <<std::endl;
    std::uintmax_t size = fs::file_size((path / req_file));


    Layer l = {
        digest,
        size,
        "DocksmithBaseLayer"
    };

    storeAlpineLayer(l,req_file);

    
    
    Image base;
    
    base.setName("AlpineLinux");
    base.setTag("Latest");
    base.setDigest(digest);
    base.addLayer(l);

    saveManifest(base);

}

bool isBaseImageAvailable(){
    const auto exe_dir = getExecutableDir();
    const auto path  = exe_dir / "images";
    
    auto files = getAllFilesUnderDir(path,".json");
    for(auto i : files){
        if(i == "AlpineLinux.json"){
            return true;
        }
    }

    return false;

}