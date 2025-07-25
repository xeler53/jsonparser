#include "jsonparser.h"

#include <cmath>
#include <sstream>
#include <stdio.h>
#include <math.h>
#include <cstring>
#include <algorithm>
#include <immintrin.h>  // For SIMD
#include <bit>

// Thread-local memory pool for better performance
thread_local JsonMemoryPool* g_threadPool = nullptr;

static JsonValue s_emptyObject;
static std::string s_emptyString = "null";

// Lookup tables for faster character classification
static constexpr bool DIGIT_TABLE[256] = {
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false
};

static constexpr bool WHITESPACE_TABLE[256] = {
    false, false, false, false, false, false, false, false, false, true,  true,  false, false, true,  false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    true,  false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false
};

// Memory Pool Implementation
JsonMemoryPool::JsonMemoryPool() {
    addBlock();
}

JsonMemoryPool::~JsonMemoryPool() = default;

void JsonMemoryPool::addBlock() {
    blocks.emplace_back(std::make_unique<Block>());
    currentBlock = blocks.size() - 1;
}

void* JsonMemoryPool::allocate(size_t size) {
    size = (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1); // Align
    
    if (currentBlock >= blocks.size() || blocks[currentBlock]->used + size > BLOCK_SIZE) {
        addBlock();
    }
    
    void* ptr = blocks[currentBlock]->data.data() + blocks[currentBlock]->used;
    blocks[currentBlock]->used += size;
    return ptr;
}

void JsonMemoryPool::reset() {
    for (auto& block : blocks) {
        block->used = 0;
    }
    currentBlock = 0;
}

// JsonValue Implementation
JsonValue::JsonValue() : m_type(JsonValueType::Null) {}

JsonValue::JsonValue(JsonValue&& other) noexcept {
    m_type = other.m_type;
    m_number = other.m_number; // Copy entire union
    other.m_type = JsonValueType::Null;
}

JsonValue& JsonValue::operator=(JsonValue&& other) noexcept {
    if (this != &other) {
        cleanup();
        m_type = other.m_type;
        m_number = other.m_number;
        other.m_type = JsonValueType::Null;
    }
    return *this;
}

JsonValue& JsonValue::operator[](const std::string& fieldName) noexcept {
    if (__builtin_expect(m_type == JsonValueType::Object, 1)) {
        auto it = m_object->find(fieldName);
        if (__builtin_expect(it != m_object->end(), 1)) {
            return it->second;
        }
    }
    return s_emptyObject;
}

JsonValue& JsonValue::operator[](const char* fieldName) noexcept {
    if (__builtin_expect(m_type == JsonValueType::Object, 1)) {
        auto it = m_object->find(fieldName);
        if (__builtin_expect(it != m_object->end(), 1)) {
            return it->second;
        }
    }
    return s_emptyObject;
}

JsonValue& JsonValue::operator[](std::string_view fieldName) noexcept {
    if (__builtin_expect(m_type == JsonValueType::Object, 1)) {
        std::string key(fieldName);
        auto it = m_object->find(key);
        if (__builtin_expect(it != m_object->end(), 1)) {
            return it->second;
        }
    }
    return s_emptyObject;
}

JsonValue& JsonValue::operator[](int index) noexcept {
    if (__builtin_expect(m_type == JsonValueType::Array && index >= 0 && 
                        static_cast<size_t>(index) < m_array->size(), 1)) {
        return (*m_array)[index];
    }
    return s_emptyObject;
}

JsonValue::operator int() const noexcept {
    return __builtin_expect(m_type == JsonValueType::Number, 1) ? 
           static_cast<int>(m_number) : 0;
}

JsonValue::operator double() const noexcept {
    return __builtin_expect(m_type == JsonValueType::Number, 1) ? 
           m_number : 0.0;
}

JsonValue::operator bool() const noexcept {
    return __builtin_expect(m_type == JsonValueType::Boolean, 1) ? 
           m_boolean : false;
}

JsonValue::operator const std::string&() const noexcept {
    return __builtin_expect(m_type == JsonValueType::String, 1) ? 
           *m_text : s_emptyString;
}

bool JsonValue::contains(const char* fieldName) const noexcept {
    return __builtin_expect(m_type == JsonValueType::Object, 1) ? 
           m_object->find(fieldName) != m_object->end() : false;
}

bool JsonValue::contains(const std::string& fieldName) const noexcept {
    return __builtin_expect(m_type == JsonValueType::Object, 1) ? 
           m_object->find(fieldName) != m_object->end() : false;
}

bool JsonValue::contains(std::string_view fieldName) const noexcept {
    if (__builtin_expect(m_type == JsonValueType::Object, 1)) {
        std::string key(fieldName);
        return m_object->find(key) != m_object->end();
    }
    return false;
}

uint32_t JsonValue::size() const noexcept {
    return __builtin_expect(m_type == JsonValueType::Array, 1) ? 
           static_cast<uint32_t>(m_array->size()) : 0;
}

void JsonValue::cleanup() noexcept {
    switch(m_type) {
        case JsonValueType::Object: delete m_object; break;
        case JsonValueType::Array: delete m_array; break;
        case JsonValueType::String: delete m_text; break;
        default: break;
    }
}

JsonValue::~JsonValue() {
    cleanup();
}

// Optimized constants
static constexpr const char* boolTrueStr = "true";
static constexpr uint32_t boolTrueStrLen = 4;
static constexpr const char* boolFalseStr = "false";
static constexpr uint32_t boolFalseStrLen = 5;
static constexpr const char* nullStr = "null";
static constexpr uint32_t nullStrLen = 4;

// JsonParser Implementation
JsonParser::JsonParser() : m_memoryPool(std::make_unique<JsonMemoryPool>()) {
    if (!g_threadPool) {
        g_threadPool = m_memoryPool.get();
    }
}

JsonParser::~JsonParser() = default;

void JsonParser::reset() noexcept {
    m_error = false;
    m_position = 0;
    m_line = 0;
    if (m_memoryPool) {
        m_memoryPool->reset();
    }
}

JsonValue JsonParser::parse(const char* text, uint32_t textLength) {
    m_text = text;
    m_textLength = textLength;
    m_position = 0;
    m_line = 0;
    m_error = false;

    skipWhitespaces();
    if (__builtin_expect(isAtEnd(), 0)) {
        return JsonValue{};
    }
    return parseValue();
}

JsonValue JsonParser::parse(std::string_view text) {
    return parse(text.data(), static_cast<uint32_t>(text.size()));
}

JsonValue JsonParser::parseFast(const char* text, uint32_t textLength) {
    // Fast path for simple JSON structures
    m_text = text;
    m_textLength = textLength;
    m_position = 0;
    m_line = 0;
    m_error = false;

    // Skip whitespace with SIMD if available
    if (textLength > 64) {
        skipWhitespacesSIMD();
    } else {
        skipWhitespaces();
    }
    
    if (__builtin_expect(isAtEnd(), 0)) {
        return JsonValue{};
    }
    
    return parseValue();
}

std::string JsonParser::getErrorText() const {
    std::ostringstream text;
    switch(m_errorType) {
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

double JsonParser::parseIntegerFast() noexcept {
    // Fast path for integers without decimal point
    double result = 0.0;
    const char* start = m_text + m_position;
    const char* end = m_text + m_textLength;
    
    bool negative = false;
    if (*start == '-') {
        negative = true;
        ++start;
        ++m_position;
    }
    
    // Unrolled loop for better performance
    while (start < end && isDigit(*start)) {
        result = result * 10.0 + (*start - '0');
        ++start;
        ++m_position;
    }
    
    return negative ? -result : result;
}

double JsonParser::parseNumber() noexcept {
    // Try fast path first for integers
    const char* start = m_text + m_position;
    const char* lookahead = start;
    
    if (*lookahead == '-') ++lookahead;
    
    // Check if it's a simple integer
    bool isSimpleInt = true;
    while (lookahead < m_text + m_textLength && isDigit(*lookahead)) {
        ++lookahead;
    }
    
    if (lookahead < m_text + m_textLength && 
        (*lookahead == '.' || *lookahead == 'e' || *lookahead == 'E')) {
        isSimpleInt = false;
    }
    
    if (isSimpleInt) {
        return parseIntegerFast();
    }
    
    // Full number parsing for complex numbers
    bool negative = false;
    if (peek() == '-') {
        negative = true;
        m_position += 1;
    }

    double result = 0.0;
    while (!isAtEnd() && isDigit(peek())) {
        result = result * 10.0 + static_cast<double>(peek() - '0');
        m_position += 1;
    }

    if (!isAtEnd() && peek() == '.') {
        m_position += 1;
        double frac = 0.1;
        while (!isAtEnd() && isDigit(peek())) {
            result += frac * static_cast<double>(peek() - '0');
            frac *= 0.1;
            m_position += 1;
        }
    }
    
    if (!isAtEnd() && (peek() == 'e' || peek() == 'E')) {
        m_position += 1;
        bool negExp = false;
        if (!isAtEnd() && peek() == '-') {
            negExp = true;
            m_position += 1;
        } else if (!isAtEnd() && peek() == '+') {
            m_position += 1;
        }

        double exponent = 0.0;
        while (!isAtEnd() && isDigit(peek())) {
            exponent = exponent * 10.0 + static_cast<double>(peek() - '0');
            m_position += 1;
        }

        if (negExp) exponent = -exponent;
        result *= std::pow(10.0, exponent);
    }
    
    return negative ? -result : result;
}

bool JsonParser::parseStringFast(std::string& result) {
    const char* start = m_text + m_position + 1; // Skip opening quote
    const char* end = findNextQuote(start, m_text + m_textLength);
    
    if (!end) {
        return false; // No closing quote found
    }
    
    result.assign(start, end - start);
    m_position = static_cast<uint32_t>(end - m_text + 1); // Skip closing quote
    return true;
}

std::string JsonParser::parseString() {
    std::string result;
    
    // Try fast path first
    if (parseStringFast(result)) {
        return result;
    }
    
    // Fallback to safe parsing
    m_position += 1; // Skip opening quote
    result.reserve(64);
    
    uint32_t startLine = m_line;
    const uint32_t startPos = m_position;

    uint32_t endPos = m_position;
    while (endPos < m_textLength && m_text[endPos] != '"') {
        if (m_text[endPos] == '\n') m_line += 1;
        endPos++;
    }

    if (endPos >= m_textLength) {
        setError(JsonParserError::StringError);
        m_errorLine = startLine;
        return result;
    }

    result.assign(m_text + startPos, endPos - startPos);
    m_position = endPos + 1;
    return result;
}

JsonValue JsonParser::parseObject() {
    m_position += 1; // Skip '{'
    skipWhitespaces();

    JsonValue result;
    result.m_type = JsonValueType::Object;
    result.m_object = new JsonValue::JsonObject;
    result.m_object->reserve(16); // Larger reserve for better performance

    if (!isAtEnd() && peek() == '}') {
        m_position += 1;
        return result;
    } 

    while (true) {
        if (__builtin_expect(isAtEnd() || peek() != '"', 0)) {
            setError(JsonParserError::StringError);
            return JsonValue{};
        }

        std::string fieldName = parseString();
        if (__builtin_expect(m_error, 0)) {
            return JsonValue{};
        }
        
        skipWhitespaces();
        if (__builtin_expect(isAtEnd() || peek() != ':', 0)) {
            setError(JsonParserError::ObjectColonError);
            return JsonValue{};
        }
        m_position += 1;

        skipWhitespaces();
        JsonValue value = parseValue();
        if (__builtin_expect(m_error, 0)) {
            return JsonValue{};
        }

        result.m_object->emplace(std::move(fieldName), std::move(value));

        skipWhitespaces();
        if (__builtin_expect(isAtEnd(), 0)) {
            setError(JsonParserError::ObjectError);
            return JsonValue{};
        }
        
        if (peek() == ',') {
            m_position += 1;
            skipWhitespaces();
            continue;
        }
        if (peek() == '}') {
            m_position += 1;
            return result;
        }

        setError(JsonParserError::ObjectError);
        return JsonValue{};
    }
}

JsonValue JsonParser::parseArray() {
    m_position += 1; // Skip '['
    skipWhitespaces();

    JsonValue result;
    result.m_type = JsonValueType::Array;
    result.m_array = new JsonValue::JsonArray;
    result.m_array->reserve(16); // Larger reserve

    if (!isAtEnd() && peek() == ']') {
        m_position += 1;
        return result;
    }

    while (true) {
        JsonValue value = parseValue();
        if (__builtin_expect(m_error, 0)) {
            return JsonValue{};
        }
        
        result.m_array->push_back(std::move(value));

        skipWhitespaces();
        if (__builtin_expect(isAtEnd(), 0)) {
            setError(JsonParserError::ArrayError);
            return JsonValue{};
        }
        
        if (peek() == ',') {
            m_position += 1;
            skipWhitespaces();
            continue;
        }
        if (peek() == ']') {
            m_position += 1;
            return result;
        }

        setError(JsonParserError::ArrayError);
        return JsonValue{};
    }
}

JsonValue JsonParser::parseValue() {
    if (__builtin_expect(isAtEnd(), 0)) {
        setError(JsonParserError::BoundsError);
        return JsonValue{};
    }

    char c = peek();
    
    // Fast path with jump table-like structure
    if (__builtin_expect(c == '-' || isDigit(c), 0)) {
        double number = parseNumber();
        JsonValue result;
        result.m_type = JsonValueType::Number;
        result.m_number = number;
        return result;
    }
    
    if (__builtin_expect(c == '"', 0)) {
        std::string str = parseString();
        if (__builtin_expect(m_error, 0)) {
            return JsonValue{};
        }

        JsonValue result; 
        result.m_type = JsonValueType::String;
        result.m_text = new std::string(std::move(str));
        return result;
    }
    
    if (__builtin_expect(c == '[', 0)) {
        return parseArray();
    }
    
    if (__builtin_expect(c == '{', 1)) { // Objects are most common
        return parseObject();
    }

    // Boolean and null with optimized comparisons
    if (c == 't' && consumeString(boolTrueStr, boolTrueStrLen)) {
        JsonValue result;
        result.m_type = JsonValueType::Boolean;
        result.m_boolean = true;
        return result;
    }

    if (c == 'f' && consumeString(boolFalseStr, boolFalseStrLen)) {
        JsonValue result;
        result.m_type = JsonValueType::Boolean;
        result.m_boolean = false;
        return result;
    }

    if (c == 'n' && consumeString(nullStr, nullStrLen)) {
        JsonValue result;
        result.m_type = JsonValueType::Null;
        return result;
    }

    setError(JsonParserError::SymbolError);
    return JsonValue{};
}

void JsonParser::skipWhitespaces() noexcept {
    while (!isAtEnd() && isWhitespace(peek())) {
        if (peek() == '\n') m_line += 1;
        m_position += 1;
    }
}

void JsonParser::skipWhitespacesSIMD() noexcept {
    // SIMD-optimized whitespace skipping for large inputs
    const char* current = m_text + m_position;
    const char* end = m_text + m_textLength;
    
    // Process 16 bytes at a time with SSE
    while (current + 16 <= end) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(current));
        __m128i spaces = _mm_set1_epi8(' ');
        __m128i tabs = _mm_set1_epi8('\t');
        __m128i newlines = _mm_set1_epi8('\n');
        __m128i returns = _mm_set1_epi8('\r');
        
        __m128i isSpace = _mm_cmpeq_epi8(chunk, spaces);
        __m128i isTab = _mm_cmpeq_epi8(chunk, tabs);
        __m128i isNewline = _mm_cmpeq_epi8(chunk, newlines);
        __m128i isReturn = _mm_cmpeq_epi8(chunk, returns);
        
        __m128i isWhitespace = _mm_or_si128(_mm_or_si128(isSpace, isTab), 
                                          _mm_or_si128(isNewline, isReturn));
        
        int mask = _mm_movemask_epi8(isWhitespace);
        
        if (mask != 0xFFFF) {
            // Found non-whitespace, process byte by byte
            int firstNonWhitespace = __builtin_ctz(~mask);
            current += firstNonWhitespace;
            break;
        }
        
        // Count newlines for line tracking
        int newlineMask = _mm_movemask_epi8(isNewline);
        m_line += __builtin_popcount(newlineMask);
        
        current += 16;
    }
    
    // Process remaining bytes
    while (current < end && isWhitespace(*current)) {
        if (*current == '\n') m_line += 1;
        ++current;
    }
    
    m_position = static_cast<uint32_t>(current - m_text);
}

const char* JsonParser::findNextQuote(const char* start, const char* end) const noexcept {
    // SIMD-optimized quote finding
    const char* current = start;
    
    while (current + 16 <= end) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(current));
        __m128i quotes = _mm_set1_epi8('"');
        __m128i isQuote = _mm_cmpeq_epi8(chunk, quotes);
        
        int mask = _mm_movemask_epi8(isQuote);
        if (mask != 0) {
            int firstQuote = __builtin_ctz(mask);
            return current + firstQuote;
        }
        
        current += 16;
    }
    
    // Process remaining bytes
    while (current < end) {
        if (*current == '"') return current;
        ++current;
    }
    
    return nullptr;
}

char JsonParser::peek() const noexcept {
    return __builtin_expect(isAtEnd(), 0) ? '\0' : m_text[m_position];
}

char JsonParser::peekNext() const noexcept {
    return __builtin_expect(m_position + 1 >= m_textLength, 0) ? 
           '\0' : m_text[m_position + 1];
}

bool JsonParser::isAtEnd() const noexcept {
    return m_position >= m_textLength;
}

bool JsonParser::isDigit(char c) const noexcept {
    return DIGIT_TABLE[static_cast<unsigned char>(c)];
}

bool JsonParser::isWhitespace(char c) const noexcept {
    return WHITESPACE_TABLE[static_cast<unsigned char>(c)];
}

bool JsonParser::consumeString(const char* str, uint32_t length) noexcept {
    if (__builtin_expect(m_position + length > m_textLength, 0)) {
        return false;
    }
        
    if (std::memcmp(m_text + m_position, str, length) == 0) {
        m_position += length;
        return true;
    }
    return false;
}

void JsonParser::setError(JsonParserError errorType) noexcept {
    m_error = true;
    m_errorType = errorType;
    m_errorLine = m_line;
    m_errorPosition = m_position;
}
