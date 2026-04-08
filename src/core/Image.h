#pragma once
#include <json.hpp>
#include <vector>
#include <fstream>
#include "util/file_handling.h"


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
        l.digest = j.at("digest").get<std::string>();
        l.size = j.value("size",0);
        l.createdBy = j.value("createdBy","");
        return l;
    }
    
};

struct Config{
    std::vector<std::string> env;
    std::vector<std::string> cmds;
    std::filesystem::path working_dir;

    static Config from_json(const nlohmann::json& j){
        Config c;
        c.env = j.value("env",std::vector<std::string>{});
        c.cmds = j.value("cmds",std::vector<std::string>{});
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
        nlohmann::json toJson();


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

