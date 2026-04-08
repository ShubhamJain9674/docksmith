#include "cli.h"

using json = nlohmann::json;

void buildCmd(const std::string& build_tag,const std::string& build_context,bool no_cache){
    std::cout << "[BUILD]\n";
    std::cout << "Tag: " << build_tag << "\n";
    std::cout << "Context: " << build_context << "\n";
    std::cout << "No cache: " << (no_cache ? "true" : "false") << "\n";

    //implement build command :-


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


    // - load the image json
    Image i = loadManifest(rmi_image);
    


    // get all the layer info
    // for each layer check if its shared by other layer
    // if not shared delete the layer
    //delete the json file

}
