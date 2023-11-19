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
#include "utils.h"
#include "shamir.h"
#include "dpf.h"
#include "pirw.h"
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <cassert>

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

// Serialize three uint32_t values into a binary string
std::string Server::serializeData(uint32_t a, uint32_t b, uint32_t c) {
    std::string data;
    data.resize(12); // Each uint32_t will take 4 bytes

    std::memcpy(&data[0], &a, 4);
    std::memcpy(&data[4], &b, 4);
    std::memcpy(&data[8], &c, 4);

    return data;
}

// Deserialize a binary string into three uint32_t values
std::tuple<uint32_t, uint32_t, uint32_t> Server::deserializeData(const std::string& data) {
    uint32_t a, b, c;

    std::memcpy(&a, &data[0], 4);
    std::memcpy(&b, &data[4], 4);
    std::memcpy(&c, &data[8], 4);

    return std::make_tuple(a, b, c);
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

void Server::localMPCChecks(std::vector<uint32_t>& amount_As, std::vector<uint32_t>& amount_Bs, std::vector<uint32_t>& new_balance_As) {
//        uint32_t amount_A1, uint32_t amount_A2, uint32_t amount_A3,
//                            uint32_t amount_B1, uint32_t amount_B2, uint32_t amount_B3,
//                            uint32_t new_balance_A1, uint32_t new_balance_A2, uint32_t new_balance_A3
//                            ) {
    // TODO: Secure these tests, right now they are in plaintext
    auto amount_A_shares = encode_to_shares(amount_As);

    auto amount_B_shares = encode_to_shares(amount_Bs);

    auto new_balance_A_shares = encode_to_shares(new_balance_As);

    auto amount_A = recover_secret(amount_A_shares, PP);
    auto amount_B = recover_secret(amount_B_shares, PP);
    auto new_balance_A = recover_secret(new_balance_A_shares, PP);

    // TODO: missing tests (and obviously MPCize tests..) - like access control / valid DPF stuff..

    // amount_A and amount_B are the same
    assert( (amount_A - amount_B) == 0);

    // amount_A in range
    assert( (amount_A < 0) == false && ((amount_A - (1 << (31 - 1)) ) < 0) == true);

    // new balance is not negative
    assert( (new_balance_A < 0) == false);

}

void Server::transfer(const std::vector<std::pair<uint32_t, std::vector<uint8_t>>>& key_A,
                      const std::vector<std::pair<uint32_t, std::vector<uint8_t>>>& key_B,
                      uint32_t tag_share) {
    // Expand DPFs
    // TODO: could/should be parallelized?
    auto data_A = DPF::EvalShamir(key_A, log2N, server_index); // TODO: get and check data_A1 from this..
    auto data_B = DPF::EvalShamir(key_B, log2N, server_index);
    std::cout << "A[0]: " << data_A[0] << std::endl;

    // Prepare Validity check
    // TODO: VerifyDPF for data_B
    // TODO: check access control for data_A1
    uint32_t tag_share_prime = PIRW::innerprodff31(alphas, data_A);

    // Prepare range checks
    uint32_t amount_A = PIRW::sumvecff31(data_A);
    uint32_t amount_B = PIRW::sumvecff31(data_B);
    uint32_t balance_A = PIRW::innerprodff31(data_A, ledger); // In practice, this is the full SumProduct protocol but we will defer it to the end..?
    uint32_t new_balance_A = mod(static_cast<int64_t>(balance_A) - amount_A, PP);
    std::cout << "balance_A: " << balance_A << ", amount: " << amount_A << ", new_balance: " << new_balance_A << std::endl; // TODO : remove temp. Note, right now it's amount*balance
    // TODO: run the following checks in MPC
    std::string dataToSend = serializeData(amount_A, amount_B, new_balance_A);

    // Send data to other servers
    send(connectionHandler1, dataToSend.c_str(), dataToSend.size(), 0);
    send(connectionHandler2, dataToSend.c_str(), dataToSend.size(), 0);

    std::cout << "Server" << server_index << " has sent: " << amount_A << ", " << amount_B << ", " << new_balance_A << std::endl;

    // Receive data from other servers
    char buffer[1024] = {0};
    if (receiveFully(connectionHandler1, buffer, 12) <= 0) { // Assuming 12 bytes is the size of your data
        std::cerr << "Failed to receive data from server 1" << std::endl;
        // Handle error
    }

    auto [amount_A_from_server1, amount_B_from_server1, new_balance_A_from_server1] = deserializeData(std::string(buffer));
    std::cout << "Server" << server_index << " received: " << amount_A_from_server1 << ", " << amount_B_from_server1 << ", " << new_balance_A_from_server1 << std::endl;

    memset(buffer, 0, sizeof(buffer)); // Clear the buffer
    if (receiveFully(connectionHandler2, buffer, 12) <= 0) { // Assuming 12 bytes is the size of your data
        std::cerr << "Failed to receive data from server 2" << std::endl;
        // Handle error
    }

    auto [amount_A_from_server2, amount_B_from_server2, new_balance_A_from_server2] = deserializeData(std::string(buffer));
    std::cout << "Server" << server_index << " received: " << amount_A_from_server2 << ", " << amount_B_from_server2 << ", " << new_balance_A_from_server2 << std::endl;

    // 1. use tag_share_prime and whatever else is needed to verify access control/proper DPF to data_A, data_A1, data_B
    // 2. Open(amount_A-amount_B) and make sure it is zero. Need to show in the proof that this works later, but generally speaking the idea is we don't need an equality gate - just public open
    // 3. Check that LTZ(amount_A) == false and LTZ(amount_A - MAX_VALUE) == true // This checks that amount_A is in [0, MAX_VALUE). Assume that MAX_VALUE is greater than all the coins in the system ever..
    // 4. Check that LTZ(new_balance_A) == false // Make sure that this tx won't turn the balance negative

    std::vector<uint32_t> amount_As = {0,0,0};
    std::vector<uint32_t> amount_Bs = {0,0,0};
    std::vector<uint32_t> new_balance_As = {0,0,0};

    amount_As[server_index] = amount_A;
    amount_As[serverIndex1] = amount_A_from_server1;
    amount_As[serverIndex2] = amount_A_from_server2;

    amount_Bs[server_index] = amount_B;
    amount_Bs[serverIndex1] = amount_B_from_server1;
    amount_Bs[serverIndex2] = amount_B_from_server2;

    new_balance_As[server_index] = new_balance_A;
    new_balance_As[serverIndex1] = new_balance_A_from_server1;
    new_balance_As[serverIndex2] = new_balance_A_from_server2;

    localMPCChecks(amount_As, amount_Bs, new_balance_As);

    // Finalize the transaction after the MPC round / all checks have passed
    ledger = PIRW::subvff31(ledger, data_A); // TODO: parallelize
    ledger = PIRW::addvff31(ledger, data_B); // TODO: parallelize

//    closeConnections();
}

uint32_t Server::balance(const std::vector<std::pair<uint32_t, std::vector<uint8_t>>>& key, uint32_t tag_share) {
    auto data = DPF::EvalShamir(key, log2N, server_index);
    uint32_t tag_share_prime = PIRW::innerprodff31(alphas, data);

    // TODO: check access in MPC (Open(t-t') == 0)

    uint32_t balance = PIRW::innerprodff31(data, ledger); // In practice, this is the full SumProduct protocol but we will defer it to the end..?
    return balance;
}
