A small library for parsing JSON files I made for personal use

# Usage

This library is simplistic as posible 

Parsing file:

    JsonParser parser;
    JsonValue* val = parser.parse(text, strlen(text));

Error checking:
    
    if(parser.hasError())
    {
        std::cout << parser.getErrorText();
    }

Getting variables:
    
    JsonValue& json = *val;
    std::string desc = json["Description"];
    double height = json["Height"]; 

Checking if json object has specific field:

    bool hasName = json.contains("Name"));

Getting size of json array:
    
    uint32_t arraySize = json["Array"].size();
# > [!WARNING]
* Escape characters are not implemented 
# Compilation
This project uses premake
On Windows just run premake5.exe file and all needed files will be generated, you can also use `--examples` flag to generate example project
