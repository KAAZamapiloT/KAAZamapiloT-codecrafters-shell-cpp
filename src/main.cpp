#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <functional>
#include <unordered_map>
#include <cstdlib>   
#include <sys/stat.h>   
#include <unistd.h> 
#include <sys/types.h>
#include <sys/wait.h>
 #include <limits.h>
 #include <cstring>  // For strerror
#include <cerrno>   // For errno

/*
git add .
git commit --allow-empty -m "[any message]"
git push origin master
*/

using CommandHandler = std::function<void(const std::vector<std::string>&)>;
std::unordered_map<std::string , CommandHandler> command_registry;


std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> dirs;
    std::istringstream ss(path);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (!dir.empty()) dirs.push_back(dir);
    }
    return dirs;
}
bool is_executable(const std::string& path) {
    // access with X_OK checks executable permission
    return access(path.c_str(), X_OK) == 0;
}
void handle_pwd(const std::vector<std::string>& tokens){
 char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::cout << cwd << std::endl;
    } else {
        perror("getcwd() error");
    }
}
void exeute_external_command(const std::vector<std::string>& tokens){
 if (tokens.empty()) return;
 auto exe=tokens[0];
 std::vector<char*> args;
for (const std::string& token : tokens) {
        args.push_back(const_cast<char*>(token.c_str()));
    }
args.push_back(nullptr);

pid_t pid =fork();

       if(pid==0){
        execvp(args[0], args.data());
        std::cerr << args[0] << ": command not found\n";
        exit(1); // Only if execvp fails
}else if(pid>0){
 int status;
        waitpid(pid, &status, 0);
       
}else{
  std::cerr<<"fork failed\n";
}

 }

 void handle_cd(const std::vector<std::string>& tokens){
  if (tokens.size() != 2) {
        std::cerr << "Usage: cd <directory>\n";
        return;
    }
  const std::string path=tokens[1];
  if(tokens[1]=="~") {
    chdir(getenv("HOME"));
    return;
  }
  if(chdir(path.c_str())!= 0) std::cerr << "cd: " << path << ": " << strerror(errno) << "\n";

}

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
  const char* path_env = std::getenv("PATH");
    if (!path_env) {
        std::cout << cmd << ": not found\n";
        return;
    }
    
    auto it = command_registry.find(cmd);

    if (it != command_registry.end()) {
        std::cout << cmd << " is a shell builtin\n";
    } else {
        std::vector<std::string> loc=split_path(path_env);
        for (const auto& dir : loc) {
        std::string full_path = dir + "/" + cmd;
        if (is_executable(full_path)) {
            std::cout << cmd << " is " << full_path << "\n";
            return;
        }
        
    }
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
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaping = false;
 
    for(size_t i =0;i<input.size();++i){
        char c =input[i];

         if (escaping) {
            token += c;
            escaping = false;
            continue;
        }
         if (c == '\\') {
            // Inside double quotes: escape only certain characters
            if (in_double_quote) {
                if (i + 1 < input.size()) {
                    char next = input[i + 1];
                    if (next == '\\' || next == '"' || next == '$' || next == '`' || next == '\n') {
                        escaping = true;
                        continue;
                    } else {
                        // Literal backslash
                        token += '\\';
                    }
                } else {
                    token += '\\';
                }
            }
            // Inside single quotes: backslash is literal
            else if (in_single_quote) {
                token += '\\';
            }
            // Outside quotes: escape next char
            else {
                escaping = true;
            }
        }else
        if (c == '\\'&&!in_double_quote&&!in_single_quote) {
            escaping = true;
        } else if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;  // toggle single quote mode
        } else if (c == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;  // toggle double quote mode
        } else if (std::isspace(c) && !in_single_quote && !in_double_quote) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        }else{
            token += c;
        }
    }

     if (!token.empty()) {
        tokens.push_back(token);
    }

    if (in_single_quote || in_double_quote) {
        std::cerr << "Error: unmatched quote\n";
        
    }
    return tokens;
}
void Execute_Command(const std::string& input){

    auto tokens = tokenize(input);
    if (tokens.empty()) return;

    auto it = command_registry.find(tokens[0]);
    if (it != command_registry.end()) {
        it->second(tokens); // Call the handler
    } else {
      exeute_external_command(tokens);
       // std::cout << tokens[0] << ": command not found\n";
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
  command_registry["pwd"] = handle_pwd;
  command_registry["cd"] = handle_cd;
  while(1){
  std::cout << "$ ";
  std::string input;
  std::getline(std::cin, input);
   Execute_Command(input);
   }
 
}
