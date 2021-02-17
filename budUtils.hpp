#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>

namespace bud {

template<typename ErrType, ErrType expected>
inline void checkErrorCode(ErrType got, const std::string& errMessage)
{
    if (got != expected) throw std::runtime_error(errMessage);
}

inline std::string readCodeFromFile(const std::string& fileName)
{
    std::ifstream file(fileName);
    std::stringstream fileStream;
    fileStream << file.rdbuf();
    return fileStream.str();
}

inline const std::vector<char> readSpirvFromFile(const std::string& fileName)
{
    std::ifstream file(fileName, std::ios::ate | std::ios::binary);
    checkErrorCode<bool, true>(file.is_open(), "failed to open file!");

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    return buffer;
}

template<typename T>
inline T genRandomData(T top, T bottom)
{
    srand(0);
    return static_cast<T>(bottom + static_cast<double>(top - bottom) * std::abs(std::sin(static_cast<double>(rand()))));
}

}
