#include <iostream>
#include <cassert>

#include "CLI11.hpp"


void buildCmd(const std::string& build_tag,const std::string& build_context,bool no_cache){
    std::cout << "[BUILD]\n";
    std::cout << "Tag: " << build_tag << "\n";
    std::cout << "Context: " << build_context << "\n";
    std::cout << "No cache: " << (no_cache ? "true" : "false") << "\n";

    //implement build command :-


}

void runCmd(const std::string& run_image,
            const std::vector<std::string>& run_cmd,
            const std::vector<std::string>& env_vars)
{
    std::cout << "[RUN]\n";
    std::cout << "Image: " << run_image << "\n";

    std::cout << "Env:\n";
    for (auto &e : env_vars) {
        std::cout << "  " << e << "\n";
    }

    std::cout << "Cmd:\n";
    for (auto &c : run_cmd) {
        std::cout << "  " << c << "\n";
    }


    //implement run command:-

}

void imagesCmd(){
    std::cout << "images_cmd called" << std::endl;

    //implement images command:-
}


void rmiCmd(const std::string& rmi_image){
    std::cout << "[RMI]\n";
    std::cout << "Removing: " << rmi_image << "\n";

    //implement rmi command:-

}


int main(int argc, char* argv[]) {

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