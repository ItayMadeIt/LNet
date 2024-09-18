#ifndef LNET_ENDIANNESS_HANDLER_HPP
#define LNET_ENDIANNESS_HANDLER_HPP

#include <cstdint>
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

        // Convert a 32-bit value to big-endian
        static uint32_t toBigEndian(uint32_t value)
        {
            if (bigEndian) {
                return value; // System is already big-endian, no conversion needed
            }
            else {
                return swapEndian(value); // Convert little-endian to big-endian
            }
        }

        // Convert a 32-bit value from big-endian to system's native endian
        static uint32_t fromBigEndian(uint32_t value)
        {
            if (bigEndian) {
                return value; // No conversion needed for big-endian systems
            }
            else {
                return swapEndian(value); // Convert big-endian to little-endian
            }
        }

    private:
        // Static const that stores whether the system is big-endian (computed once)
        static const bool bigEndian;

        // Function to swap endianness (convert between little and big endian)
        static uint32_t swapEndian(uint32_t value)
        {
            return ((value >> 24) & 0x000000FF) |
                ((value >> 8) & 0x0000FF00) |
                ((value << 8) & 0x00FF0000) |
                ((value << 24) & 0xFF000000);
        }

        // Static initializer to compute endianness once
        static bool detectEndianness()
        {
            uint32_t test = 1;
            return reinterpret_cast<const char*>(&test)[0] == 0;
        }
    };

    // Initialize the static const value
    const bool LNetEndiannessHandler::bigEndian = LNetEndiannessHandler::detectEndianness();
}
#endif 
