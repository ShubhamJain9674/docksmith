#include "cli.h"

using json = nlohmann::json;
namespace fs = std::filesystem;


static std::string formatColumn(const std::string& text, size_t width) {
    if (text.size() <= width) {
        return text + std::string(width - text.size(), ' ');
    }

    // truncate and add ellipsis
    if (width > 3) {
        return text.substr(0, width - 3) + "...";
    }

    return text.substr(0, width); // fallback
}

void buildCmd(const std::string& build_tag,const std::string& build_context,bool no_cache){
    
    // debug messages
    // std::cout << "[BUILD]\n";
    // std::cout << "Tag: " << build_tag << "\n";
    // std::cout << "Context: " << build_context << "\n";
    // std::cout << "No cache: " << (no_cache ? "true" : "false") << "\n";

    //implement build command :-

    //check context path

    if(!checkPath(build_context)){
        std::cerr << RED <<"invalid build context\n" << RESET;
        return;
    }

    if(!checkForDocksmithFile(build_context)){
        std::cerr << RED << "Docksmithfile not found in context\n" << RESET;
        return;
    }

    fs::path  docksmith_filepath = fs::absolute(build_context) / "Docksmithfile";
    auto instructions = parseDocksmithFile(docksmith_filepath);
    

    if (!instructions.has_value()) {
        std::cerr << RED <<"No instructions found!" << RESET <<std::endl;
        return;
    }
    if(instructions.value().size() == 0){
        std::cerr << RED <<"No instructions found!" << RESET <<std::endl;
        return;
    }

    fs::path contextDir = fs::absolute(build_context);


    BuildEngine engine(contextDir);
   
    auto pos = build_tag.find(':');
    std::string image_name = build_tag.substr(0, pos);
    std::string image_tag  = (pos == std::string::npos) ? "latest" : build_tag.substr(pos + 1);
    Image image;

    try {
        image = engine.Build(instructions.value(), image_name,image_tag, no_cache);
    } catch (const std::exception& e) {
        std::cerr << "Build failed: " << e.what() << "\n";
        return;
    }

    auto data = image.toJson().dump();
    auto digest = sha256String(data);
    image.setDigest(digest);


    saveManifest(image);

    std::cout << "Build Complete" << std::endl;


}

void runCmd(const std::string& run_image,
            const std::vector<std::string>& run_cmd,
            const std::vector<std::string>& env_vars)
{
    std::cout << "[RUN]\n";
    std::cout << "Image: " << run_image << "\n";

    std::cout << "Env:\n";
    for (auto &e : env_vars) {
        std::cout << "  " << e << "\n";
    }

    std::cout << "Cmd:\n";
    for (auto &c : run_cmd) {
        std::cout << "  " << c << "\n";
    }


    //implement run command:-
    
    // 1 parse name:tag 
    std::string name , tag;
    auto pos = run_image.find(":");
    if(pos == std::string::npos){
        name = run_image;
        tag = "latest";
    }
    else{
        name = run_image.substr(0,pos);
        tag = run_image.substr(pos + 1);
    }

    //2 load image json

    Image image = loadManifest(run_image + ".json");

    //3 creating temp dir using RAII object
    TempDir temp;
    const auto tmp = fs::absolute(temp.get());
 
    //4 extract layers

    auto layers = image.getLayers();
    const fs::path layer_dir = getLayerDir();
    
    for(auto layer : layers){
        fs::path layer_path = layer_dir / (layer.digest + ".tar");
        if(!fs::exists(layer_path)){
            std::cerr << "missing layer : " << layer.digest << "\n";
            return;
        }

        // extract tar

        std::stringstream cmd;
        cmd << "tar -xf " << layer_path << " -C " << tmp;

        if (system(cmd.str().c_str()) != 0) {
            std::cerr << "Failed to extract layer\n";
            return;
        }

        // apply whiteout

        handleWhiteouts(tmp);
    }
    // after extraction, add debug:
    fprintf(stderr, "=== ROOTFS ROOT ===\n");
    for (auto& p : fs::directory_iterator(tmp))
        fprintf(stderr, "  %s\n", p.path().c_str());

    fprintf(stderr, "=== ROOTFS /app ===\n");
    for (auto& p : fs::recursive_directory_iterator(tmp / "app"))
        fprintf(stderr, "  %s\n", p.path().c_str());
    fprintf(stderr, "=== END ===\n");

    fprintf(stderr, "layers count: %zu\n", layers.size());
    for (auto& l : layers)
        fprintf(stderr, "  layer: %s\n", l.digest.c_str());


    // prepare env :-
    std::vector<std::string> finalEnv;

    // image env
    for (auto& e : image.getConfig().env) {
        finalEnv.push_back(e);
    }

    // CLI overrides
    for (auto& e : env_vars) {
        finalEnv.push_back(e);
    }

    // prepare cmd 

    std::vector<std::string> finalCmd;

    if (!run_cmd.empty()) {
        finalCmd = run_cmd;  // CLI override
    } else {
        for (auto& c : image.getConfig().cmds) {
            finalCmd.push_back(c);
        }
    }

    // Working dir :-

    auto workdir = image.getConfig().working_dir;

    bool execDirect = finalCmd.size()  > 1; 

    bool ok = runInRootLinux(tmp,workdir,finalEnv,finalCmd,execDirect);

    if(!ok){
        std::cerr << "Container execution failed\n";
    }

    //end
    std::cout << "RUN COMPLETE" << std::endl;
    // temp file cleanup handled by RAII obbkect

}

void imagesCmd() {
    auto images = getAllFilesUnderDir("images", ".json", false);

    const int NAME_W = 20;
    const int ID_W = 15;
    const int CREATED_W = 25;
    const int SIZE_W = 13;

    std::cout << BLUE
              << formatColumn("IMAGE", NAME_W)
              << formatColumn("ID", ID_W)
              << formatColumn("CREATED", CREATED_W)
              << formatColumn("CONTENT SIZE" , SIZE_W)
              << RESET
              << "\n";

    
    for (const auto& file : images) {
        
        Image i = loadManifest(file);
        auto id = i.getDigest();
        if(id.size() > 12) id = id.substr(0,12);

        std::cout << BLUE
                  << formatColumn(i.getName()+":"+i.getTag(), NAME_W)
                  << RESET
                  << formatColumn(id, ID_W)
                  << formatColumn(i.getCreated(), CREATED_W)
                  << formatColumn(getImageSizeFormatted(i),SIZE_W)
                  << "\n";

    } 

}


void rmiCmd(const std::string& rmi_image){
    std::cout << "[RMI]\n";
    std::cout << "Removing: " << rmi_image << "\n";

    std::string name, tag;
    auto pos = rmi_image.find(':');
    if (pos == std::string::npos) {
        name = rmi_image;
        tag  = "latest";
    } else {
        name = rmi_image.substr(0, pos);
        tag  = rmi_image.substr(pos + 1);
    }

    const fs::path image_dir = getExecutableDir() / "images";

    if (!fs::exists(image_dir / (name + ".json"))) {
        std::cerr << "Image not found: " << name << "\n";
        return;
    }

    // load target image
    Image target = loadManifest(name);
    auto target_layers = target.getLayers();

    // collect layers used by all OTHER images
    std::unordered_set<std::string> used_layers;
    for (auto& entry : fs::directory_iterator(image_dir)) {
        if (entry.path().extension() != ".json") continue;
        if (entry.path().stem().string() == name) continue; // skip target

        try {
            Image other = loadManifest(entry.path().stem().string());
            for (auto& l : other.getLayers())
                used_layers.insert(l.digest);
        } catch (...) {
            continue;
        }
    }

    // collect digests being removed (for cache cleanup)
    std::unordered_set<std::string> removed_digests;

    // delete unused layers
    for (auto& layer : target_layers) {
        if (used_layers.count(layer.digest)) {
            std::cout << "Keeping shared layer: " << layer.digest << "\n";
        } else {
            fs::path layer_path = getLayerDir() / (layer.digest + ".tar");
            if (fs::exists(layer_path)) {
                fs::remove(layer_path);
                std::cout << "Removed layer: " << layer.digest << "\n";
            }
            removed_digests.insert(layer.digest);
        }
    }

    // clean up cache entries that reference removed layers
    CacheIndex cache = loadCache();
    bool cache_dirty = false;

    for (auto it = cache.begin(); it != cache.end(); ) {
        std::string digest = stripSHA256(it->second);
        if (removed_digests.count(digest)) {
            it = cache.erase(it);
            cache_dirty = true;
        } else {
            ++it;
        }
    }

    if (cache_dirty)
        saveCache(cache);

    // delete the image manifest
    deleteJsonFile(name);
    std::cout << "Removed image: " << name << "\n";
}