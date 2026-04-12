#pragma once
#include <iostream>
#include <vector>
#include <fstream>
#include <json.hpp>
#include <filesystem>

#include "file_handling.h"
#include "Image.h"

#include "parser.h"
#include "build_engine.h"





void buildCmd(const std::string& build_tag,const std::string& build_context,bool no_cache);

void runCmd(const std::string& run_image,
            const std::vector<std::string>& run_cmd,
            const std::vector<std::string>& env_vars);

void imagesCmd();
void rmiCmd(const std::string& rmi_image);