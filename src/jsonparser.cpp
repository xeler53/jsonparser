#include "jsonparser.h"

#include <cmath>
#include <sstream>
#include <stdio.h>
#include <math.h>
#include <cstring>
#include <algorithm>

static JsonValue s_emptyObject;
static std::string s_emptyString = "null";

JsonValue::JsonValue()
{
	m_type = JsonValueType::Null;
}

JsonValue::JsonValue(JsonValue&& other) noexcept
{
	m_type = other.m_type;
	
	// Copy union data
	m_number = other.m_number; // This covers the entire union
	
	// Reset the other object to prevent double deletion
	other.m_type = JsonValueType::Null;
}

JsonValue& JsonValue::operator=(JsonValue&& other) noexcept
{
	if (this != &other)
	{
		cleanup(); // Clean up current resources
		
		m_type = other.m_type;
		m_number = other.m_number; // Copy union data
		
		other.m_type = JsonValueType::Null;
	}
	return *this;
}

JsonValue& JsonValue::operator[](const std::string& fieldName) 
{
	if(m_type == JsonValueType::Object)
	{
		auto it = m_object->find(fieldName);
		if(it != m_object->end())
			return it->second;
	}
	return s_emptyObject;
}

JsonValue& JsonValue::operator[](const char* fieldName)
{
	if(m_type == JsonValueType::Object)
	{
		auto it = m_object->find(fieldName);
		if(it != m_object->end())
			return it->second;
	}
	return s_emptyObject;
}

JsonValue& JsonValue::operator[](std::string_view fieldName)
{
	if(m_type == JsonValueType::Object)
	{
		// Use find with string_view compatible lookup
		std::string key(fieldName); // Only construct string when needed
		auto it = m_object->find(key);
		if(it != m_object->end())
			return it->second;
	}
	return s_emptyObject;
}

JsonValue& JsonValue::operator[](int index) 
{
	if(m_type == JsonValueType::Array && index >= 0 && static_cast<size_t>(index) < m_array->size())
	{
		return (*m_array)[index];
	}
	return s_emptyObject;
}

JsonValue::operator int() const
{
	if(m_type == JsonValueType::Number)
	{
		return static_cast<int>(m_number);
	}
	return 0;
}

JsonValue::operator double() const
{
	if(m_type == JsonValueType::Number)
	{
		return m_number;
	}
	return 0.0;
}

JsonValue::operator bool() const
{
	if(m_type == JsonValueType::Boolean)
	{
		return m_boolean;
	}
	return false;
}

JsonValue::operator const std::string&() const
{
	if(m_type == JsonValueType::String)
	{
		return *m_text;
	}
	return s_emptyString;
}

bool JsonValue::contains(const char* fieldName) const
{
	if(m_type == JsonValueType::Object)
	{
		return m_object->find(fieldName) != m_object->end();
	}
	return false;
}

bool JsonValue::contains(const std::string& fieldName) const
{
	if(m_type == JsonValueType::Object)
	{
		return m_object->find(fieldName) != m_object->end();
	}
	return false;
}

bool JsonValue::contains(std::string_view fieldName) const
{
	if(m_type == JsonValueType::Object)
	{
		std::string key(fieldName);
		return m_object->find(key) != m_object->end();
	}
	return false;
}

uint32_t JsonValue::size() const
{
	if(m_type == JsonValueType::Array)
	{
		return static_cast<uint32_t>(m_array->size());
	}
	return 0;
}

void JsonValue::cleanup()
{
	switch(m_type)
	{
		case JsonValueType::Object:
			delete m_object;
			break;
		case JsonValueType::Array:
			delete m_array;
			break;
		case JsonValueType::String:
			delete m_text;
			break;
		default:
			break;
	}
}

JsonValue::~JsonValue()
{
	cleanup();
}

// Optimized string constants
static constexpr const char* boolTrueStr = "true";
static constexpr uint32_t boolTrueStrLen = 4;

static constexpr const char* boolFalseStr = "false";
static constexpr uint32_t boolFalseStrLen = 5;

static constexpr const char* nullStr = "null";
static constexpr uint32_t nullStrLen = 4;

JsonValue JsonParser::parse(const char* text, uint32_t textLength)
{
	m_text = text;
	m_textLength = textLength;

	m_position = 0;
	m_line = 0;

	m_error = false;

	skipWhitespaces();
	if(isAtEnd())
	{
		return JsonValue{};
	}
	return parseValue();
}

JsonValue JsonParser::parse(std::string_view text)
{
	return parse(text.data(), static_cast<uint32_t>(text.size()));
}

std::string JsonParser::getErrorText() const
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
			text << "Unknown symbolic literal ";
			break;
			
		case JsonParserError::BoundsError:
			text << "Unexpected end of input ";
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
	
	// Optimized integer part parsing
	while(!isAtEnd() && isDigit(peek()))
	{
		result = result * 10.0 + static_cast<double>(peek() - '0');
		m_position += 1;
	}

	// Fractional part
	if(!isAtEnd() && peek() == '.')
	{
		m_position += 1;
		double frac = 0.1;
		while(!isAtEnd() && isDigit(peek()))
		{
			result += frac * static_cast<double>(peek() - '0');
			frac *= 0.1;
			m_position += 1;
		}
	}
	
	// Scientific notation
	if(!isAtEnd() && (peek() == 'e' || peek() == 'E'))
	{
		m_position += 1;

		bool negExp = false;
		if(!isAtEnd() && peek() == '-')
		{
			negExp = true;
			m_position += 1;
		}
		else if(!isAtEnd() && peek() == '+')
		{
			m_position += 1;
		}

		double exponent = 0.0;
		while(!isAtEnd() && isDigit(peek()))
		{
			exponent = exponent * 10.0 + static_cast<double>(peek() - '0');
			m_position += 1;
		}

		if(negExp)
			exponent = -exponent;

		result *= std::pow(10.0, exponent);
	}
	
	return negative ? -result : result;
}

std::string JsonParser::parseString()
{
	m_position += 1; // Skip opening quote

	std::string result;
	result.reserve(64); // Reserve space to reduce allocations
	
	uint32_t startLine = m_line;
	const uint32_t startPos = m_position;

	// Find the end quote first for better performance
	uint32_t endPos = m_position;
	while(endPos < m_textLength && m_text[endPos] != '"')
	{
		if(m_text[endPos] == '\n')
			m_line += 1;
		endPos++;
	}

	if(endPos >= m_textLength)
	{
		setError(JsonParserError::StringError);
		m_errorLine = startLine;
		return result;
	}

	// Copy the string content in one go
	result.assign(m_text + startPos, endPos - startPos);
	m_position = endPos + 1; // Skip closing quote

	return result;
}

JsonValue JsonParser::parseObject()
{
	m_position += 1; // Skip '{'

	skipWhitespaces();

	JsonValue result;
	result.m_type = JsonValueType::Object;
	result.m_object = new JsonValue::JsonObject;
	result.m_object->reserve(8); // Reserve space for better performance

	if(!isAtEnd() && peek() == '}')
	{
		m_position += 1;
		return result;
	} 

	while(true)
	{
		if(isAtEnd() || peek() != '"')
		{
			setError(JsonParserError::StringError);
			return JsonValue{};
		}

		std::string fieldName = parseString();
		if(m_error)
		{
			return JsonValue{};
		}
		
		skipWhitespaces();
		if(isAtEnd() || peek() != ':')
		{
			setError(JsonParserError::ObjectColonError);
			return JsonValue{};
		}
		m_position += 1;

		skipWhitespaces();

		JsonValue value = parseValue();
		if(m_error)
		{
			return JsonValue{};
		}

		result.m_object->emplace(std::move(fieldName), std::move(value));

		skipWhitespaces();
		if(isAtEnd())
		{
			setError(JsonParserError::ObjectError);
			return JsonValue{};
		}
		
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

		setError(JsonParserError::ObjectError);
		return JsonValue{};
	}
}

JsonValue JsonParser::parseArray()
{
	m_position += 1; // Skip '['

	skipWhitespaces();

	JsonValue result;
	result.m_type = JsonValueType::Array;
	result.m_array = new JsonValue::JsonArray;
	result.m_array->reserve(8); // Reserve space for better performance

	if(!isAtEnd() && peek() == ']')
	{
		m_position += 1;
		return result;
	}

	while(true)
	{
		JsonValue value = parseValue();
		if(m_error)
		{
			return JsonValue{};
		}
		
		result.m_array->push_back(std::move(value));

		skipWhitespaces();
		if(isAtEnd())
		{
			setError(JsonParserError::ArrayError);
			return JsonValue{};
		}
		
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

		setError(JsonParserError::ArrayError);
		return JsonValue{};
	}
}

JsonValue JsonParser::parseValue()
{
	if(isAtEnd())
	{
		setError(JsonParserError::BoundsError);
		return JsonValue{};
	}

	char c = peek();
	
	// Number parsing
	if(c == '-' || isDigit(c))
	{
		double number = parseNumber();
		JsonValue result;
		result.m_type = JsonValueType::Number;
		result.m_number = number;
		return result;
	}
	
	// String parsing
	if(c == '"')
	{
		std::string str = parseString();
		if(m_error)
		{
			return JsonValue{};
		}

		JsonValue result; 
		result.m_type = JsonValueType::String;
		result.m_text = new std::string(std::move(str));
		return result;
	}
	
	// Array parsing
	if(c == '[')
	{
		return parseArray();
	}
	
	// Object parsing
	if(c == '{')
	{
		return parseObject();
	}

	// Boolean and null parsing - optimized with early character checks
	if(c == 't' && consumeString(boolTrueStr, boolTrueStrLen))
	{
		JsonValue result;
		result.m_type = JsonValueType::Boolean;
		result.m_boolean = true;
		return result;
	}

	if(c == 'f' && consumeString(boolFalseStr, boolFalseStrLen))
	{
		JsonValue result;
		result.m_type = JsonValueType::Boolean;
		result.m_boolean = false;
		return result;
	}

	if(c == 'n' && consumeString(nullStr, nullStrLen))
	{
		JsonValue result;
		result.m_type = JsonValueType::Null;
		return result;
	}

	setError(JsonParserError::SymbolError);
	return JsonValue{};
}

void JsonParser::skipWhitespaces()
{
	while(!isAtEnd())
	{
		char c = peek();
		if(c == ' ' || c == '\t' || c == '\n' || c == '\r')
		{
			if(c == '\n')
				m_line += 1;
			m_position += 1;
		}
		else
		{
			break;
		}
	}
}

char JsonParser::peek() const
{
	if(isAtEnd())
		return '\0';
	return m_text[m_position];
}

bool JsonParser::isAtEnd() const
{
	return m_position >= m_textLength;
}

bool JsonParser::isDigit(char c) const
{
	return c >= '0' && c <= '9';
}

bool JsonParser::consumeString(const char* str, uint32_t length)
{
	if(m_position + length > m_textLength)
		return false;
		
	// Use memcmp for faster string comparison
	if(std::memcmp(m_text + m_position, str, length) == 0)
	{
		m_position += length;
		return true;
	}
	return false;
}

void JsonParser::setError(JsonParserError errorType)
{
	m_error = true;
	m_errorType = errorType;
	m_errorLine = m_line;
	m_errorPosition = m_position;
}
