#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <unistd.h>
#include <filesystem>

namespace fs = std::filesystem;
// using namespace std;

#ifdef _WIN32
  const char ENV_SEP = ';';
#else 
  const char ENV_SEP = ':';
#endif

std::vector<std::string> builtins = {"pwd","exit","type","echo"};

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

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage

  const char* raw_env = std::getenv("PATH");

  std::string path_env(raw_env);
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
    
    if (command == "exit") break;
    if(command == "pwd"){
      fs::path cwd = fs::current_path();
      std::cout << cwd.string() << '\n';
    } else if(command.substr(0,4) == "echo") {
      std::cout << command.substr(5) << '\n';
    } else if(command.substr(0,4) == "type") {
      std::string args = command.substr(5);
      if(checkBuiltin(args)){
        std::cout << args << " is a shell builtin\n";
      }else{
        bool found = false;
        
        for(auto dir: directories){
          fs::path filePath = dir / args;
          if(is_runnable(filePath.string())){
            std::cout<< args << " is " << filePath.make_preferred().string() << '\n';
            found = true;
          }
          if(found)break;
        }

        if(!found) std:: cout << args <<": not found\n";
      } 
    } else {
      std::stringstream ss(command);
      std::string program;
      ss >> program; 
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
