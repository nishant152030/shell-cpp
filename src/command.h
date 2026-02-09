#pragma once
#include <iostream>
#include <vector>
#include <fcntl.h>
#include <unistd.h>

#ifdef _WIN32
  #include <windows.h>
  #include <io.h>
  #include <sys/stat.h>
  #include <conio.h>
#else 
  #include <sys/types.h>
  #include <sys/wait.h>
  #include <termios.h>
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
  
  Command() = default;

    // 2. Custom Copy Constructor
    Command(const Command& other) {
        // Copy all the standard data
        this->args = other.args;
        this->out_file = other.out_file;
        this->saved_out = other.saved_out;
        this->saved_err = other.saved_err;
        this->should_out_redirect = other.should_out_redirect;
        this->should_err_redirect = other.should_err_redirect;
        this->append_redirect = other.append_redirect;
        this->err_append_redirect = other.err_append_redirect;

        // REBUILD the pointers to point to OUR args, not 'other's args
        this->get_argv(); 
    }

    // 3. Custom Copy Assignment Operator (for cmd1 = cmd2)
    Command& operator=(const Command& other) {
        if (this != &other) {
            this->args = other.args;
            this->out_file = other.out_file;
            this->saved_out = other.saved_out;
            this->saved_err = other.saved_err;
            this->should_out_redirect = other.should_out_redirect;
            this->should_err_redirect = other.should_err_redirect;
            this->append_redirect = other.append_redirect;
            this->err_append_redirect = other.err_append_redirect;

            this->get_argv(); // Rebuild pointers
        }
        return *this;
    }

  void get_argv() {
    argv.clear();
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