#pragma once 
#include "command.h"
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>

namespace HISTORY {
    void read_history(const std::string &file_loc, std::vector<Command> &history) {
      std::ifstream fd(file_loc);
      if(fd.is_open()) {
        std::string cmds;
        while (std::getline(fd, cmds)) {
          if(cmds.empty()) continue;  // Skip empty lines
          Command cmd;
          std::stringstream ss(cmds);
          std::string arg;
          while(ss >> arg) {
            cmd.args.push_back(arg);
          }
          history.push_back(cmd);
          history.back().get_argv();
        }
        fd.close();
      } else {
        std::cerr << "history: cannot open file '" << file_loc << "'" << std::endl;
      }
    }

    void write_history(const std::string &file_loc, std::vector<Command> &history) {
      std::ofstream fd(file_loc);
      if(fd.is_open()) {
        for(auto &cmd: history){
          for(size_t i = 0; i < cmd.args.size() ; ++i) fd << cmd.args[i] << ((i != cmd.args.size()-1)?" ":"\n");
        }
        fd.close();
      } else {
        std::cerr << "history: cannot open file '" << file_loc << "'" << std::endl;
      }
    }

    void append_history(const std::string &file_loc, int append_pointer, std::vector<Command> &history){
      std::ofstream fd(file_loc, std::ios::app);
      if(fd.is_open()) {
        for(append_pointer; append_pointer < history.size(); ++append_pointer){
          for(size_t i = 0; i < history[append_pointer].args.size() ; ++i) fd << history[append_pointer].args[i] << ((i != history[append_pointer].args.size()-1)?" ":"\n");
        }
        // std::cout<<append_pointer << std::endl;
        fd.close();
      } else {
        std::cerr << "history: cannot open file '" << file_loc << "'" << std::endl;
      }
    }
}