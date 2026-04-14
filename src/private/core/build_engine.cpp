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


    fs::path absDest;

    if (relDest.is_absolute()) {
        // absolute → relative to container root
        absDest = staging / relDest.relative_path();
    } else {
        // relative → relative to staging root (or WORKDIR later)
        absDest = staging / relDest;
    }

    
    // fs::path absDest = fs::weakly_canonical(staging / relDest);
    // fs::path absDest = (staging / relDest).lexically_normal();

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


static std::vector<fs::path> getDeltaFiles(Snapshot& beforeSnapshot, Snapshot& afterSnapshot) {
    std::vector<fs::path> delta;

    for (const auto& [path, afterInfo] : afterSnapshot) {
        auto it = beforeSnapshot.find(path);

        bool changed = false;
        if (it == beforeSnapshot.end()) {
            changed = true;
        } else {
            const auto& beforeInfo = it->second;
            if (beforeInfo.isSymlink && afterInfo.isSymlink) {
                changed = beforeInfo.symlinkTarget != afterInfo.symlinkTarget;
            } else {
                changed = beforeInfo.mtime != afterInfo.mtime ||
                          beforeInfo.size  != afterInfo.size;
            }
        }

        if (changed) {
            // normalize to ./path so tar has consistent prefixes
            std::string normalized = path;
            if (normalized.substr(0, 2) != "./")
                normalized = "./" + normalized;
            delta.push_back(normalized);
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

    if(cmds == json(nullptr)){
        return result;
    }

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

    return stripSHA256(layers.back().digest);
}





std::optional<Layer> buildLayer(
    BuildState& state,
    std::vector<std::string>& cmds
) {
    TempDir temp_dir;
    fs::path tmp = fs::absolute(temp_dir.get());
    fs::path layer_dir = getLayerDir();

    for (auto& layer : state.getLayers()) {
        fs::path tar_path = layer_dir / (layer.digest + ".tar");
        // fprintf(stderr, "extracting layer: %s exists=%d\n", 
        //     tar_path.c_str(),
        //     fs::exists(tar_path));
        extractTar(tar_path, tmp);
        handleWhiteouts(tmp);
    }

    std::string chmod_cmd = "chmod 755 " + tmp.string();
    system(chmod_cmd.c_str());


    // fs::path workdir = tmp / state.getWorkdir();
    
    std::string wd = state.getWorkdir();
    if (!wd.empty() && wd[0] == '/')
        wd = wd.substr(1);  // strip leading /


    // fprintf(stderr, "workdir from state: '%s'\n", state.getWorkdir().c_str());
    fs::path workdir = tmp / wd;
    // fprintf(stderr, "workdir full path: '%s'\n", workdir.c_str());    




    // fs::create_directories(workdir);
    if (!fs::exists(workdir)) {
        // create it on host with a shell command so permissions aren't an issue
        std::string cmd = "mkdir -p " + workdir.string();
        system(cmd.c_str());
    }

    auto beforeSnapshot = snapshotMtimes(tmp);

    bool success = runInRootLinux(tmp, wd, state.getEnv(), cmds);
    if (!success)
        throw std::runtime_error("Build failed");

    // fprintf(stderr, "=== HOST VIEW OF /app AFTER RUN ===\n");
    // for (auto& p : fs::directory_iterator(tmp / "app")) {
    //     fprintf(stderr, "  %s exists=%d\n", 
    //         p.path().c_str(),
    //         fs::exists(p.path()));
    // }

    auto afterSnapshot = snapshotMtimes(tmp);
    auto delta = getDeltaFiles(beforeSnapshot, afterSnapshot);
    // fprintf(stderr, "=== DELTA ===\n");
    // for (auto& p : delta)
        // fprintf(stderr, "  %s\n", p.c_str());
    // fprintf(stderr, "=== END DELTA ===\n");

    auto deleted = getDeletedFiles(beforeSnapshot, afterSnapshot);

    std::vector<std::string> files;
    for (auto& p : delta)
        files.push_back(p.string());

    for (auto& p : deleted) {
        fs::path wh = p.parent_path() / (".wh." + p.filename().string());
        files.push_back(wh.string());
        std::ofstream(tmp / wh).close();
    }

    std::sort(files.begin(), files.end());

    fs::path tarfile = fs::absolute("/tmp/layer.tar");
    
    if (files.empty()) {
        // no changes, don't create a new layer
        return std::nullopt;  // return empty layer or handle in caller
    }

    createTarFromDelta(tmp.string(), tarfile.string(), files);

    // fprintf(stderr, "=== TAR CONTENTS ===\n");
    std::string list_cmd = "tar -tvf /tmp/layer.tar 2>&1";
    system(list_cmd.c_str());
    // fprintf(stderr, "=== END TAR ===\n");

    Layer new_layer;
    new_layer.digest = encryptSHA256(tarfile);
    new_layer.size = fs::file_size(tarfile);

    try {
        fs::copy_file(tarfile, layer_dir / (new_layer.digest + ".tar"),
                      fs::copy_options::overwrite_existing);
        fs::remove(tarfile);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Layer move failed: " << e.what() << "\n";
    }

    return new_layer;
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
    std::string prev_digest = state.getLastLayerDigest();
    std::string cache_key = stripSHA256(computeCacheKey(
        prev_digest,
        "WORKDIR " + dir,
        state.getWorkdir(),
        state.getEnv(),
        ""
    ));
    // fprintf(stderr, "looking up key: '%s'\n", cache_key.c_str());
    // fprintf(stderr, "cache size: %zu\n", cache.size());
    // for (auto& [k,v] : cache)
    //     fprintf(stderr, "  index key: '%s'\n", k.c_str());

    if (!cache_broken && cache.find(cache_key) != cache.end()) {
        std::string digest = stripSHA256(cache[cache_key]);
        if (layerExists(digest)) {
            std::cout << "CACHE HIT WORKDIR " << dir << "\n";
            Layer l; l.digest = digest;
            state.addLayer(l);
            state.setWorkdir(dir);
            return;
        }
    }

    cache_broken = true;
    std::vector<std::string> mkdirCmd = { "mkdir -p " + dir };
    auto l = buildLayer(state, mkdirCmd);

    if (l.has_value()) {
        state.addLayer(l.value());
        cache[cache_key] = "sha256:" + l.value().digest;
    }
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
    // std::string source_hash = hashDirectory(from);
    fs::path absSrc = fs::weakly_canonical(getContextDir() / from);

    std::string source_hash;

    if (fs::is_directory(absSrc)) {
        source_hash = hashDirectory(absSrc);
    } else if (fs::is_regular_file(absSrc)) {
        source_hash = stripSHA256(encryptSHA256(absSrc));
    } else {
        throw std::runtime_error("Invalid COPY source");
    }

    // fprintf(stderr, "prev_digest: %s\n", prev_digest.c_str());
    std::string cache_key = stripSHA256(computeCacheKey(
        prev_digest,
        instruction_text,
        state.getWorkdir(),
        state.getEnv(),
        source_hash
    ));
    // fprintf(stderr, "cache_key: %s\n", cache_key.c_str());

    // fprintf(stderr, "looking up key: '%s'\n", cache_key.c_str());
    // fprintf(stderr, "cache size: %zu\n", cache.size());
    // for (auto& [k,v] : cache)
    //     fprintf(stderr, "  index key: '%s'\n", k.c_str());


    //cache hit
    if(!cache_broken && (cache.find(cache_key)!=cache.end()) ){
        std::string digest = stripSHA256(cache[cache_key]);
        // fprintf(stderr, "cache hit candidate: %s exists=%d\n", 
        //     digest.c_str(), layerExists(digest));

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
        fs::rename(tarPath,layers_path / (digest + ".tar"));
        file_size = std::filesystem::file_size(layers_path / (digest +".tar"));
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error  file operation: " << e.what() << std::endl;
    }

    Layer l;
    l.digest = digest;
    l.size = file_size;
    l.createdBy = "COPY " + from + dest;
    state.addLayer(l);
    cache[cache_key] = "sha256:" + l.digest;
    saveCache(cache);

    std::cout << "cache miss !" << std::endl;

}


void RunInstruction::Execute(
    BuildState& state,
    CacheIndex& cache,
    bool& cache_broken
){
    
    std::string prev_digest = state.getLastLayerDigest();
    std::string instruction_text = getCmd();
    // std::cout << "get cmd() function check : " << getCmd() << "\n";
    // fprintf(stderr, "prev_digest: %s\n", prev_digest.c_str());
    std::string cache_key = stripSHA256(computeCacheKey(
        prev_digest,
        instruction_text,
        state.getWorkdir(),
        state.getEnv(),
        ""
    ));
    // fprintf(stderr, "cache_key: %s\n", cache_key.c_str());
    
    // fprintf(stderr, "looking up key: '%s'\n", cache_key.c_str());
    // fprintf(stderr, "cache size: %zu\n", cache.size());
    // for (auto& [k,v] : cache)
    //     fprintf(stderr, "  index key: '%s'\n", k.c_str());
    
    // cache hit condition :-

    if(!cache_broken && (cache.find(cache_key)!= cache.end())){
        std::string digest = stripSHA256(cache[cache_key]);

        // fprintf(stderr, "cache hit candidate: %s exists=%d\n", 
        //     digest.c_str(), layerExists(digest));

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


    auto delta_layer = buildLayer(state,cmd);



    if(delta_layer.has_value()){
        delta_layer.value().createdBy = std::string("RUN ") + getCmd();
        state.addLayer(delta_layer.value());
        cache[cache_key] = "sha256:" + delta_layer.value().digest;
        saveCache(cache);
    }else{
        cache[cache_key] = "sha256:" + prev_digest;
    }

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
        if(!layer.digest.empty())
            img.addLayer(layer);
    }

    return img;

}