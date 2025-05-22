#include <iostream>
#include <string>
void Invalid(std::string input){
  std::cout << input << ": command not found" << std::endl;
}
int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // Uncomment this block to pass the first stage
  
  while(input!="exit"){
  std::cout << "$ ";
  std::string input;
  std::getline(std::cin, input);
  Invalid(input);
  input.clear();
  }
 
}
