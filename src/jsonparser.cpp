#include "jsonparser.h"

#include <cmath>
#include <sstream>
#include <stdio.h>
#include <math.h>

static JsonValue s_emptyObject = {};
static std::string s_emptyString = "null";

JsonValue::JsonValue()
{
  m_type == JsonValueType::Null;
}

JsonValue& JsonValue::operator[](const std::string& fieldName) 
{
  if(m_type == JsonValueType::Object)
  {
    auto& map = std::get<JsonObject>(m_value);
    if(map.find(fieldName) != map.end())
      return *map[fieldName];
    else 
      return s_emptyObject;  
  }

  return s_emptyObject;
}

JsonValue& JsonValue::operator[](const char* fieldName)
{
  if(m_type == JsonValueType::Object)
  {
    auto& map = std::get<JsonObject>(m_value);
    if(map.find(fieldName) != map.end())
      return *map[fieldName];
    else 
      return s_emptyObject;  
  }
  return s_emptyObject;
}

JsonValue& JsonValue::operator[](int index) 
{
  if(m_type == JsonValueType::Array)
  {
    return *std::get<JsonArray>(m_value)[index];
  }
  return s_emptyObject;
}

JsonValue::operator int() const
{
  if(m_type == JsonValueType::Number)
  {
    return (int) std::get<double>(m_value);
  }
  return 0;
}

JsonValue::operator double() const
{
  if(m_type == JsonValueType::Number)
  {
    return std::get<double>(m_value);
  }
  return 0;
}

JsonValue::operator bool() const
{
  if(m_type == JsonValueType::Boolean)
  {
    return std::get<bool>(m_value);
  }
  return false;
}

JsonValue::operator const std::string&() const
{
  if(m_type == JsonValueType::String)
  {
    return std::get<std::string>(m_value);
  }
  return s_emptyString;
}

JsonValue::operator double() const
{
  if(m_type == JsonValueType::Number)
  {
    return std::get<double>(m_value);
  }
  return 0;
}

bool JsonValue::contains(const char* fieldName)
{
  if(m_type == JsonValueType::Object)
  {

    auto& map = std::get<JsonObject>(m_value);
    return map.find(fieldName) != map.end();
  }
  return false;
}

bool JsonValue::contains(const std::string& fieldName)
{
  if(m_type == JsonValueType::Object)
  {

    auto& map = std::get<JsonObject>(m_value);
    return map.find(fieldName) != map.end();
  }
  return false;
}

uint32_t JsonValue::size()
{
  if(m_type == JsonValueType::Array)
  {
    return std::get<JsonArray>(m_value).size();
  }
  return 0;
}

JsonValue::~JsonValue()
{
  if(m_type == JsonValueType::Object)
  {
    for(auto kv : std::get<JsonObject>(m_value))
    {
      delete kv.second;
    }
  }
  if(m_type == JsonValueType::Array)
  {
    for(auto v : std::get<JsonArray>(m_value))
    {
      delete v;
    }
  }
}

const char* boolTrueStr = "true";
const uint32_t boolTrueStrLen = 4;

const char* boolFalseStr = "false";
const uint32_t boolFalseStrLen = 5;

const char* nullStr = "null";
const uint32_t nullStrLen = 4;

JsonValue* JsonParser::parse(const char* text, uint32_t textLength)
{
  m_text = text;
  m_textLength = textLength;

  m_position = 0;
  m_line = 0;
  
  m_error = false;

  skipWhitespaces();
  if(m_position >= textLength)
  {
    return nullptr;
  }
  return parseValue();
}

std::string JsonParser::getErrorText()
{
  std::ostringstream text;
  switch(m_errorType)
  {
    case JsonParserError::ArrayError:
      text << "']' is expected ";
    break;
    
    case JsonParserError::ObjectError:
      text << "'}' is expected ";
    break;
    
    case JsonParserError::ObjectColonError:
      text << "':' is expected after object's member name ";
    break;
    
    case JsonParserError::StringError:
      text << "No closing \" for string value ";
    break;

    case JsonParserError::SymbolError:
      text << "Unkown symbolic literal ";
    break;
  }
  text << "at line " << m_errorLine + 1 << ':' << m_errorPosition;

 return text.str(); 
}

double JsonParser::parseNumber()
{
  bool negative = false;
  if(peek() == '-')
  {
    negative = true;
    m_position += 1;
  }

  double result = 0.0;
  while(isDigit(peek()))
  {
    result = result * 10 + (peek() - '0');
    m_position += 1;
  }

  if(peek() == '.')
  {
    m_position += 1;
    double frac = 0.1;
    while(isDigit(peek()))
    {
JsonValue::operator double() const
{
  if(m_type == JsonValueType::Number)
  {
    return std::get<double>(m_value);
  }
  return 0;
}

      result = result + frac * (peek() - '0');
      frac = frac * 0.1;
      m_position += 1;
    }
  }
  if(peek() == 'e' || peek() == 'E')
  {
    m_position += 1;

    bool negExp = peek() == '-';
    if(peek() == '+' || peek() == '-')
      m_position += 1;
    
    double exponent = 0.0;
    while(isDigit(peek()))
    {
      exponent = exponent * 10 + (peek() - '0');
      m_position += 1;
    }
    
    if(negExp)
      exponent = -exponent;

    result = result * std::pow(10, exponent);
  }
  if(negative)
    result = -result;

  return result;
}

std::string JsonParser::parseString()
{
  m_position += 1;

  std::string result;
  uint32_t startLine = m_line;

  while(true)
  {
    if(m_position >= m_textLength)
    {
      m_error = true;
      m_errorType = JsonParserError::StringError;
      m_errorLine = startLine;
      m_errorPosition = m_position;

      break;
    }
    
    if(peek() == '\n')
      m_line += 1;

    if(peek() == '"')
    {
      m_position += 1;
      break;
    }
    result = result + peek();
    m_position += 1;
  }
  return result;
}

JsonValue* JsonParser::parseObject()
{

  m_position += 1;

  skipWhitespaces();

  JsonValue* result = new JsonValue();
  result->m_type = JsonValueType::Object;
  result->m_value= JsonValue::JsonObject();

  if(peek() == '}')
  {
    m_position += 1;
    return result;
  } 

  while(true)
  {
    std::string fieldName = parseString();
    if(m_error)
    {
      return nullptr;
    }
    skipWhitespaces();
    if(peek() != ':')
    {
      m_error = true;
      m_errorType = JsonParserError::ObjectColonError;
      m_errorLine = m_line;
      m_errorPosition = m_position;
      return nullptr;
    }
    m_position += 1;

    skipWhitespaces();

    JsonValue* fieldValue = parseValue();
    (std::get<JsonValue::JsonObject>(result->m_value))[fieldName] = fieldValue;

    skipWhitespaces();
    if(peek() == ',')
    {
      m_position += 1;
      skipWhitespaces();

      continue;
    }
    if(peek() == '}')
    {
      m_position += 1;
  
      return result;
    }

    //error
    m_error = true;
    m_errorType = JsonParserError::ObjectError;
    m_errorLine = m_line;
    m_errorPosition = m_position;
    return nullptr;
  }

  //unreachable 
  return nullptr;
}

JsonValue* JsonParser::parseArray()
{
  m_position += 1;

  skipWhitespaces();
  
  JsonValue* result = new JsonValue();
  result->m_type = JsonValueType::Array;
  result->m_value= JsonValue::JsonArray();

  if(peek() == ']')
  {
    m_position += 1;
    return result;
  }
  
  while(true)
  {
    JsonValue* value = parseValue();
    std::get<JsonValue::JsonArray>(result->m_value).push_back(value);
    
    skipWhitespaces();
    if(peek() == ',')
    {
      m_position += 1;
      skipWhitespaces();

      continue;
    }
    if(peek() == ']')
    {
      m_position += 1;
  
      return result;
    }

    //error
    m_error = true;
    m_errorType = JsonParserError::ArrayError;
    m_errorLine = m_line;
    m_errorPosition = m_position;
    return nullptr;
  }

  //unreachable
  return nullptr;
}

JsonValue* JsonParser::parseValue()
{
  if(peek() == '-' || isDigit(peek()))
  {
    double number = parseNumber();
    JsonValue* result = new JsonValue();
    result->m_type = JsonValueType::Number;
    result->m_value = number;
    return result;
  }
  if(peek() == '"')
  {
    std::string str = parseString();
    if(m_error)
    {
      return nullptr;
    }

    JsonValue* result = new JsonValue();
    result->m_type = JsonValueType::String;
    result->m_value = str;
    return result;
  }
  if(peek() == '[')
  {
    return parseArray();
  }
  if(peek() == '{')
  {
    return parseObject();
  }
  
  if(peek() == 't')
  {
    if(consumeString(boolTrueStr, boolTrueStrLen))
    {
      JsonValue* result = new JsonValue();
      result->m_type = JsonValueType::Boolean;
      result->m_value = true;
      return result;
    }
  }
  
  if(peek() == 'f')
  {
    if(consumeString(boolFalseStr, boolFalseStrLen))
    {
      JsonValue* result = new JsonValue();
      result->m_type = JsonValueType::Boolean;
      result->m_value = false;
      return result;
    }
  }
  
  if(peek() == 'n')
  {
    if(consumeString(nullStr, nullStrLen))
    {
      JsonValue* result = new JsonValue();
      result->m_type = JsonValueType::Null;
      return result;
    }
  }

  m_error = true;
  m_errorType = JsonParserError::SymbolError;
  m_errorLine = m_line;
  m_errorPosition = m_position;
  return nullptr;
}

void JsonParser::skipWhitespaces()
{
  while(peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r')
  {
    if(peek() == '\n')
      m_line += 1;
    m_position += 1;
  }
}

char JsonParser::peek()
{
  return m_text[m_position];
}

bool JsonParser::isDigit(char c)
{
  return c >= '0' && c <= '9';
}

bool JsonParser::consumeString(const char* str, uint32_t length)
{
  for(int i = 0; i < length; ++i)
  {
    if(m_text[m_position + i] != str[i])
      return false;
  }
  m_position += length;
  return true;
}
