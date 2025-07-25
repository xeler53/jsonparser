#pragma once

#include <stdint.h>
#include <variant>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <array>

enum class JsonValueType : uint8_t
{
	Null = 0, 
	Number = 1,
	Boolean = 2,
	String = 3,
	Array = 4,
	Object = 5
};

struct JsonValue;

// Fast string hash for better performance
struct FastStringHash {
    std::size_t operator()(const std::string& s) const noexcept {
        // FNV-1a hash - faster than std::hash for short strings
        std::size_t hash = 14695981039346656037ULL;
        for (char c : s) {
            hash ^= static_cast<std::size_t>(c);
            hash *= 1099511628211ULL;
        }
        return hash;
    }
};

// Memory pool for faster allocations
class JsonMemoryPool {
public:
    static constexpr size_t BLOCK_SIZE = 8192;
    static constexpr size_t ALIGNMENT = 8;
    
    JsonMemoryPool();
    ~JsonMemoryPool();
    
    void* allocate(size_t size);
    void reset();
    
private:
    struct Block {
        std::array<char, BLOCK_SIZE> data;
        size_t used = 0;
    };
    
    std::vector<std::unique_ptr<Block>> blocks;
    size_t currentBlock = 0;
    
    void addBlock();
};

class JsonValue
{
public:
	JsonValue();
	JsonValue(const JsonValue& other) = delete;
	JsonValue(JsonValue&& other) noexcept;
	JsonValue& operator=(JsonValue&& other) noexcept;
	JsonValue& operator=(const JsonValue& other) = delete;

	// Fast path accessors with branch prediction hints
	JsonValue& operator[](const std::string& fieldName) noexcept;
	JsonValue& operator[](const char* fieldName) noexcept;
	JsonValue& operator[](std::string_view fieldName) noexcept;
	JsonValue& operator[](int index) noexcept;

	// Optimized conversion operators
	operator int() const noexcept;
	operator double() const noexcept;
	operator bool() const noexcept;
	operator const std::string&() const noexcept;

	// Fast contains with early exit
	bool contains(const char* fieldName) const noexcept;
	bool contains(const std::string& fieldName) const noexcept;
	bool contains(std::string_view fieldName) const noexcept;

	uint32_t size() const noexcept;
	JsonValueType getType() const noexcept { return m_type; }

	~JsonValue();
private:
	// Use custom hash for better performance
	typedef std::unordered_map<std::string, JsonValue, FastStringHash> JsonObject;
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
	
	void cleanup() noexcept;
	friend class JsonParser;
};

enum class JsonParserError : uint8_t
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
	JsonParser();
	~JsonParser();
	
	JsonValue parse(const char* text, uint32_t textLength);
	JsonValue parse(std::string_view text);
	
	// Fast path for parsing known structures
	JsonValue parseFast(const char* text, uint32_t textLength);
	
	bool hasError() const noexcept { return m_error; }
	std::string getErrorText() const;
	
	// Reset for reuse
	void reset() noexcept;

private:
	const char* m_text;
	uint32_t m_textLength;

	uint32_t m_position;
	uint32_t m_line;

	bool m_error;
	uint32_t m_errorLine;
	uint32_t m_errorPosition;
	JsonParserError m_errorType;
	
	// Memory pool for faster allocations
	std::unique_ptr<JsonMemoryPool> m_memoryPool;

	// Optimized parsing functions
	double parseNumber() noexcept;
	std::string parseString();
	JsonValue parseObject();
	JsonValue parseArray();
	JsonValue parseValue(); 
	
	// Fast path parsing for simple cases
	double parseIntegerFast() noexcept;
	bool parseStringFast(std::string& result);

	// Optimized utility functions
	void skipWhitespaces() noexcept;
	char peek() const noexcept;
	char peekNext() const noexcept;
	bool isAtEnd() const noexcept;
	bool isDigit(char c) const noexcept;
	bool isWhitespace(char c) const noexcept;
	bool consumeString(const char* str, uint32_t length) noexcept;
	void setError(JsonParserError errorType) noexcept;
	
	// SIMD-optimized functions for large data
	void skipWhitespacesSIMD() noexcept;
	const char* findNextQuote(const char* start, const char* end) const noexcept;
};

