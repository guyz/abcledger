#pragma once

#include <cstdlib>
#include <vector>
#include <iostream>
#include <mutex>
#include <atomic>
#include "AES.h"
#include "Defines.h"
#include "utils.h"
#include <cryptopp/sha.h>


namespace DPF {

    class HackyVectorAllocator {

    public:
        HackyVectorAllocator() {
            jobMutex = new std::mutex();
            ptr = new std::atomic<size_t>;
            *ptr = 0;
        };

        // allocate enough vs for the entire run - fuck releasing memory!
        void init(size_t toInit, size_t logn) {
//            vms.reserve(toInit);
            for (auto i = 0; i < toInit; i++) {
                auto new_vec = std::vector<uint32_t>((1ULL << logn));
                vms.emplace_back(new_vec);
            }
        }

        std::vector<uint32_t> allocate() {
            *ptr=+1;
//            std::lock_guard<std::mutex> lock(*jobMutex);
//            auto my = vms.back();
//            jobMutex->unlock();
            return vms[*ptr];
        }

    private:
        std::atomic<size_t>* ptr;
        std::vector<std::vector<uint32_t>> vms;
        std::mutex* jobMutex;
    };

    struct KeyShare {
        std::vector<uint8_t> key;
        std::array<block, 4> cs;
        uint32_t z;
    };

    struct DeferredKeyShare {
        std::vector<uint8_t> key;
        std::vector<std::pair<uint8_t , uint8_t>> s0_share;
        std::vector<std::pair<uint8_t, uint8_t>> s1_share;

        std::pair<uint8_t, uint8_t> t0_share;
    };

    std::pair<std::vector<uint8_t>, std::vector<uint8_t> > Gen(size_t alpha, size_t logn);
    bool Eval(const std::vector<uint8_t>& key, size_t x, size_t logn);
    std::vector<uint8_t> EvalFull(const std::vector<uint8_t>& key, size_t logn);
    std::vector<uint8_t> EvalFull8(const std::vector<uint8_t>& key, size_t logn);

    std::pair<std::array<std::vector<uint8_t>, 31>, std::array<std::vector<uint8_t>, 31>> GenM1bit(size_t alpha, size_t logn, int32_t msg);
    std::vector<int32_t> EvalFullM1bit(const std::array<std::vector<uint8_t>, 31>& key, size_t logn);

    // 32-bit messages
    std::pair<std::vector<uint8_t>, std::vector<uint8_t>> GenM(size_t alpha, size_t logn, uint32_t msg);
    void EvalFullRecursive8M(const std::vector<uint8_t>& key, std::array<block, 8>& s, std::array<uint8_t,8>& t, size_t lvl, size_t stop, std::array<uint32_t*,8>& res, std::array<block*,8>& res_nodes, block *CW, bool party_index = false, bool verifiable = false);
    uint32_t EvalM(const std::vector<uint8_t>& key, size_t x, size_t logn);
    void EvalFull8M(const std::vector<uint8_t>& key, std::vector<uint32_t>& vm, size_t logn, bool party_index = false);

    // New DPF constructions (Updated 2023)
//    void EvalFull8M_helper(const std::vector<uint8_t>& key, size_t logn, bool party_index, bool verifiable, std::vector<uint32_t>& data, bool pi_index);
    void EvalFull8M_helper(const std::vector<uint8_t>& key, size_t logn, bool party_index, bool verifiable, uint32_t* dataStart, uint32_t* dataEnd, bool pi_index);

    // (1,2)-DPF+
    std::pair<KeyShare, KeyShare>
    GenP(size_t alpha, size_t logn, uint32_t m1, uint32_t m2, bool verifiable = false);
    int EvalFull8P(const KeyShare& key, std::vector<uint32_t>& vm, size_t logn, bool party_index = false, bool verifiable = false, bool pi_index = false);
    int EvalFull8P(const KeyShare& key, uint32_t* vm_start, uint32_t* vm_end, size_t logn, bool party_index, bool verifiable = false, bool pi_index = false);

    // (1,3)-SS-DPF
    std::vector<std::vector<KeyShare>>
    GenShamir(size_t alpha, size_t logn, uint32_t m, bool verifiable = false);
    int EvalShamir(const std::vector<KeyShare>& key, std::vector<uint32_t>& vm0, std::vector<uint32_t>& vm1, size_t logn, uint64_t party_index, bool verifiable = false);
    int EvalShamir(const std::vector<KeyShare>& key, uint32_t* vm0_start, uint32_t* vm0_end, uint32_t* vm1_start, uint32_t* vm1_end, size_t logn, uint64_t party_index, bool verifiable = false);

    std::vector<std::vector<KeyShare>>
    GenShamirMulti(size_t alpha, size_t logn, uint32_t m, bool verifiable);
    int EvalShamirMulti(const std::vector<KeyShare>& key, std::array<std::vector<uint32_t>, 2*N_SPLITS>& vms, std::vector<uint32_t>& out, size_t logn, uint64_t party_index, bool verifiable);

    std::vector<std::pair<DeferredKeyShare, DeferredKeyShare>> DeferredGenShamir(size_t alpha, size_t logn);

    // Fast ORAM (increases the degree to 2
    std::vector<std::vector<KeyShare>> GenFast(size_t alpha, size_t logn, uint32_t m);
    int EvalFast(const std::vector<KeyShare>& key, std::vector<uint32_t>& vm1, std::vector<uint32_t>& vm2, std::vector<uint32_t>& vm3, std::vector<uint32_t>& vm4, std::vector<uint32_t>& out, size_t logn, uint64_t party_index);

    namespace prg {
//        std::array<block, 4> hash1(const block& seed, uint32_t x);
//        std::array<block, 4> hash1v2(const block& seed, uint32_t x);
        inline std::array<block, 4> hash1(const block& seed, uint32_t x) {
            std::array<block, 4> full_seed, output;
            reg_arr_union tmp;
            tmp.reg = seed;

            for (int i = 0; i < 4; ++i) {
                tmp.arr32[0] = tmp.arr32[0] << i;
                full_seed[i] = tmp.reg;
                output[i] = mAesFixedKey.encryptECB_MMO(full_seed[i]);
            }

            // TODO: this prob does nothing because mmo blocks need 8 blocks I think..
//            mAesFixedKey.encryptECB_MMO_Blocks(full_seed.data(), 4, output.data());
            return output;
        }

//        void hash1v2(const block& seed, uint32_t x, block* output) {
        inline std::array<block, 4> hash1v2(const block& seed, uint32_t x) {
            std::array<block, 4> full_seed, output;
            reg_arr_union tmp;
            tmp.reg = seed;

            for (int i = 0; i < 4; ++i) {
                tmp.arr32[0] = tmp.arr32[0] << i;
                full_seed[i] = tmp.reg;
                EncryptAesEcb(full_seed[i], output[i]);
//                output[i] = mAesFixedKey.encryptECB_MMO(full_seed[i]);
            }
        }
//        void hash1v2(const block& seed, uint32_t x, block* output);
        std::array<block, 4> hash2(const std::array<block, 4>& h);

    } // namespace prg

    std::pair<KeyShare, KeyShare> VerGenM(size_t alpha, size_t logn, uint32_t msg);
    std::vector<uint32_t> VerEvalFull8M(const KeyShare& key, size_t logn, bool party_index, bool pi_index = false);
}
