#pragma once

#include <stdint.h>
#include <variant>
#include <string>
#include <vector>
#include <unordered_map>

enum class JsonValueType
{
  Null, 
  Number,
  Boolean,
  String,
  Array,
  Object
};

struct JsonValue;


class JsonValue
{
public:


  JsonValue(); 

  JsonValue& operator[](const std::string& fieldName);
  JsonValue& operator[](const char* fieldName);
  JsonValue& operator[](int index);

  operator double() const;
  operator bool() const;
  operator const std::string&() const;

  bool contains(const char* fieldName);
  bool contains(const std::string& fieldName);

  uint32_t size();

  ~JsonValue();
private:

  typedef std::unordered_map<std::string, JsonValue*> JsonObject;
  typedef std::vector<JsonValue*> JsonArray;
  
  JsonValueType m_type;
  std::variant<double, bool, std::string, JsonArray, JsonObject> m_value;

  friend class JsonParser;
};

enum class JsonParserError
{
  ArrayError,
  ObjectError,
  ObjectColonError,
  StringError,
  SymbolError,
};

class JsonParser
{
public:
  JsonValue* parse(const char* text, uint32_t textLength);
  bool hasError() { return m_error; };
  std::string getErrorText();

private:
  const char* m_text;
  uint32_t m_textLength;

  uint32_t m_position;
  uint32_t m_line;

  bool m_error;
  uint32_t m_errorLine;
  uint32_t m_errorPosition;
  JsonParserError m_errorType;

  double parseNumber();
  std::string parseString();
  JsonValue* parseObject();
  JsonValue* parseArray();
  JsonValue* parseValue(); 

  void skipWhitespaces();

  char peek();
  bool isDigit(char c);
  bool consumeString(const char* str, uint32_t length);
};

