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

inline void createWorkDir(fs::path& dir){
    if(!fs::exists(dir)){
        fs::create_directory(dir);
    }
}

static std::vector<fs::path> getDeltaFiles(Snapshot& beforeSnapshot,Snapshot& afterSnapshot){

    std::vector<fs::path> delta;

    for (const auto& [path, afterInfo] : afterSnapshot) {
        auto it = beforeSnapshot.find(path);

        if (it == beforeSnapshot.end()) {
            // NEW file
            delta.push_back(path);
        } else {
            const auto& beforeInfo = it->second;

            if (beforeInfo.mtime != afterInfo.mtime ||
                beforeInfo.size  != afterInfo.size) {
                // MODIFIED file
                delta.push_back(path);
            }
        }
    }

    return delta;

}
static std::vector<fs::path> getDeletedFiles(Snapshot& beforeSnapshot,Snapshot& afterSnapshot){
    std::vector<std::filesystem::path> deleted;

    for (const auto& [path, _] : beforeSnapshot) {
        if (afterSnapshot.find(path) == afterSnapshot.end()) {
            deleted.push_back(path);
        }
    }

    return deleted;
}


static std::vector<std::string> parseCmds(const nlohmann::json& cmds) {
    std::vector<std::string> result;

    if (!cmds.is_array()) {
        throw std::runtime_error("Expected JSON array");
    }

    for (const auto& item : cmds) {
        if (!item.is_string()) {
            throw std::runtime_error("Expected all elements to be strings");
        }
        result.push_back(item.get<std::string>());
    }

    return result;
}

std::string BuildState::getLastLayerDigest() const {
    if (layers.empty())
        return "base";   // or base image digest later

    return layers.back().digest;
}


void FromInstruction::Execute(
    BuildState& state,
    CacheIndex& cache,
    bool& cache_broken
){

    (void)cache;
    (void)cache_broken;
    
    auto layers = image.getLayers();
    for(auto& layer : layers){
        state.addLayer(layer);
    }

}

void WorkingdirInstruction::Execute(
    BuildState& state,
    CacheIndex& cache,
    bool& cache_broken
){
    (void)cache;
    (void)cache_broken;
    state.setWorkdir(dir);
}

void EnvInstruction::Execute(
    BuildState& state,
    CacheIndex& cache,
    bool& cache_broken

){
    (void)cache;
    (void)cache_broken;
    state.env.push_back(env);
}

void CmdInstruction::Execute(
    BuildState& state,
    CacheIndex& cache,
    bool& cache_broken
){
    (void)cache;
    (void)cache_broken;
    state.setCmd(cmd);
}

void CopyInstruction::Execute(
    BuildState& state,
    CacheIndex& cache,
    bool& cache_broken
) {
    
    std::string prev_digest = state.getLastLayerDigest();
    std::string instruction_text = "COPY " + from + " " + dest;
    std::string source_hash = hashDirectory(from);
    std::string cache_key = computeCacheKey(
        prev_digest,
        instruction_text,
        state.getWorkdir(),
        state.getEnv(),
        source_hash
    );


    //cache hit
    if(!cache_broken && (cache.find(cache_key)!=cache.end()) ){
        std::string digest = cache[cache_key];
        if(layerExists(digest)){
            std::cout << "CACHE HIT " << instruction_text << "\n";
            Layer l;
            l.digest = digest;
            state.addLayer(l);  //check
            return;
        }
    }

    //cache miss
    cache_broken = true;


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
    std::uintmax_t file_size = 0;

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


void RunInstruction::Execute(
    BuildState& state,
    CacheIndex& cache,
    bool& cache_broken
){
    
    std::string prev_digest = state.getLastLayerDigest();
    std::string instruction_text = getCmd();
    std::string cache_key = computeCacheKey(
        prev_digest,
        instruction_text,
        state.getWorkdir(),
        state.getEnv(),
        ""
    );

    // cache hit condition :-

    if(!cache_broken && (cache.find(cache_key)!= cache.end())){
        std::string digest = cache[cache_key];

        if(layerExists(digest)){
            std::cout << "CACHE HIT " << instruction_text << "\n";
            Layer l;
            l.digest = digest;
            state.addLayer(l);
            return;
        }
    }

    //cache miss
    cache_broken = true;


    TempDir temp_dir;
    fs::path tmp  = fs::absolute(temp_dir.get());
    fs::path layer_dir = getLayerDir();

    auto layers = state.getLayers();
    // for each layer copy and extract tar file.
    for(auto& layer : layers){

        fs::path tar_path = layer_dir / layer.digest;
        // extract tar file :-
        extractTar(tar_path,tmp);
        handleWhiteouts(tmp);
    }

    fs::path workdir = tmp / state.getWorkdir();
    createWorkDir(workdir);

    auto beforeSnapshot = snapshotMtimes(tmp);
    // std::vector<std::string> commands = parseCmds(state.getCmds());

    runInRootLinux(tmp,workdir,state.getEnv(),cmd);

    auto afterSnapshot = snapshotMtimes(tmp);

    auto delta = getDeltaFiles(beforeSnapshot,afterSnapshot);
    auto deleted = getDeletedFiles(beforeSnapshot,afterSnapshot);

    //create file list
    std::vector<std::string> files;

    for (auto& p : delta) {
        files.push_back(p.string());
    }

    // add whiteouts
    for (auto& p : deleted) {
        std::filesystem::path wh = p.parent_path() / (".wh." + p.filename().string());
        files.push_back(wh.string());

        // create the whiteout file in rootfs
        std::ofstream(tmp / wh).close();
    }
    //sort files
    std::sort(files.begin(), files.end());

    fs::path tarfile = fs::absolute("/tmp/layer.tar");

    createTarFromDelta(tmp.string(),"/tmp/layer.tar",files);


    Layer delta_layer; 
    auto digest = encryptSHA256(tarfile);
    delta_layer.digest = digest;
    fs::rename(tarfile,(layer_dir / (digest + ".tar")));
    
    std::uintmax_t size = fs::file_size((layer_dir / (digest + ".tar")));
    delta_layer.size = size;

    


    delta_layer.createdBy = std::string("RUN ") + getCmd();

    state.addLayer(delta_layer);

    std::cout << "cache miss!" << std::endl;

    // temp dir handled by RAII object temp dir when it gets out of scope;


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
        return std::make_unique<RunInstruction>(instr["args"]);
    }
    else{
        std::cout << "Invalid instruction encountered!\n" << std::endl;
    }

    return nullptr;

}


Image BuildEngine::Build(std::vector<json>& Instructions,
    const std::string& name,
    const std::string& tag,
    bool no_cache
){

    InstructionFactory instr_fact;
    BuildState bs;
    
    CacheIndex cache_index = (no_cache)? CacheIndex{} :loadCache();
    bool cache_broken = no_cache;

    Image img;

    // execute all instructions
    for (auto& i : Instructions){
        auto instr = instr_fact.Create(i);
        instr->Execute(bs, cache_index,cache_broken);
        saveCache(cache_index);
    }

    //create image
    img.setName(name);
    img.setTag(tag);
    img.setCreated(getCurrentTimeISO8601());


    Config conf;
    conf.env = bs.env;
    conf.cmds = parseCmds(bs.cmd);
    conf.working_dir = bs.workdir;

    img.setConfig(conf);

    for(auto layer : bs.layers){
        img.addLayer(layer);
    }

    return img;

}