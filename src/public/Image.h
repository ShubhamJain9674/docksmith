#pragma once
#include <json.hpp>
#include <vector>
#include <fstream>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <cmath>

#include "file_handling.h"

std::string stripSHA256(std::string d);

struct Layer{
    std::string digest;  //hash of tar file compressed.
    size_t size;   //size of the tar file.
    std::string createdBy;

    nlohmann::json to_json(){
        return nlohmann::json{
            {"digest" , digest},
            {"size" , size},
            {"createdBy",createdBy}
        };
    }

    static Layer from_json(const nlohmann::json& j){
        Layer l;
        l.digest = stripSHA256(j.at("digest").get<std::string>());
        l.size = j.value("size",0);
        l.createdBy = j.value("createdBy","");
        return l;
    }
    
};

inline bool layerExists(const std::string& digest) {
    std::filesystem::path p = getLayerDir() / (digest+".tar");
    return std::filesystem::exists(p);
}



struct Config{
    std::vector<std::string> env;
    std::vector<std::string> cmds;
    std::filesystem::path working_dir;

    static Config from_json(const nlohmann::json& j){
        Config c;
        c.env = j.value("env",std::vector<std::string>{});
        c.cmds = j.value("Cmd",std::vector<std::string>{});
        if (j.contains("WorkingDir")) {
            c.working_dir = j["WorkingDir"].get<std::string>();
        }
        return c;
    }

    nlohmann::json to_json() const {
        return nlohmann::json{
            {"Env", env},
            {"Cmd", cmds},
            {"WorkingDir", working_dir.string()}
        };  
    }

};

class Image{

    public:
        Image() = default;
        Image(nlohmann::json j);

        const std::string& getName() { return name; }
        const std::string& getTag() { return tag; }
        const std::string& getDigest() { return digest; }
        const std::string& getCreated() { return created; }
        const std::vector<Layer>& getLayers() { return layers; }
        const Config& getConfig() { return conf; }
        nlohmann::json toJson();

        void setName(std::string n) { name = n; }
        void setTag(std::string t){ tag = t; }
        void setDigest(std::string d){ digest = d; }
        void setCreated(std::string c){ created = c; }
        void setConfig(Config c){ conf = c; }
        void addLayer(Layer l) { layers.push_back(l); }

    private:
        std::string name;
        std::string tag;
        std::string digest;
        std::string created;
        Config conf;
        std::vector<Layer> layers;

};


Image loadManifest(const std::string& file);
void saveManifest(Image& i);

size_t calculateImageSize(Image& i);
std::string getImageSizeFormatted(Image& i);


std::string getCurrentTimeISO8601();