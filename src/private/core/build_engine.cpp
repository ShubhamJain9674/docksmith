#include "build_engine.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

bool startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

static void copy_dir_to_tmp(const std::string& tmp_path,std::string from,std::string dest){

    fs::path staging = fs::weakly_canonical(tmp_path);

    fs::path absSrc = fs::weakly_canonical(getContextDir() / from);
    fs::path contextRoot = fs::weakly_canonical(getContextDir());

    if (!fs::exists(absSrc)) {
        throw std::runtime_error("Source path does not exist");
    }

    fs::path rel = fs::relative(absSrc, contextRoot);
    if (rel.empty() || startsWith(rel.string(),"..")) {
        throw std::runtime_error("COPY outside build context not allowed");
    }

    fs::path relDest = fs::path(dest).lexically_normal();
    if (relDest.is_absolute()) {
        throw std::runtime_error("Invalid destination path: must not be absolute");
    }

    fs::path absDest = fs::weakly_canonical(staging / relDest);
    std::string stagingStr = staging.string();
    if (absDest.string().find(stagingStr) != 0) {
        throw std::runtime_error("Destination escapes staging directory");
    }

    if (fs::is_directory(absSrc)) {
        fs::create_directories(absDest);
        for (auto& entry : fs::recursive_directory_iterator(absSrc)) {
            fs::path entryRel = fs::relative(entry.path(), absSrc);
            fs::path target = absDest / entryRel;
            if (entry.is_directory()) {
                fs::create_directories(target);
            } else {
                fs::create_directories(target.parent_path());
                fs::copy_file(entry.path(), target,
                    fs::copy_options::overwrite_existing);
            }
        }
    } else {
        fs::create_directories(absDest.parent_path());
        fs::copy_file(absSrc, absDest,
            fs::copy_options::overwrite_existing);
    }
}

void FromInstruction::Execute(BuildState& state){

    auto layers = image.getLayers();
    for(auto& layer : layers){
        state.addLayer(layer);
    }

}

void WorkingdirInstruction::Execute(BuildState& state){
    state.setWorkdir(dir);
}

void EnvInstruction::Execute(BuildState& state){
    state.env.push_back(env);
}

void CmdInstruction::Execute(BuildState& state){
    state.setCmd(cmd);
}

void CopyInstruction::Execute(BuildState& state) {
    
    TempDir temp_dir;
    fs::path staging = fs::absolute(temp_dir.get());
    copy_dir_to_tmp(staging, from, dest);

    const fs::path layers_path = getExecutableDir() / "layers";

    fs::path tarPath =  layers_path / "layer.tar";

    std::string cmd =
        "tar "
        "--sort=name "
        "--mtime='UTC 1970-01-01' "
        "--owner=0 --group=0 --numeric-owner "
        "--format=ustar "
        "-cf \"" + tarPath.string() + "\" "
        "-C \"" + staging.string() + "\" .";

    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        throw std::runtime_error("Failed to create tar");
    }

    std::string digest = encryptSHA256(tarPath);
    std::uintmax_t file_size;

    try {
        file_size = std::filesystem::file_size(layers_path / digest);
        fs::rename(tarPath,layers_path / digest);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error  file operation: " << e.what() << std::endl;
    }

    Layer l;
    l.digest = digest;
    l.size = file_size;
    l.createdBy = "COPY " + from + dest;
    state.addLayer(l);

    std::cout << "cache miss !" << std::endl;

}
std::unique_ptr<Instruction> InstructionFactory::Create(json& instr){

    std::string cmd = instr["cmd"];
    
    if(cmd == "FROM"){
        return std::make_unique<FromInstruction>(instr["args"][0]);
    }
    else if(cmd == "WORKDIR"){
        return std::make_unique<WorkingdirInstruction>(instr["args"][0]);
    }
    else if(cmd == "ENV"){
        return std::make_unique<EnvInstruction>(instr["args"][0]);
    }
    else if(cmd == "CMD"){
        return std::make_unique<CmdInstruction>(instr["args"]);
    }
    else if(cmd == "COPY"){
        return std::make_unique<CopyInstruction>(instr["args"][0],instr["args"][1]);
    }
    else if(cmd == "RUN"){
        return nullptr; // not implemented yet.
    }
    else{
        std::cout << "Invalid instruction encountered!\n" << std::endl;
    }

    return nullptr;

}


