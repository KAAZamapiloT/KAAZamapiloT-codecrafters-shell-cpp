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
#include <readline/readline.h>
#include <readline/history.h>
#include <termios.h>
#include<memory>
#include <dirent.h>
/*
git add .
git commit --allow-empty -m "[any message]"
git push origin master
*/

struct termios orig_termios;

void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode); // auto-disable when program exits

  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON); // turn off echo and canonical mode
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
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
struct Command{
    std::string executable;
    std::vector<std::string> args;

    std::string stdin_file;
    std::string stdout_file;
    std::string stderr_file;

    bool append_stdout = false;
    bool append_stderr = false;

    Command* next = nullptr;
};

using CommandHandler = std::function<void(const Command&)>;

std::unordered_map<std::string , CommandHandler> command_registry;

class ShellHistory {
private:
    std::vector<std::string> history;
    size_t max_size;

public:
    ShellHistory(size_t max_history = 10000) : max_size(max_history) {}

    void add_command(const std::string& command) {
        // Skip empty commands and history command itself
        if (command.empty() ) {
            return;
        }
        
        // Avoid duplicate consecutive commands
        if (!history.empty() && history.back() == command) {
            return;
        }
        
        history.push_back(command);
        
        // Keep history size within limit
        if (history.size() > max_size) {
            history.erase(history.begin());
        }
    }

    void print_history() const {
        if (history.empty()) {
            return;  // Don't print anything if history is empty
        }
        
        for (size_t i = start; i < history.size(); ++i) {
            std::cout << "    " << (i + 1) << "  " << history[i] << std::endl;
        }
    }
   void print_int(int n = -1) const {
        if (history.empty()) {
            return;  
        }
        
        size_t start = 0;
        if (n > 0 && static_cast<size_t>(n) < history.size()) {
            start = history.size() - n;
        }
        
        for (size_t i = start; i < history.size(); ++i) {
            std::cout << "    " << (i + 1) << "  " << history[i] << std::endl;
        }
    }

    void clear_history() {
        history.clear();
    }

    std::string get_last_command() const {
        return history.empty() ? "" : history.back();
    }

    std::string get_command(size_t index) const {
        if (index > 0 && index <= history.size()) {
            return history[index - 1];
        }
        return "";
    }

    size_t size() const {
        return history.size();
    }

    bool empty() const {
        return history.empty();
    }

    // Search for commands containing a substring
    void search_history(const std::string& pattern) const {
        bool found = false;
        for (size_t i = 0; i < history.size(); ++i) {
            if (history[i].find(pattern) != std::string::npos) {
                std::cout << (i + 1) << ": " << history[i] << std::endl;
                found = true;
            }
        }
        if (!found) {
            std::cout << "No commands found containing: " << pattern << std::endl;
        }
    }
};
class CommandTrie {
private:
    struct TrieNode {
        std::unordered_map<char, std::unique_ptr<TrieNode>> children;
        bool is_complete_command = false;
        std::string full_command; // Store complete command for easy retrieval
    };
    
    std::unique_ptr<TrieNode> root;

public:
    CommandTrie() : root(std::make_unique<TrieNode>()) {}
    
    // Insert a command into the trie
    void insert_command(const std::string& command) {
        TrieNode* current = root.get();
        
        // Navigate through each character, creating nodes as needed
        for (char c : command) {
            if (current->children.find(c) == current->children.end()) {
                current->children[c] = std::make_unique<TrieNode>();
            }
            current = current->children[c].get();
        }
        
        // Mark this node as representing a complete command
        current->is_complete_command = true;
        current->full_command = command;
    }
    
    // Find all possible completions for a given prefix
    std::vector<std::string> find_completions(const std::string& prefix) {
        TrieNode* current = root.get();
        
        // First, navigate to the prefix location in the trie
        for (char c : prefix) {
            if (current->children.find(c) == current->children.end()) {
                return {}; // Prefix doesn't exist, no completions possible
            }
            current = current->children[c].get();
        }
        
        // Now collect all complete commands that extend this prefix
        std::vector<std::string> completions;
        collect_completions(current, completions);
        return completions;
    }
std::string get_longest_common_prefix(const std::string& prefix) {
    TrieNode* current = root.get();

    // Navigate to prefix node
    for (char c : prefix) {
        if (current->children.find(c) == current->children.end()) {
            return prefix; // No such prefix in trie
        }
        current = current->children[c].get();
    }

    std::string lcp = prefix;

    // Continue down trie until we hit:
    // - multiple branches (ambiguous)
    // - a complete command (end of unique path)
    while (current->children.size() == 1 && !current->is_complete_command) {
        auto it = current->children.begin(); // only one child
        lcp += it->first;
        current = it->second.get();
    }

    return lcp;
}
private:
    // Recursively collect all completions from a given node
    void collect_completions(TrieNode* node, std::vector<std::string>& completions) {
        if (node->is_complete_command) {
            completions.push_back(node->full_command);
        }
        
        for (auto& [char_key, child_node] : node->children) {
            collect_completions(child_node.get(), completions);
        }
    }
};


void scan_directory_for_executables(const std::string& dir_path, CommandTrie& trie) {
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) return; // skip if directory can't be opened

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;

        // Skip "." and ".."
        if (filename == "." || filename == "..")
            continue;

        std::string full_path = dir_path + "/" + filename;

        // Check if it's executable and a regular file
        struct stat sb;
        if (stat(full_path.c_str(), &sb) == 0 &&
            S_ISREG(sb.st_mode) &&
            access(full_path.c_str(), X_OK) == 0) {
            trie.insert_command(filename); // Insert just the command name
        }
    }

    closedir(dir);
}

void populate_command_trie(CommandTrie& trie) {
    // Add built-in commands
    for (const auto& [command_name, handler] : command_registry) {
        trie.insert_command(command_name);
    }
    
    // Add external commands from PATH
    const char* path_env = std::getenv("PATH");
    if (path_env) {
        std::vector<std::string> path_dirs = split_path(path_env);
        
        for (const std::string& dir : path_dirs) {
            // Scan each directory for executable files
            scan_directory_for_executables(dir, trie);
        }
    }
}
bool is_executable(const std::string& path) {
    // access with X_OK checks executable permission
    return access(path.c_str(), X_OK) == 0;
}
bool command_exists_in_path(const std::string& command) {
    // If the command contains a slash, it's a path, so check it directly
    if (command.find('/') != std::string::npos) {
        return is_executable(command);
    }
    
    // Otherwise, search through PATH directories
    const char* path_env = std::getenv("PATH");
    if (!path_env) {
        return false;
    }
    
    std::vector<std::string> path_dirs = split_path(path_env);
    for (const auto& dir : path_dirs) {
        std::string full_path = dir + "/" + command;
        if (is_executable(full_path)) {
            return true;
        }
    }
    
    return false;
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

void execute_external_command(const Command& cmd) {
    std::vector<char*> args;
    for (const auto& arg : cmd.args) {
        args.push_back(const_cast<char*>(arg.c_str()));
    }
    args.push_back(nullptr);
  if (!command_exists_in_path(cmd.executable)) {
        std::cerr << cmd.executable << ": command not found\n";
        return;
    }
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
        perror("Invalid Command");
        exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else {
        std::cerr << "fork failed\n";
    }
}

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


std::vector<Command> parse_pipeline(const std::vector<std::string>& tokens) {
    std::vector<Command> pipeline;
    std::vector<std::string> current_tokens;

    for (const auto& token : tokens) {
        if (token == "|") {
            if (!current_tokens.empty()) {
                Command cmd = parse_command(current_tokens);
                pipeline.push_back(cmd);
                current_tokens.clear();
            }
        } else {
            current_tokens.push_back(token);
        }
    }

    if (!current_tokens.empty()) {
        Command cmd = parse_command(current_tokens);
        pipeline.push_back(cmd);
    }

    return pipeline;
}


void execute_pipeline(const std::vector<Command>& pipeline) {
    int num_commands = pipeline.size();
    if (num_commands == 0) return;

    std::vector<int> pipefds(2 * (num_commands - 1));
    std::vector<pid_t> child_pids;

    // Create pipes
    for (int i = 0; i < num_commands - 1; ++i) {
        if (pipe(pipefds.data() + 2 * i) < 0) {
            perror("pipe");
            return;
        }
    }

    for (int i = 0; i < num_commands; ++i) {
        pid_t pid = fork();
        if (pid == 0) { // Child process
            // Connect input from previous pipe
            if (i > 0) {
                dup2(pipefds[2 * (i - 1)], STDIN_FILENO);
            }

            // Connect output to next pipe
            if (i < num_commands - 1) {
                dup2(pipefds[2 * i + 1], STDOUT_FILENO);
            }

            // Close all pipe file descriptors
            for (int fd : pipefds) close(fd);

            // Apply command-specific redirections (override pipes if needed)
            const Command& cmd = pipeline[i];
            if (!cmd.stdin_file.empty()) {
                int fd = open(cmd.stdin_file.c_str(), O_RDONLY);
                if (fd != -1) dup2(fd, STDIN_FILENO);
            }
            if (!cmd.stdout_file.empty()) {
                int flags = O_WRONLY | O_CREAT | (cmd.append_stdout ? O_APPEND : O_TRUNC);
                int fd = open(cmd.stdout_file.c_str(), flags, 0644);
                if (fd != -1) dup2(fd, STDOUT_FILENO);
            }

            // Execute built-in or external command
            auto it = command_registry.find(cmd.executable);
            if (it != command_registry.end()) {
                // Built-in: Run in child process and exit
                execute_builtin_with_redirection(cmd, it->second);
                exit(0); // Critical: Exit child after built-in completes
            } else {
                // External command
                execute_external_command(cmd);
            }
            exit(EXIT_FAILURE); // If execution fails
        } else if (pid < 0) {
            perror("fork");
            return;
        }
        child_pids.push_back(pid);
    }

    // Parent closes all pipes
    for (int fd : pipefds) close(fd);

    // Wait for all children to finish
    for (pid_t pid : child_pids) {
        waitpid(pid, nullptr, 0);
    }
}

void handle_pwd(const Command& cmd){
 char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::cout << cwd << std::endl;
    } else {
        perror("getcwd() error");
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

ShellHistory History(100);
void  handle_history(const Command& cmd){
    if(cmd.args.size()==0){
History.print_history();
    }else if(cmd.args.size()==2){
        History.print_int(stoi(cmd.args[1]));
    }

}
void Execute_Command(const std::string& input) {
    auto tokens = tokenize(input);
    if (tokens.empty()) return;

    // Check for pipeline operator
    if (std::find(tokens.begin(), tokens.end(), "|") != tokens.end()) {
        auto pipeline = parse_pipeline(tokens);
        execute_pipeline(pipeline);
    } else {
        Command cmd = parse_command(tokens);
        if (cmd.executable.empty()) return;

        auto it = command_registry.find(cmd.executable);
        if (it != command_registry.end()) {
            // Built-in command (non-pipeline)
            execute_builtin_with_redirection(cmd, it->second);
        } else {
            // External command (non-pipeline)
            execute_external_command(cmd);
        }
    }
}


char** command_completion(CommandTrie trie,const char* text, int start, int end) {
    // text contains what user has typed so far
    // start and end indicate position in the line
    
std::vector<std::string> matches = trie.find_completions(text);
    
    // Convert to format expected by readline
    char** completion_matches = nullptr;
    if (!matches.empty()) {
        completion_matches = (char**)malloc(sizeof(char*) * (matches.size() + 1));
        for (size_t i = 0; i < matches.size(); ++i) {
            completion_matches[i] = strdup(matches[i].c_str());
        }
        completion_matches[matches.size()] = nullptr;
    }
    
    return completion_matches;
}

std::string read_line_with_autocomplete(CommandTrie& trie) {
    std::string buffer;
    char c;

    std::cout << "$ " << std::flush;
    bool first_tab_pressed=false;
    while (true) {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) break;

        if (c == '\n') {
            std::cout << std::endl;
            break;
        } else if (c == '\t') {
            auto completions = trie.find_completions(buffer);

            if (!completions.empty()) {
        std::string lcp = trie.get_longest_common_prefix(buffer);
        if (lcp.size() > buffer.size()) {
            // Extend buffer by the missing lcp suffix
            std::string to_add = lcp.substr(buffer.size());
            std::cout << to_add << std::flush;
            buffer += to_add;

            // If that LCP *is* the entire single command, add a trailing space:
            if (completions.size() == 1) {
                std::cout << ' ' << std::flush;
                buffer += ' ';
            }

            // Reset ambiguous-Tab state and skip to next keypress
            first_tab_pressed = false;
            continue;
        }
    }
            if(completions.size()>1){
                       if(!first_tab_pressed){
                        first_tab_pressed=true;
                        std::cout<<'\a'<<std::flush;

                       }else{
                       std::sort(completions.begin(), completions.end());
        std::cout << "\n";
        for (const auto& suggestion : completions) {
            std::cout << suggestion << "  ";
        }
                        std::cout << "\n$ " << buffer << std::flush;

                        std::string lcp = trie.get_longest_common_prefix(buffer);
                        first_tab_pressed=false;
                       }

            }else 
            if (completions.size()==1) {
             const std::string& match = completions[0];
              // Compute what's missing
              std::string to_add = match.substr(buffer.size());
              // Print it plus a space
              std::cout << to_add << ' ' << std::flush;
              // Update the buffer to include it and the space
              buffer += to_add;
              buffer += ' ';
    // Reset the tab state
    first_tab_pressed = false;
            }else{
                  std::cout << "\x07" << std::flush;
                  first_tab_pressed=false;
            }
        } else if (c == 127) {
            if (!buffer.empty()) {
                buffer.pop_back();
                std::cout << "\b \b" << std::flush;
            }
        } else {
            buffer += c;
            std::cout << c << std::flush;
        }
    }

    return buffer;
}




int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  CommandTrie AutoComplete;
  
  // Uncomment this block to pass the first stage
  command_registry["echo"] = handle_echo;
  command_registry["type"] = handle_type;
  command_registry["exit"] = handle_exit;
  command_registry["pwd"] = handle_pwd;
  command_registry["cd"] = handle_cd;
  command_registry["history"]=handle_history;
  //command_registry["exit0"] = handle_exit;
  populate_command_trie(AutoComplete);
 
  while(1){
   enableRawMode();
   std::string input = read_line_with_autocomplete(AutoComplete);
   disableRawMode();
   History.add_command(input);
   Execute_Command(input);
   }
 
}
