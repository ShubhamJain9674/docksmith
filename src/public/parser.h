#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>

#include "json.hpp"


bool parseDocksmithFile(std::filesystem::path& path);
