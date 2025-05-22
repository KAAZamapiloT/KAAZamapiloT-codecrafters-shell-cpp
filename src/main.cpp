#include <iostream>
#include <string>
#include <sstream>
/*
git add .
git commit --allow-empty -m "[any message]"
git push origin master
*/

std::vector<std::string> tokenize(std::string input){
   std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) tokens.push_back(token);
    return tokens;
}

void Invalid(std::string input){

  auto tokens = tokenize(input);
   if(!tokens.empty()&&tokens[0]=="echo"){
    for(int i=1;i<tokens.size();++i){
      std::cout<<tokens[i]<<" ";
    }
    return;
   }
  if(input=="exit 0"||input =="exit") exit(0);
  
  std::cout << input << ": command not found" << std::endl;
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
 
  Invalid(input);
   }
 
}
