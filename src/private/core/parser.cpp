#include "parser.h"


namespace fs = std::filesystem;

using json = nlohmann::json;

enum ParserStatus{
    OK,
    IGNORE,
    ERROR
};

struct ParserResult{
    json data;
    ParserStatus status;
    std::string error;

};


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


static ParserResult parseLine(const std::string& line){

    const std::array<std::string,7> commandList = {"FROM","COPY","RUN","DIR","WORKDIR","ENV","CMD"} ; 
    ParserResult result{json(),ParserStatus::ERROR,""};

    std::vector<std::string> tokens = getTokenList(line);
    json j;

    if(tokens.empty()){
        result.status = ParserStatus::IGNORE;
        return result;
    }
        

    std::string cmd; 
    for(auto c : tokens[0]){
        cmd += std::toupper(static_cast<unsigned char>(c));
    }

    j["cmd"] = cmd;
    if(std::find(commandList.begin(),commandList.end(),cmd) == commandList.end()){
        result.error = "Unknown Instruction " + cmd;
        result.status = ParserStatus::ERROR;
        return result;
    }
    
    if(cmd == "CMD"){
        
        size_t pos = line.find(cmd);
        if(pos == std::string::npos || pos+tokens[0].size() > line.size()){
            result.error = "Malformed cmd instruction";
            result.status = ParserStatus::ERROR;
            return result; 
        }

        std::string args = line.substr(pos + tokens[0].size());
        args.erase(0, args.find_first_not_of(" \t"));
        try{
            j["args"] = json::parse(args);
            result.data = j;
            result.status = ParserStatus::OK;
        }
        catch(...){
            result.error = "invalid json in command";
            result.status = ParserStatus::ERROR;
            return result;
        }

    }
    else{
        std::vector<std::string> args;
        for(size_t i = 1;i < tokens.size();i++){
            args.push_back(tokens[i]);
        }
        j["args"] = args;
        result.data = j;
        result.status = ParserStatus::OK;
    }
    return result;
}




std::optional<std::vector<json>> parseDocksmithFile(const fs::path& path){

    std::ifstream f(path);
    if(!f.is_open()){
        std::cerr << "failed to open docksmith file !\n" ;
        return std::nullopt;
    }

    std::string line;
    int line_no = 1;
    std::vector<json> list;
    bool error = false;
    
    while(std::getline(f,line)){
        
        auto result = parseLine(line);
        if(result.status == ParserStatus::OK)
            list.push_back(result.data);
        else if(result.status == ParserStatus::ERROR)    {
            // throw std::runtime_error(result.error + " at line: " + std::to_string(line_no));
            std::cout << result.error << " at line: " << line_no << std::endl;
            error = true;
        }
        line_no++;
    }


    if(error)
        throw std::runtime_error(" ");
    return list;
    
}


