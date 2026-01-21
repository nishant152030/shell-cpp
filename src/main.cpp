#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <unistd.h>
#include <filesystem>
#include <regex>
// #include <boost/program_options/parser.hpp>
namespace fs = std::filesystem;
// using namespace std;

#ifdef _WIN32
  const char ENV_SEP = ';';
#else 
  const char ENV_SEP = ':';
#endif

std::vector<std::string> builtins = {"pwd","exit","type","echo","cd"};

bool is_runnable(const std::string& path){
  if(!fs::exists(path) || !fs::is_regular_file(path)) return false;

  return (access(path.c_str(), X_OK) == 0);
}

bool checkBuiltin(const std::string& command){
  for(auto &s: builtins) {
    if(s == command)return true;
  }
  return false;
}

std::pair<std::string,std::vector<std::string>> getCommandArgs(const std::string &command){
  std::vector<std::string> args;
  std::string program;
  std::string current;

  bool inQuotes = false;
  char nullChar = '\0';
  char quoteChar = nullChar;
  bool escaped = false;

  for(char c: command){
    if(escaped){
      current += c;
      escaped = false;
      continue;
    }
    
    if(c == '\\' && quoteChar != '\''){
      escaped = true;
      continue;
    }
    
    if(inQuotes){
      if( c == quoteChar ){
        inQuotes = false;
        c = nullChar;
      } else {
        current += c;
      }
    } else {
      if( c == '\'' || c == '\"'){
        inQuotes = true;
        quoteChar = c;
      } else if( std::isspace(c)){
        if(!current.empty()) {
          if(program.empty()) program = current;
          else args.push_back(current);
          current.erase();
        }
      } else {
        current += c;
      }
    }
  }

  if(!current.empty()) {
    if(program.empty()) program = current;
    else args.push_back(current);
    current.erase();
  }
  return std::make_pair(program,args);
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage

  const char* raw_env = std::getenv("PATH");
  const char* raw_home_env = std::getenv("HOME");

  std::string path_env(raw_env);
  fs::path home_env(raw_home_env);

  std::vector<fs::path> directories;
  
  std::stringstream ss(path_env);
  std::string item;
  while(std::getline(ss,item,ENV_SEP)){
    if(!item.empty()){
      directories.push_back(fs::path(item));
    }
  }

  while(true){
    std::cout << "$ ";
    std::string command;
    std::getline(std::cin, command);
    std::stringstream ss(command);

    auto [program,args] = getCommandArgs(command);

    // std::string program = commandAndargs[0];
    
    if (program == "exit") break;

    if(program == "pwd")
    {
      fs::path cwd = fs::current_path();
      std::cout << cwd.string() << '\n';
    } 
    else if(program == "echo") 
    {
      for(size_t i=0; i<args.size() ; i++){
        std::cout << args[i] << ((i+1 != args.size()) ? ' ':'\n');
      }
    } 
    else if(program == "type") 
    {
      for(auto &arg: args){
        if(checkBuiltin(arg))
        {
          std::cout << arg << " is a shell builtin\n";
        }
        else
        {
          bool found = false;

          for(auto dir: directories){
            fs::path filePath = dir / arg;
            if(is_runnable(filePath.string())){
              std::cout<< arg << " is " << filePath.make_preferred().string() << '\n';
              found = true;
            }
            if(found) break;
          }

          if(!found) std:: cout << arg <<": not found\n";
        } 
      }
    } 
    else if(program == "cd") {
      for(auto &new_dir: args){
        new_dir = (new_dir == "~") ? home_env.string() : new_dir;
        try {
          fs::current_path(new_dir);
        } catch (const fs::filesystem_error& e){
          std::cerr << "cd: " << new_dir << ": " << e.code().message() << '\n';
        }
      }
    }
    else 
    {
      // std::stringstream ss(command);
      bool found = false;
      for(auto dir: directories){
        fs::path filePath = dir / program;
        if(is_runnable(filePath.string())){
          std::system(command.c_str());
          found = true;
        }
        if(found)break;
      }
      if(!found) std:: cout << program <<": not found\n";
    }
  }
}
