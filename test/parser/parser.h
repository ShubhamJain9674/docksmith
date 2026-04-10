#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>

#include "../../include/json.hpp"

bool parseDocksmithFile(std::filesystem::path& path);