#pragma once

#include <filesystem>
#include "file_handling.h"
#include "Image.h"


bool isBaseImageAvailable();                                    // looks if the base linux image is present?
void initDocksmithDir();                                        // creates the dir structure for the application.
void storeAlpineLayer(Layer l,std::string name);                // store alpine linux layer file
void saveBaseLinuxImage(Layer l);            // saves manifest base linux image file.  
Layer createBaseLinuxLayer(std::string digest,size_t size);     // returns layer for base linux file
std::string getBaseImageTarFile();                              // get base image tar file


std::string calculateBaseLinuxDigest();                          // get digest

