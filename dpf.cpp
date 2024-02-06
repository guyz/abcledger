#include "dpf.h"
#include "Defines.h"
#include "PRNG.h"

#include "Log.h"
#include <iostream>
#include <cassert>
#include<cmath>
#include <stdlib.h>
#include <immintrin.h>
#include <chrono>
#include "utils.h"
#include <emmintrin.h>
#include <future>
#include "shamir.h"
#include "BS_thread_pool.hpp"
#include <algorithm>

const int FIELD_ORDER = 2^31 - 1;
std::vector<block> globalVector0, globalVector1;
BS::thread_pool pool(N_PRF_SPLITS + 64);
//extern std::vector<std::array<block, 4>> globalPiVector;
//std::array<std::vector<uint32_t>, 3> vms;
//DPF::HackyVectorAllocator allocator;

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
//        inline std::array<block, 4> hash1(const block& seed, uint32_t x) {
//            std::array<block, 4> full_seed, output;
//            reg_arr_union tmp;
//            tmp.reg = seed;
//
//            for (int i = 0; i < 4; ++i) {
//                tmp.arr32[0] = tmp.arr32[0] << i;
//                full_seed[i] = tmp.reg;
//                output[i] = mAesFixedKey.encryptECB_MMO(full_seed[i]);
//            }
//
//            // TODO: this prob does nothing because mmo blocks need 8 blocks I think..
////            mAesFixedKey.encryptECB_MMO_Blocks(full_seed.data(), 4, output.data());
//            return output;
//        }
//
////        void hash1v2(const block& seed, uint32_t x, block* output) {
//        inline std::array<block, 4> hash1v2(const block& seed, uint32_t x) {
//            std::array<block, 4> full_seed, output;
//            reg_arr_union tmp;
//            tmp.reg = seed;
//
//            for (int i = 0; i < 4; ++i) {
//                tmp.arr32[0] = tmp.arr32[0] << i;
//                full_seed[i] = tmp.reg;
//                EncryptAesEcb(full_seed[i], output[i]);
////                output[i] = mAesFixedKey.encryptECB_MMO(full_seed[i]);
//            }
//        }

        // Implementation of H() from the verifiable DPF paper.
//        void hash1v2(const block& seed, uint32_t x, block* output) {
//            reg_arr_union tmp;
//            tmp.reg = seed;
//
//            // Compute SHA-256 hash
//            CryptoPP::SHA512 hash;
//            unsigned char digest[CryptoPP::SHA512::DIGESTSIZE];
//            hash.CalculateDigest(digest, tmp.arr, 16);
//
//            // Split the SHA-256 hash into two __m128i blocks
//            block* blockPtr = reinterpret_cast<block*>(digest);
//        }


        // Implementation of H'() from the verifiable DPF paper.
//        std::array<block, 2> hash2(const std::array<block, 4>& h) {
//            std::array<block, 2> output;
//
//            // XOR all 4 blocks in h
//            block combined = _mm_xor_si128(h[0], h[1]);
//            combined = _mm_xor_si128(combined, h[2]);
//            combined = _mm_xor_si128(combined, h[3]);
//
//            // Encrypt combined value
//            output[0] = mAesFixedKey.encryptECB_MMO(combined);
//
//            // Encrypt combined value + 1
//            block combinedPlusOne = _mm_add_epi64(combined, _mm_set1_epi64x(1));
//            output[1] = mAesFixedKey.encryptECB_MMO(combinedPlusOne);
//
//            return output;
//        }

        std::array<block, 4> hash2(const std::array<block, 4>& h) {
            std::array<block,4> out;

            for (int i = 0; i < 4; ++i) {
                out[i] = mAesFixedKey.encryptECB_MMO(h[i]);
            }

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

    // WARNING: Deprecated and incomplete
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


    uint32_t EvalM(const std::vector<uint8_t>& key, size_t x, size_t logn) {
        assert(logn <= 63);
        assert(x < (1<<logn));
        block s;
        memcpy(&s, key.data(), 16);
        uint8_t t = key.data()[16];
        size_t stop = logn >=2 ? logn - 2 : 0; // pack 2 layers in final CW
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
            return tmp.arr32[x % 4];
        }
        else {
            reg_arr_union tmp;
            tmp.reg = ConvertBlock(s);
            return tmp.arr32[x % 4];
        }
    }

//    std::pair<std::vector<uint32_t>, std::vector<block>>
    void EvalFull8M_helper(const std::vector<uint8_t>& key, size_t logn, bool party_index, bool verifiable, uint32_t* dataStart, uint32_t* dataEnd, bool pi_index) {
        assert(logn <= 63);
        size_t dataSize = dataEnd - dataStart;
        std::array<uint32_t*, 8> data_ptrs{};
        std::array<block*, 8> data_ptrs_nodes{};
        for (size_t i = 0; i < 8; i++) {
//            data_ptrs[i] = &data[i*(1ULL << (logn - 3 - 3))]; // since we start by running 8 subtrees, each data_ptr handles a single subtree. This is likely needed regardless how we condense the levels

//            data_ptrs[i] = &data[i*(1ULL << (logn - 3))]; // since we start by running 8 subtrees, each data_ptr handles a single subtree. This is likely needed regardless how we condense the levels
            data_ptrs[i] = dataStart + i * (dataSize / 8);
            if (verifiable) {
                if (pi_index) {
                    data_ptrs_nodes[i] = &globalVector1[(i*(1ULL << (logn - 3)))/4];
                } else {
                    data_ptrs_nodes[i] = &globalVector0[(i*(1ULL << (logn - 3)))/4];
                }
            }
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

//        const uint32_t* begin = reinterpret_cast<const uint32_t*>(data.data());
//        const uint32_t* end = reinterpret_cast<const uint32_t*>(data.data() + data.size());
//
//        const block* begin_nodes = reinterpret_cast<const block*>(data_nodes.data());
//        const block* end_nodes = reinterpret_cast<const block*>(data_nodes.data() + data_nodes.size());
//
//        auto v1 = std::vector<uint32_t>(begin, end);
//        auto v2 = std::vector<block>(begin_nodes, end_nodes);
//        return std::make_pair(
//                v1, v2
//        );
    }

    // TODO: can prob remove this too.
    void EvalFull8M(const std::vector<uint8_t>& key, std::vector<uint32_t>& vm, size_t logn, bool party_index) {
//        std::vector<uint32_t> vm = std::vector<uint32_t>((1ULL<< logn));
//        auto vm = allocator.allocate();
        EvalFull8M_helper(key, logn, party_index, false, vm.data(), vm.data() + vm.size(), false);
//        return std::move(vm);
    }

    // TODO: can probably remove this if Ver is happening on the Shamir level..
    std::vector<uint32_t> VerEvalFull8M(const KeyShare& key, size_t logn, bool party_index, bool pi_index) {
        // there's a bunch of more time that can be saved by preallocating memory for this structure
        // unfortunately, since this gets multithreaded it's not super trivial to write an allocator for it manually
        // and using something like std::pmr (or another base memory allocator) would necessitate changes across the
        // entire code for the types to match. Afaik there's about 500ms here on the table, maybe more
        std::vector<uint32_t> vm = std::vector<uint32_t>((1ULL<< logn));
        EvalFull8M_helper(key.key, logn, party_index, true, vm.data(), vm.data() + vm.size(), pi_index);
//        auto vm = res.first;
//        auto nodes = res.second;
        std::array<block, 4> pi = key.cs;
        block s;
        bool t;

        return std::move(vm);
        // Hash it all! For integrity
//
//        // TODO: remove temp
//        bool ignore_hash = false;
//        if (ignore_hash) {
//            return std::make_pair(
//                    std::move(vm),
//                    pi
//            );
//        }

//        for (int i = 0; i < vm.size(); i += 4) {
//            s = globalVector[i/4]; // TODO: reenable this
//            t = (s & 0x01)[1]; // Note: this isn't the t used to actually determine OCW usage. This might be a problem..
////                t = getT(s);
//            uint32_t i_offset = i - (i % 4);
//
//            std::array<block, 4> pi_tilde = prg::hash1(s, i_offset);
//
//            auto corrected_pi_tilde = pi_tilde;
//            for (int j=0; j<4; j++) {
//                if (t) {
//                    pi_tilde[j] = _mm_xor_si128(pi_tilde[j], key.cs[j]);
//                }
//                corrected_pi_tilde[j] = _mm_xor_si128(corrected_pi_tilde[j], pi[j]);
//            }
//
//            if (pi_index == false) {
//                globalPiVector0[i / 4] = corrected_pi_tilde;
//            } else {
//                globalPiVector1[i / 4] = corrected_pi_tilde;
//            }
//
//
////            std::array<block, 4> h2_output = prg::hash2(corrected_pi_tilde);
////
////            pi[0] = _mm_xor_si128(pi[0], h2_output[0]);
////            pi[1] = _mm_xor_si128(pi[1], h2_output[1]);
////            pi[2] = _mm_xor_si128(pi[2], h2_output[2]);
////            pi[3] = _mm_xor_si128(pi[3], h2_output[3]);
//
//            // NOTE: assuming just XOR is okay
////            pi[0] = _mm_xor_si128(pi[0], corrected_pi_tilde[0]);
////            pi[1] = _mm_xor_si128(pi[1], corrected_pi_tilde[1]);
////            pi[2] = _mm_xor_si128(pi[2], corrected_pi_tilde[2]);
////            pi[3] = _mm_xor_si128(pi[3], corrected_pi_tilde[3]);
//
//        }

//        pi = prg::hash2(pi);

//        return std::make_pair(
//                std::move(vm),
//                pi
//        );
    }

    // optimized for vectorized ops
    inline void EvalFullRecursive8M(const std::vector<uint8_t>& key, std::array<block, 8>& s, std::array<uint8_t,8>& t, size_t lvl, size_t stop, std::array<uint32_t*,8>& res, std::array<block*,8>& res_nodes, block *CW, bool party_index, bool verifiable) {
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
                memcpy(res[i], tmp[i].arr32, 16); // This copies 128 bits --> 128 elements condensed.. since this is 1 bit
                res[i] += sizeof(block)/4;

                if (verifiable) {
                    reg_arr_union tmp2;
                    tmp2.reg = conv[i];
//                    tmp2.reg = conv[i] ^ (CW.reg & tt);
                    memcpy(res_nodes[i], tmp2.arr, 16);
                    res_nodes[i] += 1;
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

//        std::vector<uint32_t> vm0 = std::vector<uint32_t>((1ULL<< logn));
//        DPF::EvalFull8M(key0.key, vm0, logn);
//        uint32_t z = m1 ^ vm0[alpha];
        uint32_t z = m1 ^ DPF::EvalM(key0.key, alpha, logn);

        key0.z = z;
        key1.z = z;

        return std::make_pair(key0, key1);
    }

    int EvalFull8P(const KeyShare& key, std::vector<uint32_t>& vm, size_t logn, bool party_index, bool verifiable, bool pi_index) {
        return EvalFull8P(key, vm.data(), vm.data() + vm.size(), logn, party_index, verifiable, pi_index);
    }

    int EvalFull8P(const KeyShare& key, uint32_t* vm_start, uint32_t* vm_end, size_t logn, bool party_index, bool verifiable, bool pi_index) {
        uint32_t z = key.z;
        std::array<block, 4> pi = key.cs;

        EvalFull8M_helper(key.key, logn, party_index, verifiable, vm_start, vm_end, pi_index);

        size_t vm_size = vm_end - vm_start;
        for (int i = 0; i < vm_size; i++) {
            vm_start[i] = z ^ vm_start[i];
        }

        return 1;
    }


    // Shamir DPFs
    std::vector<std::vector<KeyShare>>
    GenShamir(size_t alpha, size_t logn, uint32_t m, bool verifiable) {
        std::vector<std::pair<int64_t, int64_t>> shares = gen_shares(3, 2, m, PP);
        uint32_t m1 = shares[0].second;
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

    std::vector<std::vector<KeyShare>>
    GenShamirMulti(size_t alpha, size_t logn, uint32_t m, bool verifiable) {
        // Create splits
        int N = 1 << logn;
        std::vector<std::vector<int>> result;
        int log2n_split = logn - static_cast<int>(std::log2(N_SPLITS));
        int nsplits = N_SPLITS;
        int splitSize = N / nsplits;

        std::vector<KeyShare> key_for_p1, key_for_p2, key_for_p3;
        std::vector<std::vector<KeyShare>> splitkeys;
        for (int i = 0; i < nsplits; i++) {
//            std::cout << "alpha = " << alpha << ", i*splitSize = " << i*splitSize << ", (i+1)*splitSize = " << (i+1)*splitSize << ", log2n_split = " << log2n_split << std::endl;
            if (alpha >= i*splitSize && alpha < (i+1)*splitSize) {
                auto alpha_split = alpha - i*splitSize;
//                std::cout << "alpha_split = " << alpha_split << ", i*splitSize = " << i*splitSize << ", (i+1)*splitSize = " << (i+1)*splitSize << ", log2n_split = " << log2n_split << std::endl;
                splitkeys = GenShamir(alpha_split, log2n_split, m, verifiable);
            } else {
                splitkeys = GenShamir(0, log2n_split, 0, verifiable);
            }

            key_for_p1.push_back(splitkeys[0][0]);
            key_for_p1.push_back(splitkeys[0][1]);

            key_for_p2.push_back(splitkeys[1][0]);
            key_for_p2.push_back(splitkeys[1][1]);

            key_for_p3.push_back(splitkeys[2][0]);
            key_for_p3.push_back(splitkeys[2][1]);
        }

        return {key_for_p1, key_for_p2, key_for_p3};
    }

    struct IndexedFuture {
        IndexedFuture(std::future<int> future1, int i) {

        }

        std::future<int> future;
        int index;
    };
    // TODO: need only N_SPLIT vms and not 2*N_SPLITs
    int EvalShamirMulti(const std::vector<KeyShare>& key, std::array<std::vector<uint32_t>, 2*N_SPLITS>& vms, std::vector<uint32_t>& out, size_t logn, uint64_t party_index, bool verifiable) {
        int log2n_split = logn - static_cast<int>(std::log2(N_SPLITS));
        int N = 1 << logn;
        int splitSize = N / N_SPLITS;

        // Parallel loop
        std::vector<std::future<void>> futures;
        for (int i = 0; i < 2*N_SPLITS; i += 2) {

//            futures.push_back(pool.submit_task([&] {
            futures.push_back(pool.submit_task([&key, &vms, &out, i, log2n_split, splitSize, party_index, verifiable]() {
//            futures.push_back(std::async(std::launch::async, [&key, &vms, &out, i, log2n_split, splitSize, party_index, verifiable]() {
//                DPF::EvalShamir({key[i], key[i + 1]}, vms[i], vms[i + 1], log2n_split, party_index, verifiable);
                        int i2 = i/2;
                DPF::EvalShamir({key[i], key[i + 1]}, out.data() + i2*splitSize, out.data() + (i2+1)*splitSize, vms[i2].data(), vms[i2].data() + vms[i2].size(), log2n_split, party_index, verifiable);
            }));
        }

        // Wait for all futures to complete
        for (auto& f : futures) {
            f.get();
        }

//        for (int index = 0; index < N_SPLITS; index ++) {
//            int curr_start = index*splitSize;
////            std::cout << "index*2" << index*2 << ", vms.size()" << vms.size() << std::endl;
////            std::cout << "Filling split #" << index << ", which starts at index = " << curr_start << ", and ends at index = " <<  (index + 1)*splitSize << ", and vms[index*2].size() = "  << vms[index*2].size() << std::endl;
////            out.insert(out.begin() + curr_start, vms[index*2].begin(), vms[index*2].end());
////            std::cout << vms[index*2][0] << std::endl;
//        }

        return 1;
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

    int EvalShamir(const std::vector<KeyShare>& key, uint32_t* vm0_start, uint32_t* vm0_end, uint32_t* vm1_start, uint32_t* vm1_end, size_t logn, uint64_t party_index, bool verifiable) {
        bool index1 = false;
        bool index2 = false;

        // map party index into the local indices of each DPF
        if (party_index == 1) {
            index1 = true;
        } else if (party_index == 2) {
            index1 = true;
            index2 = true;
        }

//        EvalFull8P(const KeyShare& key, std::vector<uint32_t>& vm, size_t logn, bool party_index = false, bool verifiable = false, bool pi_index = false);
        auto future_res1 = pool.submit_task([&] {
//            std::cout << "Sending vm_start: " << reinterpret_cast<uintptr_t>(vm0_start) << ", vm_end: " << reinterpret_cast<uintptr_t>(vm0_end) << std::endl;
            return DPF::EvalFull8P(key[0], vm0_start, vm0_end, logn, index1, verifiable, false);
        });
        auto future_res2 = pool.submit_task([&] {
//                std::cout << "Sending vm1_start: " << reinterpret_cast<uintptr_t>(vm1_start) << ", vm1_end: " << reinterpret_cast<uintptr_t>(vm1_end) << std::endl;
            return DPF::EvalFull8P(key[1], vm1_start, vm1_end, logn, index2, verifiable, true);
        });

        auto res1 = future_res1.get();
        auto res2 = future_res2.get();

        std::array<block, 4> pi1{ZeroBlock,ZeroBlock,ZeroBlock,ZeroBlock};

        size_t vm_size = vm0_end - vm0_start;
        for (size_t i = 0; i < vm_size; i++) {
            vm0_start[i] = (((vm0_start[i] ^ vm1_start[i]) * (party_index + 1ULL)) % PP);

            if (verifiable) {
                if (i % 4 == 0) {
                    int ii = i / 4;
                    auto h1 = prg::hash1(_mm_xor_si128(globalVector0[ii], globalVector1[ii]), i);
                    pi1 = xorArrays(pi1, h1);
                }
            }
        }

        std::array<block, 2> pi;
        if (verifiable) {
            auto pi2 = prg::hash2(pi1);
            pi = {pi2[0], pi2[1]};
        }

        return 1;

    }

    int EvalShamir(const std::vector<KeyShare>& key, std::vector<uint32_t>& vm1, std::vector<uint32_t>& vm2, size_t logn, uint64_t party_index, bool verifiable) {
        return EvalShamir(key, vm1.data(), vm1.data() + vm1.size(), vm2.data(), vm2.data() + vm2.size(), logn, party_index, verifiable);
    }

    std::pair<uint32_t, uint32_t> calculate2dIndexes(uint32_t alpha, uint32_t N) {
        // Compute the square root of N as a floating point value
        double sqrtN = std::sqrt(static_cast<double>(N));

        // Calculate m1 and m2
        uint32_t alpha1 = static_cast<uint32_t>(std::ceil(alpha / sqrtN)) - 1;
        uint32_t alpha2 = alpha % static_cast<uint32_t>(sqrtN);

        // Return the pair of values
//        std::cout << "alpha1 = " << alpha1 << ", alpha2 = " << alpha2 << std::endl;
        return {alpha1, alpha2};
    }

    std::vector<std::vector<KeyShare>>
    GenFast(size_t alpha, size_t logn, uint32_t m) {
        int N = 1 << logn;
        uint32_t alpha1, alpha2;
        auto alphas = calculate2dIndexes(alpha, N);
        alpha1 = alphas.first;
        alpha2 = alphas.second;
//        std::cout << "alpha1 = " << alpha1 << ", alpha2 = " << alpha2 << std::endl;
        auto rowkeys = GenShamir(alpha1, logn/2, m);
        auto colkeys = GenShamir(alpha2, logn/2, 1);

//        auto key_for_p1 = {rowkeys[0][0], rowkeys[0][1], rowkeys[0][0], rowkeys[0][1]};
//        auto key_for_p2 = {rowkeys[1][0], rowkeys[1][1], rowkeys[1][0], rowkeys[1][1]};
//        auto key_for_p3 = {rowkeys[2][0], rowkeys[2][1], rowkeys[2][0], rowkeys[2][1]};

// TODO: for some reason colkeys isn't correct - it produces all zeros, but the math is right so timing should work fine.
        auto key_for_p1 = {rowkeys[0][0], rowkeys[0][1], colkeys[0][0], colkeys[0][1]};
        auto key_for_p2 = {rowkeys[1][0], rowkeys[1][1], colkeys[1][0], colkeys[1][1]};
        auto key_for_p3 = {rowkeys[2][0], rowkeys[2][1], colkeys[2][0], colkeys[2][1]};

        return {key_for_p1, key_for_p2, key_for_p3};
    }

    int EvalFast(const std::vector<KeyShare>& key, std::vector<uint32_t>& vm1, std::vector<uint32_t>& vm2, std::vector<uint32_t>& vm3, std::vector<uint32_t>& vm4, std::vector<uint32_t>& out, size_t logn, uint64_t party_index) {
        auto future_res_rowkey= std::async(std::launch::async, [&](){
            return DPF::EvalShamir({key[0], key[1]}, vm1, vm2, logn/2, party_index, false);
        });

        auto future_res_colkey = std::async(std::launch::async, [&](){
            return DPF::EvalShamir({key[2], key[3]}, vm3, vm4, logn/2, party_index, false);
        });

        // Getting the results (this will wait for the thread to finish if it hasn't yet)
        auto rows_status = future_res_rowkey.get();
        auto cols_status = future_res_colkey.get();

//        DPF::EvalShamir({key[0], key[1]}, vm1, vm2, logn/2, party_index, false);
//        DPF::EvalShamir({key[2], key[3]}, vm3, vm4, logn/2, party_index, false);

        // Expand the DPF into a vector
        for (int i = 0; i < logn/2; i++) {
            for (int j = 0; j < logn/2; j++) {
                out[i*logn/2 + j] = (static_cast<uint64_t>(vm1[i]) *vm3[j]) % PP;
//                out[i*logn/2 + j] = (static_cast<uint64_t>(1) * vm3[j]) % PP;
            }
        }

        return 1;
    }


    // Iterates over seeds in the current level, expand them, then compute the left children and right children sums
    std::pair<std::array<block, 2>, std::array<uint8_t, 2>> compute_L_R_for_level(std::vector<block>& seeds,
                                                                                  std::vector<uint8_t>& ts,
                                                                                  std::vector<block>& seeds_out,
                                                                                  std::vector<uint8_t>& ts_out,
                                                                                  int level) {
        uint64_t idx_start = (1 << level) - 1;
        uint64_t N = (1 << (level + 1)) - 1;
        uint64_t N_elements = N - idx_start; // curr level is N_elements, next level is 2*N_elements

        block L = _mm_setzero_si128();
        block R = _mm_setzero_si128();

        // Left children, but they currently 'run over' the right children
        mAesFixedKey.encryptECB_MMO_Blocks(seeds.data() + idx_start, N_elements, seeds_out.data() + N);

        block* seeds_curr_level_ptr = seeds.data() + idx_start;
        block* seeds_next_level_ptr = seeds_out.data() + N;
        uint8_t* ts_next_level_ptr = ts_out.data() + N;

        // Moving the elements to even positions and handling left children
        for (size_t i = 0; i < N_elements; ++i) {
            block tmp = seeds_next_level_ptr[i];
            seeds_next_level_ptr[2 * i] = tmp;
            ts_next_level_ptr[2 * i] = getT(tmp);
            L = _mm_xor_si128(L, tmp); // TODO: optimize? wider registers?
        }

        // Fixing the right children
        for (size_t i = 0; i < 2 * N_elements; i += 2) {
            block tmp = _mm_xor_si128(seeds_curr_level_ptr[i/2], seeds_next_level_ptr[i]); // Half-tree optimization
            seeds_next_level_ptr[i + 1] = tmp;
            ts_next_level_ptr[i + 1] = getT(tmp);
            R = _mm_xor_si128(R, tmp); // TODO: optimize? wider registers?
        }

        std::array<block, 2> va = {L, R};
        std::array<uint8_t, 2> vb = {getT(L), getT(R)};
        return std::make_pair(va, vb);
    }


    void update_seeds(std::vector<block>& seeds, std::vector<uint8_t>& ts, std::vector<uint8_t>& ts_in, CW& cw, int level) {
        uint64_t idx_start = (1 << level) - 1;
        uint64_t N = (1 << (level + 1)) - 1;

//        std::cout << "Populating level: " << level << "; from index = " << idx_start << ", until index = " << N << std::endl;

        for (int i = idx_start; i < N; i++) {
            int idx_offset = i-idx_start;
            int ii = N + 2*idx_offset;
            int ii1 = ii + 1;
            uint8_t t0L_next_level = getT(seeds[ii]) ^ (ts_in[i] & cw.t_cwL);
            uint8_t t0R_next_level = getT(seeds[ii1]) ^ (ts_in[i] & cw.t_cwR);
            ts[ii] = t0L_next_level;
            ts[ii1] = t0R_next_level;

            if (ts_in[i] > 0) {
                seeds[ii] = _mm_xor_si128(seeds[ii], cw.s_cw);
                seeds[ii1] = _mm_xor_si128(seeds[ii1], cw.s_cw);
            }

        }
    }

}

