#pragma once

#include <vector>
#include <memory>
#include <filesystem>

#include "json.hpp"
#include "file_handling.h"
#include "Image.h"
#include "crypto.h"

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

};

class Instruction{

    public:
        virtual ~Instruction() = default;
        virtual void Execute(BuildState& state) = 0;

};


class FromInstruction : public Instruction{

    public:
        FromInstruction(std::string image_name){
            image = loadManifest(image_name);
        }
        void Execute(BuildState& state) override;

    private:
        Image image;

};



class WorkingdirInstruction : public Instruction{

    public:
        WorkingdirInstruction(std::string dir) : dir(dir){};

        void Execute(BuildState& state)override;


    private:
        std::string dir;
};


class EnvInstruction : public Instruction{

    public:
        EnvInstruction(std::string env) : env(env){}

        void Execute(BuildState& state)override;
    
    private:
        std::string env;

};

class CmdInstruction : public Instruction {

    public:
        CmdInstruction(nlohmann::json cmd) : cmd(cmd){};
        void Execute(BuildState& state)override;

    private:
        nlohmann::json cmd;
};

class CopyInstruction : public Instruction {

    public:
        CopyInstruction(std::string from,std::string dest) 
            : from(from),dest(dest){}
        
        void Execute(BuildState& state)override;


    private:
        std::string from;
        std::string dest;

};



class InstructionFactory{

    public:
        static std::unique_ptr<Instruction> Create(nlohmann::json& instr);

};

class BuildEngine{

    public:
        Image Build(const std::vector<nlohmann::json>& Instructions);

};