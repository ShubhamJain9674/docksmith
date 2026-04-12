#pragma once
#include <openssl/evp.h>
#include <openssl/err.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <filesystem>

std::string encryptSHA256(const std::filesystem::path& filePath);
std::string sha256String(const std::string& data);