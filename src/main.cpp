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
if((auto it = command_registry.find(tokens[1]))!= command_registry.end()){
std::cout<<it<<"is a shell "<<"builtin"<<std::endl;
}else{
  std::cout<<tokens[1]<<"invalid_command: not found"<<std::endl;
}
  
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
