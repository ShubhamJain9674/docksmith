#pragma once
#include <iostream>
#include <vector>
#include <fstream>
#include <json.hpp>
#include <filesystem>
#include <unordered_set>
#include <iomanip>

#include "file_handling.h"
#include "Image.h"

#include "parser.h"
#include "build_engine.h"


#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[1;34m"




void buildCmd(const std::string& build_tag,const std::string& build_context,bool no_cache);

void runCmd(const std::string& run_image,
            const std::vector<std::string>& run_cmd,
            const std::vector<std::string>& env_vars);

void imagesCmd();
void rmiCmd(const std::string& rmi_image);