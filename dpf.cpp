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
#include "shamir.h"

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

        // Implementation of H() from the verifiable DPF paper.
        std::array<block, 4> hash1(const block& seed, uint32_t x) {
            std::array<block, 4> output;

            // Convert x to block type and XOR with seed to create 'v'
            block v = _mm_xor_si128(seed, _mm_set1_epi32(x));

            // Run encryption for i between [0, 3]
            for (int i = 0; i < 4; ++i) {
                block temp = _mm_add_epi32(v, _mm_set1_epi32(i)); // Add i to v
                output[i] = mAesFixedKey.encryptECB_MMO(temp); // Encrypt and store in output
            }

            return output;
        }

        // Implementation of H'() from the verifiable DPF paper.
        std::array<block, 2> hash2(const std::array<block, 4>& h) {
            std::array<block, 2> output;

            // XOR all 4 blocks in h
            block combined = _mm_xor_si128(h[0], h[1]);
            combined = _mm_xor_si128(combined, h[2]);
            combined = _mm_xor_si128(combined, h[3]);

            // Encrypt combined value
            output[0] = mAesFixedKey.encryptECB_MMO(combined);

            // Encrypt combined value + 1
            block combinedPlusOne = _mm_add_epi64(combined, _mm_set1_epi64x(1));
            output[1] = mAesFixedKey.encryptECB_MMO(combinedPlusOne);

            return output;
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

    struct GenResult {
        std::pair<std::vector<uint8_t>, std::vector<uint8_t>> keys;
        block s0;
        block s1;
        bool t0;
        bool t1;
    };

    GenResult GenM_helper(size_t alpha, size_t logn, uint32_t msg) {
        assert(logn <= 63);
        assert(alpha < (1<<logn));
        GenResult res;
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

                // NOTE: Always get the t0,t1 before last because they set whether to apply the correction word
                // TODO: may need to also get the last t0, t1 as before, for verifiability. Need to check..
                res.t0 = t0;
                res.t1 = t1;

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

                // NOTE: Always get the t0,t1 before last because they set whether to apply the correction word
                // TODO: may need to also get the last t0, t1 as before, for verifiability. Need to check..
                res.t0 = t0;
                res.t1 = t1;

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
        // TODO: change everything to 32bit?
        uint64_t arr[4] = {0, 0, 0, 0};
        arr[alpha % 4] = msg;
        for (int i = 0; i < 4; i++) {
            tmp.arr32[i] = arr[i];
        }

        tmp.reg = tmp.reg ^ ConvertBlock(s0) ^ ConvertBlock(s1);
        CW.insert(CW.end(), (uint8_t*)&tmp.reg, ((uint8_t*)&tmp.reg) + sizeof(tmp.reg));
        ka.insert(ka.end(), CW.begin(), CW.end());
        kb.insert(kb.end(), CW.begin(), CW.end());

        res.keys = std::make_pair(ka, kb);
        res.s0 = ConvertBlock(s0);
        res.s1 = ConvertBlock(s1);

//        res.t0 = (res.s0 & 0x01)[1];
//        res.t1 = (res.s1 & 0x01)[1];
        res.t0 = t0;
        res.t1 = t1;

        return res;
    }

    std::pair<std::vector<uint8_t>, std::vector<uint8_t>> GenM(size_t alpha, size_t logn, uint32_t msg) {
        auto genres = GenM_helper(alpha, logn, msg);
        return genres.keys;
    }

    std::array<block, 4> xorArrays(const std::array<block, 4>& array1, const std::array<block, 4>& array2) {
        std::array<block, 4> result;

        for (size_t i = 0; i < 4; ++i) {
            result[i] = _mm_xor_si128(array1[i], array2[i]);
        }

        return result;
    }

    std::pair<KeyShare, KeyShare> VerGenM(size_t alpha, size_t logn, uint32_t msg) {
        bool t0 = false;
        bool t1 = false;
        GenResult genres;

        while (t0 == t1) {
            genres = GenM_helper(alpha, logn, msg);
//            t0 = genres.t0;
//            t1 = genres.t1;
            t0 = (genres.s0 & 0x01)[1];
            t1 = (genres.s1 & 0x01)[1];

        }

        uint32_t alpha_offset = alpha - (alpha % 4);

        auto pi_tilde0 = prg::hash1(genres.s0, alpha_offset);
        auto pi_tilde1 = prg::hash1(genres.s1, alpha_offset);
        auto cs = xorArrays(pi_tilde0, pi_tilde1);
        KeyShare key0, key1;

        key0.key = genres.keys.first;
        key0.cs = cs;
        key0.z = 0;

        key1.key = genres.keys.second;
        key1.cs = cs;
        key1.z = 0;

        return std::make_pair(key0, key1);
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

    std::pair<std::vector<uint32_t>, std::vector<block>> EvalFull8M_helper(const std::vector<uint8_t>& key, size_t logn, bool party_index, bool verifiable) {
        assert(logn <= 63);
        std::vector<uint8_t> data, data_nodes;
        data.resize( 4*(1ULL << logn) );
        data_nodes.resize( 4*(1ULL << logn) );
        std::array<uint8_t*,8> data_ptrs;
        std::array<uint8_t*,8> data_ptrs_nodes;
        for(size_t i = 0; i < 8; i++) {
//            data_ptrs[i] = &data[i*(1ULL << (logn - 3 - 3))]; // since we start by running 8 subtrees, each data_ptr handles a single subtree. This is likely needed regardless how we condense the levels
            data_ptrs[i] = &data[4*i*(1ULL << (logn - 3))]; // since we start by running 8 subtrees, each data_ptr handles a single subtree. This is likely needed regardless how we condense the levels
            data_ptrs_nodes[i] = &data_nodes[4*i*(1ULL << (logn - 3))];
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

        EvalFullRecursive8M(key, s_array, t_array, 3, stop, data_ptrs, data_ptrs_nodes, nullptr, party_index, verifiable);

        const uint32_t* begin = reinterpret_cast<const uint32_t*>(data.data());
        const uint32_t* end = reinterpret_cast<const uint32_t*>(data.data() + data.size());

        const block* begin_nodes = reinterpret_cast<const block*>(data_nodes.data());
        const block* end_nodes = reinterpret_cast<const block*>(data_nodes.data() + data_nodes.size());

        auto v1 = std::vector<uint32_t>(begin, end);
        auto v2 = std::vector<block>(begin_nodes, end_nodes);
        return std::make_pair(
                v1, v2
        );
    }

    std::vector<uint32_t> EvalFull8M(const std::vector<uint8_t>& key, size_t logn, bool party_index) {
        auto res = EvalFull8M_helper(key, logn, party_index, false);
        return res.first;
    }

    std::pair<std::vector<uint32_t>, std::array<block, 4>> VerEvalFull8M(const KeyShare& key, size_t logn, bool party_index) {
        auto res = EvalFull8M_helper(key.key, logn, party_index, true);
        auto vm = res.first;
        auto nodes = res.second;
        std::array<block, 4> pi = key.cs;
        block s;
        bool t;

        // Hash it all! For integrity
        for (int i = 0; i < vm.size(); i += 4) {
            s = nodes[i/4];
            t = (s & 0x01)[1]; // Note: this isn't the t used to actually determine OCW usage. This might be a problem..
//                t = getT(s);
            uint32_t i_offset = i - (i % 4);

            std::array<block, 4> pi_tilde = prg::hash1(s, i_offset);

            auto corrected_pi_tilde = pi_tilde;
            for (int j=0; j<4; j++) {
                if (t) {
                    corrected_pi_tilde[j] = _mm_xor_si128(corrected_pi_tilde[j], key.cs[j]);
                }
                corrected_pi_tilde[j] = _mm_xor_si128(corrected_pi_tilde[j], pi[j]);
            }
            std::array<block, 2> h2_output = prg::hash2(corrected_pi_tilde);

            pi[0] = _mm_xor_si128(pi[0], h2_output[0]);
            pi[1] = _mm_xor_si128(pi[1], h2_output[1]);
            pi[2] = _mm_xor_si128(pi[2], h2_output[0]);
            pi[3] = _mm_xor_si128(pi[3], h2_output[1]);
        }

        return std::make_pair(
                res.first,
                pi
        );
    }

    // optimized for vectorized ops
    void EvalFullRecursive8M(const std::vector<uint8_t>& key, std::array<block, 8>& s, std::array<uint8_t,8>& t, size_t lvl, size_t stop, std::array<uint8_t*,8>& res, std::array<uint8_t*,8>& res_nodes, block *CW, bool party_index, bool verifiable) {
        if(lvl == stop) {
            std::array<reg_arr_union,8> tmp;
            reg_arr_union CW;
            memcpy(CW.arr, key.data() + key.size() - 16, 16);
            std::array<block, 8> conv =  ConvertBlock8(s);
            for (int i = 0; i < 8; i++) {
                // If verifiable, then get t from the last batch of s's.
                uint8_t t_i = t[i];
//                if (verifiable) {
//                    t_i = static_cast<uint8_t>(getT(conv[i]));
//                }
                block tt = _mm_set1_epi8(-(t_i));
                tmp[i].reg = conv[i] ^ (CW.reg & tt);
                memcpy(res[i], tmp[i].arr, 16); // This copies 128 bits --> 128 elements condensed.. since this is 1 bit
                res[i] += sizeof(block);

                if (verifiable) {
                    reg_arr_union tmp2;
                    tmp2.reg = conv[i];
//                    tmp2.reg = conv[i] ^ (CW.reg & tt);
                    memcpy(res_nodes[i], tmp2.arr, 16);
                    res_nodes[i] += sizeof(block);
                }
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
        EvalFullRecursive8M(key, sL, tL, lvl+1, stop, res, res_nodes, CW, party_index, verifiable);
        EvalFullRecursive8M(key, sR, tR, lvl+1, stop, res, res_nodes, CW, party_index, verifiable);
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

    // New DPF Constructions
    std::pair<KeyShare, KeyShare>
    GenP(size_t alpha, size_t logn, uint32_t m1, uint32_t m2, bool verifiable) {
        // TODO: pack values - not sure if needed actually? Go over this..
        uint32_t m = m1 ^ m2;
        KeyShare key0, key1;

        if (verifiable) {
            auto keys = VerGenM(alpha, logn, m);
            key0 = keys.first;
            key1 = keys.second;
        } else {
            auto keys = GenM(alpha, logn, m);
            key0.key = keys.first;
            key1.key = keys.second;
        }

        // TODO: Implement Eval8M for a single value..
        auto vm0 = DPF::EvalFull8M(key0.key, logn);
        uint32_t z = m1 ^ vm0[alpha];

        key0.z = z;
        key1.z = z;

        return std::make_pair(key0, key1);
    }

    std::pair<std::vector<uint32_t>, std::array<block, 4>> EvalFull8P(const KeyShare& key, size_t logn, bool party_index, bool verifiable) {
        uint32_t z = key.z;
        std::vector<uint32_t> vm;
        std::array<block, 4> pi;

        if (verifiable) {
            auto res = DPF::VerEvalFull8M(key, logn, party_index);
            vm = res.first;
            pi = res.second;
        } else {
            vm = DPF::EvalFull8M(key.key, logn, party_index);
        }

        // TODO: low priority - insert this into the recursive function instead of looping all values again. May help..
        for (int i = 0; i < vm.size(); i++) {
            vm[i] = z ^ vm[i];
        }

        return std::make_pair(vm, pi);
    }


    // Shamir DPFs
    std::vector<std::vector<KeyShare>>
    GenShamir(size_t alpha, size_t logn, uint32_t m, bool verifiable) {
        std::vector<std::pair<int64_t, int64_t>> shares = gen_shares(3, 2, m, PP);
        uint32_t m1 = shares[0].second;
//        uint32_t m2 = modmersenne31(static_cast<uint64_t>(shares[1].second) * MODINV2);
//        uint32_t m3 = modmersenne31(static_cast<uint64_t>(shares[2].second) * MODINV3);
        uint32_t m2 = (shares[1].second * MODINV2) % PP;
        uint32_t m3 = (shares[2].second * MODINV3) % PP;

//        std::cout << "m: " << m << ", m1 (Share1): " << m1 << ", m2 (Share2): " << m2 << ", m3 (Share3): " << m3 << std::endl;

        uint32_t v1 = rand();
        uint32_t v3 = m1 ^ v1;
        uint32_t v2 = m2 ^ v3;
        uint32_t v4 = m3 ^ v2;

        auto keys1 = GenP(alpha, logn, v1, v2, verifiable);
        auto keys2 = GenP(alpha, logn, v3, v4, verifiable);

        auto key_for_p1 = {keys1.first, keys2.first};
        auto key_for_p2 = {keys1.second, keys2.first};
        auto key_for_p3 = {keys1.second, keys2.second};

        return {key_for_p1, key_for_p2, key_for_p3};
    }

    std::vector<std::vector<std::pair<uint8_t, uint8_t>>> share_seed_helper(block seed) {
        std::vector<std::vector<std::pair<uint8_t, uint8_t>>> res = {
                {}, {}, {}
        };

        reg_arr_union seed_reg;
        seed_reg.reg = seed;
        std::vector<uint8_t> vec(seed_reg.arr, seed_reg.arr + 16);
        for (int i = 0; i < 16; i++) {
            auto shares = share_gf256(seed_reg.arr[i], 3, 1);
            res[0].push_back(shares[0]);
            res[1].push_back(shares[1]);
            res[2].push_back(shares[2]);
        }

        return res;
    }

    std::vector<std::pair<DeferredKeyShare, DeferredKeyShare>> DeferredGenShamir(size_t alpha, size_t logn) {
        auto out0 = GenM_helper(alpha, logn, 0);
        auto out1 = GenM_helper(alpha, logn, 0);

        auto s00_shares = share_seed_helper(out0.s0);
        auto s01_shares = share_seed_helper(out0.s1);
        auto s10_shares = share_seed_helper(out1.s0);
        auto s11_shares = share_seed_helper(out1.s1);

        // We actually need to share 0xFF, not simply 1, if t = 1
        uint8_t mask = 0xFF;
        std::vector<std::pair<uint8_t, uint8_t>> t00_shares, t10_shares;
        if (out0.t0 == 1) {
            t00_shares = share_gf256(mask, 3, 1);
        } else {
            t00_shares = share_gf256(0, 3, 1);
        }

        if (out1.t0 == 1) {
            t10_shares = share_gf256(mask, 3, 1);
        } else {
            t10_shares = share_gf256(0, 3, 1);
        }

        // Party 0 share
        DeferredKeyShare key0_0 = {
                out0.keys.first,
                s00_shares[0],
                s01_shares[0],
                t00_shares[0]
        };
        DeferredKeyShare key0_1 = {
                out1.keys.first,
                s10_shares[0],
                s11_shares[0],
                t10_shares[0]
        };
        auto key0 = std::make_pair(key0_0, key0_1);

        // Party 1 share
        DeferredKeyShare key1_0 = {
                out0.keys.second,
                s00_shares[1],
                s01_shares[1],
                t00_shares[1]
        };
        DeferredKeyShare key1_1 = {
                out1.keys.first,
                s10_shares[1],
                s11_shares[1],
                t10_shares[1]
        };
        auto key1 = std::make_pair(key1_0, key1_1);

        // Party 1 share
        DeferredKeyShare key2_0 = {
                out0.keys.second,
                s00_shares[2],
                s01_shares[2],
                t00_shares[2]
        };
        DeferredKeyShare key2_1 = {
                out1.keys.second,
                s10_shares[2],
                s11_shares[2],
                t10_shares[2]
        };
        auto key2 = std::make_pair(key2_0, key2_1);

        return {key0, key1, key2};
    }

    std::pair<std::vector<uint32_t>, std::array<block, 2>> EvalShamir(const std::vector<KeyShare>& key, size_t logn, uint64_t party_index, bool verifiable) {
        // TODO: might be able to save some performance with using (2, 4) as the relevant points so I can use shifts instead of multiplication. Worth taking a look.
        bool index1 = false;
        bool index2 = false;

        // map party index into the local indices of each DPF
        if (party_index == 1) {
            index1 = true;
        } else if (party_index == 2) {
            index1 = true;
            index2 = true;
        }

        auto res1 = DPF::EvalFull8P(key[0], logn, index1, verifiable);
        auto res2 = DPF::EvalFull8P(key[1], logn, index2, verifiable);

        auto vm1 = res1.first;
        auto vm2 = res2.first;

        std::array<block, 2> pi;
        if (verifiable) {
            pi = prg::hash2(xorArrays(res1.second, res2.second));
        }

        // TODO: check if better to define as a uint64 vector if you're looping anyway
        std::vector<uint32_t> vm(vm1.size());
        // TODO: check if losing a lot of performance for re-looping. Also, parallelize.
        for (int i = 0; i < vm1.size(); i++) {
//            auto tmptmp = (vm1[i] ^ vm2[i]);
//            vm[i] = modmersenne31(static_cast<uint64_t>(vm1[i] ^ vm2[i]) * (party_index + 1ULL));
//            vm[i] = modmersenne31(static_cast<uint64_t>(vm1[i] ^ vm2[i]) * (party_index + 1ULL));
//            uint64_t tmptmp = static_cast<uint64_t>(vm1[i] ^ vm2[i]);
            vm[i] = ((vm1[i] ^ vm2[i]) * (party_index + 1ULL)) % PP;
        }

        return std::make_pair(vm, pi);
    }

}