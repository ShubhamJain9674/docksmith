#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>

#include "json.hpp"


std::optional<std::vector<nlohmann::json>> parseDocksmithFile(const std::filesystem::path& path);
