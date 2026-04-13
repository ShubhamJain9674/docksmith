#include "setup.h"


namespace fs = std::filesystem;




static bool checkLayersCache(){
    const fs::path cache = getExecutableDir() / "cache" / "layers.json";
    return fs::exists(cache);
}

static void createLayersCache(){
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

            const auto exe_dir = getExecutableDir();
            const auto path  = exe_dir / "base_image";

            auto req_file = getBaseImageTarFile();
            if(req_file == "")
                return;

            std::uintmax_t size = fs::file_size((path / req_file));
            
            auto digest = calculateBaseLinuxDigest();
            auto layer = createBaseLinuxLayer(digest,size);
            
            saveBaseLinuxImage(digest,layer);
            storeAlpineLayer(layer,req_file);

        }
    }

}


std::string getBaseImageTarFile(){

    auto exe_dir = getExecutableDir();
    auto path  = exe_dir / "base_image";
    std::string req_file;

    std::vector<std::string> files = getAllFilesUnderDir(path,".tar");

    if(files.size() == 0){
        std::cerr << "base linux file not found.Look into readme for more info\n";
        return "";
    }
    req_file = files[0];

    return req_file;
}

std::string calculateBaseLinuxDigest(){
    
    
    auto exe_dir = getExecutableDir();
    auto path  = exe_dir / "base_image";
    std::string req_file;

    std::string digest = encryptSHA256((path / req_file)); 
    std::cout << "digest : " << digest <<std::endl;
    return digest;

}

void saveBaseLinuxImage(std::string digest,Layer l){

    Image base;
    
    base.setName("AlpineLinux");
    base.setTag("Latest");
    base.setDigest(digest);
    base.addLayer(l);

    saveManifest(base);
}


Layer createBaseLinuxLayer(std::string digest,size_t size){

    Layer l = {
        digest,
        size,
        "DocksmithBaseLayer"
    };
    return l;
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


void storeAlpineLayer(Layer l,std::string name){

    const auto exe_dir = getExecutableDir();
    std::cout << name;
    const auto source_alpine_path = exe_dir / "base_image" / name ;
    const auto dest_path = exe_dir / "layers" / (l.digest + ".tar");

    
    fs::copy_file(source_alpine_path,dest_path,fs::copy_options::overwrite_existing);

    try{
        std::cout << "layer created and stored\n";
    }catch(const fs::filesystem_error& e){
        std::cout << "error : " << e.what() << std::endl;
    }

    // req storing in cache file.
}