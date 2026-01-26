#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <unistd.h>
#include <filesystem>
#include <regex>
#include <fcntl.h>
// #include <boost/program_options/parser.hpp>
namespace fs = std::filesystem;
// using namespace std;

#ifdef _WIN32
  #include <windows.h>
  #include <io.h>
  #include <sys/stat.h>
  const char PATH_SEP = ';';
#else 
  #include <sys/types.h>
  #include <sys/wait.h>
  const char PATH_SEP = ':';
#endif

std::vector<std::string> builtins = {"pwd","exit","type","echo","cd"};

bool isExecutable(const std::string& path){
  if(!fs::exists(path) || !fs::is_regular_file(path)) return false;

  auto perms = fs::status(path).permissions();
  return ((perms & fs::perms::owner_exec) == fs::perms::owner_exec) ||
         ((perms & fs::perms::group_exec) == fs::perms::group_exec) ||
         ((perms & fs::perms::others_exec) == fs::perms::others_exec);
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

  char quoteChar = '\0';  // '\0' means not in quotes, '"' or '\'' means in that type of quote
  bool escaped = false;

  for(size_t i = 0; i < command.size() ; i++){
    char c = command[i];
    if(escaped){
      current += c;
      escaped = false;
      continue;
    }
    
    // Backslash escaping works:
    // - Outside quotes: escapes any character
    // - Inside double quotes: escapes any character
    // - Inside single quotes: backslash has no special meaning
    if(c == '\\'){
      if(quoteChar == '\''){
        current += c;
      }else if(quoteChar == '\"') {
        char next_char = (i+1 == command.size()) ? '\0' : command[i+1];
        if(next_char == '\"' || next_char == '$' || next_char == '\\' || next_char == '\n' || next_char == '`') {
          escaped = true;
        }else {
          current += c;
        }
      }else escaped = true;
      continue;
    }
    
    // If in quotes, only the matching quote char can end the quotes
    if(quoteChar != '\0'){
      if(c == quoteChar){
        quoteChar = '\0';  // End quotes
      } else {
        current += c;
      }
    } else {
      // Not in quotes
      if(c == '\'' || c == '\"'){
        quoteChar = c;  // Start quotes
      } else if(std::isspace(c)){
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

std::vector<char*> handle_args(const std::string &program, std::vector<std::string> &args){
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(program.c_str()));
    bool should_out_redirect = false;
    bool should_err_redirect = false;
    bool append_redirect = false;
    bool err_append_redirect = false;
    std::string out_loc = "";
    std::string err_loc = "";
    for(size_t i = 0; i < args.size(); ++i) {
      if (args[i] == ">" || args[i] == "1>") {
        should_out_redirect = true;
        if (i + 1 < args.size()) {
            out_loc = args[i + 1]; // Get file following the operator
        }
        break; // Stop adding to argv once redirection starts
      } else if (args[i] == ">>" || args[i] == "1>>") {
        append_redirect = true;
        if (i + 1 < args.size()) {
            out_loc = args[i + 1]; // Get file following the operator
        }
        break; // Stop adding to argv once redirection starts
      } else if (args[i] == "2>") {
        should_err_redirect = true;
        if (i + 1 < args.size()) {
            err_loc = args[i + 1]; // Get file following the operator
        }
        break; // Stop adding to argv once redirection starts
      } else if (args[i] == "2>>") {
        err_append_redirect = true;
        if (i + 1 < args.size()) {
            err_loc = args[i + 1]; // Get file following the operator
        }
        break; // Stop adding to argv once redirection starts
      }
      argv.push_back(const_cast<char*>(args[i].c_str()));
    }
    argv.push_back(nullptr);

    if((should_out_redirect || append_redirect) && !out_loc.empty()){
      int fd = open(out_loc.c_str(), O_WRONLY | O_CREAT | ((should_out_redirect) ? O_TRUNC : O_APPEND), 0644);
      if(fd == -1) {
        perror("open");
      }
      if(dup2(fd, STDOUT_FILENO) == -1){
        perror("dup2");
      }
      close(fd);
    }
    if((should_err_redirect || err_append_redirect) && !err_loc.empty()){
      int fd = open(err_loc.c_str(), O_WRONLY | O_CREAT | ((should_err_redirect) ? O_TRUNC : O_APPEND), 0644);
      if(fd == -1) {
        perror("open");
      }
      if(dup2(fd, STDERR_FILENO) == -1){
        perror("dup2");
      }
      close(fd);
    }
    return argv;
}

bool external_command_run(const std::string &program, std::vector<std::string> &args, std::vector<fs::path> &dirs){

  for(auto &dir: dirs){
    fs::path full_path = dir / program; 
    if(!isExecutable(full_path.string())) continue;;
  
    #ifdef _WIN32
      std::string cmd = "\"" + full_path.string() + "\"";\
      for(const auto &a: args) cmd += "\"" + a + "\"";
  
      STARTUPINFOA si{};
      PROCESS_INFORMATION pi{};
      si.cb = sizeof(si);
  
      if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) continue;
  
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
  
      return true;
    #else
      pid_t pid = fork();
      if(pid == 0){
        std::vector<char*> argv = handle_args(program,args);
        execvp(full_path.c_str(), argv.data());
        perror("execv failed");
        exit(1);
  
      } else if ( pid > 0){
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 127) continue;
        return true;
      } else {
        perror("fork failed");
      }
    #endif 

  }
  return false;
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
  while(std::getline(ss,item,PATH_SEP)){
    if(!item.empty()){
      directories.push_back(fs::path(item));
    }
  }

  while(true){
    std::cout << "$ ";
    std::string command;
    
    std::getline(std::cin, command);
    std::stringstream ss(command);

    auto [program, args] = getCommandArgs(command);

    if (program == "exit") break;

    if(program == "pwd")
    {
      fs::path cwd = fs::current_path();
      std::cout << cwd.string() << '\n';
    } 
    else if(program == "echo") 
    {
      #ifdef _WIN32
      #else
        pid_t pid = fork();
        if(pid == 0) {
          std::vector<char*> argv = handle_args(program,args);
          for(size_t i = 1; i < argv.size()-1; ++i) {
            std::cout << argv[i] ;
            std::cout << ((i == argv.size()-2) ? '\n' : ' ');
          }
          exit(0);
        } else if ( pid > 0){
          int status;
          waitpid(pid, &status, 0);
        } else {
          perror("unable to fork");
        }
      #endif
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
            if(isExecutable(filePath.string())){
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
      //handle not builtin command
      if(!external_command_run(program,args,directories)) std:: cout << program << ": not found\n";
    }
  }
}
