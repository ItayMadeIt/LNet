#ifndef LNET_ENDIANNESS_HANDLER_HPP
#define LNET_ENDIANNESS_HANDLER_HPP

#include <cstdint>
#include "LNetTypes.hpp"

namespace lnet
{
    class LNetEndiannessHandler
    {
    public:
        // Static function to check system's endianness (computed once)
        static bool isBigEndian()
        {
            return bigEndian;
        }

        // Convert a 32-bit value from system to little endian
        static LNet4Byte toNetworkEndian(LNet4Byte value)
        {
            if (bigEndian) {
                return swapEndian(value); // Convert little-endian to big-endian
            }
            else {
                return value; // System is already little-endian, no conversion needed
            }
        }

        // Convert a 32-bit value from little endian to system
        static LNet4Byte fromNetworkEndian(LNet4Byte value)
        {
            if (bigEndian) {
                return swapEndian(value); // Convert little-endian to big-endian
            }
            else {
                return value; // No conversion needed for little-endian systems
            }
        }
        // Overload function for network endian
        static LNetByte toNetworkEndian(LNetByte value)
        {
            // we return the value as 1 byte, no shuffling
            return value;
        }
        // Overload function for network endian
        static LNetByte fromNetworkEndian(LNetByte value)
        {
            // we return the value as 1 byte, no shuffling
            return value;
        }

    private:
        // Static const that stores whether the system is big-endian (computed once)
        static const bool bigEndian;

        // Function to swap endianness (convert between little and big endian)
#if defined(__GNUC__) || defined(__clang__)
        static const LNet4Byte swapEndian(LNet4Byte value)
        {
            return __builtin_bswap32(value);  // GCC / Clang intrinsic
        }
#elif defined(_MSC_VER)
        static const LNet4Byte swapEndian(LNet4Byte value)
        {
            return _byteswap_ulong(value);  // MSVC intrinsic
        }
#else
// Fallback to manual swapping for other compilers
        static const LNet4Byte swapEndian(LNet4Byte value)
        {
            return ((value >> 24) & 0x000000FF) |
                ((value >> 8) & 0x0000FF00) |
                ((value << 8) & 0x00FF0000) |
                ((value << 24) & 0xFF000000);
        }
#endif


        // Static initializer to compute endianness once
        static bool detectEndianness()
        {
            LNet4Byte test = 1;
            return reinterpret_cast<const char*>(&test)[0] == 0;
        }
    };

    // Initialize the static const value
    const bool LNetEndiannessHandler::bigEndian = LNetEndiannessHandler::detectEndianness();
}
#endif 
