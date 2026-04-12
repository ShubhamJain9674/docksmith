#pragma once

#include <vector>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <iomanip>

#include "file_handling.h"
#include "json.hpp"
#include "crypto.h"



using CacheIndex = std::unordered_map<std::string, std::string>;

// load / save
CacheIndex loadCache();
void saveCache(const CacheIndex& index);

// cache key
std::string computeCacheKey(
    const std::string& prevDigest,
    const std::string& instructionText,
    const std::string& workdir,
    const std::vector<std::string>& env,
    const std::string& sourceHash // empty for RUN
);

// COPY hashing
std::string hashDirectory(const std::filesystem::path& dir);

// utils
std::string normalize(const std::string& s);
std::filesystem::path getCacheDir();