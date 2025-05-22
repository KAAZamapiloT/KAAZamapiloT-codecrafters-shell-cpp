#include <iostream>
#include <string>
void Invalid(std::string input){
  if(input=="exit 0") exit(0);
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
  if(input=="exit") exit(0);
  Invalid(input);
  input.clear();
  }
 
}
