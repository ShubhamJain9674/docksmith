#include "cache.h"



using json = nlohmann::json;
namespace fs = std::filesystem;





CacheIndex loadCache() {
    CacheIndex index;

    fs::path cache_path = getCacheDir() / "index.json";

    std::ifstream f(cache_path);
    if (!f) return index;

    json j;
    f >> j;

    for (auto& [k, v] : j.items()) {
        index[k] = v.get<std::string>();
    }

    return index;
}


void saveCache(const CacheIndex& index) {
    fs::path cache_path = getCacheDir() / "index.json";


    fs::create_directories(cache_path.parent_path());

    json j;
    for (auto& [k, v] : index) {
        j[k] = v;
    }

    std::ofstream(cache_path) << j.dump(2);
}


std::string normalize(const std::string& s) {
    std::stringstream ss(s);
    std::string word, result;
    while (ss >> word) {
        if (!result.empty()) result += " ";  // add space between words
        result += word;
    }
    return result;
}


std::string computeCacheKey(
    const std::string& prevDigest,
    const std::string& instructionText,
    const std::string& workdir,
    const std::vector<std::string>& env,
    const std::string& sourceHash
) {
    std::stringstream ss;

    ss << prevDigest << "\n";
    ss << normalize(instructionText) << "\n";
    ss << workdir << "\n";

    // sort env for determinism
    auto sortedEnv = env;
    std::sort(sortedEnv.begin(), sortedEnv.end());

    for (auto& e : sortedEnv) {
        ss << e << "\n";
    }

    ss << sourceHash;

    return "sha256:" + sha256String(ss.str());
}


std::string hashDirectory(const fs::path& dir) {
    std::vector<fs::path> entries;

    for (auto& p : fs::recursive_directory_iterator(dir)) {
        if (fs::is_regular_file(p)) {
            // RELATIVE PATH (CRITICAL)
            entries.push_back(fs::relative(p.path(), dir));
        }
    }

    std::sort(entries.begin(), entries.end());

    std::stringstream ss;

    for (auto& rel : entries) {
        fs::path full = dir / rel;

        // add separators to avoid collisions
        ss << rel.string() << "\n";
        ss << fs::file_size(full) << "\n";

        // content-based hashing
        std::ifstream f(full, std::ios::binary);
        ss << f.rdbuf();
    }

    return "sha256:" + sha256String(ss.str());
}