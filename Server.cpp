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
                            std::vector<field>& new_balances_A, std::vector<field>& tag_share_A_primes, std::vector<field>& tag_share_A1_primes) {

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

    // TODO: Pi and shamir sharing of DPF B

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

    std::cout << "amount_delta: " << amount_delta << ", amount_Amax: " << amount_Amax << ", tag_delta: " << tag_share_A_prime << std::endl;

    auto inputs = std::make_tuple(
            amount_delta, // Check that they are equal
            amount_A, // Input to FLTZ(amount_A)
            amount_Amax, // Input to FLTZ(amount_A - MAX_VALID_INT)
            new_balance_A, // Input to FLTZ(balance_A - amount_A)
            tag_share_A_prime, // access control proof
            tag_share_A1_prime // access control proof
            // TODO: missing DPF_B proof
            );

    // Run the round of communication
    auto [output1, output2] = run_round(inputs);

    // Process the received data
    auto [amount_delta1, amount_A1, amount_Amax1, new_balance_A1, tag_share_A_prime1, tag_share_A1_prime1] = output1;
    auto [amount_delta2, amount_A2, amount_Amax2, new_balance_A2, tag_share_A_prime2, tag_share_A1_prime2] = output2;
//    std::cout << "Server" << server_index << " received: " << amount_A_from_server1 << ", " << amount_B_from_server1 << ", " << new_balance_A_from_server1 << std::endl;
//    std::cout << "Server" << server_index << " received: " << amount_A_from_server2 << ", " << amount_B_from_server2 << ", " << new_balance_A_from_server2 << std::endl;

    // 1. use tag_share_prime and whatever else is needed to verify access control/proper DPF to data_A, data_A1, data_B
    // 2. Open(amount_A-amount_B) and make sure it is zero. Need to show in the proof that this works later, but generally speaking the idea is we don't need an equality gate - just public open
    // 3. Check that LTZ(amount_A) == false and LTZ(amount_A - MAX_VALUE) == true // This checks that amount_A is in [0, MAX_VALUE). Assume that MAX_VALUE is greater than all the coins in the system ever..
    // 4. Check that LTZ(new_balance_A) == false // Make sure that this tx won't turn the balance negative

    std::vector<uint32_t> amount_deltas(3, 0), amount_As(3, 0), amount_Amaxs(3, 0),
            new_balances_A(3, 0), tag_share_A_primes(3, 0), tag_share_A1_primes(3, 0);

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


    localMPCChecks(amount_deltas, amount_As, amount_Amaxs,
                   new_balances_A, tag_share_A_primes, tag_share_A1_primes);

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
