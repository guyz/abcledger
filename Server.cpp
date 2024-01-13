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
#include <future>

#include <sstream>
#include <variant>
#include <vector>
#include <cstring>
#include "DebugPrint.h"

// TODO: Things to implement:
// Frand, Fzero, Fltz, PRZS, PRSS

Server::Server(int index, size_t N, bool local) : N(N), server_index(index), ledger(N), alphas(N), xorrand(N_RANDS), xorzero(N_RANDS), randt(N_RANDS), rand2t(N_RANDS), zero2t(N_RANDS) {
    log2N = static_cast<int>(std::log2(N));

    std::ifstream ledgerFile(DATA_DIR + "ledger-" + std::to_string(server_index + 1) + ".txt");
    std::ifstream alphasFile(DATA_DIR + "alphas-" + std::to_string(server_index + 1) + ".txt");
    std::ifstream xorFile(DATA_DIR + "xor-" + std::to_string(server_index + 1) + ".txt");
    std::ifstream xorzeroFile(DATA_DIR + "xorzero-" + std::to_string(server_index + 1) + ".txt");
    std::ifstream randtFile(DATA_DIR + "randt-" + std::to_string(server_index + 1) + ".txt");
    std::ifstream rand2tFile(DATA_DIR + "rand2t-" + std::to_string(server_index + 1) + ".txt");
    std::ifstream zero2tFile(DATA_DIR + "zero2t-" + std::to_string(server_index + 1) + ".txt");

    // If any file doesn't exist, call initData
    if (!ledgerFile || !alphasFile) {
        initData(N);
    }

    if (!xorFile) {
        initPreprocessingData();
    }

    if (xorFile) {
        for (uint8_t &value : xorrand) {
            uint32_t temp;
            xorFile >> temp;  // Read a uint32_t value from the file

            // Convert the uint32_t to uint8_t
            // This example takes the least significant byte.
            value = static_cast<uint8_t>(temp & 0xFF);
        }
        xorFile.close();
    }

    if (xorzeroFile) {
        for (uint8_t &value : xorzero) {
            uint32_t temp;
            xorzeroFile >> temp;  // Read a uint32_t value from the file

            // Convert the uint32_t to uint8_t
            // This example takes the least significant byte.
            value = static_cast<uint8_t>(temp & 0xFF);
        }
        xorzeroFile.close();
    }

    if (randtFile) {
        for (uint32_t &value : randt) {
            randtFile >> value;
        }
        randtFile.close();
    }

    if (rand2tFile) {
        for (uint32_t &value : rand2t) {
            rand2tFile >> value;
        }
        rand2tFile.close();
    }

    if (zero2tFile) {
        for (uint32_t &value : zero2t) {
            zero2tFile >> value;
        }
        zero2tFile.close();
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
    if (!local) {
        initNetworking();
    }
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

void Server::initPreprocessingData() {
    int n = N_RANDS;
    RandData randData = generate_random_sharings(n, PP, 123);

//    std::vector<uint8_t> ;
    std::vector<uint32_t> xor1, xor2, xor3, xorzero1, xorzero2, xorzero3, randt1, randt2, randt3, rand2t1, rand2t2, rand2t3, zero2t1, zero2t2, zero2t3;

    for (int i = 0; i < n; i++) {
        xor1.push_back(randData.xor_rands[i][0]);
        xor2.push_back(randData.xor_rands[i][1]);
        xor3.push_back(randData.xor_rands[i][2]);

        xorzero1.push_back(randData.xor_zeros[i][0]);
        xorzero2.push_back(randData.xor_zeros[i][1]);
        xorzero3.push_back(randData.xor_zeros[i][2]);

        randt1.push_back(randData.rands_degt[i][0]);
        randt2.push_back(randData.rands_degt[i][1]);
        randt3.push_back(randData.rands_degt[i][2]);

        rand2t1.push_back(randData.rands_deg2t[i][0]);
        rand2t2.push_back(randData.rands_deg2t[i][1]);
        rand2t3.push_back(randData.rands_deg2t[i][2]);

        zero2t1.push_back(randData.zeros_deg2t[i][0]);
        zero2t2.push_back(randData.zeros_deg2t[i][1]);
        zero2t3.push_back(randData.zeros_deg2t[i][2]);
    }

    saveToFile(xor1, DATA_DIR + "xor-1.txt");
    saveToFile(xor2, DATA_DIR + "xor-2.txt");
    saveToFile(xor3, DATA_DIR + "xor-3.txt");

    saveToFile(xorzero1, DATA_DIR + "xorzero-1.txt");
    saveToFile(xorzero2, DATA_DIR + "xorzero-2.txt");
    saveToFile(xorzero3, DATA_DIR + "xorzero-3.txt");

    saveToFile(randt1, DATA_DIR + "randt-1.txt");
    saveToFile(randt2, DATA_DIR + "randt-2.txt");
    saveToFile(randt3, DATA_DIR + "randt-3.txt");

    saveToFile(rand2t1, DATA_DIR + "rand2t-1.txt");
    saveToFile(rand2t2, DATA_DIR + "rand2t-2.txt");
    saveToFile(rand2t3, DATA_DIR + "rand2t-3.txt");

    saveToFile(rand2t1, DATA_DIR + "zero2t-1.txt");
    saveToFile(rand2t2, DATA_DIR + "zero2t-2.txt");
    saveToFile(rand2t3, DATA_DIR + "zero2t-3.txt");
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
    field r = randt[rand_counter];
    rand_counter = (rand_counter + 1) % randt.size();
    return r;
}

uint8_t Server::XORPRSS() {
    uint8_t r = xorrand[rand_counter];
    rand_counter = (rand_counter + 1) % randt.size();
    return r;
}

// NOTE: this is degree t - not 2t
uint8_t Server::XORPRZS() {
    uint8_t r = xorzero[rand_counter];
    rand_counter = (rand_counter + 1) % randt.size();
    return r;
}

std::pair<field, field> Server::PRSS2() {
    field r = randt[rand_counter];
    field r2 = rand2t[rand_counter];
    rand_counter = (rand_counter + 1) % randt.size();
    return std::make_pair(r, r2);
}

field Server::PRZS() {
    field z = zero2t[rand_counter];
    rand_counter = (rand_counter + 1) % randt.size();
    return z;
}

field Server::SinglePRSS() {
    // TODO: real PRSS
    field rs[3] = {153685505, 402498915, 651312325};
    return rs[server_index];
}

std::pair<field, field> Server::SinglePRSS2() {
    // TODO: real PRSS2
    field rs[3] = {153685505, 402498915, 651312325};
    field rs2[3] = {1270736703, 985527247, 1196727374};
    field r = rs[server_index];
    field r2 = rs2[server_index];
    return std::make_pair(r, r2);
}

field Server::SinglePRZS() {
    // TODO: real PRZS
    field rs[3] = {299355974, 2120311573, 1167899503};
    return rs[server_index];
}

bool Server::LTZ(std::vector<std::pair<int64_t, int64_t>> shares) {
    auto x = recover_secret(shares, PP);
    return x < 0;
}

template <typename... Args>
std::pair<std::tuple<Args...>, std::tuple<Args...>> Server::run_round(const std::tuple<Args...>& inputs) {
    // Serialize the data
    std::string dataToSend = serializeData(inputs);

    // Send data to other servers
    send(connectionHandler1, dataToSend.c_str(), dataToSend.size(), 0);
    send(connectionHandler2, dataToSend.c_str(), dataToSend.size(), 0);

    // Logging sent data - optional
    // std::apply([](const auto&... args) { ((debugPrint << args << ", "), ...); }, inputs);
    debugPrint << "Server" << server_index << " has sent data." << std::endl;

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

    //// Semi-honest to malicious changes:
    // 1. Randomize all inputs (check in the code) and batchverify at the end
    // 2. FCheckZero gates compared to Open() for zero tests
    // 3. Amount_Br validity - might as well leave it in the code..
    // Conclusion: can just write the semi-honest protocol with a couple of notes in another color..

    // Expand DPFs
    // TODO: could/should be parallelized?
    auto res_A = DPF::EvalShamir(key_A, log2N, server_index, false);
    auto res_A1 = DPF::EvalShamir(key_A1, log2N, server_index, false);
    auto res_B = DPF::EvalShamir(key_B, log2N, server_index, true);

    auto data_A = std::move(res_A.first);
    auto data_A1 = std::move(res_A1.first);
    auto data_B = std::move(res_B.first);
    auto pi_B = std::move(res_B.second);
    block pi0_B = pi_B[0];
    block pi1_B = pi_B[1]; // TODO: check pis..

    debugPrint << "A[0]: " << data_A[0] << std::endl;
//    debugPrint << "pi_B: " << pi_B[0][0] << std::endl;
    debugPrint << "alphas[0]: " << alphas[0] << std::endl; // TODO: remove temp

    field amount_A = PIRW::sumvecff31(data_A);
    field amount_B = PIRW::sumvecff31(data_B);

    // FProduct gates
    field tag_share_A_prime = mod(static_cast<int64_t>(PIRW::innerprodff31(alphas, data_A)), PP);
    field tag_share_A1_prime = mod(static_cast<int64_t>(PIRW::innerprodff31(alphas, data_A1)), PP);
    field balance_A = mod(static_cast<int64_t>(PIRW::innerprodff31(data_A1, ledger)), PP);

    auto outputs = multfproduct_open({ tag_share_A_prime, tag_share_A1_prime, balance_A });

    // refresh shares
    tag_share_A_prime = outputs[0];
    tag_share_A1_prime = outputs[1];
    balance_A = outputs[2];

    debugPrint << "Finished Fproduct gates" << std::endl;

    field tag_delta_A_share = mod(static_cast<int64_t>(tag_A_share) - tag_share_A_prime, PP);
    field tag_delta_A1_share = mod(static_cast<int64_t>(tag_A1_share) - tag_share_A1_prime, PP);
    field amount_delta = mod(static_cast<int64_t>(amount_A) - amount_B, PP);

    field amount_Amax = mod(static_cast<int64_t>(amount_A) - MAX_VALID_INT, PP);
    field new_balance_A = mod(static_cast<int64_t>(balance_A) - amount_A, PP);

    // Note: need to check for Pi_B.. I think just in semi honest in lieu of other tests..
    field r = PRSS();
    field amount_Br = mod(static_cast<int64_t>(amount_B) + r, PP);

    // Check Zero by opening. In semi-honest no need for FCheckZero, because if a users cheats then the adv already has the data. Saves one round
    // TODO: do we need to open and verify the degree? or just open? Same repeating question.. In semi-honest I think def not..
    auto inputs = std::make_tuple(
            tag_delta_A_share,
            tag_delta_A1_share,
            amount_delta,
            amount_A, // Input to FLTZ(amount_A)
            amount_Amax, // Input to FLTZ(amount_A - MAX_VALID_INT)
            new_balance_A, // Input to FLTZ(balance_A - amount_A)
            amount_Br,
            pi0_B, pi1_B // DPF B proof part 1
    );

    // Run the round of communication
    auto [output1, output2] = run_round(inputs);

    // Process the received data
    auto [zero_check_a_1, zero_check_b_1, zero_check_c_1, amount_A1, amount_Amax1, new_balance_A1, amount_Br1, pi0_B1, pi1_B1] = output1;
    auto [zero_check_a_2, zero_check_b_2, zero_check_c_2, amount_A2, amount_Amax2, new_balance_A2, amount_Br2, pi0_B2, pi1_B2] = output2;
    debugPrint << "Finished CheckZero Round 2" << std::endl;

    // Run Access Control checks
    // TODO: check Pis..

    // Reconstruct
    std::vector<field> shares0 = {tag_delta_A_share, tag_delta_A1_share, amount_delta, amount_A, amount_Amax, new_balance_A, amount_Br};
    std::vector<field> shares1 = {zero_check_a_1, zero_check_b_1, zero_check_c_1, amount_A1, amount_Amax1, new_balance_A1, amount_Br1};
    std::vector<field> shares2 = {zero_check_a_2, zero_check_b_2, zero_check_c_2, amount_A2, amount_Amax2, new_balance_A2, amount_Br2};
    auto reconstructed = reconstruct_helper(shares0, shares1, shares2);

    assert(reconstructed[0] == 0);
    assert(reconstructed[1] == 0);
    assert(reconstructed[2] == 0);
    assert(reconstructed[3] >= 0 && reconstructed[3] < MAX_VALID_INT);
    assert(reconstructed[4] > MAX_VALID_INT); // Because in the field we only should encode unsigned numbers.. TODO: be consistent about this.. probably best to move to uint everywhere ..
    assert(reconstructed[5] >= 0 && reconstructed[3] < MAX_VALID_INT);

    // Finalize the transaction after the MPC round / all checks have passed
    ledger = PIRW::subvff31(ledger, data_A); // TODO: parallelize
    ledger = PIRW::addvff31(ledger, data_B); // TODO: parallelize

    debugPrint << "Transfer (semi-honest) succeeded!" << std::endl;

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
        uint8_t r1 = XORPRZS();
        uint8_t r2 = XORPRZS();
        uint8_t r3 = XORPRZS();

        uint8_t v0 = b0[i] ^ r1;
        uint8_t v1 = b1[i] ^ r2;
        uint8_t v2 = b2[i] ^ r3;

//        uint8_t v0 = b0[i] ^ XORRAND0[i][server_index];
//        uint8_t v1 = b1[i] ^ XORRAND1[i][server_index];
//        uint8_t v2 = b2[i] ^ XORRAND2[i][server_index];

        betas_shares[0].push_back({server_index + 1, v0});
        betas_shares[1].push_back({server_index + 1, v1});
        betas_shares[2].push_back({server_index + 1, v2});
    }

    return betas_shares;
}

std::vector<DPF::KeyShare> Server::fixCodeword(std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare> &key, field beta_0, field beta_1, field beta_2) {
    // TODO: MAC or verify interpolation for malicious security, review this entire function

    auto betas = AtoB(beta_0, beta_1, beta_2);

    std::vector<std::pair<uint8_t, uint8_t>> beta0 = betas[0];
    std::vector<std::pair<uint8_t, uint8_t>> beta1 = betas[1];
    std::vector<std::pair<uint8_t, uint8_t>> beta2 = betas[2];

    auto v0_share = fake_xor_rand(server_index); // TODO: really Frand(xor) - still need to fix this..
    auto v2_share = xor_shares_vector(beta0, v0_share);

//    debugPrint << "beta0: ";
//    printVector(extract_values_gf256(beta0));
//    debugPrint << "beta1: ";
//    printVector(extract_values_gf256(beta1));
//    debugPrint << "beta2: ";
//    printVector(extract_values_gf256(beta2));

    auto beta01 = xor_shares_vector(beta0, beta1);
    auto beta12 = xor_shares_vector(beta1, beta2);


//    // TODO: remove temp
//
//    auto vm2tmp = DPF::EvalFull8M(key.second.key, log2N, 0);
//    for (int i = 0; i<10; i++) {
//        debugPrint << "ZeroDPF (1 - second) at " << i << ": " << vm2tmp[i] << std::endl;
//    }
//
//    uint8_t t0_0tmp = key.first.t0_share.second;
//    uint8_t t0_1tmp = key.second.t0_share.second;
//    std::vector<uint8_t> t0_0tmp_vec;
//    std::vector<uint8_t> t0_1tmp_vec;
//
//    t0_0tmp_vec.push_back(t0_0tmp);
//    t0_1tmp_vec.push_back(t0_1tmp);
//
//    auto inputstmp = std::make_tuple(
//            extract_values_gf256(beta0),
//            extract_values_gf256(beta1),
//            extract_values_gf256(beta2),
//            extract_values_gf256(beta01),
//            extract_values_gf256(beta12),
//            t0_0tmp_vec,
//            t0_1tmp_vec,
//            extract_values_gf256(key.first.s0_share),
//            extract_values_gf256(key.second.s0_share),
//            extract_values_gf256(key.first.s1_share),
//            extract_values_gf256(key.second.s1_share)
//    );
//
//    // Run the round of communication
//    auto [output1tmp, output2tmp] = run_round(inputstmp);
//
//    std::vector<uint8_t> share1tmp = std::get<0>(output1tmp);
//    std::vector<uint8_t> share2tmp = std::get<0>(output2tmp);
//    auto beta0tmp = reconstruct_helper_gf256(extract_values_gf256(beta0), share1tmp, share2tmp);
//
//    share1tmp = std::get<1>(output1tmp);
//    share2tmp = std::get<1>(output2tmp);
//    auto beta1tmp = reconstruct_helper_gf256(extract_values_gf256(beta1), share1tmp, share2tmp);
//
//    share1tmp = std::get<2>(output1tmp);
//    share2tmp = std::get<2>(output2tmp);
//    auto beta2tmp = reconstruct_helper_gf256(extract_values_gf256(beta2), share1tmp, share2tmp);
//
//    share1tmp = std::get<3>(output1tmp);
//    share2tmp = std::get<3>(output2tmp);
//    auto beta01tmp = reconstruct_helper_gf256(extract_values_gf256(beta01), share1tmp, share2tmp);
//
//    share1tmp = std::get<4>(output1tmp);
//    share2tmp = std::get<4>(output2tmp);
//    auto beta12tmp = reconstruct_helper_gf256(extract_values_gf256(beta12), share1tmp, share2tmp);
//
//    auto tshare1 = std::get<5>(output1tmp);
//    auto tshare2 = std::get<5>(output2tmp);
//
//    std::vector<uint8_t> ytmp(3, 0);
//
//    ytmp[server_index] = t0_0tmp;
//    ytmp[serverIndex1] = tshare1[0];
//    ytmp[serverIndex2] = tshare2[0];
//    auto sharestmp = encode_to_shares_gf256(ytmp);
//
//    uint8_t t0tmp = reconstruct_gf256(sharestmp);
//
//    tshare1 = std::get<6>(output1tmp);
//    tshare2 = std::get<6>(output2tmp);
//
//    ytmp[server_index] = t0_1tmp;
//    ytmp[serverIndex1] = tshare1[0];
//    ytmp[serverIndex2] = tshare2[0];
//    sharestmp = encode_to_shares_gf256(ytmp);
//
//    uint8_t t1tmp = reconstruct_gf256(sharestmp);
//
//    share1tmp = std::get<7>(output1tmp);
//    share2tmp = std::get<7>(output2tmp);
//    auto s0tmp = reconstruct_helper_gf256(extract_values_gf256(key.first.s0_share), share1tmp, share2tmp);
//
//    share1tmp = std::get<8>(output1tmp);
//    share2tmp = std::get<8>(output2tmp);
//    auto s1tmp = reconstruct_helper_gf256(extract_values_gf256(key.second.s0_share), share1tmp, share2tmp);
//
//    share1tmp = std::get<9>(output1tmp);
//    share2tmp = std::get<9>(output2tmp);
//    auto s1_0tmp = reconstruct_helper_gf256(extract_values_gf256(key.first.s1_share), share1tmp, share2tmp);
//
//    share1tmp = std::get<10>(output1tmp);
//    share2tmp = std::get<10>(output2tmp);
//    auto s1_1tmp = reconstruct_helper_gf256(extract_values_gf256(key.second.s1_share), share1tmp, share2tmp);
//
//    debugPrint << "t0 (0) reconstructed: " << static_cast<int>(t0tmp) << ", t0 (1): " << static_cast<int>(t1tmp) << std::endl;
//
//    debugPrint << "beta0 reconstructed: ";
//    printVector(beta0tmp);
//    debugPrint << "beta1 reconstructed: ";
//    printVector(beta1tmp);
//    debugPrint << "beta2 reconstructed: ";
//    printVector(beta2tmp);
//    debugPrint << "beta0 XOR beta1 reconstructed: ";
//    printVector(beta01tmp);
//    debugPrint << "beta1 XOR beta2 reconstructed: ";
//    printVector(beta12tmp);
//    debugPrint << "s0 (0) reconstructed: ";
//    printVector(s0tmp);
//    debugPrint << "s0 (1) reconstructed: ";
//    printVector(s1tmp);
//    debugPrint << "s1 (0) reconstructed: ";
//    printVector(s1_0tmp);
//    debugPrint << "s1 (1) reconstructed: ";
//    printVector(s1_1tmp);
    // END REMOVE TEMP


//    debugPrint << "beta01: ";
//    printVector(extract_values_gf256(beta01));
//    debugPrint << "beta12: ";
//    printVector(extract_values_gf256(beta12));

    auto s01 = xor_shares_vector(key.first.s0_share, key.first.s1_share);
//    debugPrint << "s0 xor s1 for DPF0: ";
//    printVector(extract_values_gf256(s01));
    auto ocw0_share = xor_shares_vector(s01, beta01);
    s01 = xor_shares_vector(key.second.s0_share, key.second.s1_share);
    auto ocw1_share = xor_shares_vector(s01, beta12);
//    debugPrint << "s0 xor s1 for DPF1: ";
//    printVector(extract_values_gf256(beta2));

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
//    debugPrint << "ocw0: ";
//    for (int i = 0; i < ocw0.size(); i++) {
//        debugPrint << static_cast<int>(ocw0[i]) << ", ";
//    }
//    debugPrint << std::endl;
//
//    debugPrint << "ocw1: ";
//    for (int i = 0; i < ocw1.size(); i++) {
//        debugPrint << static_cast<int>(ocw1[i]) << ", ";
//    }
//    debugPrint << std::endl;

    uint8_t t0_0 = key.first.t0_share.second;
    uint8_t t0_1 = key.second.t0_share.second;
    std::vector<std::pair<uint8_t, uint8_t>> tocw0, tocw1;
    for (int i = 0; i < 16; i++) {
        // Conditional addition of ocw (i.e., [t] * ocw)
        tocw0.push_back(std::make_pair( server_index + 1, t0_0 & ocw0[i]) ); // TODO: fix this
        tocw1.push_back(std::make_pair( server_index + 1, t0_1 & ocw1[i]) ); // TODO: fix this
    }

    auto v0s0_0 = xor_shares_vector(v0_share, key.first.s0_share);
    auto v2s0_1 = xor_shares_vector(v2_share, key.second.s0_share);
//    auto z0_share = extract_values_gf256(xor_shares_vector(v0s0_0, tocw_fake)); // TODO: these are temp
//    auto z1_share = extract_values_gf256(xor_shares_vector(v2s0_1, tocw1_fake)); // TODO: this works, which prob means I am taking the wrong t's. Clean up and understand - can print out whatever shamir eval gets as the right t.. or simply do LSB of the seeds..

    auto z0_share = extract_values_gf256(xor_shares_vector(v0s0_0, tocw0)); // TODO: reenable
    auto z1_share = extract_values_gf256(xor_shares_vector(v2s0_1, tocw1)); // TODO: reenable

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
    debugPrint << "z0 is : " << key0.z << std::endl;

    key1.key = key.second.key;
    std::copy(ocw1.begin(), ocw1.end(), key1.key.end() - 16); // replace the output CW
    // TODO: need to fix the problem that z is uint32, where here its 128bit.. This is a problem with packing..
    key1.z = convertToUint32(z1);
    debugPrint << "z1 is : " << key1.z << std::endl;
    return {key0, key1};
}

// This is an implementation of TDDPF.BEval - including verbose printouts to test correctness. It's in Server as it's a protocol.
// However, the goal isn't to call this ad-hoc. This is just for tests and benchmarking.
void Server::evalDeferredTest(std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare>& key, field beta_0, field beta_1, field beta_2) {
    debugPrint << "Starting with OCW: ";
    for (int i = 0; i < 16; i++) {
        debugPrint << static_cast<int>(key.first.key[key.first.key.size() - 16 + i]) << ", ";
    }
    debugPrint << "and key size=: " << key.first.key.size() << std::endl;;

    debugPrint << "Starting with OCW1: ";
    for (int i = 0; i < 16; i++) {
        debugPrint << static_cast<int>(key.second.key[key.second.key.size() - 16 + i]) << ", ";
    }
    debugPrint << "and key size=: " << key.second.key.size() << std::endl;;

    auto fullkey = fixCodeword(key, beta_0, beta_1, beta_2);


    debugPrint << "New OCW: ";
    for (int i = 0; i < 16; i++) {
        debugPrint << static_cast<int>(fullkey[0].key[fullkey[0].key.size() - 16 + i]) << ", ";
    }
    debugPrint << "and key size=: " << fullkey[0].key.size() << std::endl;

    debugPrint << "New OCW1: ";
    for (int i = 0; i < 16; i++) {
        debugPrint << static_cast<int>(fullkey[1].key[fullkey[1].key.size() - 16 + i]) << ", ";
    }
    debugPrint << std::endl;

    auto vms = DPF::EvalShamir(fullkey, log2N, server_index).first;

    for (int i = 0; i<50; i++) {
        debugPrint << "Value at " << i << ": " << vms[i] << std::endl;
    }

    debugPrint << "Sanity checks below.." << std::endl;

    bool idx = 0;

    if (server_index > 0) {
        idx = 1;
    }
    auto vm1 = DPF::EvalFull8M(key.first.key, log2N, idx);
    idx = 0;

    if (server_index == 2) {
        idx = 1;
    }
    debugPrint << "Server index: " << server_index << ", idx: " << idx << std::endl;

    auto vm2 = DPF::EvalFull8M(key.second.key, log2N, idx);
    for (int i = 0; i<50; i++) {
        debugPrint << "DPF0 Value at " << i << ": " << vm1[i] << std::endl;
        debugPrint << "DPF1 Value at " << i << ": " << vm2[i] << std::endl;
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
    for (int i = 0; i<50; i++) {
        debugPrint << "DPF1 0 Value at " << i << ": " << vm11[i] << std::endl;
        debugPrint << "DPF1 1 Value at " << i << ": " << vm12[i] << std::endl;
    }


}

// This is an implementation of TDDPF.BEval. It's in Server as it's a protocol.
// However, the goal isn't to call this ad-hoc. This is just for tests and benchmarking.
std::pair<std::vector<uint32_t>, std::array<block, 2>> Server::evalDeferred(std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare>& key, field beta_0, field beta_1, field beta_2) {
    auto fullkey = fixCodeword(key, beta_0, beta_1, beta_2);
    return DPF::EvalShamir(fullkey, log2N, server_index);
}


// Batch FMult Gates
std::vector<field> Server::multgate_helper(std::vector<field> inputs1, std::vector<field> inputs2) {
    std::vector<field> mult;
    for (int i = 0; i < inputs1.size(); i++) {
        mult.push_back(mod(static_cast<int64_t>(inputs1[i])*inputs2[i], PP));
    }

    return multfproduct_open(mult);
}

// Batch FMult/FProduct gates degree-reduction round
std::vector<field> Server::multfproduct_open(std::vector<field> inputs) {
    std::vector<field> rand_inputs, randt_shares;

    for (int i = 0; i < inputs.size(); i++) {
        auto rr = PRSS2();
        randt_shares.push_back(rr.first);
        rand_inputs.push_back(mod(static_cast<int64_t>(inputs[i]) + rr.second, PP));
//        debugPrint << "FmultOpen: inputs[" << i << "]: " << inputs[i] << std::endl;
//        debugPrint << "FmultOpen: rand_inputs[" << i << "]: " << rand_inputs[i] << std::endl;
    }


    // Run the round of communication
    auto [output1, output2] = run_round(make_tuple(rand_inputs));

    // Process the received data
    auto [rand_outputs1] = output1;
    auto [rand_outputs2] = output2;

    // Finalize FMult/Fproduct Gate
    std::vector<field> outputs;
    auto rand_outputs = reconstruct_helper(rand_inputs, rand_outputs1, rand_outputs2);
    for (int i = 0; i < rand_outputs.size(); i++) {
        int64_t ro = mod(rand_outputs[i], PP);
//        debugPrint << "FmultOpen: rand_outputs[" << i << "]: " << ro << std::endl;
        auto res_i = mod(ro - randt_shares[i], PP);
        outputs.push_back(res_i);
//        debugPrint << "FmultOpen: outputs[" << i << "]: " << res_i << std::endl;
    }

    return outputs;
}

// Malicious version of the protocol
void Server::transferMalicious(const std::vector<DPF::KeyShare>& key_A,
                               std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare>& deferredKey_A,
                               const std::vector<DPF::KeyShare>& key_A1,
                               std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare>& deferredKey_A1,
                               const std::vector<DPF::KeyShare>& key_B,
                               field tag_A_share, field tag_A1_share,
                               field amount_0, field amount_1, field amount_2,
                               field one_0, field one_1, field one_2) {

    //// Protocol description. Items in the same line --> round happens in parallel (or they have the same context)
    // TODO: figure out what is not a must for security.. Open stuff:
    // 1. Do we need to check amount_A, amount_B, ones_i and amount_i inputs? I think we do because they go through Fmult to randomize and adv can cheat..
    // 2. Do we need to check [[one]] = 1 or if [[amount]] == [[amount_A]] and [[amount_B]]? TODO: need to make sure TDDPF is maliciously secure first.. Assuming it is, then I think we're covered on [amount], but not sure about [one] - to see why, imagine what happens if the client shares [two] instead - they can make the balance appear larger no?
    // 3. Do we need to FCheckZero instead of partial openings for access control? See below..
    // 4. Do we need to check amount_A and amount_B are valid 1-degree polynomials? I don't think so because the MAC [r] already verifies this.. Also FLTZ will fail later..
    // 5. Do we need to randomize input tags? I don't think so..
    // 6. I think we can skip field amount_Br = mod(static_cast<int64_t>(amount_B) + r, PP); check because we're checkcing amount_B mac anyway?

    // Randomize inputs:
    // [r] = Frand()
    // [D_A] = TDPF.EvalAll(f_A); [amount_A] = sum(D_A);
    // [D_A1] = TDPF.EvalAll(f_A1)
    // [D_B], pi_B = TDPF.EvalAll(f_B); [amount_B] = sum(D_B);
    // for i in [0, 1, 2]: [amount_i_MAC] = FMult([amount_i],[r]); [one_i_MAC] = FMult([one_i],[r]); [amount_A_MAC] = FMult([amount_A], [r]); [amount_B_MAC] = FMult([amount_B], [r])
    // for i in [0, 1, 2]: [[amount_i]] = ([amount_i], [amount_i_MAC]); Same for all other inputs: [[one_i]], [[amount_A]],
    // [[D_A]] = ([D_A], TDDPF.EvalAll(g_A, {[amount_0_MAC], [amount_1_MAC], [amount_2_MAC]}))
    // [[D_A1]] = ([D_A1], TDDPF.EvalAll(g_A1, {[one_0_MAC], [one_1_MAC], [one_2_MAC]}))
    // CheckAccess:
    // Note: [[in1]] and [in2] means that we run twice: [out] = Gate([in1],[in2]) and [out_MAC] = Gate([in1_MAC],[in2])
    // [[tag_A]] = Fproduct([[D_A]], [Lambda]); [[tag_A1]] = Fproduct([[D_A1]], [Lambda]); [[balance]] = Fproduct([[D_A1]], [L])
    // PartialOpen([[tag_A]]); PartialOpen([[tag_A1]]); send pi_B; Check that all is kosher.. // TODO: important - does direct opening here leak information?? I think it does because colluding client and server can infer [r] and cheat. So need to replace PACL.Verify with CheckZero? Also, do we need to check the polynomial degree? Don't think so..
    // FcheckZero() and the three F_LTZ gates in parallel

    // BatchVerify these (matching explicitly below to make sure we don't miss anything out):
    //
    // for i=[0,1,2]: r*[amount_i] == Fmult([r], [amount_i]); r*[one_i] == Fmult([r], [one_i])
    // r*[amount_A] = Fmult([r], [amount_A]); r*[amount_B] = Fmult([r], [amount_B]) // TODO: do we even need to authenticate these?
    // r*[tag_A] = Fproduct([D_A_MAC], [Lambda]); r*[tag_A1] = Fproduct([[D_A1_MAC]], [Lambda]); r*[balance] = Fproduct([D_A1_MAC], [L])
    // FcheckZero, FLTZ?

    field r = PRSS();
    // Expand DPFs
    // TODO: could/should be parallelized?
//    auto res_A = DPF::EvalShamir(key_A, log2N, server_index, false);
//    auto res_A1 = DPF::EvalShamir(key_A1, log2N, server_index, false);
//    auto res_B = DPF::EvalShamir(key_B, log2N, server_index, true);

    auto future_res_A = std::async(std::launch::async, DPF::EvalShamir, key_A, log2N, server_index, false);
    auto future_res_A1 = std::async(std::launch::async, DPF::EvalShamir, key_A1, log2N, server_index, false);
    auto future_res_B = std::async(std::launch::async, DPF::EvalShamir, key_B, log2N, server_index, true);

    // Getting the results (this will wait for the thread to finish if it hasn't yet)
    auto res_A = future_res_A.get();
    auto res_A1 = future_res_A1.get();
    auto res_B = future_res_B.get();

    auto data_A = std::move(res_A.first);
    auto data_A1 = std::move(res_A1.first);
    auto data_B = std::move(res_B.first);
    auto pi_B = res_B.second;
    block pi0_B = pi_B[0];
    block pi1_B = pi_B[1]; // TODO: check pis..

    debugPrint << "A[0]: " << data_A[0] << std::endl;
//    debugPrint << "pi_B: " << pi_B[0][0] << std::endl;
    debugPrint << "alphas[0]: " << alphas[0] << std::endl; // TODO: remove temp

    field amount_A = PIRW::sumvecff31(data_A);
    field amount_B = PIRW::sumvecff31(data_B);

    // Randomize inputs
    std::vector<field> batch_outputs, batch_outputs_MACs; // collect all gates to batch check at the end
    std::vector<field> inputs1 = {
            amount_0, amount_1, amount_2,
            one_0, one_1, one_2,
            amount_A, amount_B,
            tag_A_share, tag_A1_share
    };
    batch_outputs.insert(batch_outputs.end(), inputs1.begin(), inputs1.end());

    std::vector<field> inputs2;
    for (int i = 0; i < inputs1.size(); i++) {
        inputs2.push_back(r);
    }

    std::vector<field> outputs = multgate_helper(inputs1, inputs2);
    batch_outputs_MACs.insert(batch_outputs_MACs.end(), outputs.begin(), outputs.end());

    field amount_0_MAC = outputs[0];
    field amount_1_MAC = outputs[1];
    field amount_2_MAC = outputs[2];
    field one_0_MAC = outputs[3];
    field one_1_MAC = outputs[4];
    field one_2_MAC = outputs[5];
    field amount_A_MAC = outputs[6];
    field amount_B_MAC = outputs[7];
    field tag_A_share_MAC = outputs[8];
    field tag_A1_share_MAC = outputs[9];
//#ifndef DEBUG
    debugPrint << "Finished randomizing inputs round" << std::endl;
//#endif



//    // Randomize DPF inputs (fix codewords)

    auto time1 = std::chrono::high_resolution_clock::now();

    // running these async takes ages probably related to the network communication that fucks up if you try in parallel
    auto data_A_MAC = evalDeferred(deferredKey_A, amount_0_MAC, amount_1_MAC, amount_2_MAC).first;
    auto data_A1_MAC = evalDeferred(deferredKey_A1, one_0_MAC, one_1_MAC, one_2_MAC).first;
    auto time2 = std::chrono::high_resolution_clock::now();
    auto evalT1 = time2 - time1;
//    std::printf("time evalDeferred took: %zu us", std::chrono::duration_cast<std::chrono::microseconds>(evalT1).count());
//    auto future_data_A_MAC = std::async(std::launch::async, *(this->evalDeferred), deferredKey_A, amount_0_MAC, amount_1_MAC, amount_2_MAC);
//    auto future_data_A1_MAC = std::async(std::launch::async, [&] {
//        return evalDeferred(deferredKey_A1, one_0_MAC, one_1_MAC, one_2_MAC).first;
//    });
    //    // Get the results from previous tasks
//    const auto& data_A_MAC = future_data_A_MAC.get();
//    const auto& data_A1_MAC = future_data_A1_MAC.get();

    // FProduct gates
//    field tag_share_A_prime = mod(static_cast<int64_t>(PIRW::innerprodff31(alphas, data_A)), PP);
//    field tag_share_A1_prime = mod(static_cast<int64_t>(PIRW::innerprodff31(alphas, data_A1)), PP);
//    field balance_A = mod(static_cast<int64_t>(PIRW::innerprodff31(data_A1, ledger)), PP);
//
//    field tag_share_A_prime_MAC = mod(static_cast<int64_t>(PIRW::innerprodff31(alphas, data_A_MAC)), PP);
//    field tag_share_A1_prime_MAC = mod(static_cast<int64_t>(PIRW::innerprodff31(alphas, data_A1_MAC)), PP);
//    field balance_A_MAC = mod(static_cast<int64_t>(PIRW::innerprodff31(data_A1_MAC, ledger)), PP);


//    // Start asynchronous tasks for the rest of the computations
    auto future_tag_share_A_prime = std::async(std::launch::async, [&] {
        return mod(static_cast<int64_t>(PIRW::innerprodff31(alphas, data_A)), PP);
    });
    auto future_tag_share_A1_prime = std::async(std::launch::async, [&] {
        return mod(static_cast<int64_t>(PIRW::innerprodff31(alphas, data_A1)), PP);
    });
    auto future_balance_A = std::async(std::launch::async, [&] {
        return mod(static_cast<int64_t>(PIRW::innerprodff31(data_A1, ledger)), PP);
    });
    // ... [other asynchronous tasks] ...
    field tag_share_A_prime = future_tag_share_A_prime.get();
    field tag_share_A1_prime = future_tag_share_A1_prime.get();
    field balance_A = future_balance_A.get();


    // Start asynchronous tasks for the MAC computations
    auto future_tag_share_A_prime_MAC = std::async(std::launch::async, [&] {
        return mod(static_cast<int64_t>(PIRW::innerprodff31(alphas, data_A_MAC)), PP);
    });
    auto future_tag_share_A1_prime_MAC = std::async(std::launch::async, [&] {
        return mod(static_cast<int64_t>(PIRW::innerprodff31(alphas, data_A1_MAC)), PP);
    });
    auto future_balance_A_MAC = std::async(std::launch::async, [&] {
        return mod(static_cast<int64_t>(PIRW::innerprodff31(data_A1_MAC, ledger)), PP);
    });

    // Get the results of the MAC computations
    field tag_share_A_prime_MAC = future_tag_share_A_prime_MAC.get();
    field tag_share_A1_prime_MAC = future_tag_share_A1_prime_MAC.get();
    field balance_A_MAC = future_balance_A_MAC.get();



    outputs = multfproduct_open({ tag_share_A_prime, tag_share_A1_prime, balance_A, tag_share_A_prime_MAC, tag_share_A1_prime_MAC, balance_A_MAC });
    batch_outputs.insert(batch_outputs.end(), outputs.begin(), outputs.begin() + 3);
    batch_outputs_MACs.insert(batch_outputs_MACs.end(), outputs.begin() + 3, outputs.end());

    // refresh shares
    tag_share_A_prime = outputs[0];
    tag_share_A1_prime = outputs[1];
    balance_A = outputs[2];
    tag_share_A_prime_MAC = outputs[3];
    tag_share_A1_prime_MAC = outputs[4];
    balance_A_MAC = outputs[5];

    debugPrint << "Finished Fproduct gates" << std::endl;

    // FCheckZero Gate

    // Round 1 - multiply by a random value
    // TODO: do I even need to authenticate the tags? Easier to just authenticate, but maybe can't cheat here anyway?
    field tag_delta_A_share = mod(static_cast<int64_t>(tag_A_share) - tag_share_A_prime, PP);
    field tag_delta_A1_share = mod(static_cast<int64_t>(tag_A1_share) - tag_share_A1_prime, PP);
    field amount_delta = mod(static_cast<int64_t>(amount_A) - amount_B, PP);
    field tag_delta_A_share_MAC = mod(static_cast<int64_t>(tag_A_share_MAC) - tag_share_A_prime_MAC, PP);
    field tag_delta_A1_share_MAC = mod(static_cast<int64_t>(tag_A1_share_MAC) - tag_share_A1_prime_MAC, PP);
    field amount_delta_MAC = mod(static_cast<int64_t>(amount_A_MAC) - amount_B_MAC, PP);

    field r1 = PRSS();
    field r2 = PRSS();
    field r3 = PRSS();
    outputs = multgate_helper({tag_delta_A_share, tag_delta_A1_share, amount_delta, tag_delta_A_share_MAC, tag_delta_A1_share_MAC, amount_delta_MAC}, {r1, r2, r3, r1, r2, r3});
    batch_outputs.insert(batch_outputs.end(), outputs.begin(), outputs.begin() + 3);
    batch_outputs_MACs.insert(batch_outputs_MACs.end(), outputs.begin() + 3, outputs.end());
    debugPrint << "Finished CheckZero Round 1" << std::endl;

    // Round 2 - Open and check bit in the clear. Also batch FLTZ gates and check Pi_B
    // TODO: I'm not dealing with authenticating FLTZ. Need to fix once landed on an implementation.
    field amount_Amax = mod(static_cast<int64_t>(amount_A) - MAX_VALID_INT, PP);
    field new_balance_A = mod(static_cast<int64_t>(balance_A) - amount_A, PP);

    // TODO: do we need to open and verify the degree? or just open? Same repeating question..
    auto inputs = std::make_tuple(
            outputs[0],
            outputs[1],
            outputs[2],
            amount_A, // Input to FLTZ(amount_A)
            amount_Amax, // Input to FLTZ(amount_A - MAX_VALID_INT)
            new_balance_A, // Input to FLTZ(balance_A - amount_A)
            r,
            pi0_B, pi1_B // DPF B proof part 1
    );

    // Run the round of communication
    auto [output1, output2] = run_round(inputs);

    // Process the received data
    auto [zero_check_a_1, zero_check_b_1, zero_check_c_1, amount_A1, amount_Amax1, new_balance_A1, r_share1, pi0_B1, pi1_B1] = output1;
    auto [zero_check_a_2, zero_check_b_2, zero_check_c_2, amount_A2, amount_Amax2, new_balance_A2, r_share2, pi0_B2, pi1_B2] = output2;
    debugPrint << "Finished CheckZero Round 2" << std::endl;

    // Run Access Control checks
    // TODO: check Pis..

    // Reconstruct
    std::vector<field> shares0 = {outputs[0], outputs[1], outputs[2], amount_A, amount_Amax, new_balance_A, r};
    std::vector<field> shares1 = {zero_check_a_1, zero_check_b_1, zero_check_c_1, amount_A1, amount_Amax1, new_balance_A1, r_share1};
    std::vector<field> shares2 = {zero_check_a_2, zero_check_b_2, zero_check_c_2, amount_A2, amount_Amax2, new_balance_A2, r_share2};
    auto reconstructed = reconstruct_helper(shares0, shares1, shares2);

    assert(reconstructed[0] == 0);
    assert(reconstructed[1] == 0);
    assert(reconstructed[2] == 0);
    assert(reconstructed[3] >= 0 && reconstructed[3] < MAX_VALID_INT);
    assert(reconstructed[4] > MAX_VALID_INT); // Because in the field we only should encode unsigned numbers.. TODO: be consistent about this.. probably best to move to uint everywhere ..
    assert(reconstructed[5] >= 0 && reconstructed[3] < MAX_VALID_INT);

    //// BatchVerify
    field reconstructed_r = reconstructed[6];
    std::vector<field> coeffs = detrandints(batch_outputs.size(), PP);
    int64_t u, w;
    u = 0;
    w = 0;
    for (int i = 0; i < coeffs.size(); i++) {
        field tmp = mod(static_cast<int64_t>(coeffs[i])*batch_outputs[i], PP);
        w =  mod(w + tmp, PP);
        tmp = mod(static_cast<int64_t>(coeffs[i])*batch_outputs_MACs[i], PP);
        u = mod(u + tmp, PP);
    }
    field t = mod(u - reconstructed_r*w, PP);

    // CheckZero on t
    r1 = PRSS();
    outputs = multgate_helper({t}, {r1});
    debugPrint << "Finished BatchVerify (CheckZero) Round 1" << std::endl;

    // Open
    auto inp_check = std::make_tuple(
            outputs
    );

    // Run the round of communication
    auto [out_check1, out_check2] = run_round(inp_check);

    // Process the received data
    auto [check_share1] = out_check1;
    auto [check_share2] = out_check2;
    auto t_reconstructed = reconstruct_helper(outputs, check_share1, check_share2)[0];
    debugPrint << "Finished BatchVerify (CheckZero) Round 2 and t = " << t_reconstructed << std::endl;

//    // TODO: remove temp
//    // Open
//    auto inp_tmp = std::make_tuple(
//            batch_outputs,
//            batch_outputs_MACs
//    );
//
//    // Run the round of communication
//    auto [out_tmp1, out_tmp2] = run_round(inp_tmp);
//
//    // Process the received data
//    auto [batch_outputs1, batch_outputs_MACs1] = out_tmp1;
//    auto [batch_outputs2, batch_outputs_MACs2] = out_tmp2;
//    auto batch_outputs_reconstructed = reconstruct_helper(batch_outputs, batch_outputs1, batch_outputs2);
//    auto batch_outputs_MACs_reconstructed = reconstruct_helper(batch_outputs_MACs, batch_outputs_MACs1, batch_outputs_MACs2);
//
//    for (int i = 0; i < batch_outputs_reconstructed.size(); i++) {
//        field routput = mod(static_cast<int64_t>(batch_outputs_reconstructed[i])*reconstructed_r, PP);
//        debugPrint << "output[" << i << "]: " << batch_outputs_reconstructed[i] << ", output_MAC: " << batch_outputs_MACs_reconstructed[i] << ", r*output: " << routput << std::endl;
//    }
//    // TODO: end remove temp


//    assert(t_reconstructed == 0);
    if (t_reconstructed == 0) {
        // Finalize the transaction after the MPC round / all checks have passed
        ledger = PIRW::subvff31(ledger, data_A); // TODO: parallelize
        ledger = PIRW::addvff31(ledger, data_B); // TODO: parallelize
//        std::cout << "Transfer succeeded!" << std::endl;
    } else {
        std::cout << "Transfer failed! t_reconstructed not okay" << std::endl;
    }

}