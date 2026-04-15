#include "Image.h"

namespace fs = std::filesystem; 
using json = nlohmann::json;

Image::Image(json j){
    
    name = j.at("name").get<std::string>();
    tag = j.at("tag").get<std::string>();
    
    digest = stripSHA256(j.value("digest",""));
    created = j.value("created","");

    conf = Config::from_json(j.at("config"));

    for(const auto& l : j.value("layers",json::array())){
        layers.push_back(Layer::from_json(l));
    }
}

Image loadManifest(const std::string& file){

    std::string req_file = file; 
    if (req_file.size() < 5 || req_file.substr(req_file.size() - 5) != ".json") {
        req_file += ".json";
    }
    
    std::ifstream f(getExecutableDir() / "images" / req_file);

    if (!f.is_open()) {
        std::cerr << RED <<"Failed to open file: " << req_file << "\n" << RESET;
        throw std::runtime_error("loading image failed");
    }

    if (f.peek() == std::ifstream::traits_type::eof()) {
        std::cerr << RED <<"Empty image file: " << req_file << "\n" << RESET;
        throw std::runtime_error("Image not valid");
    }

    json j;
    f >> j;

    return Image(j);
}

json Image::toJson(){
    json j;
    j["name"] = name;
    j["tag"] = tag;
    j["digest"] = "sha256:"+digest;
    j["created"] = created;
    j["config"] = conf.to_json();

    json arr = json::array();
    for (const auto& l : layers) {
        arr.push_back({
            {"digest", "sha256:"+l.digest},
            {"size", l.size},
            {"createdBy", l.createdBy}
        });
    }


    j["layers"] = arr;

    // std::cout << j.dump() << std::endl;
    return j;

}
std::string stripSHA256(std::string d){
    if (d.rfind("sha256:", 0) == 0) { // checks prefix
        d = d.substr(7); // remove first 7 characters
    }
    return d;
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

size_t calculateImageSize(Image& i){
    size_t size = 0;
    for(auto& layer : i.getLayers()){
        size += layer.size;
    }
    return size;
}

std::string getImageSizeFormatted(Image& i){

    float size = calculateImageSize(i);

    auto round = [](float value){
        return std::round(value * 1000) / 1000.0;
    };

    auto format = [](std::string s){
        return s.substr(0,s.find('.') + 3);
    };

    if(size >= 1000000000){
        size /= 1000000000;
        size = round(size);
        return format(std::to_string(size)) + " GB";
    }
    else if(size >= 1000000){
        size /= 1000000;
        size = round(size);
        return format(std::to_string(size) ) + " MB";
    }
    else if(size > 1000){
        size /= 1000;
        size = round(size);
        return format(std::to_string(size) ) + " KB";
    }
    else{
        return format(std::to_string(size) ) + " B";
    }
    return "";

}

std::string getCurrentTimeISO8601() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::tm utc{};
    gmtime_r(&t, &utc); // thread-safe version

    std::stringstream ss;
    ss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}