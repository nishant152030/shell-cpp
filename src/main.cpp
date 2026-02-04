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
#include "trie.h"

namespace fs = std::filesystem;
#define ESC_SEQ 27
#define UP_CODE 'A'
#define DOWN_CODE 'B'

#ifdef _WIN32
  #include <windows.h>
  #include <io.h>
  #include <sys/stat.h>
  #include <conio.h>
  const char PATH_SEP = ';';
#else 
  #include <sys/types.h>
  #include <sys/wait.h>
  #include <termios.h>
  const char PATH_SEP = ':';
#endif

struct Command {
  std::vector<std::string> args;
  std::string out_file;
  std::vector<char*> argv;

  int saved_out = -1;
  int saved_err = -1;

  bool should_out_redirect = false;
  bool should_err_redirect = false;
  bool append_redirect = false;
  bool err_append_redirect = false;
  
  void get_argv() {
    for(size_t i = 0; i < args.size(); ++i) {
      if (args[i] == ">" || args[i] == "1>") {
        should_out_redirect = true;
        if (i + 1 < args.size()) {
            out_file = args[i + 1]; // Get file following the operator
        }
        break; // Stop adding to argv once redirection starts
      } else if (args[i] == ">>" || args[i] == "1>>") {
        append_redirect = true;
        if (i + 1 < args.size()) {
            out_file = args[i + 1]; // Get file following the operator
        }
        break; // Stop adding to argv once redirection starts
      } else if (args[i] == "2>") {
        should_err_redirect = true;
        if (i + 1 < args.size()) {
            out_file = args[i + 1]; // Get file following the operator
        }
        break; // Stop adding to argv once redirection starts
      } else if (args[i] == "2>>") {
        err_append_redirect = true;
        if (i + 1 < args.size()) {
            out_file = args[i + 1]; // Get file following the operator
        }
        break; // Stop adding to argv once redirection starts
      }
      argv.push_back(const_cast<char*>(args[i].c_str()));
    }
    argv.push_back(nullptr);
  }

  void prepare_redirection(){
    if((should_out_redirect || append_redirect) && !out_file.empty()){
      saved_out = open(out_file.c_str(), O_WRONLY | O_CREAT | ((should_out_redirect) ? O_TRUNC : O_APPEND), 0644);
      if(saved_out == -1) {
        perror("open");
      }
      if(dup2(saved_out, STDOUT_FILENO) == -1){
        perror("dup2");
      }
      close(saved_out);
    }
    if((should_err_redirect || err_append_redirect) && !out_file.empty()){
      saved_err = open(out_file.c_str(), O_WRONLY | O_CREAT | ((should_err_redirect) ? O_TRUNC : O_APPEND), 0644);
      if(saved_err == -1) {
        perror("open");
      }
      if(dup2(saved_err, STDERR_FILENO) == -1){
        perror("dup2");
      }
      close(saved_err);
    }
  }

  void disable_redirection(){
    std::fflush(stdout);
    std::fflush(stderr);
    if (saved_out != -1) {
      dup2(saved_out, STDOUT_FILENO);
      close(saved_out);
    }
    if (saved_err != -1) {
      dup2(saved_err, STDERR_FILENO);
      close(saved_err);
    }
  }
};

std::vector<std::string> builtins = {"pwd","exit","type","echo","cd","history"};
std::vector<std::string> custom_executable = {};
std::vector<fs::path> directories;
std::vector<Command> history;
fs::path home_env;

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

std::vector<std::string> getCommandArgs(const std::string &command){
  std::vector<std::string> tokens;
  std::string token;

  char quoteChar = '\0';  // '\0' means not in quotes, '"' or '\'' means in that type of quote
  bool escaped = false;

  for(size_t i = 0; i < command.size() ; i++){
    char c = command[i];
    if(escaped){
      token += c;
      escaped = false;
      continue;
    }
    
    // Backslash escaping works:
    // - Outside quotes: escapes any character
    // - Inside double quotes: escapes any character
    // - Inside single quotes: backslash has no special meaning
    if(c == '\\'){
      if(quoteChar == '\''){
        token += c;
      }else if(quoteChar == '\"') {
        char next_char = (i+1 == command.size()) ? '\0' : command[i+1];
        if(next_char == '\"' || next_char == '$' || next_char == '\\' || next_char == '\n' || next_char == '`') {
          escaped = true;
        }else {
          token += c;
        }
      }else escaped = true;
      continue;
    }
    
    // If in quotes, only the matching quote char can end the quotes
    if(quoteChar != '\0'){
      if(c == quoteChar){
        quoteChar = '\0';  // End quotes
      } else {
        token += c;
      }
    } else {
      // Not in quotes
      if(c == '\'' || c == '\"'){
        quoteChar = c;  // Start quotes
      } else if(std::isspace(c)){
        if(!token.empty()) {
          tokens.push_back(token);
          token.erase();
        }
      } else {
        token += c;
      }
    }
  }

  if(!token.empty()) {
    tokens.push_back(token);
  }
  return tokens;
}

std::vector<Command> parse_input(const std::vector<std::string> &tokens){
  std::vector<Command> pipeline;
  Command current_cmd;

  for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == "|") {
            pipeline.push_back(current_cmd);
            current_cmd = Command(); // Reset for next command
        } else {
            current_cmd.args.push_back(tokens[i]);
        }
    }
    if (!current_cmd.args.empty()) pipeline.push_back(current_cmd);
    return pipeline;
}

bool external_command_run(const std::string &program, std::vector<char*> &argv){

  for(auto &dir: directories){
    fs::path full_path = dir / program; 
    if(!isExecutable(full_path.string())) continue;;
  
    #ifdef _WIN32
      std::string cmd = "\"" + full_path.string() + "\"";\
      for(const auto &a: argv) {cmd += "\""; cmd += a; cmd += "\"";}
      STARTUPINFOA si{};
      PROCESS_INFORMATION pi{};
      si.cb = sizeof(si);
  
      if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) continue;
  
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
  
      return true;
    #else
      execvp(full_path.c_str(), argv.data());
      exit(1);
      return true;
    #endif 

  }
  return false;
}

bool execute_command(const std::string &program, std::vector<char*> &argv) {
  if (program == "exit") return false;
  if(program == "pwd")
  {
    fs::path cwd = fs::current_path();
    std::cout << cwd.string() << '\n';
  } 
  else if(program == "echo") 
  {
    for(size_t i = 1; i < argv.size()-1; ++i) {
      std::cout << argv[i] ;
      std::cout << ((i == argv.size()-2) ? '\n' : ' ');
    } 
  } 
  else if(program == "type") 
  {
    for(size_t i = 1; i < argv.size()-1; ++i){
      std::string arg = argv[i];
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
    if(argv.size()>2){
      std::string new_dir = argv[1];
      if(new_dir == "~") new_dir = home_env.string();
      try {
        fs::current_path(new_dir);
      } catch (const fs::filesystem_error& e){
        std::cerr << "cd: " << new_dir << ": " << e.code().message() << '\n';
      }
    }
  }else if (program == "history"){
    int start = 0;
    if(argv.size() > 2) start = history.size() - std::stoi(argv[1]);
    for(size_t i = start; i < history.size(); ++i) {
      std::cout << '\t' << i + 1 << ' ';
      for(auto &arg: history[i].args) std::cout << arg << ' ';
      std::cout << '\n';
    }
  } else 
  {
    //handle not builtin command
    if(!external_command_run(program,argv)) std:: cout << program << ": not found\n";
  }
  return true;
}


char getChar() {
  #ifdef _WIN32
    return _getch();
  #else
    char buf = 0;
    struct termios old  = {0};
    if(tcgetattr(0, &old) < 0) perror("tcgetattr");
    old.c_lflag &= ~ICANON; // Disable line buffering
    old.c_lflag &= ~ECHO;   // Disable local echo
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0) perror("tcsetattr ICANON");
    if (read(0, &buf, 1) < 0) perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0) perror("tcsetattr ~ICANON");
    return buf;
  #endif
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  
  const char* raw_env = std::getenv("PATH");
  char* raw_home_env = std::getenv("HOME");
  
  std::string path_env(raw_env);
  home_env = raw_home_env;

  std::stringstream ss(path_env);
  std::string item;
  while(std::getline(ss,item,PATH_SEP)){
    if(!item.empty()){
      directories.push_back(fs::path(item));
    }
  }
  
  for(fs::path &dir: directories) {
    try {
      for(const auto &entry: fs::directory_iterator(dir)){
        if(isExecutable(entry.path().string())) {
          custom_executable.push_back(entry.path().filename().string());
        }
      }
    } catch (const fs::filesystem_error &e){
      std::cerr << "Error: " << e.what() << std::endl;
    }
  }
  // Make builtin Trie for auto complete
  Trie::TrieNode* root = new Trie::TrieNode();
  for(auto &cmd: builtins) {
    Trie::insert(root,cmd);
  } 
  for(auto &custom_exe: custom_executable) {
    Trie::insert(root,custom_exe);
  }
  
  while(true){
    int counter = history.size();
    std::cout << "$ ";
    std::string input;

    char prev_char = '\0';
    while(true) {
      char c = getChar();
      if(c == '\t') {
        auto [prefix,words] = Trie::autoComplete(root,input);
        if(words.empty()) std::cout << '\a' << std::flush;
        else if(prev_char == '\t') {
          prev_char = '\0';
          std::cout << '\n';
          for(auto &s: words) {
            std::cout << input << s << "  ";
          }
          std::cout << '\n';
          std::cout << "$ " << input << std::flush;
        } else {
          if(!prefix.empty()) {
            input += prefix;
            std::string add =  ((words.size() == 1) ? " ":"");
            input += add;
            std::cout << prefix << add << std::flush;
          } else {
            std::cout << '\a' << std::flush;
            prev_char = '\t';
          }
        } 
      } else if( c == ESC_SEQ ){
        char next1 = getChar(), next2 = getChar();
        if(next1 == '[' && next2 == UP_CODE) {
          if(counter > 0) {
            counter--;
            input.clear();
            std::cout << "\r$ \033[K" << std::flush;
            for(size_t i = 0; i < history[counter].args.size(); ++i) {
              input += history[counter].args[i] ;
              input += ((i != history[counter].args.size()-1) ? " " : "");
            } 
            std::cout << input << std::flush;
          }
        } else if(next1 == '[' && next2 == DOWN_CODE) {
          if(counter < history.size() - 1) {
            counter++;
            input.clear();
            std::cout << "\r$ \033[K" << std::flush;
            for(size_t i = 0; i < history[counter].args.size(); ++i) {
              input += history[counter].args[i] ;
              input += ((i != history[counter].args.size()-1) ? " " : "");
            } 
            std::cout << input << std::flush;
          }
        }
      }
      else {
        std::cout << c << std::flush;
        if (c == '\r' || c == '\n')break;
        else input += c;
        prev_char = c;
      }
    }

    std::stringstream ss(input);

    std::vector<std::string> tokens = getCommandArgs(input);
    std::vector<Command> pipeline = parse_input(tokens);
    
    int num_cmds = pipeline.size();
    int pipe_fds[2];
    int prev_pipe_read = -1;
    
    for(size_t i = 0; i < num_cmds ; ++i) {
      pipeline[i].get_argv();
      history.push_back(pipeline[i]);
      // If this is a single built-in command (no piping), run it in the parent
      // so it can modify the shell state (e.g. `cd` changes parent's cwd).
      if (num_cmds == 1 && !pipeline[i].args.empty() && (pipeline[i].args[0] == "cd" || pipeline[i].args[0] == "exit")) {
        bool cont = execute_command(pipeline[i].args[0], pipeline[i].argv);
        if (!cont) return 0;
        break; // skip forking for this command; continue REPL
      }
      
      if(i < (num_cmds - 1)) pipe(pipe_fds);
      pid_t pid = fork();
      if(pid == 0) {
        if(prev_pipe_read != -1) {
          dup2(prev_pipe_read,STDIN_FILENO);
          close(prev_pipe_read);
        }

        if(!pipeline[i].out_file.empty()) {
          pipeline[i].prepare_redirection();
          if(i < num_cmds - 1) close(pipe_fds[1]);
        } else if (i < num_cmds - 1) {
          dup2(pipe_fds[1], STDOUT_FILENO);
        }

        if(i< num_cmds - 1) {
          close(pipe_fds[0]);
          close(pipe_fds[1]);
        }

        if(!execute_command(pipeline[i].argv[0], pipeline[i].argv)) {exit(1);return 0;}
        exit(0);
      } 
      if (prev_pipe_read != -1) close(prev_pipe_read);
    
      if (i < num_cmds - 1) {
          close(pipe_fds[1]);       // CRITICAL: Close write end so reader gets EOF
          prev_pipe_read = pipe_fds[0]; // Save read end for next child
      }
    }
    while(wait(NULL)>0); //parents wait for all children
  }
}
