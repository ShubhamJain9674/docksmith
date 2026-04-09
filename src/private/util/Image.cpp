#include "Image.h"

namespace fs = std::filesystem; 
using json = nlohmann::json;

Image::Image(json j){
    
    name = j.at("name").get<std::string>();
    tag = j.at("tag").get<std::string>();
    digest = j.value("digest","");
    created = j.value("created","");

    conf = Config::from_json(j.at("config"));

    for(const auto& l : j.value("layers",json::array())){
        layers.push_back(Layer::from_json(l));
    }
}

Image loadManifest(const std::string& file){

    std::ifstream f(getExecutableDir() / "images" / file);

    if (!f.is_open()) {
        std::cerr << "Failed to open file: " << file << "\n";
    }

    if (f.peek() == std::ifstream::traits_type::eof()) {
        std::cerr << "Skipping empty file: " << file << "\n";
    }

    json j;
    f >> j;

    return Image(j);
}

json Image::toJson(){
    json j;
    j["name"] = name;
    j["tag"] = tag;
    j["digest"] = digest;
    j["created"] = created;
    j["config"] = conf.to_json();

    json arr = json::array();
    for (const auto& l : layers) {
        arr.push_back({
            {"digest", l.digest},
            {"size", l.size},
            {"createdBy", l.createdBy}
        });
    }


    j["layers"] = arr;

    // std::cout << j.dump() << std::endl;
    return j;

}

void saveManifest(Image& i){

    json j = i.toJson();
    const fs::path path = getExecutableDir() / "images" / (j.at("name").get<std::string>() + ".json");


    std::ofstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    f << j.dump(4);
    f.close();
}


