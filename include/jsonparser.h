#pragma once

#include <stdint.h>
#include <variant>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>

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
	JsonValue(const JsonValue& other) = delete; // Prevent copying for performance
	JsonValue(JsonValue&& other) noexcept;
	JsonValue& operator=(JsonValue&& other) noexcept;
	JsonValue& operator=(const JsonValue& other) = delete; // Prevent copying

	JsonValue& operator[](const std::string& fieldName);
	JsonValue& operator[](const char* fieldName);
	JsonValue& operator[](std::string_view fieldName);
	JsonValue& operator[](int index);

	operator int() const;
	operator double() const;
	operator bool() const;
	operator const std::string&() const;

	bool contains(const char* fieldName) const;
	bool contains(const std::string& fieldName) const;
	bool contains(std::string_view fieldName) const;

	uint32_t size() const;

	~JsonValue();
private:

	typedef std::unordered_map<std::string, JsonValue, std::hash<std::string>, std::equal_to<std::string>> JsonObject;
	typedef std::vector<JsonValue> JsonArray;

	JsonValueType m_type;
	union
	{
		double m_number;
		bool m_boolean;
		std::string* m_text;
		JsonArray* m_array;
		JsonObject* m_object;
	};
	
	void cleanup();
	friend class JsonParser;
};

enum class JsonParserError
{
	ArrayError,
	ObjectError,
	ObjectColonError,
	StringError,
	SymbolError,
	BoundsError,
};

class JsonParser
{
public:
	JsonValue parse(const char* text, uint32_t textLength);
	JsonValue parse(std::string_view text);
	bool hasError() const { return m_error; };
	std::string getErrorText() const;

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
	JsonValue parseObject();
	JsonValue parseArray();
	JsonValue parseValue(); 

	void skipWhitespaces();

	char peek() const;
	bool isAtEnd() const;
	bool isDigit(char c) const;
	bool consumeString(const char* str, uint32_t length);
	void setError(JsonParserError errorType);
};

