#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <functional>
#include <unordered_map>

/*
git add .
git commit --allow-empty -m "[any message]"
git push origin master
*/

using CommandHandler = std::function<void(const std::vector<std::string>&)>;
std::unordered_map<std::string , CommandHandler> command_registry;

void handle_echo(const std::vector<std::string>& tokens) {
    for (size_t i = 1; i < tokens.size(); ++i) {
        std::cout << tokens[i];
        if (i != tokens.size() - 1) std::cout << " ";
    }
    std::cout << std::endl;
}
void handle_type(const std::vector<std::string>& tokens){
 if (tokens.size() < 2) {
        std::cerr << "Usage: type <command>\n";
        return;
    }

    const std::string& cmd = tokens[1];
    auto it = command_registry.find(cmd);

    if (it != command_registry.end()) {
        std::cout << cmd << " is a shell builtin\n";
    } else {
        std::cout << cmd << ": not found\n";
    }
  
}
void handle_exit(const std::vector<std::string>& tokens){
  exit(0);
}
std::vector<std::string> tokenize(std::string input){
   std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) tokens.push_back(token);
    return tokens;
}
void Execute_Command(const std::string& input){

    auto tokens = tokenize(input);
    if (tokens.empty()) return;

    auto it = command_registry.find(tokens[0]);
    if (it != command_registry.end()) {
        it->second(tokens); // Call the handler
    } else {
        std::cout << tokens[0] << ": command not found\n";
    }
}



int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // Uncomment this block to pass the first stage
  command_registry["echo"] = handle_echo;
  command_registry["type"] = handle_type;
  command_registry["exit"] = handle_exit;
  while(1){
  std::cout << "$ ";
  std::string input;
  std::getline(std::cin, input);
   Execute_Command(input);
   }
 
}
