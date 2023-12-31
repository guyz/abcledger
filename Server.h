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

    // Generic run_round function declaration
    template <typename... Args>
    std::pair<std::tuple<Args...>, std::tuple<Args...>> run_round(const std::tuple<Args...>& inputs);

    template <typename T>
    void serialize(std::stringstream& ss, const T& value) {
        ss.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    void serialize(std::stringstream& ss, const std::string& value) {
        uint32_t length = static_cast<uint32_t>(value.size());
        ss.write(reinterpret_cast<const char*>(&length), sizeof(length));
        ss.write(value.data(), length);
    }

    template <typename T>
    void serialize(std::stringstream& ss, const std::vector<T>& value) {
        uint32_t length = static_cast<uint32_t>(value.size());
        ss.write(reinterpret_cast<const char*>(&length), sizeof(length));
        for (const auto& val : value) {
            serialize(ss, val);
        }
    }

    template <typename... Args>
    std::string serializeData(const Args&... args) {
        std::stringstream ss;
        (serialize(ss, args), ...);
        return ss.str();
    }

    // Deserialize individual types
    template <typename T>
    T deserialize(std::istream& is) {
        T value;
        is.read(reinterpret_cast<char*>(&value), sizeof(T));
        return value;
    }

    std::string deserialize(std::istream& is) {
        uint32_t length;
        is.read(reinterpret_cast<char*>(&length), sizeof(length));
        std::string value(length, '\0');
        is.read(value.data(), length);
        return value;
    }

    // Helper function for tuple deserialization
    template <typename Tuple, size_t... Is>
    Tuple deserializeTuple(std::istream& is, std::index_sequence<Is...>) {
        return std::make_tuple(deserialize<std::tuple_element_t<Is, Tuple>>(is)...);
    }

    template <typename Tuple, std::size_t... I>
    auto reverseTupleImpl(const Tuple& t, std::index_sequence<I...>) {
        return std::make_tuple(std::get<sizeof...(I) - 1 - I>(t)...);
    }

    template <typename... Args>
    auto reverseTuple(const std::tuple<Args...>& t) {
        return reverseTupleImpl(t, std::index_sequence_for<Args...>{});
    }

    // Deserialize a tuple using fold expressions and then reverse it
    template <typename... Args>
    auto deserializeData(const std::string& data) {
        std::stringstream ss(data);
        auto deserializedTuple = std::make_tuple(deserialize<Args>(ss)...);
//        return reverseTuple(deserializedTuple);
        return deserializedTuple;
    }


    void initData(size_t N);
    void saveToFile(const std::vector<uint32_t>& data, const std::string& filename);

    void localMPCChecks(std::vector<uint32_t>& amount_As, std::vector<uint32_t>& amount_Bs, std::vector<uint32_t>& new_balance_As);
};


#endif //DPFPIR_SERVER_H
