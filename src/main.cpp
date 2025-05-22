#include <iostream>
#include <string>
#include <sstream>
/*
git add .
git commit --allow-empty -m "[any message]"
git push origin master
*/

void handle_echo(const std::vector<std::string>& tokens) {
    for (size_t i = 1; i < tokens.size(); ++i) {
        std::cout << tokens[i];
        if (i != tokens.size() - 1) std::cout << " ";
    }
    std::cout << std::endl;
}
std::vector<std::string> tokenize(std::string input){
   std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) tokens.push_back(token);
    return tokens;
}

bool handle_command(const std::string& input) {
    auto tokens = tokenize(input);

    if (tokens.empty()) return true;

    const std::string& cmd = tokens[0];

    if (cmd == "exit" || input == "exit 0") {
        return false;
    } else if (cmd == "echo") {
        handle_echo(tokens);
    } else {
        std::cout << input << ": command not found" << std::endl;
    }

    return true;
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // Uncomment this block to pass the first stage
  
  while(1){
  std::cout << "$ ";
  std::string input;
  std::getline(std::cin, input);
 
  if(!handle_command(input)) break;
   }
 
}
