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
#include <fcntl.h> 

/*
git add .
git commit --allow-empty -m "[any message]"
git push origin master
*/
struct Command{
    std::string executable;
    std::vector<std::string> args;

    std::string stdin_file;
    std::string stdout_file;
    std::string stderr_file;

    bool append_stdout = false;
    bool append_stderr = false;
};

using CommandHandler = std::function<void(const Command&)>;

std::unordered_map<std::string , CommandHandler> command_registry;



Command parse_command(std::vector<std::string>& tokens) {
    Command cmd;

    if (tokens.empty()) return cmd;

    cmd.executable = tokens[0];
    cmd.args.push_back(tokens[0]);

    for (size_t i = 1; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];

        if (token == "<" && i + 1 < tokens.size()) {
            cmd.stdin_file = tokens[++i];
        } else if ((token == ">" || token == "1>") && i + 1 < tokens.size()) {
            cmd.stdout_file = tokens[++i];
            cmd.append_stdout = false;
        } else if ((token == ">>" || token == "1>>") && i + 1 < tokens.size()) {
            cmd.stdout_file = tokens[++i];
            cmd.append_stdout = true;
        } else if (token == "2>" && i + 1 < tokens.size()) {
            cmd.stderr_file = tokens[++i];
            cmd.append_stderr = false;
        } else if (token == "2>>" && i + 1 < tokens.size()) {
            cmd.stderr_file = tokens[++i];
            cmd.append_stderr = true;
        } else {
            cmd.args.push_back(token);
        }
    }

    return cmd;
}


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
void handle_pwd(const Command& cmd){
 char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::cout << cwd << std::endl;
    } else {
        perror("getcwd() error");
    }
}
void execute_external_command(const Command& cmd) {
    std::vector<char*> args;
    for (const auto& arg : cmd.args) {
        args.push_back(const_cast<char*>(arg.c_str()));
    }
    args.push_back(nullptr);

    pid_t pid = fork();
    if (pid == 0) {
        // Redirections
        if (!cmd.stdin_file.empty()) {
            int fd = open(cmd.stdin_file.c_str(), O_RDONLY);
            if (fd < 0) { perror("open stdin"); exit(1); }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (!cmd.stdout_file.empty()) {
            int flags = O_WRONLY | O_CREAT | (cmd.append_stdout ? O_APPEND : O_TRUNC);
            int fd = open(cmd.stdout_file.c_str(), flags, 0644);
            if (fd < 0) { perror("open stdout"); exit(1); }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        if (!cmd.stderr_file.empty()) {
            int flags = O_WRONLY | O_CREAT | (cmd.append_stderr ? O_APPEND : O_TRUNC);
            int fd = open(cmd.stderr_file.c_str(), flags, 0644);
            if (fd < 0) { perror("open stderr"); exit(1); }
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        execvp(cmd.executable.c_str(), args.data());
        perror("execvp failed");
        exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else {
        std::cerr << "fork failed\n";
    }
}

 void handle_cd(const Command& cmd){
    if (cmd.args.size() != 2) {
        std::cerr << "Usage: cd <directory>\n";
        return;
    }
    
    const std::string& path = cmd.args[1];
    if (path == "~") {
        const char* home = getenv("HOME");
        if (home && chdir(home) != 0) {
            std::cerr << "cd: " << home << ": " << strerror(errno) << "\n";
        }
        return;
    }
    
    if (chdir(path.c_str()) != 0) {
        std::cerr << "cd: " << path << ": " << strerror(errno) << "\n";
    }
}

void handle_echo(const Command& cmd) {
  for (size_t i = 1; i < cmd.args.size(); ++i) {
    std::cout << cmd.args[i];
    if (i + 1 < cmd.args.size()) std::cout << " ";
  }
  std::cout << "\n";
}

void handle_type(const Command& K){
 if (K.args.size() < 2) {
        std::cerr << "Usage: type <command>\n";
        return;
    }
  const std::string& cmd = K.args[1];
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
void handle_exit(const Command& cmd) {
    (void)cmd;  // suppress unused-parameter warning
    exit(0);
}

void execute_builtin_with_redirection(const Command& cmd, const CommandHandler& handler) {
    // Save original file descriptors
    int saved_stdin  = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stderr = dup(STDERR_FILENO);

    // Redirect stdin if requested
    if (!cmd.stdin_file.empty()) {
        int fd = open(cmd.stdin_file.c_str(), O_RDONLY);
        if (fd < 0) {
            std::cerr << "open " << cmd.stdin_file << ": " << strerror(errno) << "\n";
        } else {
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
    }

    // Redirect stdout if requested
    if (!cmd.stdout_file.empty()) {
        int flags = O_WRONLY | O_CREAT | (cmd.append_stdout ? O_APPEND : O_TRUNC);
        int fd = open(cmd.stdout_file.c_str(), flags, 0644);
        if (fd < 0) {
            std::cerr << "open " << cmd.stdout_file << ": " << strerror(errno) << "\n";
        } else {
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
    }

    // Redirect stderr if requested
    if (!cmd.stderr_file.empty()) {
        int flags = O_WRONLY | O_CREAT | (cmd.append_stderr ? O_APPEND : O_TRUNC);
        int fd = open(cmd.stderr_file.c_str(), flags, 0644);
        if (fd < 0) {
            std::cerr << "open " << cmd.stderr_file << ": " << strerror(errno) << "\n";
        } else {
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
    }

    // Execute the builtin command
    handler(cmd);

    // Restore original file descriptors
    dup2(saved_stdin,  STDIN_FILENO);  close(saved_stdin);
    dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout);
    dup2(saved_stderr, STDERR_FILENO); close(saved_stderr);
}

std::vector<std::string> tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string token;
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaping = false;
 
    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];

        // Handle escaping
        if (escaping) {
            token += c;
            escaping = false;
            continue;
        }

        // Handle backslash escaping
        if (c == '\\') {
            if (in_double_quote) {
                // Inside double quotes: escape only certain characters
                if (i + 1 < input.size()) {
                    char next = input[i + 1];
                    if (next == '\\' || next == '"' || next == '$' || next == '`' || next == '\n') {
                        escaping = true;
                        continue;
                    } else {
                        token += '\\'; // Literal backslash
                    }
                } else {
                    token += '\\';
                }
            } else if (in_single_quote) {
                token += '\\'; // Inside single quotes: backslash is literal
            } else {
                escaping = true; // Outside quotes: escape next char
            }
            continue;
        }

        // Handle quotes
        if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            continue;
        } else if (c == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            continue;
        }

        // Don't process special characters inside quotes
        if (in_single_quote || in_double_quote) {
            token += c;
            continue;
        }

        // Handle redirection operators (outside of quotes)
        if (c == '<') {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            tokens.push_back("<");
            continue;
        }

        if (c == '>') {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            // Check for >>
            if (i + 1 < input.size() && input[i + 1] == '>') {
                tokens.push_back(">>");
                i++; // Skip next '>'
            } else {
                tokens.push_back(">");
            }
            continue;
        }

        // Handle numbered redirections (1>, 2>, 1>>, 2>>)
        if ((c == '1' || c == '2') && i + 1 < input.size() && input[i + 1] == '>') {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            
            // Check for append (1>>, 2>>)
            if (i + 2 < input.size() && input[i + 2] == '>') {
                tokens.push_back(std::string(1, c) + ">>");
                i += 2; // Skip '>>'
            } else {
                tokens.push_back(std::string(1, c) + ">");
                i++; // Skip '>'
            }
            continue;
        }

        // Handle whitespace (split tokens)
        if (std::isspace(c)) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += c;
        }
    }

    // Add final token if exists
    if (!token.empty()) {
        tokens.push_back(token);
    }

    // Check for unmatched quotes
    if (in_single_quote || in_double_quote) {
        std::cerr << "Error: unmatched quote\n";
        return {}; // Return empty vector on error
    }
    
    return tokens;
}

void Execute_Command(const std::string& input) {
    auto tokens = tokenize(input);
    if (tokens.empty()) return;

Command cmd = parse_command(tokens);
if (cmd.executable.empty()) return;

auto it  = command_registry.find(cmd.executable);

if (it != command_registry.end()) {
    //it->second(cmd);
    execute_builtin_with_redirection(cmd,it->second);
} else {
    execute_external_command(cmd);
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
  command_registry["exit0"] = handle_exit;

 
  while(1){
  std::cout << "$ ";
  std::string input;
  std::getline(std::cin, input);
   Execute_Command(input);
   }
 
}
