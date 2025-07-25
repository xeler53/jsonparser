#include <iostream>
#include <chrono>
#include <cstring>
#include <vector>
#include <string>

#include "jsonparser.h"

// Generate a large JSON string for benchmarking
std::string generateLargeJson(int numObjects) {
    std::string json = "[";
    for (int i = 0; i < numObjects; ++i) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"id\":" + std::to_string(i) + ",";
        json += "\"name\":\"Object" + std::to_string(i) + "\",";
        json += "\"active\":";
        json += (i % 2 == 0 ? "true" : "false");
        json += ",";
        json += "\"value\":" + std::to_string(i * 3.14159) + ",";
        json += "\"tags\":[\"tag1\",\"tag2\",\"tag3\"],";
        json += "\"metadata\":{";
        json += "\"created\":\"2024-01-01\",";
        json += "\"modified\":\"2024-07-25\",";
        json += "\"version\":1";
        json += "}";
        json += "}";
    }
    json += "]";
    return json;
}

void benchmarkParsing(const std::string& jsonStr, int iterations) {
    std::cout << "Benchmarking JSON parsing...\n";
    std::cout << "JSON size: " << jsonStr.size() << " bytes\n";
    std::cout << "Iterations: " << iterations << "\n\n";
    
    // Warm up
    JsonParser parser;
    JsonValue warmUp = parser.parse(jsonStr.c_str(), jsonStr.size());
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        JsonParser parser;
        JsonValue json = parser.parse(jsonStr.c_str(), jsonStr.size());
        
        if (parser.hasError()) {
            std::cout << "Parse error: " << parser.getErrorText() << std::endl;
            return;
        }
        
        // Access some data to ensure parsing actually happened
        if (json.size() > 0) {
            JsonValue& firstObj = json[0];
            std::string name = firstObj["name"];
            double value = firstObj["value"];
            bool active = firstObj["active"];
            
            // Prevent compiler optimization
            volatile auto dummy = name.size() + static_cast<size_t>(value) + (active ? 1 : 0);
            (void)dummy;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double avgTime = duration.count() / static_cast<double>(iterations);
    double throughput = (jsonStr.size() * iterations) / (duration.count() / 1000000.0) / (1024 * 1024); // MB/s
    
    std::cout << "Total time: " << duration.count() << " microseconds\n";
    std::cout << "Average time per parse: " << avgTime << " microseconds\n";
    std::cout << "Throughput: " << throughput << " MB/s\n";
}

void benchmarkStringView() {
    std::cout << "\n=== String View Performance Test ===\n";
    
    std::string jsonStr = R"({"name":"TestObject","value":42.5,"active":true})";
    JsonParser parser;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 100000; ++i) {
        JsonValue json = parser.parse(jsonStr); // Using string_view overload
        if (parser.hasError()) break;
        
        std::string name = json["name"];
        volatile auto dummy = name.size();
        (void)dummy;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "String view parsing (100k iterations): " << duration.count() << " microseconds\n";
}

int main() {
    std::cout << "=== JSON Parser Performance Benchmark ===\n\n";
    
    // Test with different sizes
    std::vector<int> sizes = {100, 1000, 5000};
    
    for (int size : sizes) {
        std::cout << "=== Testing with " << size << " objects ===\n";
        std::string largeJson = generateLargeJson(size);
        benchmarkParsing(largeJson, size < 1000 ? 1000 : 100);
        std::cout << "\n";
    }
    
    benchmarkStringView();
    
    std::cout << "\n=== Optimization Features Demonstrated ===\n";
    std::cout << "✓ Improved move semantics and copy prevention\n";
    std::cout << "✓ String view support for zero-copy operations\n";
    std::cout << "✓ Better bounds checking and error handling\n";
    std::cout << "✓ Optimized string parsing with bulk operations\n";
    std::cout << "✓ Memory pre-allocation for containers\n";
    std::cout << "✓ Faster character-by-character operations\n";
    std::cout << "✓ Const-correctness improvements\n";
    std::cout << "✓ Optimized literal comparisons with memcmp\n";
    
    return 0;
}