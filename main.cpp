#include "dpf.h"
#include "hashdatastore.h"

#include <chrono>
#include <iostream>
#include <cassert>

int main(int argc, char** argv) {

    if(argc != 2) {
	    std::cout << "Usage: ./dpf_pir <log_tree_size>" << std::endl;
        return -1;
    }
    size_t N = std::strtoull(argv[1], nullptr, 10);
    std::chrono::duration<double> buildT, evalT1, evalT2, evalT3;
    size_t keysizeT = 0;
    buildT = evalT1 = evalT2 = evalT3 = std::chrono::duration<double>::zero();

    // Benchmark a single execution of Gen
    auto time1 = std::chrono::high_resolution_clock::now();
    auto keys = DPF::Gen(0, N);
    auto a = keys.first;
    auto b = keys.second;
    keysizeT = a.size();
    auto time2 = std::chrono::high_resolution_clock::now();

    // Benchmark a single execution of EvalFull
    std::vector<uint8_t> v0, v1;
    v0 = DPF::EvalFull(a, N);
    auto time3 = std::chrono::high_resolution_clock::now();

    if(N > 10) {
        v1 = DPF::EvalFull8(a, N);
    }
    auto time4 = std::chrono::high_resolution_clock::now();

    buildT += time2 - time1;
    evalT1 += time3 - time2;
    evalT2 += time4 - time3;

    std::cout << "DPF.Gen: "        <<  buildT.count() << "sec" << std::endl;
    std::cout << "DPF.Eval: "       << evalT1.count() << "sec" << std::endl;
    std::cout << "DPF.Eval8 "     << evalT2.count() << "sec" << std::endl;
    std::cout << keysizeT << "; "   <<  N*32 << " bytes total transfer" << std::endl;

    // Benchmark N length 31-bit DPF vs (N + 5) 1-bit DPF vs length N 32-bit DPF. Without threading, ideally, should take about the same time
    size_t N1 = N + 5;

    keys = DPF::Gen(0, N1);
    a = keys.first;
    b = keys.second;

    auto keys2 = DPF::GenM1bit(0, N, 25);
    auto a2 = keys2.first;
    auto b2 = keys2.second;

    auto keys3 = DPF::GenM(0, N, 25);
    auto a3 = keys.first;
    auto b3 = keys.second;

    time1 = std::chrono::high_resolution_clock::now();
    v0 = DPF::EvalFull8(a, N1);
    time2 = std::chrono::high_resolution_clock::now();
    auto u = DPF::EvalFullM1bit(a2, N);
    time3 = std::chrono::high_resolution_clock::now();
    auto w = DPF::EvalFull8M(a3, N);
    time4 = std::chrono::high_resolution_clock::now();

    evalT1 = time2 - time1;
    evalT2 = time3 - time2;
    evalT3 = time4 - time3;
    std::cout << "DPF.EvalFull8: "       << evalT1.count() << "sec" << std::endl;
    std::cout << "DPF.EvalFullM1bit "     << evalT2.count() << "sec" << std::endl;
    std::cout << "DPF.EvalFull8M "     << evalT3.count() << "sec" << std::endl;

    // test correctness

    N = 10;
    int alpha = 21;
    int beta = 25;
    auto km = DPF::GenM(alpha, N, beta);
    auto km0 = km.first;
    auto km1 = km.second;
    auto vm0 = DPF::EvalFull8M(km0, N);
    auto vm1 = DPF::EvalFull8M(km1, N);

    for (int i = 0; i < vm0.size(); i++) {
////        std::cout << "a1[" << i << "] =" << a1[i] << std::endl;
////        std::cout << "a2[" << i << "] =" << a2[i] << std::endl;
//        std::cout << "res[" << i << "] = " << (vm0[i] ^ vm1[i]) << std::endl;
        if (i == alpha) {
            assert( (vm0[i] ^ vm1[i]) == beta);
        } else {
            assert( (vm0[i] ^ vm1[i]) == 0);
        }
    }

    return 0;
}
