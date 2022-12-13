#include "dpf.h"
#include "Defines.h"
#include "PRNG.h"
#include "AES.h"
#include "Log.h"
#include <iostream>
#include <cassert>
#include<cmath>
#include <stdlib.h>
#include <immintrin.h>
#include <chrono>
#include "utils.h"
#include <emmintrin.h>

const int FIELD_ORDER = 2^31 - 1;

namespace DPF {
    namespace prg {
        inline block getL(const block& seed) {
            return mAesFixedKey.encryptECB_MMO(seed);
        }

        inline block getR(const block& seed) {
            return mAesFixedKey2.encryptECB_MMO(seed);
        }
        inline std::array<block,8> getL8(const std::array<block,8>& seed) {
            std::array<block,8> out;
            mAesFixedKey.encryptECB_MMO_Blocks(seed.data(), 8, out.data());
            return out;
        }

        inline std::array<block,8> getR8(const std::array<block,8>& seed) {
            std::array<block,8> out;
            mAesFixedKey2.encryptECB_MMO_Blocks(seed.data(), 8, out.data());
            return out;
        }
    }
    inline block clr(block in) {
        return in & ~MSBBlock;
    }
    inline bool getT(block in) {
        return !is_zero(in & MSBBlock);
    }
    inline void clr8(std::array<block, 8>& in) {
        for(int i = 0; i < 8; i++) {
            in[i] &= ~MSBBlock;
        }
    }
    inline std::array<uint8_t,8> getT8(const std::array<block,8>& in) {
        std::array<uint8_t,8> out;
        for(int i = 0; i < 8; i++) {
            out[i] = !is_zero(in[i] & MSBBlock);
        }
        return out;
    }
    inline bool ConvertBit(block in) {
        return !is_zero(in & LSBBlock);
    }
    inline block ConvertBlock(block in) {
        return mAesFixedKey.encryptECB(in);
    }

    inline std::array<block,8> ConvertBlock8(const std::array<block,8>& in) {
        std::array<block,8> out;
        mAesFixedKey.encryptECBBlocks(in.data(), 8, out.data());
        return out;
    }

    // These also convert "down" to the field of interest (31-bit prime)
    inline block ConvertBlockField(block in) {
        block blk = ConvertBlock(in);
        return _mm_and_si128(blk, PP_block); // NOTE: probably can safely just take the first 31bits here
//        return modmersenne31block(blk);
    }

    inline std::array<block,8> ConvertBlock8Field(const std::array<block,8>& in) {
        auto res = ConvertBlock8(in);
        for (int i = 0; i < 8; i++) {
//            res[i] = modmersenne31block(res[i]);
            res[i] = _mm_and_si128(res[i], PP_block); // NOTE: probably can safely just take the first 31bits here
        }
        return res;
    }

    block generate_random_128bit_number()
    {
        // Generate four random 32-bit numbers using the rand() function
        uint32_t r1 = rand();
        uint32_t r2 = rand();
        uint32_t r3 = rand();
        uint32_t r4 = rand();

        // Combine the four 32-bit numbers into a 128-bit number
        block result = _mm_set_epi32(r1, r2, r3, r4);

        // Return the generated number
        return result;
    }

    std::pair<std::vector<uint8_t>, std::vector<uint8_t>> Gen(size_t alpha, size_t logn) {
        assert(logn <= 63);
        assert(alpha < (1<<logn));
        std::vector<uint8_t> ka, kb, CW;
//        PRNG p = PRNG::getTestPRNG();
        PRNG p = PRNG(generate_random_128bit_number()); // NOTE: TestPRNG is deterministic and hard to combine DPFs that way
        block s0, s1;
        uint8_t t0, t1;
        p.get((uint8_t *) &s0, sizeof(s0));
        p.get((uint8_t *) &s1, sizeof(s1));
        t0 = getT(s0);
        t1 = !t0;

        s0 = clr(s0);
        s1 = clr(s1);

        ka.insert(ka.end(), (uint8_t*)&s0, ((uint8_t*)&s0) + sizeof(s0));
        ka.push_back(t0);
        kb.insert(kb.end(), (uint8_t*)&s1, ((uint8_t*)&s1) + sizeof(s1));
        kb.push_back(t1);
//        std::cout << ka.hex() << std::endl;
//        std::cout << kb.hex() << std::endl;
        size_t stop = logn >=7 ? logn - 7 : 0; // pack 7 layers in final CW
        for(size_t i = 0; i < stop; i++) {
            Log::v("gen", "%d, %d", t0, t1);
            Log::v("gen", s0);
            Log::v("gen", s1);

            block s0L = prg::getL(s0);
            uint8_t t0L = getT(s0L);
            s0L = clr(s0L);
            block s0R = prg::getR(s0);
            uint8_t t0R = getT(s0R);
            s0R = clr(s0R);

            block s1L = prg::getL(s1);
            uint8_t t1L = getT(s1L);
            s1L = clr(s1L);
            block s1R = prg::getR(s1);
            uint8_t t1R = getT(s1R);
            s1R = clr(s1R);

            if(alpha & (1ULL << (logn-1-i))) {
                //KEEP = R, LOSE = L
                block scw = s0L ^ s1L;
                uint8_t tLCW = t0L ^ t1L;
                uint8_t tRCW = t0R ^ t1R ^ 1;
                CW.insert(CW.end(), (uint8_t*)&scw, ((uint8_t*)&scw) + sizeof(scw));
                CW.push_back(tLCW);
                CW.push_back(tRCW);

                s0 = s0R;
                if(t0) s0 =  s0 ^ scw;
                s1 = s1R;
                if(t1) s1 =  s1 ^ scw;
                // new t
                if(t0) t0 = t0R ^ tRCW;
                else   t0 = t0R;
                if(t1) t1 = t1R ^ tRCW;
                else   t1 = t1R;
            }
            else {
                //KEEP = L, LOSE = R
                block scw = s0R ^ s1R;
                uint8_t tLCW = t0L ^ t1L ^ 1;
                uint8_t tRCW = t0R ^ t1R;
                CW.insert(CW.end(), (uint8_t*)&scw, ((uint8_t*)&scw) + sizeof(scw));
                CW.push_back(tLCW);
                CW.push_back(tRCW);
                //new s
                s0 = s0L;
                if(t0) s0 =  s0 ^ scw;
                s1 = s1L;
                if(t1) s1 =  s1 ^ scw;
                // new t
                if(t0) t0 = t0L ^ tLCW;
                else   t0 = t0L;
                if(t1) t1 = t1L ^ tLCW;
                else   t1 = t1L;
            }

        }
        reg_arr_union tmp = {ZeroBlock};
        tmp.arr[(alpha&127)/8] = (uint8_t)(1U<<((alpha&127)%8));
        tmp.reg = tmp.reg ^ ConvertBlock(s0) ^ ConvertBlock(s1);
        CW.insert(CW.end(), (uint8_t*)&tmp.reg, ((uint8_t*)&tmp.reg) + sizeof(tmp.reg));
        ka.insert(ka.end(), CW.begin(), CW.end());
        kb.insert(kb.end(), CW.begin(), CW.end());

        return std::make_pair(ka, kb);
    }

    std::pair<std::vector<uint8_t>, std::vector<uint8_t>> GenM(size_t alpha, size_t logn, uint32_t msg) {
        assert(logn <= 63);
        assert(alpha < (1<<logn));
        std::vector<uint8_t> ka, kb, CW;
//        PRNG p = PRNG::getTestPRNG();
        PRNG p = PRNG(generate_random_128bit_number()); // NOTE: TestPRNG is deterministic and hard to combine DPFs that way
        block s0, s1;
        uint8_t t0, t1;
        p.get((uint8_t *) &s0, sizeof(s0));
        p.get((uint8_t *) &s1, sizeof(s1));
        t0 = getT(s0);
        t1 = !t0;

        s0 = clr(s0);
        s1 = clr(s1);

        ka.insert(ka.end(), (uint8_t*)&s0, ((uint8_t*)&s0) + sizeof(s0));
        ka.push_back(t0);
        kb.insert(kb.end(), (uint8_t*)&s1, ((uint8_t*)&s1) + sizeof(s1));
        kb.push_back(t1);
//        std::cout << ka.hex() << std::endl;
//        std::cout << kb.hex() << std::endl;
        size_t stop = logn >=2 ? logn - 2 : 0; // pack 2 layers in final CW
        for(size_t i = 0; i < stop; i++) {
            Log::v("gen", "%d, %d", t0, t1);
            Log::v("gen", s0);
            Log::v("gen", s1);

            block s0L = prg::getL(s0);
            uint8_t t0L = getT(s0L);
            s0L = clr(s0L);
            block s0R = prg::getR(s0);
            uint8_t t0R = getT(s0R);
            s0R = clr(s0R);

            block s1L = prg::getL(s1);
            uint8_t t1L = getT(s1L);
            s1L = clr(s1L);
            block s1R = prg::getR(s1);
            uint8_t t1R = getT(s1R);
            s1R = clr(s1R);

            if(alpha & (1ULL << (logn-1-i))) {
                //KEEP = R, LOSE = L
                block scw = s0L ^ s1L;
                uint8_t tLCW = t0L ^ t1L;
                uint8_t tRCW = t0R ^ t1R ^ 1;
                CW.insert(CW.end(), (uint8_t*)&scw, ((uint8_t*)&scw) + sizeof(scw));
                CW.push_back(tLCW);
                CW.push_back(tRCW);

                s0 = s0R;
                if(t0) s0 =  s0 ^ scw;
                s1 = s1R;
                if(t1) s1 =  s1 ^ scw;
                // new t
                if(t0) t0 = t0R ^ tRCW;
                else   t0 = t0R;
                if(t1) t1 = t1R ^ tRCW;
                else   t1 = t1R;
            }
            else {
                //KEEP = L, LOSE = R
                block scw = s0R ^ s1R;
                uint8_t tLCW = t0L ^ t1L ^ 1;
                uint8_t tRCW = t0R ^ t1R;
                CW.insert(CW.end(), (uint8_t*)&scw, ((uint8_t*)&scw) + sizeof(scw));
                CW.push_back(tLCW);
                CW.push_back(tRCW);
                //new s
                s0 = s0L;
                if(t0) s0 =  s0 ^ scw;
                s1 = s1L;
                if(t1) s1 =  s1 ^ scw;
                // new t
                if(t0) t0 = t0L ^ tLCW;
                else   t0 = t0L;
                if(t1) t1 = t1L ^ tLCW;
                else   t1 = t1L;
            }

        }
        reg_arr_union tmp = {ZeroBlock};
        reg_arr_union tmp2 = {ZeroBlock};
        reg_arr_union s1f = {ZeroBlock};
        reg_arr_union s0f = {ZeroBlock};

        uint64_t arr[4] = {0, 0, 0, 0};
        arr[alpha % 4] = msg;

        // Vectorized code - not very important since Gen is fast anyway..
//        tmp.reg = _mm_set_epi32(arr[3], arr[2], arr[1], arr[0]);
//        tmp.reg = tmp.reg ^ ConvertBlockField(s0) ^ ConvertBlockField(s1);

//        tmp2.reg = _mm_sub_epi32(ConvertBlockField(s1), ConvertBlockField(s0));
//        tmp.reg = _mm_add_epi32(tmp.reg, tmp2.reg);

        // TODO: vectorize?
        s0f.reg = ConvertBlockField(s0);
        s1f.reg = ConvertBlockField(s1);

        for (int i = 0; i < 4; i++) {
            tmp2.arr32[i] = modmersenne31safe64(arr[i] - s0f.arr32[i] + s1f.arr32[i]); // Can also overflow here (2 additions.. so need to reduce mod field)

            if (t1) {
                tmp2.arr32[i] = modmersenne31safe64( (-1L) * tmp2.arr32[i] ); // TODO: key to vectorize this. Can be done with bit ops..
            }
        }

        CW.insert(CW.end(), (uint8_t*)&tmp2.reg, ((uint8_t*)&tmp2.reg) + sizeof(tmp2.reg));
        ka.insert(ka.end(), CW.begin(), CW.end());
        kb.insert(kb.end(), CW.begin(), CW.end());

        return std::make_pair(ka, kb);
    }

    // This allows merging multiple 1-bit DPFs into a single one in parallel.
    // While promising for parallelism of DPF executions, it is 10x slower because of the memory copies/bit operations
    // So not used for now. A direction to expand this is to instead of combining 1-bit DPFs, to combine 8-bit DPFs.
    // The natural byte alignment may achieve the best of both worlds - allow some parallelism while not having expensive
    // post-processing
    // NOTE: incomplete, simply replicates 31 DPFs together..
    std::pair<std::array<std::vector<uint8_t>, 31>, std::array<std::vector<uint8_t>, 31>> GenM1bit(size_t alpha, size_t logn, int32_t msg) {
        std::array<std::vector<uint8_t>, 31> ka, kb;

        // TODO: fix this. Currently, returns dummy 31 'ones' DPFs..
        for (int i = 0; i < 31; i++) {
            auto keys = DPF::Gen(alpha, logn);
            ka[i] = keys.first;
            kb[i] = keys.second;
        }

        return std::make_pair(ka, kb);
    }

    bool Eval(const std::vector<uint8_t>& key, size_t x, size_t logn) {
        assert(logn <= 63);
        assert(x < (1<<logn));
        block s;
        memcpy(&s, key.data(), 16);
        uint8_t t = key.data()[16];
        size_t stop = logn >=7 ? logn - 7 : 0; // pack 7 layers in final CW
        for(size_t i = 0; i < stop; i++) {
            Log::v("eval", s);
            Log::v("eval", "t: %d", t);
            block sL = prg::getL(s);
            uint8_t tL = getT(sL);
            sL = clr(sL);
            block sR = prg::getR(s);
            uint8_t tR = getT(sR);
            sR = clr(sR);
            if(t) {
                block sCW;
                memcpy(&sCW, key.data() + 17 + i*18, 16);
                uint8_t tLCW = key.data()[17+i*18+16];
                uint8_t tRCW = key.data()[17+i*18+17];
                Log::v("eval", "tcw %d %d", tLCW, tRCW);
                tL^=tLCW;
                tR^=tRCW;
                sL^=sCW;
                sR^=sCW;
            }
            if(x & (1ULL<<(logn-1-i))) {
                s = sR;
                t = tR;
            } else {
                s = sL;
                t = tL;
            }
        }
        Log::v("evalfin", s);
        if(t) {
            reg_arr_union tmp;
            reg_arr_union CW;
            memcpy(CW.arr, key.data()+key.size()-16, 16);
            tmp.reg = CW.reg ^ ConvertBlock(s);
            return (tmp.arr[(x&127)/8] & (1UL << ((x&127)%8))) != 0;
        }
        else {
            reg_arr_union tmp;
            tmp.reg = ConvertBlock(s);
            return (tmp.arr[(x&127)/8] & (1UL << ((x&127)%8))) != 0;
        }
    }

    void EvalFullRecursive(const std::vector<uint8_t>& key, block s, uint8_t t, size_t lvl, size_t stop, std::vector<uint8_t>& res) {
        if(lvl == stop) {
            if(t) {
                reg_arr_union tmp;
                reg_arr_union CW;
                memcpy(CW.arr, key.data()+key.size()-16, 16);
                tmp.reg = CW.reg ^ ConvertBlock(s);
                res.insert(res.end(), &tmp.arr[0], &tmp.arr[16]);
            }
            else {
                reg_arr_union tmp;
                tmp.reg = ConvertBlock(s);
                res.insert(res.end(), &tmp.arr[0], &tmp.arr[16]);
            }
            return;
        }
        block sL = prg::getL(s);
        uint8_t tL = getT(sL);
        sL = clr(sL);
        block sR = prg::getR(s);
        uint8_t tR = getT(sR);
        sR = clr(sR);
        if(t) {
            block sCW;
            memcpy(&sCW, key.data() + 17 + lvl*18, 16);
//            block* sCW = (block*) key.data() + 17 + lvl*18;
            uint8_t tLCW = key.data()[17+lvl*18+16];
            uint8_t tRCW = key.data()[17+lvl*18+17];
            tL^=tLCW;
            tR^=tRCW;
            sL^=sCW;
            sR^=sCW;
        }
        Log::v("-sL", sL);
        EvalFullRecursive(key, sL, tL, lvl+1, stop, res);
        Log::v("-sR", sR);
        EvalFullRecursive(key, sR, tR, lvl+1, stop, res);
    }

    std::vector<uint8_t> EvalFull(const std::vector<uint8_t>& key, size_t logn) {
        assert(logn <= 63);
        std::vector<uint8_t> data;
        if(logn >= 7)
            data.reserve(1ULL << (logn-3));
        block s;
        memcpy(&s, key.data(), 16);
        uint8_t t = key.data()[16];
        size_t stop = logn >=7 ? logn - 7 : 0; // pack 7 layers in final CW
        EvalFullRecursive(key, s, t, 0, stop, data);
        return data;
    }

    // This allows merging multiple 1-bit DPFs into a single one in parallel.
    // While promising for parallelism of DPF executions, it is 10x slower because of the memory copies/bit operations
    // So not used for now. A direction to expand this is to instead of combining 1-bit DPFs, to combine 8-bit DPFs.
    // The natural byte alignment may achieve the best of both worlds - allow some parallelism while not having expensive
    // post-processing
    std::vector<int32_t> EvalFullM1bit(const std::array<std::vector<uint8_t>, 31>& key, size_t logn) {
        std::vector<int32_t> vec;
        assert(logn >= 10);

        auto time1 = std::chrono::high_resolution_clock::now();
        std::array<std::vector<uint8_t>,31> singlebit_dpfs;
        // Expand all 1-bit DPFs
        for (int i = 0; i < 31; i++) {
            singlebit_dpfs[i] = EvalFull8(key[i], logn);
        }
        auto time2 = std::chrono::high_resolution_clock::now();

//        uint64_t N = std::pow(2, logn);
        uint64_t N_bytes = singlebit_dpfs[0].size();
        for (int i = 0; i < N_bytes; i++) { //TODO: benchmark and improve..?
            std::array<int32_t,8> tmp = {0, 0, 0, 0, 0, 0, 0, 0};

            for (int j = 0; j < 31; j++) {
                uint8_t b = singlebit_dpfs[j][i];
                bool b0 = b & 1;
                bool b1 = b & 2;
                bool b2 = b & 4;
                bool b3 = b & 8;
                bool b4 = b & 16;
                bool b5 = b & 32;
                bool b6 = b & 64;
                bool b7 = b & 128;

                tmp[0] |= (b0 << j);
                tmp[1] |= (b1 << j);
                tmp[2] |= (b2 << j);
                tmp[3] |= (b3 << j);
                tmp[4] |= (b4 << j);
                tmp[5] |= (b5 << j);
                tmp[6] |= (b6 << j);
                tmp[7] |= (b7 << j);
            }

            for (int j = 0; j < 8; j++) {
                vec.push_back(tmp[j]);
            }
        }
        auto time3 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> evalT1, evalT2;
        evalT1 = evalT2 = std::chrono::duration<double>::zero();
        evalT1 += time2 - time1;
        evalT2 += time3 - time2;

        std::cout << "EVAL part 1 "       << evalT1.count() << "sec" << std::endl;
        std::cout << "EVAL part 2 "     << evalT2.count() << "sec" << std::endl;

        return vec;
    }

    // optimized for vectorized ops
    void EvalFullRecursive8(const std::vector<uint8_t>& key, std::array<block, 8>& s, std::array<uint8_t,8>& t, size_t lvl, size_t stop, std::array<uint8_t*,8>& res) {
        if(lvl == stop) {
            std::array<reg_arr_union,8> tmp;
            reg_arr_union CW;
            memcpy(CW.arr, key.data() + key.size() - 16, 16);
            std::array<block, 8> conv =  ConvertBlock8(s);
            for (int i = 0; i < 8; i++) {
                block tt = _mm_set1_epi8(-(t[i]));
                tmp[i].reg = conv[i] ^ (CW.reg & tt);
                memcpy(res[i], tmp[i].arr, 16); // This copies 128 bits --> 128 elements condensed.. since this is 1 bit
                res[i] += sizeof(block);
            }
            return;
        }
        std::array<block,8> sL = prg::getL8(s);
        std::array<uint8_t,8> tL = getT8(sL);
        clr8(sL);
        std::array<block,8> sR = prg::getR8(s);
        std::array<uint8_t,8> tR = getT8(sR);
        clr8(sR);
        block sCW;
        memcpy(&sCW, key.data() + 17 + lvl*18, 16);
        uint8_t tLCW = key.data()[17+lvl*18+16];
        uint8_t tRCW = key.data()[17+lvl*18+17];
        for(int i = 0; i < 8; i++) {
            tL[i] ^= (tLCW & t[i]);
            tR[i] ^= (tRCW & t[i]);
            block tt = _mm_set1_epi8(-(t[i]));
            sL[i] ^= (sCW & tt);
            sR[i] ^= (sCW & tt);
        }
        EvalFullRecursive8(key, sL, tL, lvl+1, stop, res);
        EvalFullRecursive8(key, sR, tR, lvl+1, stop, res);
    }

    std::vector<uint32_t> EvalFull8M(const std::vector<uint8_t>& key, size_t logn, bool party_index) {
        assert(logn <= 63);
        std::vector<uint32_t> data;
        data.resize( (1ULL << logn) );
        std::array<uint32_t*,8> data_ptrs;
        for(size_t i = 0; i < 8; i++) {
            data_ptrs[i] = &data[i*(1ULL << (logn - 3))]; // since we start by running 8 subtrees, each data_ptr handles a single subtree. This is likely needed regardless how we condense the levels
        }
        block s;
        memcpy(&s, key.data(), 16);
        uint8_t t = key.data()[16];
        size_t stop = logn >=2 ? logn - 2 : 0; // pack 2 layers in final CW
        assert(stop >= 3); // need 3 or more layers for this to make sense
        // evaluate first 3 layers
        size_t lvl = 0;
        block sL = prg::getL(s);
        uint8_t tL = getT(sL);
        sL = clr(sL);
        block sR = prg::getR(s);
        uint8_t tR = getT(sR);
        sR = clr(sR);
        if(t) {
            block sCW;
            memcpy(&sCW, key.data() + 17 + lvl*18, 16);
            uint8_t tLCW = key.data()[17+lvl*18+16];
            uint8_t tRCW = key.data()[17+lvl*18+17];
            tL^=tLCW;
            tR^=tRCW;
            sL^=sCW;
            sR^=sCW;
        }

        lvl = 1;
        block sLL = prg::getL(sL);
        uint8_t tLL = getT(sLL);
        sLL = clr(sLL);
        block sRL = prg::getR(sL);
        uint8_t tRL = getT(sRL);
        sRL = clr(sRL);
        if(tL) {
            block sCW;
            memcpy(&sCW, key.data() + 17 + lvl*18, 16);
            uint8_t tLCW = key.data()[17+lvl*18+16];
            uint8_t tRCW = key.data()[17+lvl*18+17];
            tLL^=tLCW;
            tRL^=tRCW;
            sLL^=sCW;
            sRL^=sCW;
        }
        block sLR = prg::getL(sR);
        uint8_t tLR = getT(sLR);
        sLR = clr(sLR);
        block sRR = prg::getR(sR);
        uint8_t tRR = getT(sRR);
        sRR = clr(sRR);
        if(tR) {
            block sCW;
            memcpy(&sCW, key.data() + 17 + lvl*18, 16);
            uint8_t tLCW = key.data()[17+lvl*18+16];
            uint8_t tRCW = key.data()[17+lvl*18+17];
            tLR^=tLCW;
            tRR^=tRCW;
            sLR^=sCW;
            sRR^=sCW;
        }

        lvl = 2;
        block sLLL = prg::getL(sLL);
        uint8_t tLLL = getT(sLLL);
        sLLL = clr(sLLL);
        block sRLL = prg::getR(sLL);
        uint8_t tRLL = getT(sRLL);
        sRLL = clr(sRLL);
        if(tLL) {
            block sCW;
            memcpy(&sCW, key.data() + 17 + lvl*18, 16);
            uint8_t tLCW = key.data()[17+lvl*18+16];
            uint8_t tRCW = key.data()[17+lvl*18+17];
            tLLL^=tLCW;
            tRLL^=tRCW;
            sLLL^=sCW;
            sRLL^=sCW;
        }
        block sLRL = prg::getL(sRL);
        uint8_t tLRL = getT(sLRL);
        sLRL = clr(sLRL);
        block sRRL = prg::getR(sRL);
        uint8_t tRRL = getT(sRRL);
        sRRL = clr(sRRL);
        if(tRL) {
            block sCW;
            memcpy(&sCW, key.data() + 17 + lvl*18, 16);
            uint8_t tLCW = key.data()[17+lvl*18+16];
            uint8_t tRCW = key.data()[17+lvl*18+17];
            tLRL^=tLCW;
            tRRL^=tRCW;
            sLRL^=sCW;
            sRRL^=sCW;
        }
        block sLLR = prg::getL(sLR);
        uint8_t tLLR = getT(sLLR);
        sLLR = clr(sLLR);
        block sRLR = prg::getR(sLR);
        uint8_t tRLR = getT(sRLR);
        sRLR = clr(sRLR);
        if(tLR) {
            block sCW;
            memcpy(&sCW, key.data() + 17 + lvl*18, 16);
            uint8_t tLCW = key.data()[17+lvl*18+16];
            uint8_t tRCW = key.data()[17+lvl*18+17];
            tLLR^=tLCW;
            tRLR^=tRCW;
            sLLR^=sCW;
            sRLR^=sCW;
        }
        block sLRR = prg::getL(sRR);
        uint8_t tLRR = getT(sLRR);
        sLRR = clr(sLRR);
        block sRRR = prg::getR(sRR);
        uint8_t tRRR = getT(sRRR);
        sRRR = clr(sRRR);
        if(tRR) {
            block sCW;
            memcpy(&sCW, key.data() + 17 + lvl*18, 16);
            uint8_t tLCW = key.data()[17+lvl*18+16];
            uint8_t tRCW = key.data()[17+lvl*18+17];
            tLRR^=tLCW;
            tRRR^=tRCW;
            sLRR^=sCW;
            sRRR^=sCW;
        }
        std::array<block, 8> s_array{sLLL, sRLL, sLRL, sRRL, sLLR, sRLR, sLRR, sRRR};
        std::array<uint8_t, 8> t_array{tLLL, tRLL, tLRL, tRRL, tLLR, tRLR, tLRR, tRRR};

//        reg_arr_union CW;
//        memcpy(CW.arr, key.data() + key.size() - 16, 16);
        EvalFullRecursive8M(key, s_array, t_array, 3, stop, data_ptrs, nullptr, party_index);
        return data;
    }


    // optimized for vectorized ops
    void EvalFullRecursive8M(const std::vector<uint8_t>& key, std::array<block, 8>& s, std::array<uint8_t,8>& t, size_t lvl, size_t stop, std::array<uint32_t*,8>& res, block *CW, bool party_index) {
        if(lvl == stop) {

            std::array<reg_arr_union,8> tmp;
            reg_arr_union CW2, tmp2, tmp3;
            memcpy(CW2.arr, key.data() + key.size() - 16, 16);
//            reg_arr_union256 tmp256;
            std::array<block, 8> conv =  ConvertBlock8Field(s);
            for (int i = 0; i < 8; i++) {

                // TODO: vectorized code?
                block tt = _mm_set1_epi8(-(t[i]) );
                tt = _mm_and_si128(tt, PP_block);

                tmp[i].reg = modmersenne31block(_mm_add_epi32(conv[i], (CW2.reg & tt) ));
                tmp2.reg = modmersenne31block(_mm_add_epi32(conv[i], (CW2.reg & tt) ));

                if (party_index) {
                    // Multiply by (-1) --> just taking a NOT? This way we avoid overflowing when we use multiplication.
                    tmp[i].reg = _mm_andnot_si128(tmp[i].reg, PP_block);
//                    tmp[i].reg = modmersenne31block(_mm_add_epi32(tmp[i].reg, ONES_block));

//                    std::cout << "tmp: " << tmp[i].arr32[3] << ", " << tmp[i].arr32[2] << ", " << tmp[i].arr32[1] << ", " << tmp[i].arr32[0] << std::endl;
                    // Hurts performance of party 1
//                    tmp2.reg = _mm_set_epi32(  // TODO: key to vectorize this. Can be done with bit ops..
//                            modmersenne31safe64( (-1L) * tmp2.arr32[3] ),
//                            modmersenne31safe64( (-1L) * tmp2.arr32[2] ),
//                            modmersenne31safe64( (-1L) * tmp2.arr32[1] ),
//                            modmersenne31safe64( (-1L) * tmp2.arr32[0] )
//                            );
//                    std::cout << "tmp2: " << tmp2.arr32[3] << ", " << tmp2.arr32[2] << ", " << tmp2.arr32[1] << ", " << tmp2.arr32[0] << std::endl;


                }

                memcpy(res[i], tmp[i].arr, 16); // This copies 128 bits --> 4 elements condensed.. since this is 32 bit

                // this expands 4 32bit numbers into 64bit ones. Don't need this for 31-bit mersenne field, but useful if we want to expand beyond.
//                unsigned char * dest = reinterpret_cast<unsigned char*>(res[i]);
//                memcpy(dest, tmp[i].arr, 4);
//                memset(dest+4, 0, 4);
//                memcpy(dest+8, tmp[i].arr + 4, 4);
//                memset(dest+4, 0, 4);
//                memcpy(dest+16, tmp[i].arr + 8, 4);
//                memset(dest+4, 0, 4);
//                memcpy(dest+24, tmp[i].arr + 12, 4);
//                memset(dest+4, 0, 4);
                // END

                res[i] += 4;
            }
            return;
        }
        std::array<block,8> sL = prg::getL8(s);
        std::array<uint8_t,8> tL = getT8(sL);
        clr8(sL);
        std::array<block,8> sR = prg::getR8(s);
        std::array<uint8_t,8> tR = getT8(sR);
        clr8(sR);
        block sCW;
        memcpy(&sCW, key.data() + 17 + lvl*18, 16);
        uint8_t tLCW = key.data()[17+lvl*18+16];
        uint8_t tRCW = key.data()[17+lvl*18+17];
        for(int i = 0; i < 8; i++) {
            tL[i] ^= (tLCW & t[i]);
            tR[i] ^= (tRCW & t[i]);
            block tt = _mm_set1_epi8(-(t[i]));
            sL[i] ^= (sCW & tt);
            sR[i] ^= (sCW & tt);
        }
        EvalFullRecursive8M(key, sL, tL, lvl+1, stop, res, CW, party_index);
        EvalFullRecursive8M(key, sR, tR, lvl+1, stop, res, CW, party_index);
    }

    std::vector<uint8_t> EvalFull8(const std::vector<uint8_t>& key, size_t logn) {
        assert(logn <= 63);
        std::vector<uint8_t> data;
        data.resize(1ULL << (logn - 3)); // Since data is a byte-array and each byte has 8 DPF values, the size of data needs to be N/8. This isn't relevant in non-1 bit most likely.
        std::array<uint8_t*,8> data_ptrs;
        for(size_t i = 0; i < 8; i++) {
            data_ptrs[i] = &data[i*(1ULL << (logn - 3 - 3))]; // since we start by running 8 subtrees, each data_ptr handles a single subtree. This is likely needed regardless how we condense the levels
        }
        block s;
        memcpy(&s, key.data(), 16);
        uint8_t t = key.data()[16];
        size_t stop = logn >=7 ? logn - 7 : 0; // pack 7 layers in final CW
        assert(stop >= 3); // need 3 or more layers for this to make sense
        // evaluate first 3 layers
        size_t lvl = 0;
        block sL = prg::getL(s);
        uint8_t tL = getT(sL);
        sL = clr(sL);
        block sR = prg::getR(s);
        uint8_t tR = getT(sR);
        sR = clr(sR);
        if(t) {
            block sCW;
            memcpy(&sCW, key.data() + 17 + lvl*18, 16);
            uint8_t tLCW = key.data()[17+lvl*18+16];
            uint8_t tRCW = key.data()[17+lvl*18+17];
            tL^=tLCW;
            tR^=tRCW;
            sL^=sCW;
            sR^=sCW;
        }

        lvl = 1;
        block sLL = prg::getL(sL);
        uint8_t tLL = getT(sLL);
        sLL = clr(sLL);
        block sRL = prg::getR(sL);
        uint8_t tRL = getT(sRL);
        sRL = clr(sRL);
        if(tL) {
            block sCW;
            memcpy(&sCW, key.data() + 17 + lvl*18, 16);
            uint8_t tLCW = key.data()[17+lvl*18+16];
            uint8_t tRCW = key.data()[17+lvl*18+17];
            tLL^=tLCW;
            tRL^=tRCW;
            sLL^=sCW;
            sRL^=sCW;
        }
        block sLR = prg::getL(sR);
        uint8_t tLR = getT(sLR);
        sLR = clr(sLR);
        block sRR = prg::getR(sR);
        uint8_t tRR = getT(sRR);
        sRR = clr(sRR);
        if(tR) {
            block sCW;
            memcpy(&sCW, key.data() + 17 + lvl*18, 16);
            uint8_t tLCW = key.data()[17+lvl*18+16];
            uint8_t tRCW = key.data()[17+lvl*18+17];
            tLR^=tLCW;
            tRR^=tRCW;
            sLR^=sCW;
            sRR^=sCW;
        }

        lvl = 2;
        block sLLL = prg::getL(sLL);
        uint8_t tLLL = getT(sLLL);
        sLLL = clr(sLLL);
        block sRLL = prg::getR(sLL);
        uint8_t tRLL = getT(sRLL);
        sRLL = clr(sRLL);
        if(tLL) {
            block sCW;
            memcpy(&sCW, key.data() + 17 + lvl*18, 16);
            uint8_t tLCW = key.data()[17+lvl*18+16];
            uint8_t tRCW = key.data()[17+lvl*18+17];
            tLLL^=tLCW;
            tRLL^=tRCW;
            sLLL^=sCW;
            sRLL^=sCW;
        }
        block sLRL = prg::getL(sRL);
        uint8_t tLRL = getT(sLRL);
        sLRL = clr(sLRL);
        block sRRL = prg::getR(sRL);
        uint8_t tRRL = getT(sRRL);
        sRRL = clr(sRRL);
        if(tRL) {
            block sCW;
            memcpy(&sCW, key.data() + 17 + lvl*18, 16);
            uint8_t tLCW = key.data()[17+lvl*18+16];
            uint8_t tRCW = key.data()[17+lvl*18+17];
            tLRL^=tLCW;
            tRRL^=tRCW;
            sLRL^=sCW;
            sRRL^=sCW;
        }
        block sLLR = prg::getL(sLR);
        uint8_t tLLR = getT(sLLR);
        sLLR = clr(sLLR);
        block sRLR = prg::getR(sLR);
        uint8_t tRLR = getT(sRLR);
        sRLR = clr(sRLR);
        if(tLR) {
            block sCW;
            memcpy(&sCW, key.data() + 17 + lvl*18, 16);
            uint8_t tLCW = key.data()[17+lvl*18+16];
            uint8_t tRCW = key.data()[17+lvl*18+17];
            tLLR^=tLCW;
            tRLR^=tRCW;
            sLLR^=sCW;
            sRLR^=sCW;
        }
        block sLRR = prg::getL(sRR);
        uint8_t tLRR = getT(sLRR);
        sLRR = clr(sLRR);
        block sRRR = prg::getR(sRR);
        uint8_t tRRR = getT(sRRR);
        sRRR = clr(sRRR);
        if(tRR) {
            block sCW;
            memcpy(&sCW, key.data() + 17 + lvl*18, 16);
            uint8_t tLCW = key.data()[17+lvl*18+16];
            uint8_t tRCW = key.data()[17+lvl*18+17];
            tLRR^=tLCW;
            tRRR^=tRCW;
            sLRR^=sCW;
            sRRR^=sCW;
        }
        std::array<block, 8> s_array{sLLL, sRLL, sLRL, sRRL, sLLR, sRLR, sLRR, sRRR};
        std::array<uint8_t, 8> t_array{tLLL, tRLL, tLRL, tRRL, tLLR, tRLR, tLRR, tRRR};

        EvalFullRecursive8(key, s_array, t_array, 3, stop, data_ptrs);
        return data;
    }


//    std::vector<uint8_t> EvalFullNonRec(const std::vector<uint8_t>& key, size_t logn) {
//        assert(logn <= 63);
//        std::vector<uint8_t> data;
//        std::vector<block> sL_vals;
//        std::vector<block> sR_vals;
//        std::vector<int> tL_vals;
//        std::vector<int> tR_vals;
//        data.reserve(1ULL << (logn-3));
//        block s;
//        memcpy(&s, key.data(), 16);
//        uint8_t t = key.data()[16];
//        size_t stop = logn >=7 ? logn - 7 : 0; // pack 7 layers in final CW
//
//        for(size_t lvl = 0; lvl < stop; lvl++) {
//            const size_t layersize = (1 << lvl);
//            block sCW;
//            memcpy(&sCW, key.data() + 17 + lvl * 18, 16);
//            uint8_t tLCW = key.data()[17 + lvl * 18 + 16];
//            uint8_t tRCW = key.data()[17 + lvl * 18 + 17];
//            for(int j = 0; j < layersize; j++) {
//                block sL = prg::getL(s);
//                uint8_t tL = getT(sL);
//                sL = clr(sL);
//                block sR = prg::getR(s);
//                uint8_t tR = getT(sR);
//                sR = clr(sR);
//                if (t) {
//                    Log::v("eval", "tcw %d %d", tLCW, tRCW);
//                    tL ^= tLCW;
//                    tR ^= tRCW;
//                    sL ^= sCW;
//                    sR ^= sCW;
//                }
//            }
//        }
//
//        if(lvl == stop) {
//            if(t) {
//                reg_arr_union tmp;
//                reg_arr_union CW;
//                memcpy(CW.arr, key.data()+key.size()-16, 16);
//                tmp.reg = CW.reg ^ ConvertBlock(s);
//                res.insert(res.end(), &tmp.arr[0], &tmp.arr[16]);
//            }
//            else {
//                reg_arr_union tmp;
//                tmp.reg = ConvertBlock(s);
//                res.insert(res.end(), &tmp.arr[0], &tmp.arr[16]);
//            }
//            return;
//        }
//        return data;
//    }
}