
#include "crypto.h"


std::string encryptSHA256(const std::filesystem::path& filePath) {
    // Open file in binary mode
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return "";
    }

    // Create EVP message digest context
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        std::cerr << "EVP_MD_CTX_new failed" << std::endl;
        return "";
    }

    // Initialize with SHA256
    if (!EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr)) {
        std::cerr << "EVP_DigestInit_ex failed" << std::endl;
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    // Read file in chunks and update digest
    char buffer[8192]; // 8 KB chunks
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        if (!EVP_DigestUpdate(mdctx, buffer, file.gcount())) {
            std::cerr << "EVP_DigestUpdate failed" << std::endl;
            EVP_MD_CTX_free(mdctx);
            return "";
        }
    }

    // Finalize and get digest
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int result_len = 0;
    if (!EVP_DigestFinal_ex(mdctx, result, &result_len)) {
        std::cerr << "EVP_DigestFinal_ex failed" << std::endl;
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    EVP_MD_CTX_free(mdctx);

    // Convert to hexadecimal string with "sha256:" prefix
    std::stringstream shastr;
    shastr << "sha256:" << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < result_len; ++i) {
        shastr << std::setw(2) << (int)result[i];
    }

    return shastr.str();
}

