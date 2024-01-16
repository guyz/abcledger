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
#include <sstream>
#include <variant>
#include <tuple>
#include <type_traits>
#include "utils.h"
#include "dpf.h"

const int N_RANDS = 10000;

class Server {
public:
    Server(int index, size_t N, bool local = false);
    void transfer(const std::vector<DPF::KeyShare>& key_A,
                  const std::vector<DPF::KeyShare>& key_A1,
                  const std::vector<DPF::KeyShare>& key_B,
                  field tag_A_share, field tag_A1_share, std::array<std::vector<uint32_t>, 10>& vms);

    uint32_t balance(const std::vector<DPF::KeyShare>& key, uint32_t tag_share, std::array<std::vector<uint32_t>, 10>& vms);
    void evalDeferredTest(std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare>& key, field beta_0, field beta_1, field beta_2);
    std::vector<DPF::KeyShare> evalDeferred(std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare>& key, field beta_0, field beta_1, field beta_2);
    void transferMalicious(const std::vector<DPF::KeyShare>& key_A,
                                   std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare>& deferredKey_A,
                                   const std::vector<DPF::KeyShare>& key_A1,
                                   std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare>& deferredKey_A1,
                                   const std::vector<DPF::KeyShare>& key_B,
                                   field tag_A_share, field tag_A1_share,
                                   field amount_0, field amount_1, field amount_2,
                                   field one_0, field one_1, field one_2, std::array<std::vector<uint32_t>, 10>& vms);

    uint32_t balanceMalicious(const std::vector<DPF::KeyShare>& key, std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare>& deferredKey, uint32_t tag_share,
                              field one_0, field one_1, field one_2, std::array<std::vector<uint32_t>, 10>& vms);

    // ORAM (Essentially PIR and PIRW. No further checks)
    // Note: read and write also work with malicious servers because we have redundancy. Also no server-to-server communication!
    // The last part is important and should go in a table comparing the schemes
    uint32_t read(const std::vector<DPF::KeyShare>& key, std::array<std::vector<uint32_t>, 10>& vms);
//    void write(const std::vector<DPF::KeyShare>& key, std::array<std::vector<uint32_t>, 10>& vms);
    std::vector<uint32_t> write(const std::vector<DPF::KeyShare>& key, std::array<std::vector<uint32_t>, 10>& vms);

    void closeConnections();

    std::vector<uint32_t> ledger;
    std::vector<uint32_t> alphas;
    std::vector<uint8_t> xorrand;
    std::vector<uint8_t> xorzero;
    std::vector<uint32_t> randt;
    std::vector<uint32_t> rand2t;
    std::vector<uint32_t> zero2t;
    int rand_counter = 0;

private:
    int log2N;
    int N;
    int server_index;

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
    bool waitForAck(int socket);

    void initPreprocessingData();

    // Generic run_round function declaration
    template <typename... Args>
    std::pair<std::tuple<Args...>, std::tuple<Args...>> run_round(const std::tuple<Args...>& inputs);


    // Serialize uint32_t
    void serialize(std::ostringstream& oss, uint32_t value) {
        oss.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

// Serialize std::vector<uint32_t>
    void serialize(std::ostringstream& oss, const std::vector<uint32_t>& vec) {
        uint32_t size = vec.size();
        serialize(oss, size);
        for (auto val : vec) {
            serialize(oss, val);
        }
    }

// Serialize std::vector<uint8_t>
    void serialize(std::ostringstream& oss, const std::vector<uint8_t>& vec) {
        uint32_t size = vec.size();
        serialize(oss, size);
        oss.write(reinterpret_cast<const char*>(vec.data()), vec.size());
    }

// Serialize std::vector<long long int>
    void serialize(std::ostringstream& oss, const std::vector<long long int>& vec) {
        uint32_t size = vec.size();
        serialize(oss, size);
        for (auto val : vec) {
            oss.write(reinterpret_cast<const char*>(&val), sizeof(val));
        }
    }

// General serialize function using variadic templates
    template <typename... Args>
    std::string serializeData(const std::tuple<Args...>& data) {
        std::ostringstream oss;
        // This lambda calls 'serialize' for each element in the tuple
        std::apply([&oss, this](const auto&... args) { ((serialize(oss, args)), ...); }, data);
        return oss.str();
    }

// Deserialize uint32_t
    void deserialize(std::istringstream& iss, uint32_t& value) {
        iss.read(reinterpret_cast<char*>(&value), sizeof(value));
    }

// Deserialize std::vector<uint32_t>
    void deserialize(std::istringstream& iss, std::vector<uint32_t>& vec) {
        uint32_t size;
        deserialize(iss, size);
        vec.resize(size);
        for (auto& val : vec) {
            deserialize(iss, val);
        }
    }

// Deserialize std::vector<uint8_t>
    void deserialize(std::istringstream& iss, std::vector<uint8_t>& vec) {
        uint32_t size;
        deserialize(iss, size);
        vec.resize(size);
        iss.read(reinterpret_cast<char*>(vec.data()), size);
    }

// Deserialize std::vector<long long int>
    void deserialize(std::istringstream& iss, std::vector<long long int>& vec) {
        uint32_t size;
        deserialize(iss, size);
        vec.resize(size);
        for (auto& val : vec) {
            iss.read(reinterpret_cast<char*>(&val), sizeof(val));
        }
    }

// Function to deserialize data into a tuple
    template <typename... Args, std::size_t... I>
    std::tuple<Args...> deserializeDataImpl(const std::string& data, std::index_sequence<I...>) {
        std::istringstream iss(data);
        std::tuple<Args...> result;
        ((deserialize(iss, std::get<I>(result))), ...);
        return result;
    }

    template <typename... Args>
    std::tuple<Args...> deserializeData(const std::string& data) {
        return deserializeDataImpl<Args...>(data, std::index_sequence_for<Args...>{});
    }

    // Serialize __m128i
    void serialize(std::ostringstream& oss, const __m128i& value) {
        oss.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

// Deserialize __m128i
    void deserialize(std::istringstream& iss, __m128i& value) {
        iss.read(reinterpret_cast<char*>(&value), sizeof(value));
    }

// Template versions to catch unsupported types
    template <typename T>
    void serialize(std::ostringstream& oss, const T& value) {
        static_assert(sizeof(T) == 0, "serialize not implemented for this type");
    }

    template <typename T>
    void deserialize(std::istringstream& iss, T& value) {
        static_assert(sizeof(T) == 0, "deserialize not implemented for this type");
    }


    void initData(size_t N);

    std::vector<int64_t> reconstruct_helper(const std::vector<field>& shares0, const std::vector<field>& shares1, const std::vector<field>& shares2);
    std::vector<uint8_t> reconstruct_helper_gf256(const std::vector<uint8_t>& shares0, const std::vector<uint8_t>& shares1, const std::vector<uint8_t>& shares2);

    // Other MPC gates - // TODO: MPC versions
    bool LTZ(std::vector<std::pair<int64_t, int64_t>> shares);
    field PRSS();
    uint8_t XORPRSS();
    uint8_t XORPRZS();
    std::pair<field, field> PRSS2();
    field PRZS();
    field SinglePRSS();
    std::pair<field, field> SinglePRSS2();
    field SinglePRZS();
    std::vector<std::vector<std::pair<uint8_t, uint8_t>>> AtoB(field beta_0, field beta_1, field beta_2);
    std::vector<field> multgate_helper(std::vector<field> inputs1, std::vector<field> inputs2);
    std::vector<field> multfproduct_open(std::vector<field> inputs);

    void reshare(field beta);
    std::vector<DPF::KeyShare> fixCodeword(std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare> &key, field beta_0, field beta_1, field beta_2);

//    int getPair(const std::vector<DPF::KeyShare> &fullkey, std::vector<uint32_t>& vm0, std::vector<uint32_t>& vm1) const;
};


#endif //DPFPIR_SERVER_H
