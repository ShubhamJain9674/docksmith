#pragma once

#include <vector>
#include <memory>
#include <filesystem>

#include "json.hpp"
#include "file_handling.h"
#include "Image.h"
#include "crypto.h"
#include "runtime.h"
#include "cache.h"
#include "timer.h"
#include "logger.h"


struct InstructionResult{
    std::string message;
    bool valid;
};

class BuildState{

    

    public:
        BuildState(){}

        std::vector<Layer> layers;
        std::string workdir = "/";
        std::vector<std::string> env;
        nlohmann::json cmd;
    
        std::string rootfs; //temp filesystem during build

        void addLayer(const Layer& layer){
            layers.push_back(layer);
        }

        void setWorkdir(std::string dir){
            workdir = dir;
        }

        void addEnv(std::string e) { env.push_back(e); }

        void setCmd(const nlohmann::json& c ) { this->cmd = c; }

        std::string getLastLayerDigest() const;

        std::vector<Layer>& getLayers() { return layers; }
        std::string getWorkdir() { return workdir; }
        std::vector<std::string>& getEnv() { return env; }
        nlohmann::json& getCmds() { return cmd; }

};

class Instruction{

    public:
        virtual ~Instruction() = default;
        virtual InstructionResult Execute(
            BuildState& state,
            CacheIndex& cache,
            bool& cache_broken
        ) = 0;

};


class FromInstruction : public Instruction{

    public:
        FromInstruction(std::string image_name){
            size_t pos = image_name.find(":");
            if(pos != std::string::npos){
                image_name = image_name.substr(0,pos);
            }
            image = loadManifest(image_name);
        }
        InstructionResult Execute(
            BuildState& state,
            CacheIndex& cache,
            bool& cache_broken
        ) override;

    private:
        Image image;

};



class WorkingdirInstruction : public Instruction{

    public:
        WorkingdirInstruction(std::string dir) : dir(dir){};

        InstructionResult Execute(
            BuildState& state,
            CacheIndex& cache,
            bool& cache_broken
        ) override;


    private:
        std::string dir;
};


class EnvInstruction : public Instruction{

    public:
        EnvInstruction(std::string env) : env(env){}

        InstructionResult Execute(
            BuildState& state,
            CacheIndex& cache,
            bool& cache_broken
        ) override;
    
    private:
        std::string env;

};

class CmdInstruction : public Instruction {

    public:
        CmdInstruction(nlohmann::json cmd) : cmd(cmd){};
        InstructionResult Execute(
            BuildState& state,
            CacheIndex& cache,
            bool& cache_broken
        ) override;

    private:
        nlohmann::json cmd;
};

class CopyInstruction : public Instruction {

    public:
        CopyInstruction(std::string from,std::string dest,std::filesystem::path context_dir) 
            : from(from),dest(dest),context_dir(context_dir){}
        
        InstructionResult Execute(
            BuildState& state,
            CacheIndex& cache,
            bool& cache_broken
        ) override;


    private:
        std::string from;
        std::string dest;
        std::filesystem::path context_dir;

};

class RunInstruction : public Instruction{

    public:
        RunInstruction(std::vector<std::string> cmd): cmd(cmd) {}

        InstructionResult Execute(
            BuildState& state,
            CacheIndex& cache,
            bool& cache_broken
        ) override;

        std::string getCmd(){
            std::string l_cmd = "";
            for(auto str : cmd){
                if (!l_cmd.empty()) 
                    l_cmd += " "; 
                l_cmd += str;
            }
            return l_cmd;
        }

    private:
        std::vector<std::string> cmd;
};



class InstructionFactory{

    public:
        InstructionFactory(std::filesystem::path context_dir): context_dir(context_dir){}
        std::unique_ptr<Instruction> Create(nlohmann::json& instr);

    private:
        std::filesystem::path context_dir;
};

class BuildEngine{

    public:
        BuildEngine(std::filesystem::path contextDir): context_dir(contextDir){}
        Image Build(std::vector<nlohmann::json>& Instructions,
        const std::string& name,
        const std::string& tag,
        bool no_cache
        );
    private:
        std::filesystem::path context_dir;

};