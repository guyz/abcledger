//
// Created by Guy Zyskind on 14/11/2023.
//

#include "Server.h"
#include "utils.h"
#include "shamir.h"
#include "dpf.h"
#include "pirw.h"
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <filesystem>

Server::Server(int index, size_t N) : N(N), server_index(index), ledger(N), alphas(N) {
    log2N = static_cast<int>(std::log2(N));

    std::ifstream ledgerFile("data/ledger-" + std::to_string(server_index + 1) + ".txt");
    std::ifstream alphasFile("data/alphas-" + std::to_string(server_index + 1) + ".txt");

    // If any file doesn't exist, call initData
    if (!ledgerFile || !alphasFile) {
        initData(N);
    }

    // Load ledger if file exists
    if (ledgerFile) {
        for (uint32_t &value : ledger) {
            ledgerFile >> value;
        }
        ledgerFile.close();
    }
    // Load alphas if file exists
    if (alphasFile) {
        for (uint32_t &alpha : alphas) {
            alphasFile >> alpha;
        }
        alphasFile.close();
    }
}

void Server::initData(size_t N) {
    // Seed the random number generator
    srand(static_cast<unsigned int>(time(nullptr)));

    // Generate ledgers and alphas
    std::vector<uint32_t> ledger_raw(N), ledger1(N), ledger2(N), ledger3(N);
    std::vector<uint32_t> alphas_raw(N), alphas1(N), alphas2(N), alphas3(N);

    for (int i = 0; i < N; i++) {
        // Generate ledger shares
        auto ledger_i = rand() % (1 << 14) - 1;
        ledger_raw[i] = ledger_i;
        std::vector<std::pair<int64_t, int64_t>> ledger_i_shares = gen_shares(3, 2, ledger_i, PP);
        ledger1[i] = ledger_i_shares[0].second;
        ledger2[i] = ledger_i_shares[1].second;
        ledger3[i] = ledger_i_shares[2].second;

        // Generate alphas
        auto alphas_i = rand() % PP - 1;
        alphas_raw[i] = alphas_i;
        std::vector<std::pair<int64_t, int64_t>> alphas_i_shares = gen_shares(3, 2, alphas_i, PP);
        alphas1[i] = alphas_i_shares[0].second;
        alphas2[i] = alphas_i_shares[1].second;
        alphas3[i] = alphas_i_shares[2].second;
    }

    // Save to files
    saveToFile(ledger_raw, "data/ledger.txt");
    saveToFile(ledger1, "data/ledger-1.txt");
    saveToFile(ledger2, "data/ledger-2.txt");
    saveToFile(ledger3, "data/ledger-3.txt");
    saveToFile(alphas_raw, "data/alphas.txt");
    saveToFile(alphas1, "data/alphas-1.txt");
    saveToFile(alphas2, "data/alphas-2.txt");
    saveToFile(alphas3, "data/alphas-3.txt");
}

void Server::saveToFile(const std::vector<uint32_t>& data, const std::string& filename) {
    if (!std::filesystem::exists("data")) {
        std::filesystem::create_directory("data");
    }

    std::ofstream outFile(filename);
    for (const auto &value : data) {
        outFile << value << std::endl;
    }
    outFile.close();
}


void Server::transfer(const std::vector<std::pair<uint32_t, std::vector<uint8_t>>>& key_A,
                      const std::vector<std::pair<uint32_t, std::vector<uint8_t>>>& key_B,
                      uint32_t tag_share) {
    // Expand DPFs
    // TODO: could/should be parallelized?
    auto data_A = DPF::EvalShamir(key_A, log2N, server_index); // TODO: get and check data_A1 from this..
    auto data_B = DPF::EvalShamir(key_B, log2N, server_index);

    // Prepare Validity check
    // TODO: VerifyDPF for data_B
    // TODO: check access control for data_A1
    uint32_t tag_share_prime = PIRW::innerprodff31(alphas, data_A);

    // Prepare range checks
    uint32_t amount_A = PIRW::sumvecff31(data_A);
    uint32_t amount_B = PIRW::sumvecff31(data_B);
    uint32_t balance_A = PIRW::innerprodff31(data_A, ledger); // In practice, this is the full SumProduct protocol but we will defer it to the end..?
    uint32_t new_balance_A = (balance_A - amount_A) % PP;

    // TODO: run the following checks in MPC
    // 1. use tag_share_prime and whatever else is needed to verify access control/proper DPF to data_A, data_A1, data_B
    // 2. Open(amount_A-amount_B) and make sure it is zero. Need to show in the proof that this works later, but generally speaking the idea is we don't need an equality gate - just public open
    // 3. Check that LTZ(amount_A) == false and LTZ(amount_A - MAX_VALUE) == true // This checks that amount_A is in [0, MAX_VALUE). Assume that MAX_VALUE is greater than all the coins in the system ever..
    // 4. Check that LTZ(new_balance_A) == false // Make sure that this tx won't turn the balance negative

    // Finalize the transaction after the MPC round / all checks have passed
    ledger = PIRW::subvff31(ledger, data_A); // TODO: parallelize
    ledger = PIRW::addvff31(ledger, data_B); // TODO: parallelize
}

uint32_t Server::balance(const std::vector<std::pair<uint32_t, std::vector<uint8_t>>>& key, uint32_t tag_share) {
    auto data = DPF::EvalShamir(key, log2N, server_index);
    uint32_t tag_share_prime = PIRW::innerprodff31(alphas, data);

    // TODO: check access in MPC (Open(t-t') == 0)

    uint32_t balance = PIRW::innerprodff31(data, ledger); // In practice, this is the full SumProduct protocol but we will defer it to the end..?
    return balance;
}
