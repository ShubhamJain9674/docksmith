#include "cli.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

void buildCmd(const std::string& build_tag,const std::string& build_context,bool no_cache){
    std::cout << "[BUILD]\n";
    std::cout << "Tag: " << build_tag << "\n";
    std::cout << "Context: " << build_context << "\n";
    std::cout << "No cache: " << (no_cache ? "true" : "false") << "\n";

    //implement build command :-

    //check context path

    if(!checkPath(build_context)){
        std::cerr << "invalid build context\n";
    }

    if(!checkForDocksmithFile(build_context)){
        std::cerr << "Docksmithfile not found in context\n";
    }

    fs::path  docksmith_filepath = fs::absolute(build_context) / "Docksmithfile";
    auto instructions = parseDocksmithFile(docksmith_filepath);

    if (!instructions.has_value()) {
        std::cerr << "No instructions found!" << std::endl;
        return;
    }




    BuildEngine engine;
   
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

    bool ok = runInRootLinux(tmp,workdir,finalEnv,finalCmd);

    if(!ok){
        std::cerr << "Container execution failed\n";
    }

    //end
    std::cout << "RUN COMPLETE" << std::endl;
    // temp file cleanup handled by RAII obbkect

}

void imagesCmd() {
    auto images = getAllFilesUnderDir("images", ".json", false);

    std::cout << "NAME\tTAG\tIMAGE ID\tCREATED\n";

    for (const auto& file : images) {
        
        Image i = loadManifest(file);
        auto id = i.getDigest();
        if(id.size() > 12) id = id.substr(0,12);

        std::cout << i.getName() << "\t"
                << i.getTag() << "\t"
                << id << "\t"
                << i.getCreated() << "\n";

    } 

}


void rmiCmd(const std::string& rmi_image){
    std::cout << "[RMI]\n";
    std::cout << "Removing: " << rmi_image << "\n";

    //implement rmi command:-

    std::string name,tag;

    auto pos = rmi_image.find(':');
    if (pos == std::string::npos) {
        name = rmi_image;
        tag = "latest";
    } else {
        name = rmi_image.substr(0, pos);
        tag  = rmi_image.substr(pos + 1);
    }


    const fs::path image_dir = getExecutableDir() / "images";
    fs::path image_path = image_dir / rmi_image;

    if (!fs::exists(image_dir)) {
        std::cerr << "Image not found\n";
        return;
    }
 
    // - load the image json
    Image i = loadManifest(rmi_image+".json"); 
    

    // collect used layers
    auto target_layers = i.getLayers();
    std::unordered_set<std::string> used_layers;

    for (auto& dir : fs::directory_iterator(image_dir)) {
        if (!dir.is_directory()) continue;

        for (auto& file : fs::directory_iterator(dir.path())) {
            if (file.path() == image_path) continue; // skip target image

            if (file.path().extension() != ".json") continue;

            json other;
            std::ifstream(file.path()) >> other;

            for (auto& l : other["layers"]) {
                used_layers.insert(l["digest"]);
            }
        }
    }


    // delete unused layers.
    for (auto& layer : target_layers) {
        std::string digest = layer.digest;
        if (used_layers.count(digest) == 0) {
            fs::path layer_path = getLayerDir() / digest;

            if (fs::exists(layer_path)) {
                std::cout << "Deleting layer: " << digest << "\n";
                fs::remove(layer_path);
            }
        } else {
            std::cout << "Skipping shared layer: " << digest << "\n";
        }
    }

    //delete the json file

    deleteJsonFile(i.getName());

}
