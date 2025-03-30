#ifndef UTIL_HPP
#define UTIL_HPP

#include <random>
#include <iostream>  
#include <cstdint>
#include <vector>
#include <iomanip>
#include <cstring>

uint64_t mod_pow(uint64_t base, uint64_t exponent, uint64_t modulus);
uint64_t sample_random(uint64_t from, uint64_t to);

uint64_t mod_pow(uint64_t base, uint64_t exponent, uint64_t modulus) {  
    uint64_t result = 1;
    while (exponent > 0) {  
        if (exponent % 2 == 1) {  
            result = (result * base) % modulus;
        }  
        base = (base * base) % modulus;
        exponent /= 2;
    }  
    return result;
} 

uint64_t sample_random(uint64_t from, uint64_t to) {
    if (from > to) {  
        std::cerr << "[sample_random] ``from`` cannot be smaller than ``to``" << std::endl;
        exit(-1);
    }

    if (from == to) return from;

    std::random_device rd;  
    std::mt19937 gen(rd());   
    std::uniform_int_distribution<uint64_t> dis(from, to);  

    return dis(gen);
}

// Convert uint64 to eight uint8
std::vector<unsigned char> Uint64ToUnsignedVector(const uint64_t& value) {  
    std::vector<unsigned char> result(sizeof(uint64_t));
    std::memcpy(result.data(), &value, sizeof(uint64_t));
    return result;
}

uint64_t UnsignedVectorToUint64(const std::vector<unsigned char>& vec) {
    uint64_t result;
    if (vec.size() != sizeof(uint64_t)) {
        throw std::invalid_argument("Vector size must be equal to size of uint64_t.");
    }
    std::memcpy(&result, vec.data(), sizeof(uint64_t));
    return result;
}

// Convert int32 to four uint8
std::vector<unsigned char> Int32ToUnsignedVector(const int32_t& value) {
    std::vector<unsigned char> result(sizeof(int32_t));
    std::memcpy(result.data(), &value, sizeof(int32_t));
    return result;
}

int32_t UnsignedVectorToInt32(const std::vector<unsigned char>& vec) {
    int32_t result;
    if (vec.size() != sizeof(int32_t)) {
        throw std::invalid_argument("Vector size must be equal to size of int32_t.");
    }
    std::memcpy(&result, vec.data(), sizeof(int32_t));
    return result;
}

int32_t UnsignedVectorToInt32(const std::vector<unsigned char>& vec, const size_t offset) {
    int32_t result;
    if (vec.size() < offset+sizeof(int32_t)) {
        throw std::invalid_argument("Vector size must be larger than size of int32_t.");
    }
    std::memcpy(&result, vec.data()+offset, sizeof(int32_t));
    return result;
}

std::vector<unsigned char> VectorFloatToVectorUnsignedChar(const std::vector<float>& floatVec) {
    size_t totalSize = floatVec.size() * sizeof(float);
    
    std::vector<unsigned char> result(totalSize);
    std::memcpy(result.data(), floatVec.data(), totalSize);
    
    return result;
}

std::vector<float> UnsignedVectorToFloatVector(const std::vector<unsigned char>& ucharVec) {
    if (ucharVec.size() % sizeof(float) != 0) {
        throw std::invalid_argument("ucharVec.size() is not exact correct");
    }
    
    size_t numFloats = ucharVec.size() / sizeof(float);
    std::vector<float> result(numFloats);
    std::memcpy(result.data(), ucharVec.data(), numFloats*sizeof(float));
    
    return result;
}

std::vector<float> UnsignedVectorToFloatVector(const std::vector<unsigned char>& ucharVec, const size_t numFloats) {
    if (ucharVec.size() < sizeof(float)*numFloats) {
        throw std::invalid_argument("ucharVec.size() is smaller than numFloats");
    }
    
    std::vector<float> result(numFloats);
    std::memcpy(result.data(), ucharVec.data(), numFloats*sizeof(float));
    
    return result;
}

std::vector<float> UnsignedVectorToFloatVector(const std::vector<unsigned char>& ucharVec, const size_t numFloats, const size_t offset) {
    if (ucharVec.size() < offset+sizeof(float)*numFloats) {
        throw std::invalid_argument("ucharVec.size() is smaller than numFloats");
    }
    
    std::vector<float> result(numFloats);
    std::memcpy(result.data(), ucharVec.data()+offset, numFloats*sizeof(float));
    
    return result;
}

// Convert string to vector<unsigned char>
std::vector<unsigned char> StringToUnsignedVector(const std::string& str) {  
    std::vector<unsigned char> vec(str.size());  
    for (size_t i = 0; i < str.size(); ++i) {  
        vec[i] = static_cast<unsigned char>(str[i]);  
    }  
    return vec;  
}  

std::vector<unsigned char> FloatToUnsignedVector(const float &value) {
    std::vector<unsigned char> result(sizeof(float));
    std::memcpy(result.data(), &value, sizeof(float));
    return result;
}

float UnsignedVectorToFloat(const std::vector<unsigned char>& vec) {
    float result;
    if (vec.size() != sizeof(float)) {
        throw std::invalid_argument("Vector size must be equal to size of float.");
    }
    std::memcpy(&result, vec.data(), sizeof(float));
    return result;
}

float UnsignedVectorToFloat(const std::vector<unsigned char>& vec, const size_t offset) {
    float result;
    if (vec.size() < offset+sizeof(float)) {
        throw std::invalid_argument("Vector size must be larger than size of float.");
    }
    std::memcpy(&result, vec.data()+offset, sizeof(float));
    return result;
}

void PrintUnsignedVectorInHex(const std::vector<unsigned char>& vec, const std::string& var_name) {
    if (!var_name.empty())
        std::cout << var_name << " = ";
    size_t idx = 1; 
    for (unsigned char byte : vec) {  
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        ++idx;
    }  
    std::cout << std::dec << std::endl;
}  

#endif // UTIL_HPP