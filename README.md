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

# Compilation
This project uses premake
