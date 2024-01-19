//
// Created by Guy Zyskind on 08/12/2022.
//

#ifndef DPFPIR_PIRW_H
#define DPFPIR_PIRW_H

#include <vector>
#include <cstdint>
#include "utils.h"

namespace PIRW {

    // Takes two vectors a, b, and returns a vector c which adds them together
    void addvff31(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b, std::vector<uint32_t>& c);

    // Takes two vectors a, b, and returns a vector c which subtracts them together
    void subvff31(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b, std::vector<uint32_t>& c);

    // Takes two vectors a, b, and returns a scalar c which is the inner product of the two
    uint32_t innerprodff31(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b);
    uint32_t innerprodff31v(const uint32_t* a_start, const uint32_t* a_end, const uint32_t* b_start, const uint32_t* b_end);
    uint32_t innerprodff31v(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b);

    uint32_t sumvecff31(const std::vector<uint32_t>& a);

} // PIRW

#endif //DPFPIR_PIRW_H
