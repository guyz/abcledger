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

    void initData(size_t N);
    void saveToFile(const std::vector<uint32_t>& data, const std::string& filename);
};


#endif //DPFPIR_SERVER_H
