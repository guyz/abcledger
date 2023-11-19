//
// Created by Guy Zyskind on 14/11/2023.
//

#ifndef DPFPIR_SERVER_H
#define DPFPIR_SERVER_H

#include <vector>
#include <cstdint>
#include <utility>
#include <string>
#include <cmath>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

class Server {
public:
    Server(int index, size_t N);
    void transfer(const std::vector<std::pair<uint32_t, std::vector<uint8_t>>>& key_A,
                  const std::vector<std::pair<uint32_t, std::vector<uint8_t>>>& key_B,
                  uint32_t tag_share);
    uint32_t balance(const std::vector<std::pair<uint32_t, std::vector<uint8_t>>>& key, uint32_t tag_share);

private:
    int log2N;
    int N;
    int server_index;
    std::vector<uint32_t> ledger;
    std::vector<uint32_t> alphas;

    int serverSocket;
    int connectionHandler1;
    int serverIndex1; // Which party index is on connectionHandler1
    int connectionHandler2;
    int serverIndex2; // Which party index is on connectionHandler2

    // Networking
    void initNetworking();
    void startListening();
    void establishConnections();
    void acceptConnections();
    void closeConnections();
    bool waitForAck(int socket);
    std::string serializeData(uint32_t a, uint32_t b, uint32_t c);
    std::tuple<uint32_t, uint32_t, uint32_t> deserializeData(const std::string& data);

    void initData(size_t N);
    void saveToFile(const std::vector<uint32_t>& data, const std::string& filename);

    void localMPCChecks(std::vector<uint32_t>& amount_As, std::vector<uint32_t>& amount_Bs, std::vector<uint32_t>& new_balance_As);
};


#endif //DPFPIR_SERVER_H
