#include "parser.h"


namespace fs = std::filesystem;

using json = nlohmann::json;


static std::vector<std::string> getTokenList(const std::string& line){

    std::vector<std::string> list;
    std::string word;

    for(char c : line){
        if(c == '#')
            break;

        if(c == ' ' || c == ',' || c == '[' || c == ']'){
            if(!word.empty()){
                list.push_back(word);
                word.clear();
            }
            continue;
        }
        word += c;
    }

    if(!word.empty())
        list.push_back(word);

    return list;
}


json parseLine(const std::string& line){

    std::vector<std::string> tokens = getTokenList(line);
    json j;

    if(tokens.empty())
        return json();

    std::string cmd;
    for(auto c : tokens[0]){
        cmd += std::toupper(c);
    }

    j["cmd"] = cmd;
    
    if(cmd == "CMD"){
        
        size_t pos = line.find(tokens[0]);
        if(pos == std::string::npos || pos+3 > line.size()){
            std::cerr << "error parsing the string\n";
            return json(); 
        }

        std::string args = line.substr(pos+3,line.size() - (pos+3));
        args.erase(0, args.find_first_not_of(" \t"));
        try{
            j["args"] = json::parse(args);
        }
        catch(...){
            std::cerr << "error parsing the string\n";
            return json();
        }

    }
    else{
        std::vector<std::string> args;
        for(size_t i = 1;i < tokens.size();i++){
            args.push_back(tokens[i]);
        }
        j["args"] = args;
    }
    return j;
}




bool parseDocksmithFile(fs::path& path){

    std::ifstream f(path);
    if(!f.is_open()){
        std::cerr << "failed to open docksmith file !\n" ;
        return false;
    }

    std::string line;
    
    while(std::getline(f,line)){
        auto j = parseLine(line);
        if(j != json())
            std::cout << j << std::endl;
    }

    f.close();
    return true;
}
