//
// Created by ___ on 11/01/2024.
//

#ifndef DPFPIR_DEBUGPRINT_H
#define DPFPIR_DEBUGPRINT_H

#include <iostream>

// Define a macro for enabling or disabling debug output
// Uncomment the below line to enable debug output
// #define ENABLE_DEBUG

class DebugPrinter {
public:
    template<typename T>
    DebugPrinter& operator<<(const T& value) {
#ifdef ENABLE_DEBUG
        std::cout << value;
#endif
        return *this;
    }

    DebugPrinter& operator<<(std::ostream& (*pf)(std::ostream&)) {
#ifdef ENABLE_DEBUG
        std::cout << pf;
#endif
        return *this;
    }
};

extern DebugPrinter debugPrint;

#endif // DEBUG_PRINT_H