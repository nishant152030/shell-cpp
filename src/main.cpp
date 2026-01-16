#include <iostream>
#include <string>
#include <sstream>
#include <bits/stdc++.h>
using namespace std;

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  while(true){
    std::cout << "$ ";
    std::string command;
    std::getline(std::cin, command);
  
    if (command == "exit") break;
    else if(command.substr(0,4) == "echo") {
      std::cout << command.substr(5) << '\n';
    }else if(command.substr(0,4) == "type") {
      std::string args = command.substr(5);
      if(args == "echo" || args == "exit" || args == "type"){
        std::cout << args << " is a shell builtin\n";
      }else{
        std:: cout << args <<": not found\n";
      } 
    }else {
      std:: cout << command <<": command not found\n";
    }

  }
}
