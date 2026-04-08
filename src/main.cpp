#include <iostream>
#include <cassert>

#include "CLI11.hpp"
#include "cli/cli.h"
#include "core/setup.h"



int main(int argc, char* argv[]) {

    initDocksmithDir();
    
    CLI::App app{"Docksmith is a simplified Docker-like build and runtime system."};



    bool verbose = false;
    app.add_flag("-v,--verbose", verbose, "Enable verbose output");


    //build cmds
    std::string build_tag;
    std::string build_context = ".";
    bool no_cache = false;

    auto build = app.add_subcommand("build", "Build an image from a Docksmithfile");

    build->add_option("-t,--tag", build_tag, "Image tag (name:tag)")->required();
    build->add_option("context", build_context, "Build context directory")->default_val(".");
    build->add_flag("--no-cache", no_cache, "Disable build cache");

    build->callback([&](){
        buildCmd(build_tag,build_context,no_cache);
    });



    //run cmd;
    std::string run_image;
    std::vector<std::string> run_cmd;
    std::vector<std::string> env_vars;

    auto run = app.add_subcommand("run", "Run a container");

    run->add_option("image", run_image, "Image name:tag")->required();
    run->add_option("cmd", run_cmd, "Override command");
    run->add_option("-e", env_vars, "Environment variables (KEY=VALUE)");

    run->callback([&]() {
        runCmd(run_image,run_cmd,env_vars);
    });

    auto images = app.add_subcommand("images", "List images");

    images->callback(imagesCmd);

    std::string rmi_image;

    auto rmi = app.add_subcommand("rmi", "Remove an image");

    rmi->add_option("image", rmi_image, "Image name:tag")->required();

    rmi->callback([&]() {
        rmiCmd(rmi_image);
    });

    // Ensure at least one subcommand
    app.require_subcommand(1);

    CLI11_PARSE(app, argc, argv);

    return 0;
}