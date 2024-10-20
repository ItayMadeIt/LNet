#include "LNetEndianHandler.hpp"

namespace lnet
{
    // Initialize the static const value
    const bool LNetEndiannessHandler::bigEndian = LNetEndiannessHandler::isSystemBigEndian();

    // Static function to check system's endianness (computed once)

    bool LNetEndiannessHandler::isBigEndian()
    {
        return bigEndian;
    }

    // Static initializer to compute whether the system is big endian

    bool LNetEndiannessHandler::isSystemBigEndian()
    {
        // 2 byte values
        uint16_t test = 1;
        return reinterpret_cast<const char*>(&test)[0] == 0;
    }


}