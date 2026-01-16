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
    std::string input;
    std::getline(std::cin, input);
    std::stringstream ss(input);

    std::string command;
    ss>>command;
    
    if (command == "exit") break;
    else if(command == "echo") {
      // std:: string args = ss.str();
      // std::cout << args << "\n";
      while(ss){
        std::string args;
        ss >> args;
        std::cout << args << " ";
      }
      std::cout << "\n";
    }else {
      std:: cout << command <<": command not found\n";
    }

  }
}
