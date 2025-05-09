#include <iostream>
#include <cstring>

#include "jsonparser.h"

int main(int argc, char** argv)
{
  const char* str = "{\"Name\" : \"John\" , \"Age\":27}"; 
  
  JsonParser parser;
  JsonValue json = parser.parse(str, strlen(str));
  if(parser.hasError())
  {
    std::cout << parser.getErrorText();
    return 0;
  }
  std::string name = json["Name"];
  std::string bio = json["Bio"];
  double age = json["Age"];
  
  std::cout << "Name = " << name << '\n';
  std::cout << "Bio = " << bio << '\n';
  std::cout << "Age = " << age << '\n';
  return 0;
}
