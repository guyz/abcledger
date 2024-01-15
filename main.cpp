#include "dpf.h"
#include "pirw.h"
#include "Server.h"
#include <chrono>
#include <iostream>
#include <cassert>
#include <immintrin.h>
#include "shamir.h"
#include <cstring>
#include "utils.h"
#include <cstdlib>
#include <random>
#include <fstream>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <string>
#include "utils.h"
#include <map>

int test_hash_functions() {
    uint32_t r1 = rand();
    uint32_t r2 = rand();
    uint32_t r3 = rand();
    uint32_t r4 = rand();

    // Combine the four 32-bit numbers into a 128-bit number
    block seed = _mm_set_epi32(r1, r2, r3, r4);
    uint32_t alpha = 24323;
    auto res = DPF::prg::hash1(seed, alpha);
    auto res2 = DPF::prg::hash2(res);
    auto res3 = DPF::prg::hash1(seed, alpha);
    auto res4 = DPF::prg::hash2(res);

    // Print the seed
    std::cout << "Seed: " << std::hex << r1 << " " << r2 << " " << r3 << " " << r4 << std::endl;

    // Print alpha
    std::cout << "Alpha: " << alpha << std::endl;

    // Assert that res is equal to res3
    assert(are_arrays_equal(res, res3));

    // Assert that res2 is equal to res4
//    assert(are_arrays_equal_2(res2, res4));

    // Print res
    std::cout << "Res: ";
    for (const auto& e : res) {
        std::cout << "[";
        for (int i = 0; i < sizeof(block) / sizeof(uint32_t); ++i) {
            std::cout << std::hex << ((uint32_t*)&e)[i] << (i < sizeof(block) / sizeof(uint32_t) - 1 ? " " : "");
        }
        std::cout << "] ";
    }
    std::cout << std::endl;

    // Print res2
    std::cout << "Res2: ";
    for (const auto& e : res2) {
        std::cout << "[";
        for (int i = 0; i < sizeof(block) / sizeof(uint32_t); ++i) {
            std::cout << std::hex << ((uint32_t*)&e)[i] << (i < sizeof(block) / sizeof(uint32_t) - 1 ? " " : "");
        }
        std::cout << "] ";
    }
    std::cout << std::endl;

    return 0;
}

// TODO: refactor - move these to DPF...
// Serialize
void serialize(const std::vector<DPF::KeyShare>& data, const std::string& filename) {
    std::ofstream out(filename, std::ios::binary);
    for (const auto& keyShare : data) {
        // Serialize 'key'
        uint32_t keySize = keyShare.key.size();
        out.write(reinterpret_cast<const char*>(&keySize), sizeof(keySize));
        out.write(reinterpret_cast<const char*>(keyShare.key.data()), keySize);

        // Serialize 'cs'
        out.write(reinterpret_cast<const char*>(&keyShare.cs), sizeof(keyShare.cs));

        // Serialize 'z'
        out.write(reinterpret_cast<const char*>(&keyShare.z), sizeof(keyShare.z));
    }
    out.close();
}


// Unserialize
std::vector<DPF::KeyShare> unserialize(const std::string& filename) {
    std::vector<DPF::KeyShare> data;
    std::ifstream in(filename, std::ios::binary);
    while (!in.eof()) {
        DPF::KeyShare keyShare;

        // Unserialize 'key'
        uint32_t keySize;
        in.read(reinterpret_cast<char*>(&keySize), sizeof(keySize));
        keyShare.key.resize(keySize);
        in.read(reinterpret_cast<char*>(keyShare.key.data()), keySize);

        // Unserialize 'cs'
        in.read(reinterpret_cast<char*>(&keyShare.cs), sizeof(keyShare.cs));

        // Unserialize 'z'
        in.read(reinterpret_cast<char*>(&keyShare.z), sizeof(keyShare.z));

        data.push_back(keyShare);
    }
    in.close();
    return data;
}

void writeVector(int index, const std::vector<DPF::KeyShare>& keyShares, std::string filename) {
    serialize(keyShares, DATA_DIR + filename + std::to_string(index) + ".txt");
}

std::vector<DPF::KeyShare> loadVector(int index, std::string filename) {
    return unserialize(DATA_DIR + filename + std::to_string(index) + ".txt");
}

// Helper function to serialize a vector of pairs
template<typename T1, typename T2>
void serializeVectorPair(const std::vector<std::pair<T1, T2>>& vec, std::ofstream& outFile) {
    size_t vecSize = vec.size();
    outFile.write(reinterpret_cast<const char*>(&vecSize), sizeof(vecSize));
    for (const auto& pair : vec) {
        outFile.write(reinterpret_cast<const char*>(&pair.first), sizeof(pair.first));
        outFile.write(reinterpret_cast<const char*>(&pair.second), sizeof(pair.second));
    }
}

// Helper function to deserialize a vector of pairs
template<typename T1, typename T2>
std::vector<std::pair<T1, T2>> deserializeVectorPair(std::ifstream& inFile) {
    std::vector<std::pair<T1, T2>> vec;
    size_t vecSize;
    inFile.read(reinterpret_cast<char*>(&vecSize), sizeof(vecSize));
    vec.resize(vecSize);
    for (auto& pair : vec) {
        inFile.read(reinterpret_cast<char*>(&pair.first), sizeof(pair.first));
        inFile.read(reinterpret_cast<char*>(&pair.second), sizeof(pair.second));
    }
    return vec;
}

// Helper function to serialize a DeferredKeyShare
void serializeDeferredKeyShare(const DPF::DeferredKeyShare& dks, std::ofstream& outFile) {
    // Serialize the 'key' vector
    size_t keySize = dks.key.size();
    outFile.write(reinterpret_cast<const char*>(&keySize), sizeof(keySize));
    outFile.write(reinterpret_cast<const char*>(dks.key.data()), keySize);

    // Serialize 's0_share' and 's1_share' vectors
    serializeVectorPair(dks.s0_share, outFile);
    serializeVectorPair(dks.s1_share, outFile);

    // Serialize 't0_share'
    outFile.write(reinterpret_cast<const char*>(&dks.t0_share.first), sizeof(dks.t0_share.first));
    outFile.write(reinterpret_cast<const char*>(&dks.t0_share.second), sizeof(dks.t0_share.second));
}

// Helper function to deserialize a DeferredKeyShare
DPF::DeferredKeyShare deserializeDeferredKeyShare(std::ifstream& inFile) {
    DPF::DeferredKeyShare dks;

    // Deserialize the 'key' vector
    size_t keySize;
    inFile.read(reinterpret_cast<char*>(&keySize), sizeof(keySize));
    dks.key.resize(keySize);
    inFile.read(reinterpret_cast<char*>(dks.key.data()), keySize);

    // Deserialize 's0_share' and 's1_share' vectors
    dks.s0_share = deserializeVectorPair<uint8_t, uint8_t>(inFile);
    dks.s1_share = deserializeVectorPair<uint8_t, uint8_t>(inFile);

    // Deserialize 't0_share'
    inFile.read(reinterpret_cast<char*>(&dks.t0_share.first), sizeof(dks.t0_share.first));
    inFile.read(reinterpret_cast<char*>(&dks.t0_share.second), sizeof(dks.t0_share.second));

    return dks;
}

// Serialization function
void writePair(const std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare>& pair, const std::string& filename) {
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile.is_open()) {
        throw std::runtime_error("Unable to open file for writing");
    }

    // Serialize first DeferredKeyShare
    serializeDeferredKeyShare(pair.first, outFile);

    // Serialize second DeferredKeyShare
    serializeDeferredKeyShare(pair.second, outFile);

    outFile.close();
}

// Deserialization function
std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare> loadPair(const std::string& filename) {
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile.is_open()) {
        throw std::runtime_error("Unable to open file for reading");
    }

    // Deserialize first DeferredKeyShare
    DPF::DeferredKeyShare first = deserializeDeferredKeyShare(inFile);

    // Deserialize second DeferredKeyShare
    DPF::DeferredKeyShare second = deserializeDeferredKeyShare(inFile);

    inFile.close();
    return {first, second};
}


bool fileExists(const std::string& fileName) {
    return std::filesystem::exists(fileName);
}


// Function to compare two DeferredKeyShare objects for equality
bool areEqual(const DPF::DeferredKeyShare& dks1, const DPF::DeferredKeyShare& dks2) {
    return (dks1.key == dks2.key) &&
           (dks1.s0_share == dks2.s0_share) &&
           (dks1.s1_share == dks2.s1_share) &&
           (dks1.t0_share == dks2.t0_share);
}

// Test function
void testSerialization() {
    // Create a DeferredKeyShare object
    DPF::DeferredKeyShare originalDks;
    originalDks.key = {1, 2, 3, 4, 5};
    originalDks.s0_share = {{1, 2}, {3, 4}};
    originalDks.s1_share = {{5, 6}, {7, 8}};
    originalDks.t0_share = {9, 10};

    // Serialize it to a file
    std::string filename = "test_dks.bin";
    std::ofstream outFile(filename, std::ios::binary);
    serializeDeferredKeyShare(originalDks, outFile);
    outFile.close();

    // Deserialize it back
    std::ifstream inFile(filename, std::ios::binary);
    DPF::DeferredKeyShare deserializedDks = deserializeDeferredKeyShare(inFile);
    inFile.close();

    // Compare the original and deserialized objects
    assert(areEqual(originalDks, deserializedDks));

    std::cout << "Test passed: Serialized and deserialized DeferredKeyShare objects are identical." << std::endl;
}

// NOTE: outdated
void test_client(int serverIndex, int logN) {
    int N = 1 << logN;
    int amount = 7;
    int senderIndex = 0;
    int recvIndex = 20;
    uint64_t alpha = 2112445456;

    // Load/generate data
    std::vector<DPF::KeyShare> kmsA_i;
    std::vector<DPF::KeyShare> kmsB_i;

    //todo: 1DPF..

    auto kmsA = DPF::GenShamir(senderIndex, logN, amount, false);
    auto kmsB = DPF::GenShamir(recvIndex, logN, amount, true);

    if (fileExists(DATA_DIR + "kmsA" + std::to_string(serverIndex) + ".txt")) {
        kmsA_i = loadVector(serverIndex, "kmsA");
        kmsB_i = loadVector(serverIndex, "kmsB");
    } else {
        // File does not exist
        for (size_t i = 0; i < kmsA.size(); ++i) {
            writeVector(i, kmsA[i], "kmsA");
            writeVector(i, kmsB[i], "kmsB");
        }

        kmsA_i = kmsA[serverIndex];
        kmsB_i = kmsA[serverIndex];
    }

    field tag_A = (alpha*amount) % PP;
    std::vector<std::pair<int64_t, int64_t>> tag_A_shares = gen_shares(3, 2, tag_A, PP);
    field tag_A_share = tag_A_shares[serverIndex].second;

    // TODO: remove temp
//    std::vector<std::vector<uint32_t>> ledgers(4);  // Create a vector of 3 vectors
//
//    for (int j = 0; j < 4; j++) {
//        std::ifstream ledgerFile;
//
//        // Choose file based on the value of j
//        if (j < 3) {
//            ledgerFile.open("data/ledger-" + std::to_string(j + 1) + ".txt");
//        } else {  // When j == 3
//            ledgerFile.open("data/ledger.txt");
//        }
//
//        // Load ledger if file exists
//        if (ledgerFile) {
//            uint32_t value;
//            while (ledgerFile >> value) {
//                ledgers[j].push_back(value);
//            }
//            ledgerFile.close();
//        }
//    }
//
//    std::cout << "ledger value: " <<  ledgers[3][senderIndex] << std::endl;
//    std::cout << "ledger1 value: " <<  ledgers[0][senderIndex] << std::endl;
//    std::cout << "ledger2 value: " <<  ledgers[1][senderIndex] << std::endl;
//    std::cout << "ledger3 value: " <<  ledgers[2][senderIndex] << std::endl;
//    auto ledgervalueshares = encode_to_shares({
//                                                ledgers[0][senderIndex],
//                                                ledgers[1][senderIndex],
//                                                ledgers[2][senderIndex]
//                                    });
//    auto ledgervalue = recover_secret(ledgervalueshares, PP);
//    std::cout << "ledger reconstructed: " << ledgervalue << std::endl;
//
//    auto loadedKMSA1 = loadVector(0, "kmsA");
//    auto vmsA1 = DPF::EvalShamir(loadedKMSA1, logN, 0);
//    auto vmsA2 = DPF::EvalShamir(loadVector(1, "kmsA"), logN, 1);
//    auto vmsA3 = DPF::EvalShamir(loadVector(2, "kmsA"), logN, 2);
//
//    auto vmsA1v2 = DPF::EvalShamir(kmsA[0], logN, 0);
//    auto vmsA2v2 = DPF::EvalShamir(kmsA[1], logN, 1);
//    auto vmsA3v2 = DPF::EvalShamir(kmsA[2], logN, 2);
//
//    auto tmpres = encode_to_shares({
//                                           vmsA1[0],
//                                           vmsA2[0],
//                                           vmsA3[0]
//                                   });
//    auto tmpresres = recover_secret(tmpres, PP);
//
//    auto tmpres2 = encode_to_shares({
//                                           vmsA1v2[0],
//                                           vmsA2v2[0],
//                                           vmsA3v2[0]
//                                   });
//    auto tmpresres2 = recover_secret(tmpres2, PP);
//    std::cout << "Recovered the first value: " << tmpresres << std::endl;
//    std::cout << "Recovered the first value2: " << tmpresres2 << std::endl;
//    std::cout << "Ledger sizes: " << ledgers[0].size() << ", " << ledgers[1].size() << ", " << ledgers[2].size() << std::endl;
//    std::cout << "vms sizes: " << vmsA1.size() << ", " << vmsA2.size() << ", " << vmsA3.size() << std::endl;
//    std::cout << "vms (not disk-loaded) sizes: " << vmsA1v2.size() << ", " << vmsA2v2.size() << ", " << vmsA3v2.size() << std::endl;
//    uint32_t balance1 = PIRW::innerprodff31(vmsA1, ledgers[0]); // In practice, this is the full SumProduct protocol but we will defer it to the end..?
//    uint32_t balance2 = PIRW::innerprodff31(vmsA2, ledgers[1]); // In practice, this is the full SumProduct protocol but we will defer it to the end..?
//    uint32_t balance3 = PIRW::innerprodff31(vmsA3, ledgers[2]); // In practice, this is the full SumProduct protocol but we will defer it to the end..?
//
//    auto balanceshares = encode_to_shares({balance1, balance2, balance3});
//    std::cout << "Balances: " << balance1 << ", " << balance2 << ", " << balance3 << std::endl;
//    auto balance = recover_secret(balanceshares, PP);
//    std::cout << "Balance computed: " << balance << std::endl;
//
//    uint32_t dpfval1 = PIRW::sumvecff31(vmsA1);
//    uint32_t dpfval2 = PIRW::sumvecff31(vmsA2);
//    uint32_t dpfval3 = PIRW::sumvecff31(vmsA3);
//
//    auto dpfvalshares = encode_to_shares({dpfval1, dpfval2, dpfval3});
//    auto dapval = recover_secret(dpfvalshares, PP);
//    std::cout << "DPF Value computed: " << dapval << std::endl;
////    auto val = recover_secret(encode_to_shares({(vmsA1[0]*ledgers[0][0] % PP), (vmsA2[0]*ledgers[1][0] % PP), (vmsA3[0]*ledgers[2][0] % PP)}), PP);
//    auto valshares = encode_to_shares({(vmsA1[0]*ledgers[0][0] % PP), (vmsA2[0]*ledgers[1][0] % PP), (vmsA3[0]*ledgers[2][0] % PP)});
//    auto val = recover_secret(valshares, PP);
//    std::cout << "Trying to reconstruct the first multiplicative value: " << val << std::endl;
//    auto val2 = recover_secret(encode_to_shares({(vmsA1[0] % PP), (vmsA2[0] % PP), (vmsA3[0] % PP)}), PP);
//    std::cout << "Trying to reconstruct the first multiplicative value2: " << val2 << std::endl;
//    std::cout << "balance, adjusting for amount: : " << static_cast<float>(val) / amount << std::endl;
    // end remove temp

    // Call the transfer function
    Server server(serverIndex, N);
    // Generating the underlying vectors..
    std::array<std::vector<uint32_t>, 10> vms;
    for (int i = 0; i < 10; i++) {
        vms[i] = std::vector<uint32_t>((1ULL<< logN));
    }
    server.transfer(kmsA_i, kmsA_i, kmsB_i, tag_A_share, tag_A_share, vms); // TODO: A1 share and tag

    std::cout << "Done" << std::endl;
}

void test_client_deferred(int serverIndex, int logN) {
    int N = 1 << logN;
    int amount = 7;
    int senderIndex = 0;
    int recvIndex = 20;

    // Load/generate data
    std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare> kmsAdefer_i;

    auto kmsAdefer = DPF::DeferredGenShamir(senderIndex, logN);
    auto fn = DATA_DIR + "kmsAdefer" + std::to_string(serverIndex) + ".txt";
    if (fileExists(fn)) {
        kmsAdefer_i = loadPair(fn);
    } else {
        // File does not exist
        for (size_t i = 0; i < kmsAdefer.size(); ++i) {
            auto curr_fn = DATA_DIR + "kmsAdefer" + std::to_string(i) + ".txt";
            writePair(kmsAdefer[i], curr_fn);
        }

        kmsAdefer_i = kmsAdefer[serverIndex];
    }

//    field beta = 55; Below are shares of shares of beta
    std::vector<field> beta0 = {847777152, 1485437038, 2123096924};
    std::vector<field> beta1 = {1261625909, 1239392756, 1217159603};
    std::vector<field> beta2 = {1923965764, 58674887, 340867657};

    Server server(serverIndex, N);
    server.evalDeferredTest(kmsAdefer_i, beta0[serverIndex], beta1[serverIndex], beta2[serverIndex]);

    std::cout << "Done" << std::endl;
}

// Generates and stores data for a single client request
void generate_client_transfer_request(int logN, int reqNumber, std::vector<uint32_t> ledger, std::vector<uint32_t> alphas) {
    int N = 1 << logN;
    int amount = rand() % 512 + 1;
    int senderIndex = 4 * (rand() % ( (N - 1) / 4));
    int recvIndex = 4 * (rand() % ( (N - 1) / 4));

    std::cout << "Generating a request for sender: " << senderIndex << ", receiver: " << recvIndex << ", amount: " << amount << std::endl;

    uint64_t alpha = alphas[senderIndex];

    auto kmsA = DPF::GenShamir(senderIndex, logN, amount, false);
    auto kmsA1 = DPF::GenShamir(senderIndex, logN, 1, false);
    auto kmsB = DPF::GenShamir(recvIndex, logN, amount, true);
    auto kmsAdefer = DPF::DeferredGenShamir(senderIndex, logN);
    auto kmsA1defer = DPF::DeferredGenShamir(senderIndex, logN);

    for (size_t i = 0; i < kmsAdefer.size(); ++i) {
        auto curr_fn = DATA_DIR + "transfer_kmsAdefer" + std::to_string(reqNumber) + "_" + std::to_string(i) + ".txt";
        auto curr_fn1 = DATA_DIR + "transfer_kmsA1defer" + std::to_string(reqNumber) + "_" + std::to_string(i) + ".txt";

        writePair(kmsAdefer[i], curr_fn);
        writePair(kmsA1defer[i], curr_fn1);
        writeVector(i, kmsA[i], "transfer_kmsA" + std::to_string(reqNumber) + "_");
        writeVector(i, kmsA1[i], "transfer_kmsAone" + std::to_string(reqNumber) + "_");
        writeVector(i, kmsB[i], "transfer_kmsB" + std::to_string(reqNumber) + "_");
    }

    field tag_A = mod(alpha*amount, PP);
    std::vector<std::pair<int64_t, int64_t>> tag_A_shares = gen_shares(3, 2, tag_A, PP);

    field tag_A1 = mod(alpha, PP);
    std::vector<std::pair<int64_t, int64_t>> tag_A1_shares = gen_shares(3, 2, tag_A1, PP);

//    field beta = 55; Below are shares of shares of amount and ones
    std::vector<std::pair<int64_t, int64_t>> amount_shares = gen_shares(3, 2, amount, PP);
    std::vector<std::pair<int64_t, int64_t>> beta0_shares = gen_shares(3, 2, amount_shares[0].second, PP);
    std::vector<std::pair<int64_t, int64_t>> beta1_shares = gen_shares(3, 2, mod(amount_shares[1].second*MODINV2, PP), PP);
    std::vector<std::pair<int64_t, int64_t>> beta2_shares = gen_shares(3, 2, mod(amount_shares[2].second*MODINV3, PP), PP);
    // NOTE: this should be fine for security as even the adv know these are shares of 1. Can probably even optimize further
    std::vector<field> one0 = {951264990, 1271295445, 1591325900};
    std::vector<field> one1 = {957422624, 209868890, 1609798803};
    std::vector<field> one2 = {1729613830, 1396337361, 1063060892};

    std::vector<field> otherdata, otherdata1, otherdata2, otherdata3;

    for (int i = 0; i < 3; i++) {
        otherdata.push_back(tag_A_shares[i].second);
        otherdata.push_back(tag_A1_shares[i].second);
        otherdata.push_back(beta0_shares[i].second);
        otherdata.push_back(beta1_shares[i].second);
        otherdata.push_back(beta2_shares[i].second);
        otherdata.push_back(one0[i]);
        otherdata.push_back(one1[i]);
        otherdata.push_back(one2[i]);
    }

    otherdata1 = std::vector<field>(otherdata.begin(), otherdata.begin() + 8);
    otherdata2 = std::vector<field>(otherdata.begin() + 8, otherdata.begin() + 16);
    otherdata3 = std::vector<field>(otherdata.begin() + 16, otherdata.end());

    saveToFile(otherdata1, DATA_DIR + "transfer_otherdata" + std::to_string(reqNumber) + "_1.txt");
    saveToFile(otherdata2, DATA_DIR + "transfer_otherdata" + std::to_string(reqNumber) + "_2.txt");
    saveToFile(otherdata3, DATA_DIR + "transfer_otherdata" + std::to_string(reqNumber) + "_3.txt");

}

struct ClientTransferRequest {
    std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare> kmsAdefer_i, kmsA1defer_i;
    std::vector<DPF::KeyShare> kmsA_i;
    std::vector<DPF::KeyShare> kmsA1_i;
    std::vector<DPF::KeyShare> kmsB_i;
    field tag_A_share, tag_A1_share, beta0, beta1, beta2, one0, one1, one2;
};

ClientTransferRequest load_client_transfer_request(int reqNumber, int idx) {
    ClientTransferRequest clientTransferRequest;

    clientTransferRequest.kmsAdefer_i = loadPair(DATA_DIR + "transfer_kmsAdefer" + std::to_string(reqNumber) + "_" + std::to_string(idx) + ".txt");
    clientTransferRequest.kmsA1defer_i = loadPair(DATA_DIR + "transfer_kmsA1defer" + std::to_string(reqNumber) + "_" + std::to_string(idx) + ".txt");
//    clientTransferRequest.kmsAdefer_i = loadPair(DATA_DIR + "kmsAdefer" + std::to_string(idx) + ".txt");
//    clientTransferRequest.kmsA1defer_i = loadPair(DATA_DIR + "kmsA1defer" + std::to_string(idx) + ".txt");

    clientTransferRequest.kmsA_i = loadVector(idx, "transfer_kmsA" + std::to_string(reqNumber) + "_");
    clientTransferRequest.kmsA1_i = loadVector(idx, "transfer_kmsAone" + std::to_string(reqNumber) + "_");
    clientTransferRequest.kmsB_i = loadVector(idx, "transfer_kmsB" + std::to_string(reqNumber) + "_");

    // Load alphas if file exists
    std::vector<field> otherdata(8);
    std::ifstream otherdataFile(DATA_DIR + "transfer_otherdata" + std::to_string(reqNumber) + "_" + std::to_string(idx + 1) + ".txt");
    if (otherdataFile) {
        for (uint32_t &v : otherdata) {
            otherdataFile >> v;
        }
        otherdataFile.close();
    }

    clientTransferRequest.tag_A_share = otherdata[0];
    clientTransferRequest.tag_A1_share = otherdata[1];
    clientTransferRequest.beta0 = otherdata[2];
    clientTransferRequest.beta1 = otherdata[3];
    clientTransferRequest.beta2 = otherdata[4];
    clientTransferRequest.one0 = otherdata[5];
    clientTransferRequest.one1 = otherdata[6];
    clientTransferRequest.one2 = otherdata[7];

    return clientTransferRequest;
}

namespace fs = std::filesystem;

void deleteFiles(const std::string& prefix, const std::string& directoryPath) {
    try {
        for (const auto& entry : fs::directory_iterator(directoryPath)) {
            if (entry.is_regular_file() && entry.path().filename().string().rfind(prefix, 0) == 0) {
                fs::remove(entry.path());
                std::cout << "Deleted: " << entry.path().filename() << std::endl;
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << e.what() << std::endl;
    }
}

void generate_client_transfer_requests(int nRequests, int logN, bool refreshServers) {
    srand(time(0));
    int N = 1 << logN;

    if (refreshServers) {
        // Remove previous alphas and ledger files
        deleteFiles("ledger", DATA_DIR);
        deleteFiles("alphas", DATA_DIR);

        // Rebuild those files
        Server server0(0, N, true);
        Server server1(1, N, true);
        Server server2(2, N, true);
    }

    // Load alphas and ledger
    std::ifstream ledgerFile(DATA_DIR + "ledger.txt");
    std::ifstream alphasFile(DATA_DIR + "alphas.txt");
    std::vector<field> ledger(N), alphas(N);

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

    // Generate all requests
    for (int i = 0; i < nRequests; i++) {
        std::cout << "Generating request: " << i << std::endl;
        generate_client_transfer_request(logN, i, ledger, alphas);
    }
}

// Runs a single set of benchmarks
std::chrono::duration<double> handleBenchmark(Server* server, int serverIndex, int idx_start, int idx_end, int benchmark, std::array<std::vector<uint32_t>, 10> vms, int logN) {
    int N = 1 << logN;
    std::chrono::duration<double> evalT;
    evalT = std::chrono::duration<double>::zero();
    std::cout << "Running benchmark category: " << BENCHMARK_NAMES[benchmark] << std::endl;

    for (int i = idx_start; i < idx_end; i++) {
        ClientTransferRequest request = load_client_transfer_request(i, serverIndex);
//        std::cout << "Running benchmark number: " << i << std::endl;

        // Before timing, need to prep data
        int alpha, beta;
        std::pair<std::vector<uint8_t>, std::vector<uint8_t>> dpfkeys;
        std::vector<std::vector<DPF::KeyShare>> shamirkeys;
        if (benchmark == 0 || benchmark == 2 || benchmark == 4) {
            alpha = rand() % (N - 1); // 22;
            // Generate a random number between 1 and 2^30
            beta = rand() % (1 << 16) + 1; // 25;
        } else if (benchmark == 1 || benchmark == 3 || benchmark == 5) {
            alpha = rand() % (N - 1); // 22;
            // Generate a random number between 1 and 2^30
            beta = rand() % (1 << 16) + 1; // 25;

            if (benchmark == 1) {
                dpfkeys = DPF::GenM(alpha, logN, beta);
            }

            if (benchmark == 3) {
                shamirkeys = DPF::GenShamir(alpha, logN, beta, false);
            }

            if (benchmark == 5) {
                shamirkeys = DPF::GenShamir(alpha, logN, beta, true);
            }

        }

        auto time1 = std::chrono::high_resolution_clock::now();
        switch (benchmark) {
            case 0:
                DPF::GenM(alpha, logN, beta);
//                std::cout << "DPF.Gen benchmark" << std::endl;
                break;
            case 1:
                DPF::EvalFull8M(dpfkeys.first, vms[0], logN);
//                std::cout << "DPF.EvalAll" << std::endl;
                break;
            case 2:
                DPF::GenShamir(alpha, logN, beta, false);
//                std::cout << "ShamirDPF.Gen" << std::endl;
                break;
            case 3:
                DPF::EvalShamir(shamirkeys[0], vms[0], vms[1], logN, 0, false);
//                std::cout << "ShamirDPF.EvalAll" << std::endl;
                break;
            case 4:
                DPF::GenShamir(alpha, logN, beta, true);
//                std::cout << "VerShamirDPF.Gen" << std::endl;
                break;
            case 5:
                DPF::EvalShamir(shamirkeys[0], vms[0], vms[1], logN, 0, true);
//                std::cout << "VerShamirDPF.EvalAll" << std::endl;
                break;
            case 6:
                server->balance(request.kmsA1_i, request.tag_A1_share, vms);
//                std::cout << "balance" << std::endl;
                break;
            case 7:
                server->balanceMalicious(request.kmsA1_i, request.kmsA1defer_i, request.tag_A1_share, request.one0,
                                        request.one1, request.one2, vms);
//                std::cout << "balanceMalicious" << std::endl;
                break;
            case 8:
                server->transfer(request.kmsA_i, request.kmsA1_i, request.kmsB_i, request.tag_A_share,
                                request.tag_A1_share, vms);
//                std::cout << "transfer" << std::endl;
                break;
            case 9:
                server->transferMalicious(request.kmsA_i, request.kmsAdefer_i, request.kmsA1_i, request.kmsA1defer_i,
                                         request.kmsB_i, request.tag_A_share, request.tag_A1_share,
                                         request.beta0, request.beta1, request.beta2, request.one0,
                                         request.one1, request.one2, vms);
//                std::cout << "transferMalicious" << std::endl;
                break;
            default:
                std::cout << "No such benchmark" << std::endl;
                return evalT;
        }
        auto time2 = std::chrono::high_resolution_clock::now();

        if (i > idx_start + 5) {
            // first 5 runs are warmups
            evalT += time2 - time1;
        }

    }

    return evalT;
}

// Runs many client transfers
// Assumes generate_client_transfer_requests() was called with at least nRequests and the same logN
void test_client_transfers(int nRequests, int serverIndex, int logN, bool isMalicious, bool isTransfer = true, bool specificIndex = false) {
    int N = 1 << logN;
    Server server(serverIndex, N);

    int idx_start = 0;
    int idx_end = nRequests;
    if (specificIndex) {
        // Only run for 'nRequests' - for debugging a specific DPF
        idx_start = nRequests;
        idx_end += 1;
    }

    // Generating the underlying vectors..
    std::array<std::vector<uint32_t>, 10> vms;
    for (int i = 0; i < 10; i++) {
        vms[i] = std::vector<uint32_t>((1ULL<< logN));
    }

    int benchmark = 6;
    if (isTransfer) {
        benchmark = 8;
    }

    if (isMalicious) {
        benchmark += 1;
    }

    auto evalT = handleBenchmark(&server, serverIndex, idx_start, idx_end, benchmark, vms, logN);
    auto func = "getBalances";
    if (isTransfer) {
        func = "transfers";
    }
    std::cout << "Client " << func << " (malicious=" << isMalicious << ") took overall: " << evalT.count() << "sec. Each iteration took: " << evalT.count()/(idx_end-idx_start) << " secs. " << std::endl;

    server.closeConnections();
}

void test_client_malicious(int serverIndex, int logN) {
    int N = 1 << logN;
    int amount = 55;
    int senderIndex = 0;
    int recvIndex = 20;
    uint64_t alpha = 1969685477;

    // Load/generate data
    std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare> kmsAdefer_i, kmsA1defer_i;

    // Load/generate data
    std::vector<DPF::KeyShare> kmsA_i;
    std::vector<DPF::KeyShare> kmsA1_i;
    std::vector<DPF::KeyShare> kmsB_i;

    auto kmsA = DPF::GenShamir(senderIndex, logN, amount, false);
    auto kmsA1 = DPF::GenShamir(senderIndex, logN, 1, false);
    auto kmsB = DPF::GenShamir(recvIndex, logN, amount, true);
    auto kmsAdefer = DPF::DeferredGenShamir(senderIndex, logN);
    auto kmsA1defer = DPF::DeferredGenShamir(senderIndex, logN);
//    auto fn = DATA_DIR + "kmsAdefer" + std::to_string(serverIndex) + ".txt";
//    auto fn1 = DATA_DIR + "kmsA1defer" + std::to_string(serverIndex) + ".txt";
    auto fn = DATA_DIR + "transfer_kmsAdefer" + std::to_string(193) + "_" + std::to_string(serverIndex) + ".txt";
    auto fn1 = DATA_DIR + "transfer_kmsA1defer" + std::to_string(193) + "_" + std::to_string(serverIndex) + ".txt";

    if (fileExists(fn)) {
        kmsAdefer_i = loadPair(fn);
        kmsA1defer_i = loadPair(fn1);
        kmsA_i = loadVector(serverIndex, "kmsA");
        kmsA1_i = loadVector(serverIndex, "kmsAone");
        kmsB_i = loadVector(serverIndex, "kmsB");
    } else {
        // File does not exist
        for (size_t i = 0; i < kmsAdefer.size(); ++i) {
            auto curr_fn = DATA_DIR + "kmsAdefer" + std::to_string(i) + ".txt";
            auto curr_fn1 = DATA_DIR + "kmsA1defer" + std::to_string(i) + ".txt";
            writePair(kmsAdefer[i], curr_fn);
            writePair(kmsA1defer[i], curr_fn1);
            writeVector(i, kmsA[i], "kmsA");
            writeVector(i, kmsA1[i], "kmsAone");
            writeVector(i, kmsB[i], "kmsB");
        }

        kmsAdefer_i = kmsAdefer[serverIndex];
        kmsA1defer_i = kmsA1defer[serverIndex];
        kmsA_i = kmsA[serverIndex];
        kmsA1_i = kmsA1[serverIndex];
        kmsB_i = kmsB[serverIndex];
    }

//    field tag_A = (alpha*amount) % PP;
//    std::vector<std::pair<int64_t, int64_t>> tag_A_shares = gen_shares(3, 2, tag_A, PP);
    std::vector<std::pair<int64_t, int64_t>> tag_A_shares = {
            {1, 1235626339},
            {2, 1512733793},
            {3, 1789841247}
    };
    field tag_A_share = tag_A_shares[serverIndex].second;
//    field tag_A1 = (alpha) % PP;
//    std::vector<std::pair<int64_t, int64_t>> tag_A1_shares = gen_shares(3, 2, tag_A1, PP);
    std::vector<std::pair<int64_t, int64_t>> tag_A1_shares = {
            {1, 988340624},
            {2, 6995771},
            {3, 1173134565}
    };
    field tag_A1_share = tag_A1_shares[serverIndex].second;

//    field beta = 55; Below are shares of shares of amount and ones
    std::vector<field> beta0 = {847777152, 1485437038, 2123096924};
    std::vector<field> beta1 = {1261625909, 1239392756, 1217159603};
    std::vector<field> beta2 = {1923965764, 58674887, 340867657};

//    // Shares of 1 - [[1843535466], [1539587284], [1235639102]]. Below are shares of these shares:
//    std::vector<field> one0 = {1368805972, 894076478, 419346984};
//    std::vector<field> one1 = {1920794995, 154519059, 535726770};
//    std::vector<field> one2 = {158492188, 1228828921, 151682007};

//    Shares of 1 - [[631234535], [1262469069], [1893703603]]
// These shares are of [[631234535], [1262469069*modinv(2)], [1893703603*modinv(3)]]
//    [[951264990], [1271295445], [1591325900]]
//    [[957422624], [209868890], [1609798803]]
//    [[1729613830], [1396337361], [1063060892]]
    std::vector<field> one0 = {951264990, 1271295445, 1591325900};
    std::vector<field> one1 = {957422624, 209868890, 1609798803};
    std::vector<field> one2 = {1729613830, 1396337361, 1063060892};

    Server server(serverIndex, N);

    // Generating the underlying vectors..
    std::array<std::vector<uint32_t>, 10> vms;
    for (int i = 0; i < 10; i++) {
        vms[i] = std::vector<uint32_t>((1ULL<< logN));
    }

    // Test semi-honest
    server.transfer(kmsA_i, kmsA1_i, kmsB_i, tag_A_share, tag_A1_share, vms);

    // Test malicious
    server.transferMalicious(kmsA_i, kmsAdefer_i, kmsA1_i, kmsA1defer_i, kmsB_i, tag_A_share, tag_A1_share,
                                 beta0[serverIndex], beta1[serverIndex], beta2[serverIndex], one0[serverIndex],
                                 one1[serverIndex], one2[serverIndex], vms);

    std::cout << "Done" << std::endl;
}

// Helper function - mostly temp, to help debug what happens in the deferred stuf
void deconstruct_deferreddpf(int logN, int serverIndex) {
    int N = 1 << logN;

    std::vector<std::pair<DPF::DeferredKeyShare, DPF::DeferredKeyShare>> kmsA1defer;
    std::vector<std::vector<std::pair<uint8_t , uint8_t>>> key0_s0_shares, key1_s0_shares, key0_s1_shares, key1_s1_shares;
    std::vector<std::pair<uint8_t, uint8_t>> key0_t0_shares, key1_t0_shares;

    for (int i = 0; i < 3; i++) {
//        auto fn1 = DATA_DIR + "kmsA1defer" + std::to_string(i) + ".txt";
//        auto fn1 = DATA_DIR + "kmsAdefer" + std::to_string(i) + ".txt";
        auto fn1 = DATA_DIR + "transfer_kmsAdefer" + std::to_string(56) + "_" + std::to_string(i) + ".txt";
        auto kmsA1defer_i = loadPair(fn1);
        kmsA1defer.push_back(kmsA1defer_i);
        key0_s0_shares.push_back(kmsA1defer_i.first.s0_share);
        key0_s1_shares.push_back(kmsA1defer_i.first.s1_share);
        key1_s0_shares.push_back(kmsA1defer_i.second.s0_share);
        key1_s1_shares.push_back(kmsA1defer_i.second.s1_share);
        key0_t0_shares.push_back(kmsA1defer_i.first.t0_share);
        key1_t0_shares.push_back(kmsA1defer_i.second.t0_share);
    }

    std::vector<uint8_t> key0_s0, key0_s1, key1_s0, key1_s1;
    for (int i = 0; i < key0_s0_shares[0].size(); i++) {
        key0_s0.push_back(reconstruct_gf256({key0_s0_shares[0][i], key0_s0_shares[1][i], key0_s0_shares[2][i]}));
        key0_s1.push_back(reconstruct_gf256({key0_s1_shares[0][i], key0_s1_shares[1][i], key0_s1_shares[2][i]}));
        key1_s0.push_back(reconstruct_gf256({key1_s0_shares[0][i], key1_s0_shares[1][i], key1_s0_shares[2][i]}));
        key1_s1.push_back(reconstruct_gf256({key1_s1_shares[0][i], key1_s1_shares[1][i], key1_s1_shares[2][i]}));
    }

    std::cout << "key0, s0: ";
    printVector(key0_s0);
    std::cout << "key0, s1: ";
    printVector(key0_s1);
    std::cout << "key1, s0: ";
    printVector(key1_s0);
    std::cout << "key1, s1: ";
    printVector(key1_s1);

    auto key0_t0 = reconstruct_gf256(key0_t0_shares);
    auto key1_t0 = reconstruct_gf256(key1_t0_shares);
    std::cout << "key0, t0: " << int(key0_t0) << std::endl;
    std::cout << "key1, t0: " << int(key1_t0) << std::endl;

    if (serverIndex == -1) {
        return;
    }

    // Mock up servers and test the DPF
    Server server(serverIndex, N);
    //    field beta = 55; Below are shares of shares of beta
    std::vector<field> beta0 = {847777152, 1485437038, 2123096924};
    std::vector<field> beta1 = {1261625909, 1239392756, 1217159603};
    std::vector<field> beta2 = {1923965764, 58674887, 340867657};

    server.evalDeferredTest(kmsA1defer[serverIndex], beta0[serverIndex], beta1[serverIndex], beta2[serverIndex]);

}

void test_server(int serverIndex, int logN) {
    int N = 1 << logN;

    Server server(serverIndex, N);

    auto kms1 = DPF::GenShamir(15, logN, 150);
    auto kms2 = DPF::GenShamir(20, logN, 300);
    uint32_t tag_share = 54;

    // Call the transfer function
//    server.transfer(kms1[0], kms2[0], tag_share); //TODO: index to kms1, kms2 based on server_index

    std::cout << "Done" << std::endl;
}

// test gf256 shares library
void test_shamir_gf256() {

    // Example usage
    int secret = 123; // The secret to share
    int n = 3;       // Total number of shares
    int k = 1;       // Threshold

    auto shares = share_gf256(secret, n, k);
    // Display shares
    for (const auto& share : shares) {
        std::cout << "Share " << static_cast<int>(share.first) << ": " << static_cast<int>(share.second) << std::endl;
    }

    // Reconstruction using any k shares
//    int reconstructed_secret = reconstruct_gf256({shares[0], shares[1], shares[2]});
    int reconstructed_secret = reconstruct_gf256({shares[0], shares[1]});
    std::cout << "Reconstructed Secret: " << reconstructed_secret << std::endl;


    // Vector of shares

    // Example usage with a vector of integers
    std::vector<uint8_t> secrets = {123, 45, 67, 89}; // The secrets to share

    auto all_shares = share_gf256_vector(secrets, n, k);

    // Display shares for each secret
    for (size_t i = 0; i < all_shares.size(); ++i) {
        std::cout << "Secret " << i + 1 << " Shares:" << std::endl;
        for (const auto& share : all_shares[i]) {
            std::cout << "  Share " << share.first << ": " << share.second << std::endl;
        }
    }

    // Reconstruction using any k shares from each set
    std::vector<std::vector<std::pair<uint8_t, uint8_t>>> selected_shares;
    for (const auto& shares : all_shares) {
        selected_shares.push_back({shares[0], shares[1]});
    }
    auto reconstructed_secrets = reconstruct_gf256_vector(selected_shares);

    // Display reconstructed secrets
    std::cout << "Reconstructed Secrets:" << std::endl;
    for (int secret : reconstructed_secrets) {
        std::cout << secret << " ";
    }
    std::cout << std::endl;


    // Test XOR
    // Example usage with two vectors of integers
    std::vector<uint8_t> x = {123, 45, 67, 89}; // First vector of secrets
    std::vector<uint8_t> y = {12, 34, 56, 78};  // Second vector of secrets

    auto x_shares = share_gf256_vector(x, n, k);
    auto y_shares = share_gf256_vector(y, n, k);

    // Generate shares of z = x ^ y using the refactored xor_shares_vector function
    std::vector<std::vector<std::pair<uint8_t, uint8_t>>> z_shares(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        z_shares[i] = xor_shares_vector(x_shares[i], y_shares[i]);
    }

    // Reconstruct z and verify it equals x ^ y
    auto reconstructed_z = reconstruct_gf256_vector(z_shares);
    std::vector<int> expected_z;
    for (size_t i = 0; i < x.size(); ++i) {
        expected_z.push_back((x[i] ^ y[i]));
    }

    // Display results
    std::cout << "Reconstructed Z: ";
    for (int val : reconstructed_z) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    std::cout << "Expected Z: ";
    for (int val : expected_z) {
        std::cout << val << " ";
    }
    std::cout << std::endl;


    // Test specific values
    std::vector<std::vector<std::pair<uint8_t, uint8_t>>> v = {
            {
                    std::make_pair(1, 230),
                    std::make_pair(2, 205),
                    std::make_pair(3, 212),
            },
            {
                    std::make_pair(1, 245),
                    std::make_pair(2, 69),
                    std::make_pair(3, 222),
            },
            {
                    std::make_pair(1, 7),
                    std::make_pair(2, 140),
                    std::make_pair(3, 245),
            },
            {
                    std::make_pair(1, 103),
                    std::make_pair(2, 8),
                    std::make_pair(3, 45),
            }
    };

    for (int i = 0; i < v.size(); i++) {
        shares = v[i];
        uint8_t x1 = reconstruct_gf256(shares);
        uint8_t x2 = reconstruct_gf256({shares[0], shares[1] });
        std::cout << "x[" << i << "]: " << int(x1) << std::endl;
        assert (x1 == x2);
    }

}

void benchmark_hashfuncs() {
    std::chrono::duration<double> evalT1, evalT2;
    evalT1 = std::chrono::duration<double>::zero();
    evalT2 = std::chrono::duration<double>::zero();
    std::array<block, 4> arr = {ZeroBlock, ZeroBlock, ZeroBlock, ZeroBlock};
    std::array<block, 4> arr2 = {ZeroBlock, ZeroBlock, ZeroBlock, ZeroBlock};

    for (int i = 0; i<100000; i++) {
        uint32_t r1 = rand();
        uint32_t r2 = rand();
        uint32_t r3 = rand();
        uint32_t r4 = rand();

        // Combine the four 32-bit numbers into a 128-bit number
        block seed = _mm_set_epi32(r1, r2, r3, r4);
        uint32_t alpha = 24323;

        auto time1 = std::chrono::high_resolution_clock::now();
        auto res = DPF::prg::hash1(seed, alpha);
        auto time2 = std::chrono::high_resolution_clock::now();
        evalT1 += time2 - time1;

        arr[0] ^= res[0];
        arr[1] ^= res[1];
        arr[2] ^= res[2];
        arr[3] ^= res[3];

        unsigned char digest[CryptoPP::SHA512::DIGESTSIZE];
//        block* blk = reinterpret_cast<block*>(digest);

        time1 = std::chrono::high_resolution_clock::now();
//        DPF::prg::hash1v2(seed, alpha, blk);
        auto blk = DPF::prg::hash1v2(seed, alpha);
        time2 = std::chrono::high_resolution_clock::now();
        evalT2 += time2 - time1;

        arr[0] ^= blk[0];
        arr[1] ^= blk[1];
        arr[2] ^= blk[2];
        arr[3] ^= blk[3];
    }

    std::cout << "Hash1 took (AES):" << evalT1.count() << "sec. Hash1v2 took (SHA256)" << evalT2.count() << " secs. " << std::endl;

}

int reconstruct_fake_xor_rand() {
    auto a = fake_xor_rand(0);
    auto b = fake_xor_rand(1);
    auto c = fake_xor_rand(2);

    for (int i = 0; i < a.size(); i++) {
        auto v = reconstruct_gf256({std::make_pair(1, a[i].second), std::make_pair(2, b[i].second), std::make_pair(3, c[i].second)});
        std::cout << int(v) << std::endl;
    }
//    auto res = reconstruct_gf256_vector({a, b, c});

}

void test_shares_mult_gf256() {
    uint8_t pub = 176;
    auto t0_shares = share_gf256(0, 3, 1);
    uint8_t t1_val = 0xFF;
    auto t1_shares = share_gf256(t1_val, 3, 1);

    std::vector<std::pair<uint8_t, uint8_t>> res0, res1;

    for (int i = 0; i < t0_shares.size(); i++) {
        res0.push_back(std::make_pair(t0_shares[i].first, t0_shares[i].second & pub));
        res1.push_back(std::make_pair(t1_shares[i].first, t1_shares[i].second & pub));
    }
    auto v0 = reconstruct_gf256(res0);
    auto v1 = reconstruct_gf256(res1);

    assert(v0 == (0 & pub));
    assert(v1 == (t1_val & pub));
    std::cout << "Test done!" << std::endl;
}

void test_dpf_single_eval(int logN) {
    int N = 1 << logN;
    auto keys = DPF::GenM(10, logN, 25);

    // Generating the underlying vectors..
    std::array<std::vector<uint32_t>, 2> vms;
    for (int i = 0; i < 2; i++) {
        vms[i] = std::vector<uint32_t>((1ULL<< logN));
    }

    DPF::EvalFull8M(keys.first, vms[0], logN, false);
    DPF::EvalFull8M(keys.second, vms[1], logN, true);

//    std::cout << (vms[0][9] ^ vms[1][9]) << std::endl;
//    std::cout << (vms[0][10] ^ vms[1][10]) << std::endl;
    for (int i = 0; i < N; i++) {
        std::cout << (vms[0][i] ^ vms[1][i]) << std::endl;

        auto v1 = DPF::EvalM(keys.first, i, logN);
        auto v2 = DPF::EvalM(keys.second, i, logN);
        std::cout << "v2: " << (v1 ^ v2) << std::endl;
    }

}

void appendToCSV(const std::string& benchmarkName, int serverIndex, int logN, int nBenchmarks, double benchmarkTime) {
    std::string filename = DATA_DIR + "benchmark_results" + std::to_string(serverIndex) + ".csv";
    bool exists = fileExists(filename);

    // Open file in append mode if it exists, otherwise in write mode
    std::ofstream file(filename, exists ? std::ofstream::app : std::ofstream::out);

    if (!file.is_open()) {
        std::cerr << "Unable to open file: " << filename << std::endl;
        return;
    }

    // If file did not exist, write the header
    if (!exists) {
        file << "Benchmark Name, Log DB size, # Benchmarks, Single Benchmark Time\n";
    }

    // Append the new record
    file << benchmarkName << ", " << logN << ", " << nBenchmarks << ", " << benchmarkTime << "\n";

    file.close();
}

void benchmark_suite(std::vector<std::string>& benchmarks, int serverIndex, int logN, bool generateData = true, int nBenchmarks = 300) {
    int N = 1 << logN;

    // Vector to store the corresponding values
    std::vector<int> benchmarksToRun;
    bool networkBenchmarks = false;

    // Loop over the string vector
    for (const std::string& key : benchmarks) {
        // Check if the key exists in the map
        if (ALL_BENCHMARKS.find(key) != ALL_BENCHMARKS.end()) {
            // Add the corresponding value to the values vector
            int b = ALL_BENCHMARKS.at(key);
            if (b > 5) {
                networkBenchmarks = true;
            }
            benchmarksToRun.push_back(b);
        } else {
            // If the key is not found, you might want to handle this case
            std::cout << "Key '" << key << "' not found in the map." << std::endl;
        }
    }

    if (networkBenchmarks && generateData) {
        if (serverIndex == 0) {
            // Single dealer
            generate_client_transfer_requests(nBenchmarks, logN, true);
        }
    }

    Server* server;
    if (networkBenchmarks) {
        server = new Server(serverIndex, N);
    }

    // Generating the underlying vectors..
    std::array<std::vector<uint32_t>, 10> vms;
    for (int i = 0; i < 10; i++) {
        vms[i] = std::vector<uint32_t>((1ULL<< logN));
    }

    for (auto benchmark : benchmarksToRun) {
        auto evalT = handleBenchmark(server, serverIndex, 0, nBenchmarks, benchmark, vms, logN);

        // TODO: write to a CSV file:
        auto evalT_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(evalT);
        double benchmarkTime = evalT_milliseconds.count();
        std::cout << "Benchmark: " << BENCHMARK_NAMES[benchmark] << " took overall: " << benchmarkTime << "ms. Each iteration took: " << evalT.count()/nBenchmarks << " ms. " << std::endl;
        appendToCSV(BENCHMARK_NAMES[benchmark], serverIndex, logN, nBenchmarks, benchmarkTime/nBenchmarks);
    }

    if (networkBenchmarks) {
        server->closeConnections();
        delete server;
    }
    std::cout << "Finished running all benchmarks!" << std::endl;
}


int main(int argc, char** argv) {
    std::cout << "Current working directory: "
              << std::filesystem::current_path()
              << std::endl;

    if(argc < 4) {
	    std::cout << "Usage: ./prioram <server_index> <log_tree_size> <benchmark1>, <benchmark2>..." << std::endl;
        std::cout << "List of Available Benchmarks:" << std::endl;
        for (auto benchmarkName : BENCHMARK_NAMES) {
            std::cout << benchmarkName << std::endl;
        }
        std::cout << "An alternative to run only DPF tests (local) is to set benchmark1 = local. In this case, all DPF benchmarks will run from logN = 10 and up to log_tree_size" << std::endl;

        return -1;
    }
    size_t N = std::strtoull(argv[2], nullptr, 10);
    int serverIndex = std::atoi(argv[1]);

    std::vector<std::string> benchmarks;
    bool isLocal = std::strcmp(argv[3], "local") == 0;
    if (isLocal) {
        for (int i = 0; i < 6; i++) {
            benchmarks.push_back(BENCHMARK_NAMES[i]);
        }
    } else {
        for (int i = 3; i < argc; ++i) {
            benchmarks.push_back(argv[i]);
        }
    }

    // Pre-processing and memory allocation
    generate_tables();
    generate_random_sharings(100000, PP, 123);

    extern std::vector<block> globalVector0, globalVector1;
    globalVector0 = std::vector<block>((1ULL << N) / 4);
    globalVector1 = std::vector<block>((1ULL << N) / 4);

//    print_fake_block_sharing();
//    testSerialization();
//    print_fake_zero_triplets_code();
//    test_shamir_gf256();
//    test_hash_functions();
//    int x = run_playground_tests(N); // misc tests
//    reconstruct_fake_xor_rand();
//    test_shares_mult_gf256();
//    mock_fproduct_test(N);
//    deconstruct_deferreddpf(N, serverIndex);
//    test_dpf_single_eval(10);

    // Benchmark mersenne modulus
//    benchmark_mersenne();
//    test_sumproduct();
//    benchmark_hashfuncs();
//    test_server(serverIndex, N);
//    test_client_deferred(serverIndex, N);
//    test_client(serverIndex, N);
//    test_client_malicious(serverIndex, N);
//    generate_client_transfer_requests(100, N, true);
//    test_client_transfers(100, serverIndex, N, false);

//    bool isTransfer = true;
//    bool isMalicious = true;
//    test_client_transfers(100, serverIndex, N, isMalicious, isTransfer);
////    test_client_transfers(56, serverIndex, N, true, true);

    int nBenchmarks = 100; // How many iterations to run for each benchmark

    if (isLocal) {
        // Run all local tests
        for (int i = 10; i < N + 1; i++) {
            std::cout << "Running local benchmarks for logN = " << i << std::endl;
            benchmark_suite(benchmarks, serverIndex, i, false, nBenchmarks);
        }
    } else {
        bool generateData = true; // Need to refresh the data before running this benchmark. Can skip if already ran the same test..
        benchmark_suite(benchmarks, serverIndex, N, generateData, nBenchmarks);
    }


    return 0;
}
