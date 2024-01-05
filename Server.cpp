//
// Created by Guy Zyskind on 14/11/2023.
//

#include "Server.h"
#include "utils.h"
#include "shamir.h"
#include "dpf.h"
#include "pirw.h"
#include <ctime>
#include "utils.h"
#include "shamir.h"
#include "dpf.h"
#include "pirw.h"
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <cassert>

#include <sstream>
#include <variant>
#include <vector>
#include <cstring>

// TODO: Things to implement:
// Frand, Fzero, Fltz, PRZS, PRSS

Server::Server(int index, size_t N) : N(N), server_index(index), ledger(N), alphas(N) {
    log2N = static_cast<int>(std::log2(N));

    std::ifstream ledgerFile(DATA_DIR + "ledger-" + std::to_string(server_index + 1) + ".txt");
    std::ifstream alphasFile(DATA_DIR + "alphas-" + std::to_string(server_index + 1) + ".txt");

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

    // Connect to the other servers
    initNetworking();
}

void Server::initNetworking() {
    startListening();
    establishConnections();
    acceptConnections();
}

void Server::startListening() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        exit(1);
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(10000 + server_index);

    if (bind(serverSocket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        exit(2);
    }

    if (listen(serverSocket, 3) < 0) {
        std::cerr << "Listen failed" << std::endl;
        exit(3);
    }
}

void Server::establishConnections() {
    struct sockaddr_in serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(10000); // Base port number

    // Set the IP address of the server
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        exit(EXIT_FAILURE);
    }

    if (server_index > 0) {
        std::cout << "Server" << server_index << " is trying to connect to Server0" << std::endl;
        serv_addr.sin_port = htons(10000);
        connectionHandler1 = socket(AF_INET, SOCK_STREAM, 0);
        serverIndex1 = 0;
        if (connectionHandler1 < 0) {
            std::cerr << "Socket creation error" << std::endl;
            exit(EXIT_FAILURE);
        }

        if (connect(connectionHandler1, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "Connection Failed to Server 0" << std::endl;
            exit(EXIT_FAILURE);
        }
        std::cout << "Successfully connected!" << std::endl;
    }

    if (server_index > 1) {
        std::cout << "Server" << server_index << " is trying to connect to Server1" << std::endl;
        serv_addr.sin_port = htons(10001);
        connectionHandler2 = socket(AF_INET, SOCK_STREAM, 0);
        serverIndex2 = 1;
        if (connectionHandler2 < 0) {
            std::cerr << "Socket creation error" << std::endl;
            exit(EXIT_FAILURE);
        }

        if (connect(connectionHandler2, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "Connection Failed to Server 1" << std::endl;
            exit(EXIT_FAILURE);
        }
        std::cout << "Successfully connected!" << std::endl;
    }
}

void Server::acceptConnections() {
    if (server_index < 2) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        std::cout << "Server" << server_index << " is waiting for a connection" << std::endl;
        connectionHandler2 = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (connectionHandler2 < 0) {
            std::cerr << "Failed to accept connection" << std::endl;
            exit(4);
        }
        // NOTE: this is hacky, we assume the first connection is from server (1-->0 or 2-->1)
        if (server_index == 0) {
            serverIndex2 = 1;
        } else {
            serverIndex2 = 2;
        }
        std::cout << "Connection with party: " << serverIndex2 + 1 << std::endl;
    }

    if (server_index < 1) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        std::cout << "Server" << server_index << " is waiting for a connection" << std::endl;
        connectionHandler1 = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (connectionHandler1 < 0) {
            std::cerr << "Failed to accept connection" << std::endl;
            exit(5);
        }
        // NOTE: This is hacky, we assume only server 0 gets another connection which is from server 2
        serverIndex1 = 2;
        std::cout << "Second connection with party: " << serverIndex1 + 1 << std::endl;
    }
}

// New method to close connections
void Server::closeConnections() {
    if (connectionHandler1 >= 0) {
        close(connectionHandler1);
    }
    if (connectionHandler2 >= 0) {
        close(connectionHandler2);
    }
    if (serverSocket >= 0) {
        close(serverSocket);
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
    saveToFile(ledger_raw, DATA_DIR + "ledger.txt");
    saveToFile(ledger1, DATA_DIR + "ledger-1.txt");
    saveToFile(ledger2, DATA_DIR + "ledger-2.txt");
    saveToFile(ledger3, DATA_DIR + "ledger-3.txt");
    saveToFile(alphas_raw, DATA_DIR + "alphas.txt");
    saveToFile(alphas1, DATA_DIR + "alphas-1.txt");
    saveToFile(alphas2, DATA_DIR + "alphas-2.txt");
    saveToFile(alphas3, DATA_DIR + "alphas-3.txt");
}

void Server::saveToFile(const std::vector<uint32_t>& data, const std::string& filename) {
    if (!std::filesystem::exists(DATA_DIR)) {
        std::filesystem::create_directory(DATA_DIR);
    }

    std::ofstream outFile(filename);
    for (const auto &value : data) {
        outFile << value << std::endl;
    }
    outFile.close();
}

// Helper function to receive the specified amount of data
ssize_t receiveFully(int socket, char *buffer, size_t length) {
    size_t totalReceived = 0;
    ssize_t bytesReceived;

    while (totalReceived < length) {
        bytesReceived = recv(socket, buffer + totalReceived, length - totalReceived, 0);

        if (bytesReceived <= 0) {
            // Handle error or closed connection
            return bytesReceived;
        }

        totalReceived += bytesReceived;
    }

    return totalReceived;
}

// Helper function to wait for acknowledgement
bool Server::waitForAck(int socket) {
    char ackBuffer[4] = {0};  // Buffer to store ack
    if (receiveFully(socket, ackBuffer, 3) <= 0) {
        return false;
    }
    return std::string(ackBuffer) == "ack";
}

field Server::PRSS() {
    // TODO: real PRSS
    field rs[3] = {153685505, 402498915, 651312325};
    return rs[server_index];
}

field Server::PRZS() {
    // TODO: real PRZS
    field rs[3] = {299355974, 2120311573, 1167899503};
    return rs[server_index];
}

bool Server::LTZ(std::vector<std::pair<int64_t, int64_t>> shares) {
    auto x = recover_secret(shares, PP);
    return x < 0;
}

//localMPCChecks(amount_deltas, amount_As, amount_Amaxs,
//        new_balances_A, tag_share_A_primes, tag_share_A1_primes);
void Server::localMPCChecks(std::vector<field>& amount_deltas, std::vector<field>& amount_As, std::vector<field>& amount_Amaxs,
                            std::vector<field>& new_balances_A, std::vector<field>& tag_share_A_primes, std::vector<field>& tag_share_A1_primes, std::vector<field>& amount_Brs, std::vector<block> pi_Bs) {

    auto amount_deltas_shares = encode_to_shares(amount_deltas);
    auto amount_As_shares = encode_to_shares(amount_deltas);
    auto amount_Amaxs_shares = encode_to_shares(amount_Amaxs);
    auto new_balances_A_shares = encode_to_shares(new_balances_A);
    auto tagDelta_shares = encode_to_shares(tag_share_A_primes);
    auto tag1Delta_shares = encode_to_shares(tag_share_A1_primes);

    auto amount_delta = recover_secret(amount_deltas_shares, PP);
    // amount_A and amount_B are the same
    assert( amount_delta == 0); // NOTE: this needs to be equalzero in malicious

    // Access control checks pass // TODO: do these need to be equalzero in malicious?
    auto tagDelta = recover_secret(tagDelta_shares, PP);
    auto tag1Delta = recover_secret(tag1Delta_shares, PP);
    assert( tagDelta == 0);
    assert( tag1Delta == 0);

    // Pi and shamir sharing of DPF B
    assert(are_arrays_equal_2({pi_Bs[0], pi_Bs[1]}, {pi_Bs[2], pi_Bs[3]})
    && are_arrays_equal_2({pi_Bs[0], pi_Bs[1]}, {pi_Bs[4], pi_Bs[5]}));
    assert(verify_polynomial(amount_As, PP));

    // TODO: Proper LTZ protocol
    // LTZ gates
    // amount_A in range
    assert(LTZ(amount_As_shares) == 0);
//    assert(LTZ(amount_Amaxs_shares) == 1); // TODO: fix this..
    // new balance is not negative
    assert(LTZ(new_balances_A_shares) == 0);

}

template <typename... Args>
std::pair<std::tuple<Args...>, std::tuple<Args...>> Server::run_round(const std::tuple<Args...>& inputs) {
    // Serialize the data
    std::string dataToSend = serializeData(inputs);

    // Send data to other servers
    send(connectionHandler1, dataToSend.c_str(), dataToSend.size(), 0);
    send(connectionHandler2, dataToSend.c_str(), dataToSend.size(), 0);

    // Logging sent data - optional
    // std::apply([](const auto&... args) { ((std::cout << args << ", "), ...); }, inputs);
    std::cout << "Server" << server_index << " has sent data." << std::endl;

    // Receive data from server 1
    char buffer1[1024] = {0};
    if (receiveFully(connectionHandler1, buffer1, dataToSend.size()) <= 0) {
        std::cerr << "Failed to receive data from server 1" << std::endl;
        // Handle error
    }
    auto output1 = deserializeData<Args...>(std::string(buffer1, dataToSend.size()));

    // Receive data from server 2
    char buffer2[1024] = {0};
    if (receiveFully(connectionHandler2, buffer2, dataToSend.size()) <= 0) {
        std::cerr << "Failed to receive data from server 2" << std::endl;
        // Handle error
    }
    auto output2 = deserializeData<Args...>(std::string(buffer2, dataToSend.size()));

    return {output1, output2};
}

void Server::transfer(const std::vector<DPF::KeyShare>& key_A,
                      const std::vector<DPF::KeyShare>& key_A1,
                      const std::vector<DPF::KeyShare>& key_B,
                      field tag_A_share, field tag_A1_share) {
    // Expand DPFs
    // TODO: could/should be parallelized?
    auto res_A = DPF::EvalShamir(key_A, log2N, server_index, false);
    auto res_A1 = DPF::EvalShamir(key_A1, log2N, server_index, false);
    auto res_B = DPF::EvalShamir(key_B, log2N, server_index, true);

    auto data_A = res_A.first;
    auto data_A1 = res_A1.first;
    auto data_B = res_B.first;
    auto pi_B = res_B.second;
//    std::vector<block> pi_B(res_B.second.begin(), res_B.second.end());
    block pi0_B = pi_B[0];
    block pi1_B = pi_B[1];

    std::cout << "A[0]: " << data_A[0] << std::endl;
//    std::cout << "pi_B: " << pi_B[0][0] << std::endl;
    std::cout << "alphas[0]: " << alphas[0] << std::endl; // TODO: remove temp

    // Prepare Validity check
    // TODO: VerifyDPF for data_B - including shamir sharing..
    // TODO: proper Fproduct.. i.e., with PRZS
    field tag_share_A_prime = mod(static_cast<int64_t>(PIRW::innerprodff31(alphas, data_A)) - tag_A_share, PP); // bad naming - this is actually the delta
    field tag_share_A1_prime = mod(static_cast<int64_t>(PIRW::innerprodff31(alphas, data_A1)) - tag_A1_share, PP); // same

    // Prepare range checks
    field amount_A = PIRW::sumvecff31(data_A);
    field amount_B = PIRW::sumvecff31(data_B);
    field balance_A = PIRW::innerprodff31(data_A, ledger);
    field new_balance_A = mod(static_cast<int64_t>(balance_A) - amount_A, PP); // Note: mod and cast like this work, otherwise some weird overflows. Can optimize at some point..
    std::cout << "balance_A: " << balance_A << ", amount: " << amount_A << ", amount_B: " << amount_B << ", new_balance: " << new_balance_A << ", tag1: " << PIRW::innerprodff31(alphas, data_A) << ", tag2: " << tag_A_share << std::endl; // TODO : remove temp. Note, right now it's amount*balance
    // TODO: run the following checks in MPC

    // Prepare the data to send
    field amount_delta = mod(static_cast<int64_t>(amount_A) - amount_B, PP);
    field amount_Amax = mod(static_cast<int64_t>(amount_A) - MAX_VALID_INT, PP);

    field r = PRSS();
    field amount_Br = mod(static_cast<int64_t>(amount_B) + r, PP);

    std::cout << "amount_delta: " << amount_delta << ", amount_Amax: " << amount_Amax << ", tag_delta: " << tag_share_A_prime << std::endl;

    auto inputs = std::make_tuple(
            amount_delta, // Check that they are equal
            amount_A, // Input to FLTZ(amount_A)
            amount_Amax, // Input to FLTZ(amount_A - MAX_VALID_INT)
            new_balance_A, // Input to FLTZ(balance_A - amount_A)
            tag_share_A_prime, // access control proof
            tag_share_A1_prime, // access control proof
            amount_Br, // Masked amount_B as DPF B proof part 2
            pi0_B, pi1_B // DPF B proof part 1
            );

    // Run the round of communication
    auto [output1, output2] = run_round(inputs);

    // Process the received data
    auto [amount_delta1, amount_A1, amount_Amax1, new_balance_A1, tag_share_A_prime1, tag_share_A1_prime1, amount_Br1, pi0_B1, pi1_B1] = output1;
    auto [amount_delta2, amount_A2, amount_Amax2, new_balance_A2, tag_share_A_prime2, tag_share_A1_prime2, amount_Br2, pi0_B2, pi1_B2] = output2;
//    std::cout << "Server" << server_index << " received: " << amount_A_from_server1 << ", " << amount_B_from_server1 << ", " << new_balance_A_from_server1 << std::endl;
//    std::cout << "Server" << server_index << " received: " << amount_A_from_server2 << ", " << amount_B_from_server2 << ", " << new_balance_A_from_server2 << std::endl;

    // 1. use tag_share_prime and whatever else is needed to verify access control/proper DPF to data_A, data_A1, data_B
    // 2. Open(amount_A-amount_B) and make sure it is zero. Need to show in the proof that this works later, but generally speaking the idea is we don't need an equality gate - just public open
    // 3. Check that LTZ(amount_A) == false and LTZ(amount_A - MAX_VALUE) == true // This checks that amount_A is in [0, MAX_VALUE). Assume that MAX_VALUE is greater than all the coins in the system ever..
    // 4. Check that LTZ(new_balance_A) == false // Make sure that this tx won't turn the balance negative

    std::vector<uint32_t> amount_deltas(3, 0), amount_As(3, 0), amount_Amaxs(3, 0),
            new_balances_A(3, 0), tag_share_A_primes(3, 0), tag_share_A1_primes(3, 0), amount_Brs(3, 0);
//    std::vector<std::vector<block>> pi_Bs = {
//            {ZeroBlock, ZeroBlock},
//            {ZeroBlock, ZeroBlock},
//            {ZeroBlock, ZeroBlock}
//    };
    std::vector<block> pi_Bs = {
            pi0_B, pi1_B,
            pi0_B1, pi1_B1,
            pi0_B2, pi1_B2
    };

    amount_deltas[server_index] = amount_delta;
    amount_deltas[serverIndex1] = amount_delta1;
    amount_deltas[serverIndex2] = amount_delta2;

    amount_As[server_index] = amount_A;
    amount_As[serverIndex1] = amount_A1;
    amount_As[serverIndex2] = amount_A2;

    amount_Amaxs[server_index] = amount_Amax;
    amount_Amaxs[serverIndex1] = amount_Amax1;
    amount_Amaxs[serverIndex2] = amount_Amax2;

    new_balances_A[server_index] = new_balance_A;
    new_balances_A[serverIndex1] = new_balance_A1;
    new_balances_A[serverIndex2] = new_balance_A2;

    tag_share_A_primes[server_index] = tag_share_A_prime;
    tag_share_A_primes[serverIndex1] = tag_share_A_prime1;
    tag_share_A_primes[serverIndex2] = tag_share_A_prime2;

    tag_share_A1_primes[server_index] = tag_share_A1_prime;
    tag_share_A1_primes[serverIndex1] = tag_share_A1_prime1;
    tag_share_A1_primes[serverIndex2] = tag_share_A1_prime2;

    amount_Brs[server_index] = amount_Br;
    amount_Brs[serverIndex1] = amount_Br1;
    amount_Brs[serverIndex2] = amount_Br2;

    localMPCChecks(amount_deltas, amount_As, amount_Amaxs,
                   new_balances_A, tag_share_A_primes, tag_share_A1_primes, amount_Brs, pi_Bs);

    // Finalize the transaction after the MPC round / all checks have passed
    ledger = PIRW::subvff31(ledger, data_A); // TODO: parallelize
    ledger = PIRW::addvff31(ledger, data_B); // TODO: parallelize

//    closeConnections();
}

uint32_t Server::balance(const std::vector<DPF::KeyShare>& key, uint32_t tag_share) {
    auto data = DPF::EvalShamir(key, log2N, server_index).first;
    uint32_t tag_share_prime = PIRW::innerprodff31(alphas, data);

    // TODO: check access in MPC (Open(t-t') == 0)

    uint32_t balance = PIRW::innerprodff31(data, ledger); // In practice, this is the full SumProduct protocol but we will defer it to the end..?
    return balance;
}

void Server::reshare(field beta) {

    // TODO: real protocol without revealing
    auto inputs = std::make_tuple(
            beta // Check that they are equal
    );

    // Run the round of communication
    auto [output1, output2] = run_round(inputs);

    // TODO: refactor this as reconstruct_helper
    // Process the received data
    auto [beta1] = output1;
    auto [beta2] = output2;

    std::vector<uint32_t> betas(3, 0);

    betas[server_index] = beta;
    betas[serverIndex1] = beta1;
    betas[serverIndex2] = beta2;

    auto beta_shares = encode_to_shares(betas);
    auto beta_pt = recover_secret(beta_shares, PP);

    // TODO: fix this because we need uint8s..
    std::vector<std::pair<int64_t, int64_t>> shares = gen_shares(3, 2, beta_pt, PP);
    int b0 = shares[0].second;
    int b1 = (shares[1].second * MODINV2) % PP;
    int b2 = (shares[2].second * MODINV3) % PP;
    int b01 = b0 ^ b1;
    int b12 = b1 ^ b2; // TODO: be consistent with types..

//    auto b_xor_shares = share_gf256_vector({b0, b1, b2, b01, b12}, 3, 1); // TODO: reenable this after fixing

    // TODO: continue here. mainly, I need to send a 'fake round' so the parties realign on the shares (party 0 as the dealer)..
    // Also need to serialize/deserialize these shares using ChatGPT
    // Then need to finish 'fixCodeword'.

}

std::vector<uint8_t> Server::reconstruct_helper_gf256(const std::vector<uint8_t>& shares0, const std::vector<uint8_t>& shares1, const std::vector<uint8_t>& shares2) {
    std::vector<uint8_t> res;

    for (int i=0; i < shares0.size(); i++) {
        std::vector<uint8_t> y(3, 0);

        y[server_index] = shares0[i];
        y[serverIndex1] = shares1[i];
        y[serverIndex2] = shares2[i];
        auto shares = encode_to_shares_gf256(y);

        uint8_t v = reconstruct_gf256(shares);
        res.push_back(v);
    }

    return res;
}

std::vector<int64_t> Server::reconstruct_helper(const std::vector<field>& shares0, const std::vector<field>& shares1, const std::vector<field>& shares2) {
    std::vector<int64_t> res;

    for (int i=0; i < shares0.size(); i++) {
        std::vector<uint32_t> y(3, 0);

        y[server_index] = shares0[i];
        y[serverIndex1] = shares1[i];
        y[serverIndex2] = shares2[i];
        auto shares = encode_to_shares(y);

        field v = recover_secret(shares, PP);
        res.push_back(v);
    }

    return res;
}

std::vector<std::vector<std::pair<uint8_t, uint8_t>>> Server::AtoB(field beta_0, field beta_1, field beta_2) {
    // TODO: real AtoB
    // TODO: receive a vector of beta_0, beta_1, beta_2 of 4 (or 2) values each. For now, only mock it up in the first value
    // Fake zero-sharings of random values

    auto inputs = std::make_tuple(
            beta_0,
            beta_1,
            beta_2
    );

    // Run the round of communication
    auto [output1, output2] = run_round(inputs);
    std::vector<field> share0 = {beta_0, beta_1, beta_2};
    std::vector<field> share1 = {std::get<0>(output1), std::get<1>(output1), std::get<2>(output1)};
    std::vector<field> share2 = {std::get<0>(output2), std::get<1>(output2), std::get<2>(output2)};

    auto betas = reconstruct_helper(share0, share1, share2);
    auto b0 = convertToUint8Vector(betas[0], 16);
    auto b1 = convertToUint8Vector(betas[1], 16);
    auto b2 = convertToUint8Vector(betas[2], 16);

    std::vector<std::vector<std::pair<uint8_t, uint8_t>>> betas_shares = {{}, {}, {}};

    for (int i = 0; i < 16; i++) {
        uint8_t v0 = b0[i] ^ XORRAND0[i][server_index];
        uint8_t v1 = b1[i] ^ XORRAND1[i][server_index];
        uint8_t v2 = b2[i] ^ XORRAND2[i][server_index];

        betas_shares[0].push_back({server_index + 1, v0});
        betas_shares[1].push_back({server_index + 1, v1});
        betas_shares[2].push_back({server_index + 1, v2});
    }

    return betas_shares;
}

std::vector<DPF::KeyShare> Server::fixCodeword(std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare> &key, field beta_0, field beta_1, field beta_2) {
    // TODO: MAC or verify interpolation for malicious security, review this entire function

    auto betas = AtoB(beta_0, beta_1, beta_2);
    auto betas2 = AtoB(beta_0, beta_1, beta_2);

    std::vector<std::pair<uint8_t, uint8_t>> beta0 = betas[0];
    std::vector<std::pair<uint8_t, uint8_t>> beta1 = betas[1];
    std::vector<std::pair<uint8_t, uint8_t>> beta2 = betas[2];

    auto v0_share = fake_xor_rand(server_index); // TODO: really Frand(xor)

    // TODO: remove temp
    auto v0_share1 = fake_xor_rand(0);
    auto v0_share2 = fake_xor_rand(1);
    auto v0_share3 = fake_xor_rand(2);
    std::vector<uint8_t> v0_tmp;
    for (int i=0; i<16; i++) {
        v0_tmp.push_back(reconstruct_gf256({v0_share1[i], v0_share2[i], v0_share3[i]}));
    }
    std::cout << "r: ";
    printVector(v0_tmp);

    // END remove temp

    auto v2_share = xor_shares_vector(beta0, v0_share);

    std::cout << "beta0: ";
    printVector(extract_values_gf256(beta0));
    std::cout << "beta1: ";
    printVector(extract_values_gf256(beta1));
    std::cout << "beta2: ";
    printVector(extract_values_gf256(beta2));

    auto beta01 = xor_shares_vector(beta0, beta1);
    auto beta12 = xor_shares_vector(beta1, beta2);


    // TODO: remove temp

    auto vm2tmp = DPF::EvalFull8M(key.second.key, log2N, 0);
    for (int i = 0; i<10; i++) {
        std::cout << "ZeroDPF (1 - second) at " << i << ": " << vm2tmp[i] << std::endl;
    }

    uint8_t t0_0tmp = key.first.t0_share.second;
    uint8_t t0_1tmp = key.second.t0_share.second;
    std::vector<uint8_t> t0_0tmp_vec;
    std::vector<uint8_t> t0_1tmp_vec;

    t0_0tmp_vec.push_back(t0_0tmp);
    t0_1tmp_vec.push_back(t0_1tmp);

    auto inputstmp = std::make_tuple(
            extract_values_gf256(beta0),
            extract_values_gf256(beta1),
            extract_values_gf256(beta2),
            extract_values_gf256(beta01),
            extract_values_gf256(beta12),
            t0_0tmp_vec,
            t0_1tmp_vec,
            extract_values_gf256(key.first.s0_share),
            extract_values_gf256(key.second.s0_share),
            extract_values_gf256(key.first.s1_share),
            extract_values_gf256(key.second.s1_share)
    );

    // Run the round of communication
    auto [output1tmp, output2tmp] = run_round(inputstmp);

    std::vector<uint8_t> share1tmp = std::get<0>(output1tmp);
    std::vector<uint8_t> share2tmp = std::get<0>(output2tmp);
    auto beta0tmp = reconstruct_helper_gf256(extract_values_gf256(beta0), share1tmp, share2tmp);

    share1tmp = std::get<1>(output1tmp);
    share2tmp = std::get<1>(output2tmp);
    auto beta1tmp = reconstruct_helper_gf256(extract_values_gf256(beta1), share1tmp, share2tmp);

    share1tmp = std::get<2>(output1tmp);
    share2tmp = std::get<2>(output2tmp);
    auto beta2tmp = reconstruct_helper_gf256(extract_values_gf256(beta2), share1tmp, share2tmp);

    share1tmp = std::get<3>(output1tmp);
    share2tmp = std::get<3>(output2tmp);
    auto beta01tmp = reconstruct_helper_gf256(extract_values_gf256(beta01), share1tmp, share2tmp);

    share1tmp = std::get<4>(output1tmp);
    share2tmp = std::get<4>(output2tmp);
    auto beta12tmp = reconstruct_helper_gf256(extract_values_gf256(beta12), share1tmp, share2tmp);

    auto tshare1 = std::get<5>(output1tmp);
    auto tshare2 = std::get<5>(output2tmp);

    std::vector<uint8_t> ytmp(3, 0);

    ytmp[server_index] = t0_0tmp;
    ytmp[serverIndex1] = tshare1[0];
    ytmp[serverIndex2] = tshare2[0];
    auto sharestmp = encode_to_shares_gf256(ytmp);

    uint8_t t0tmp = reconstruct_gf256(sharestmp);

    tshare1 = std::get<6>(output1tmp);
    tshare2 = std::get<6>(output2tmp);

    ytmp[server_index] = t0_1tmp;
    ytmp[serverIndex1] = tshare1[0];
    ytmp[serverIndex2] = tshare2[0];
    sharestmp = encode_to_shares_gf256(ytmp);

    uint8_t t1tmp = reconstruct_gf256(sharestmp);

    share1tmp = std::get<7>(output1tmp);
    share2tmp = std::get<7>(output2tmp);
    auto s0tmp = reconstruct_helper_gf256(extract_values_gf256(key.first.s0_share), share1tmp, share2tmp);

    share1tmp = std::get<8>(output1tmp);
    share2tmp = std::get<8>(output2tmp);
    auto s1tmp = reconstruct_helper_gf256(extract_values_gf256(key.second.s0_share), share1tmp, share2tmp);

    share1tmp = std::get<9>(output1tmp);
    share2tmp = std::get<9>(output2tmp);
    auto s1_0tmp = reconstruct_helper_gf256(extract_values_gf256(key.first.s1_share), share1tmp, share2tmp);

    share1tmp = std::get<10>(output1tmp);
    share2tmp = std::get<10>(output2tmp);
    auto s1_1tmp = reconstruct_helper_gf256(extract_values_gf256(key.second.s1_share), share1tmp, share2tmp);

    std::cout << "t0 (0) reconstructed: " << static_cast<int>(t0tmp) << ", t0 (1): " << static_cast<int>(t1tmp) << std::endl;

    std::cout << "beta0 reconstructed: ";
    printVector(beta0tmp);
    std::cout << "beta1 reconstructed: ";
    printVector(beta1tmp);
    std::cout << "beta2 reconstructed: ";
    printVector(beta2tmp);
    std::cout << "beta0 XOR beta1 reconstructed: ";
    printVector(beta01tmp);
    std::cout << "beta1 XOR beta2 reconstructed: ";
    printVector(beta12tmp);
    std::cout << "s0 (0) reconstructed: ";
    printVector(s0tmp);
    std::cout << "s0 (1) reconstructed: ";
    printVector(s1tmp);
    std::cout << "s1 (0) reconstructed: ";
    printVector(s1_0tmp);
    std::cout << "s1 (1) reconstructed: ";
    printVector(s1_1tmp);
    // END REMOVE TEMP


    std::cout << "beta01: ";
    printVector(extract_values_gf256(beta01));
    std::cout << "beta12: ";
    printVector(extract_values_gf256(beta12));

    auto s01 = xor_shares_vector(key.first.s0_share, key.first.s1_share);
    std::cout << "s0 xor s1 for DPF0: ";
    printVector(extract_values_gf256(s01));
    auto ocw0_share = xor_shares_vector(s01, beta01);
    s01 = xor_shares_vector(key.second.s0_share, key.second.s1_share);
    auto ocw1_share = xor_shares_vector(s01, beta12);
    std::cout << "s0 xor s1 for DPF1: ";
    printVector(extract_values_gf256(beta2));

    std::vector<uint8_t> ocw0_serialized_share = extract_values_gf256(ocw0_share);
    std::vector<uint8_t> ocw1_serialized_share = extract_values_gf256(ocw1_share);

    auto inputs = std::make_tuple(
            ocw0_serialized_share,
            ocw1_serialized_share
    );

    // Run the round of communication
    auto [output1, output2] = run_round(inputs);
    std::vector<uint8_t> share1 = std::get<0>(output1);
    std::vector<uint8_t> share2 = std::get<0>(output2);
    auto ocw0 = reconstruct_helper_gf256(ocw0_serialized_share, share1, share2);

    share1 = std::get<1>(output1);
    share2 = std::get<1>(output2);
    auto ocw1 = reconstruct_helper_gf256(ocw1_serialized_share, share1, share2);


//     Debug info
    std::cout << "ocw0: ";
    for (int i = 0; i < ocw0.size(); i++) {
        std::cout << static_cast<int>(ocw0[i]) << ", ";
    }
    std::cout << std::endl;

    std::cout << "ocw1: ";
    for (int i = 0; i < ocw1.size(); i++) {
        std::cout << static_cast<int>(ocw1[i]) << ", ";
    }
    std::cout << std::endl;

    uint8_t t0_0 = key.first.t0_share.second;
    uint8_t t0_1 = key.second.t0_share.second;
    std::vector<std::pair<uint8_t, uint8_t>> tocw0, tocw1, tocw_fake, tocw1_fake;
    for (int i = 0; i < 16; i++) {
        // Conditional addition of ocw (i.e., [t] * ocw)
        tocw0.push_back(std::make_pair( server_index + 1, t0_0 & ocw0[i]) );
        tocw1.push_back(std::make_pair( server_index + 1, t0_1 & ocw1[i]) );
        tocw_fake.push_back(std::make_pair( server_index + 1, ocw0[i]) );
        tocw1_fake.push_back(std::make_pair( server_index + 1, ocw1[i]) );
    }

//    auto beta0s0_0 = xor_shares_vector(beta0, key.first.s0_share);
//    auto beta2s0_1 = xor_shares_vector(beta2, key.second.s0_share);
    auto v0s0_0 = xor_shares_vector(v0_share, key.first.s0_share);
    auto v2s0_1 = xor_shares_vector(v2_share, key.second.s0_share);
//    auto z0_share = extract_values_gf256(xor_shares_vector(beta0s0_0, tocw0));
//    auto z1_share = extract_values_gf256(xor_shares_vector(beta1s0_1, tocw1));
    auto z0_share = extract_values_gf256(xor_shares_vector(v0s0_0, tocw_fake)); // TODO: these are temp
//    auto z1_share = extract_values_gf256(v2s0_1);
    auto z1_share = extract_values_gf256(xor_shares_vector(v2s0_1, tocw1_fake)); // TODO: this works, which prob means I am taking the wrong t's. Clean up and understand - can print out whatever shamir eval gets as the right t.. or simply do LSB of the seeds..

    auto inputs2 = std::make_tuple(
            z0_share,
            z1_share
    );

    // Run the round of communication
    auto [output11, output12] = run_round(inputs2);
    share1 = std::get<0>(output11);
    share2 = std::get<0>(output12);
    auto z0 = reconstruct_helper_gf256(z0_share, share1, share2);

    share1 = std::get<1>(output11);
    share2 = std::get<1>(output12);
    auto z1 = reconstruct_helper_gf256(z1_share, share1, share2);

    DPF::KeyShare key0, key1; // Note that these are the two underlying DPF+ keys for this party only
    key0.key = key.first.key;
    std::copy(ocw0.begin(), ocw0.end(), key0.key.end() - 16); // replace the output CW
    // TODO: need to fix the problem that z is uint32, where here its 128bit.. This is a problem with packing..
    key0.z = convertToUint32(z0);
    std::cout << "z0 is : " << key0.z << std::endl;
//    key0.z = 0; // TODO: remove temp

    key1.key = key.second.key;
    std::copy(ocw1.begin(), ocw1.end(), key1.key.end() - 16); // replace the output CW
    // TODO: need to fix the problem that z is uint32, where here its 128bit.. This is a problem with packing..
    key1.z = convertToUint32(z1);
    std::cout << "z1 is : " << key1.z << std::endl;
//    key1.z = 0; // TODO: remove temp
    return {key0, key1};
}

// This is an implementation of TDDPF.BEval. It's in Server as it's a protocol.
// However, the goal isn't to call this ad-hoc. This is just for tests and benchmarking.
void Server::evalDeferred(std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare>& key, field beta_0, field beta_1, field beta_2) {
    std::cout << "Starting with OCW: ";
    for (int i = 0; i < 16; i++) {
        std::cout << static_cast<int>(key.first.key[key.first.key.size() - 16 + i]) << ", ";
    }
    std::cout << "and key size=: " << key.first.key.size() << std::endl;;

    auto fullkey = fixCodeword(key, beta_0, beta_1, beta_2);

    std::cout << "New OCW: ";
    for (int i = 0; i < 16; i++) {
        std::cout << static_cast<int>(fullkey[0].key[fullkey[0].key.size() - 16 + i]) << ", ";
    }
    std::cout << "and key size=: " << fullkey[0].key.size() << std::endl;;

    // TODO: remove temp? - test key..
    auto vms = DPF::EvalShamir(fullkey, log2N, server_index).first;

    for (int i = 0; i<10; i++) {
        std::cout << "Value at " << i << ": " << vms[i] << std::endl;
    }

    // TODO: remove temp
    std::cout << "Sanity checks below.." << std::endl;

    bool idx = 0;

    if (server_index > 0) {
        idx = 1;
    }
    auto vm1 = DPF::EvalFull8M(key.first.key, log2N, idx);
    idx = 0;

    if (server_index == 2) {
        idx = 1;
    }
    std::cout << "Server index: " << server_index << ", idx: " << idx << std::endl;

    auto vm2 = DPF::EvalFull8M(key.second.key, log2N, idx);
    for (int i = 0; i<10; i++) {
        std::cout << "DPF0 Value at " << i << ": " << vm1[i] << std::endl;
        std::cout << "DPF1 Value at " << i << ": " << vm2[i] << std::endl;
    }


    idx = 0;

    if (server_index > 0) {
        idx = 1;
    }
    auto vm11 = DPF::EvalFull8M(fullkey[0].key, log2N, idx);
    idx = 0;

    if (server_index == 2) {
        idx = 1;
    }
    auto vm12 = DPF::EvalFull8M(fullkey[1].key, log2N, idx);
    for (int i = 0; i<10; i++) {
        std::cout << "DPF1 0 Value at " << i << ": " << vm11[i] << std::endl;
        std::cout << "DPF1 1 Value at " << i << ": " << vm12[i] << std::endl;
    }


}