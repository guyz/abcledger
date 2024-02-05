#include "AES.h"
#include <cassert>

const int N_PRFS = 64;
const uint8_t base_key[16] = {36, 156, 50, 234, 92, 230, 49, 9, 174, 170, 205, 160, 98, 236, 29, 243};

const uint8_t fixed_keys[N_PRFS][16] = {
        {36, 156, 50, 234, 92, 230, 49, 9, 174, 170, 205, 160, 98, 236, 29, 243},
        {37, 157, 51, 235, 93, 231, 50, 10, 175, 171, 206, 161, 99, 237, 30, 244},
        {38, 158, 52, 236, 94, 232, 51, 11, 176, 172, 207, 162, 100, 238, 31, 245},
        {39, 159, 53, 237, 95, 233, 52, 12, 177, 173, 208, 163, 101, 239, 32, 246},
        {40, 160, 54, 238, 96, 234, 53, 13, 178, 174, 209, 164, 102, 240, 33, 247},
        {41, 161, 55, 239, 97, 235, 54, 14, 179, 175, 210, 165, 103, 241, 34, 248},
        {42, 162, 56, 240, 98, 236, 55, 15, 180, 176, 211, 166, 104, 242, 35, 249},
        {43, 163, 57, 241, 99, 237, 56, 16, 181, 177, 212, 167, 105, 243, 36, 250},
        {44, 164, 58, 242, 100, 238, 57, 17, 182, 178, 213, 168, 106, 244, 37, 251},
        {45, 165, 59, 243, 101, 239, 58, 18, 183, 179, 214, 169, 107, 245, 38, 252},
        {46, 166, 60, 244, 102, 240, 59, 19, 184, 180, 215, 170, 108, 246, 39, 253},
        {47, 167, 61, 245, 103, 241, 60, 20, 185, 181, 216, 171, 109, 247, 40, 254},
        {48, 168, 62, 246, 104, 242, 61, 21, 186, 182, 217, 172, 110, 248, 41, 255},
        {49, 169, 63, 247, 105, 243, 62, 22, 187, 183, 218, 173, 111, 249, 42, 0},
        {50, 170, 64, 248, 106, 244, 63, 23, 188, 184, 219, 174, 112, 250, 43, 1},
        {51, 171, 65, 249, 107, 245, 64, 24, 189, 185, 220, 175, 113, 251, 44, 2},
        {52, 172, 66, 250, 108, 246, 65, 25, 190, 186, 221, 176, 114, 252, 45, 3},
        {53, 173, 67, 251, 109, 247, 66, 26, 191, 187, 222, 177, 115, 253, 46, 4},
        {54, 174, 68, 252, 110, 248, 67, 27, 192, 188, 223, 178, 116, 254, 47, 5},
        {55, 175, 69, 253, 111, 249, 68, 28, 193, 189, 224, 179, 117, 255, 48, 6},
        {56, 176, 70, 254, 112, 250, 69, 29, 194, 190, 225, 180, 118, 0, 49, 7},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 50},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 51},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 52},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 53},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 54},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 55},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 56},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 57},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 58},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 59},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 60},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 61},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 62},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 63},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 64},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 65},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 66},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 67},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 68},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 69},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 70},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 71},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 72},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 73},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 74},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 75},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 76},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 77},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 78},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 79},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 80},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 81},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 82},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 83},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 84},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 85},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 86},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 87},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 88},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 89},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 90},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 91},
        {99, 219, 113, 41, 155, 37, 112, 72, 237, 233, 12, 223, 161, 43, 92, 92}

};

const AES mAesFixedKeys[N_PRFS] = {
        AES(fixed_keys[0]), AES(fixed_keys[1]), AES(fixed_keys[2]), AES(fixed_keys[3]), AES(fixed_keys[4]),
        AES(fixed_keys[5]), AES(fixed_keys[6]), AES(fixed_keys[7]), AES(fixed_keys[8]), AES(fixed_keys[9]),
        AES(fixed_keys[10]), AES(fixed_keys[11]), AES(fixed_keys[12]), AES(fixed_keys[13]), AES(fixed_keys[14]),
        AES(fixed_keys[15]), AES(fixed_keys[16]), AES(fixed_keys[17]), AES(fixed_keys[18]), AES(fixed_keys[19]),
        AES(fixed_keys[20]), AES(fixed_keys[21]), AES(fixed_keys[22]), AES(fixed_keys[23]), AES(fixed_keys[24]),
        AES(fixed_keys[25]), AES(fixed_keys[26]), AES(fixed_keys[27]), AES(fixed_keys[28]), AES(fixed_keys[29]),
        AES(fixed_keys[30]), AES(fixed_keys[31]), AES(fixed_keys[32]), AES(fixed_keys[33]), AES(fixed_keys[34]),
        AES(fixed_keys[35]), AES(fixed_keys[36]), AES(fixed_keys[37]), AES(fixed_keys[38]), AES(fixed_keys[39]),
        AES(fixed_keys[40]), AES(fixed_keys[41]), AES(fixed_keys[42]), AES(fixed_keys[43]), AES(fixed_keys[44]),
        AES(fixed_keys[45]), AES(fixed_keys[46]), AES(fixed_keys[47]), AES(fixed_keys[48]), AES(fixed_keys[49]),
        AES(fixed_keys[50]), AES(fixed_keys[51]), AES(fixed_keys[52]), AES(fixed_keys[53]), AES(fixed_keys[54]),
        AES(fixed_keys[55]), AES(fixed_keys[56]), AES(fixed_keys[57]), AES(fixed_keys[58]), AES(fixed_keys[59]),
        AES(fixed_keys[60]), AES(fixed_keys[61]), AES(fixed_keys[62]), AES(fixed_keys[63])
};
const AES mAesFixedKey = mAesFixedKeys[0];
const AES mAesFixedKey2 = mAesFixedKeys[1];
const AES mAesFixedKey3 = mAesFixedKeys[2];

//const uint8_t fixed_key[16] = {36,156,50,234,92,230,49,9,174,170,205,160,98,236,29,243};
//const AES mAesFixedKey(fixed_key);
//const uint8_t fixed_key2[16] = {209, 12, 199, 173, 29, 74, 44, 128, 194, 224, 14, 44, 2, 201, 110, 28};
//const AES mAesFixedKey2(fixed_key2);
const uint8_t fixed_key3[16] = {36,156,50,234,92,230,49,9,174,170,205,160,98,236,29,243};

CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption ecbEncryptor(fixed_key3, sizeof(fixed_key3));

// Testing a different version of AES. Conclusion is that it's probably not better, but segfault in release mode so didn't bother
void EncryptAesEcb(const block& inputBlock, block& outputBlock)
{
    // Convert __m128i to Crypto++ SecByteBlock
    CryptoPP::SecByteBlock blk(reinterpret_cast<const byte*>(&inputBlock), sizeof(block));

    // Encrypt the block
    CryptoPP::SecByteBlock encryptedBlock(blk.size());
    ecbEncryptor.ProcessData(encryptedBlock, blk, blk.size());

    // Copy the encrypted data back to __m128i
    memcpy(&outputBlock, encryptedBlock.data(), encryptedBlock.size());
}


block keyGenHelper(block key, block keyRcon)
{
    keyRcon = _mm_shuffle_epi32(keyRcon, _MM_SHUFFLE(3, 3, 3, 3));
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
    return _mm_xor_si128(key, keyRcon);
}

AES::AES() {
    uint8_t zerokey[] = {0,0,0,0, 0,0,0,0, 0,0,0,0 , 0,0,0,0};
    setKey(toBlock(zerokey));
}

AES::AES(const block& key) {
    setKey(key);
}
AES::AES(const uint8_t* key) {
    setKey(key);
}

void AES::setKey(const block& key) {
    mRoundKeysEnc[0] = key;
    mRoundKeysEnc[1] = keyGenHelper(mRoundKeysEnc[0], _mm_aeskeygenassist_si128(mRoundKeysEnc[0], 0x01));
    mRoundKeysEnc[2] = keyGenHelper(mRoundKeysEnc[1], _mm_aeskeygenassist_si128(mRoundKeysEnc[1], 0x02));
    mRoundKeysEnc[3] = keyGenHelper(mRoundKeysEnc[2], _mm_aeskeygenassist_si128(mRoundKeysEnc[2], 0x04));
    mRoundKeysEnc[4] = keyGenHelper(mRoundKeysEnc[3], _mm_aeskeygenassist_si128(mRoundKeysEnc[3], 0x08));
    mRoundKeysEnc[5] = keyGenHelper(mRoundKeysEnc[4], _mm_aeskeygenassist_si128(mRoundKeysEnc[4], 0x10));
    mRoundKeysEnc[6] = keyGenHelper(mRoundKeysEnc[5], _mm_aeskeygenassist_si128(mRoundKeysEnc[5], 0x20));
    mRoundKeysEnc[7] = keyGenHelper(mRoundKeysEnc[6], _mm_aeskeygenassist_si128(mRoundKeysEnc[6], 0x40));
    mRoundKeysEnc[8] = keyGenHelper(mRoundKeysEnc[7], _mm_aeskeygenassist_si128(mRoundKeysEnc[7], 0x80));
    mRoundKeysEnc[9] = keyGenHelper(mRoundKeysEnc[8], _mm_aeskeygenassist_si128(mRoundKeysEnc[8], 0x1B));
    mRoundKeysEnc[10] = keyGenHelper(mRoundKeysEnc[9], _mm_aeskeygenassist_si128(mRoundKeysEnc[9], 0x36));
}

void AES::setKey(const uint8_t* key) {
   setKey(toBlock(key));
}

void AES::encryptECB(const block& plaintext, block& ciphertext) const {
    ciphertext = _mm_xor_si128(plaintext, mRoundKeysEnc[0]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[1]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[2]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[3]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[4]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[5]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[6]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[7]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[8]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[9]);
    ciphertext = _mm_aesenclast_si128(ciphertext, mRoundKeysEnc[10]);
}

void AES::encryptECB_MMO(const block& plaintext, block& ciphertext) const {
    ciphertext = _mm_xor_si128(plaintext, mRoundKeysEnc[0]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[1]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[2]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[3]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[4]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[5]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[6]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[7]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[8]);
    ciphertext = _mm_aesenc_si128(ciphertext, mRoundKeysEnc[9]);
    ciphertext = _mm_aesenclast_si128(ciphertext, mRoundKeysEnc[10]);
    ciphertext = _mm_xor_si128(ciphertext, plaintext);
}

void AES::encryptECBBlocks(const block* plaintexts, uint64_t blockLength, block* ciphertexts) const {

    const uint64_t step = 8;
    uint64_t idx = 0;
    uint64_t length = blockLength - blockLength % step;

    //std::array<block, step> temp;
    block temp[step];

    for (; idx < length; idx += step)
    {
        temp[0] = _mm_xor_si128(plaintexts[idx + 0], mRoundKeysEnc[0]);
        temp[1] = _mm_xor_si128(plaintexts[idx + 1], mRoundKeysEnc[0]);
        temp[2] = _mm_xor_si128(plaintexts[idx + 2], mRoundKeysEnc[0]);
        temp[3] = _mm_xor_si128(plaintexts[idx + 3], mRoundKeysEnc[0]);
        temp[4] = _mm_xor_si128(plaintexts[idx + 4], mRoundKeysEnc[0]);
        temp[5] = _mm_xor_si128(plaintexts[idx + 5], mRoundKeysEnc[0]);
        temp[6] = _mm_xor_si128(plaintexts[idx + 6], mRoundKeysEnc[0]);
        temp[7] = _mm_xor_si128(plaintexts[idx + 7], mRoundKeysEnc[0]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[1]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[1]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[1]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[1]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[1]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[1]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[1]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[1]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[2]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[2]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[2]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[2]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[2]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[2]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[2]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[2]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[3]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[3]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[3]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[3]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[3]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[3]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[3]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[3]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[4]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[4]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[4]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[4]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[4]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[4]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[4]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[4]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[5]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[5]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[5]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[5]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[5]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[5]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[5]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[5]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[6]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[6]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[6]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[6]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[6]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[6]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[6]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[6]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[7]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[7]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[7]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[7]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[7]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[7]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[7]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[7]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[8]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[8]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[8]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[8]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[8]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[8]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[8]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[8]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[9]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[9]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[9]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[9]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[9]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[9]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[9]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[9]);

        ciphertexts[idx + 0] = _mm_aesenclast_si128(temp[0], mRoundKeysEnc[10]);
        ciphertexts[idx + 1] = _mm_aesenclast_si128(temp[1], mRoundKeysEnc[10]);
        ciphertexts[idx + 2] = _mm_aesenclast_si128(temp[2], mRoundKeysEnc[10]);
        ciphertexts[idx + 3] = _mm_aesenclast_si128(temp[3], mRoundKeysEnc[10]);
        ciphertexts[idx + 4] = _mm_aesenclast_si128(temp[4], mRoundKeysEnc[10]);
        ciphertexts[idx + 5] = _mm_aesenclast_si128(temp[5], mRoundKeysEnc[10]);
        ciphertexts[idx + 6] = _mm_aesenclast_si128(temp[6], mRoundKeysEnc[10]);
        ciphertexts[idx + 7] = _mm_aesenclast_si128(temp[7], mRoundKeysEnc[10]);
    }

    for (; idx < blockLength; ++idx)
    {
        ciphertexts[idx] = _mm_xor_si128(plaintexts[idx], mRoundKeysEnc[0]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[1]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[2]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[3]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[4]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[5]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[6]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[7]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[8]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[9]);
        ciphertexts[idx] = _mm_aesenclast_si128(ciphertexts[idx], mRoundKeysEnc[10]);
    }
}

void AES::encryptECB_MMO_Blocks(const block* plaintexts, uint64_t blockLength, block* ciphertexts) const {

    const uint64_t step = 8;
    uint64_t idx = 0;
    uint64_t length = blockLength - blockLength % step;

    //std::array<block, step> temp;
    block temp[step];

    for (; idx < length; idx += step)
    {
        temp[0] = _mm_xor_si128(plaintexts[idx + 0], mRoundKeysEnc[0]);
        temp[1] = _mm_xor_si128(plaintexts[idx + 1], mRoundKeysEnc[0]);
        temp[2] = _mm_xor_si128(plaintexts[idx + 2], mRoundKeysEnc[0]);
        temp[3] = _mm_xor_si128(plaintexts[idx + 3], mRoundKeysEnc[0]);
        temp[4] = _mm_xor_si128(plaintexts[idx + 4], mRoundKeysEnc[0]);
        temp[5] = _mm_xor_si128(plaintexts[idx + 5], mRoundKeysEnc[0]);
        temp[6] = _mm_xor_si128(plaintexts[idx + 6], mRoundKeysEnc[0]);
        temp[7] = _mm_xor_si128(plaintexts[idx + 7], mRoundKeysEnc[0]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[1]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[1]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[1]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[1]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[1]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[1]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[1]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[1]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[2]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[2]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[2]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[2]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[2]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[2]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[2]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[2]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[3]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[3]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[3]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[3]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[3]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[3]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[3]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[3]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[4]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[4]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[4]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[4]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[4]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[4]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[4]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[4]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[5]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[5]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[5]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[5]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[5]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[5]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[5]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[5]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[6]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[6]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[6]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[6]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[6]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[6]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[6]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[6]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[7]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[7]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[7]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[7]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[7]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[7]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[7]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[7]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[8]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[8]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[8]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[8]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[8]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[8]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[8]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[8]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[9]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[9]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[9]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[9]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[9]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[9]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[9]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[9]);

        temp[0] = _mm_aesenclast_si128(temp[0], mRoundKeysEnc[10]);
        temp[1] = _mm_aesenclast_si128(temp[1], mRoundKeysEnc[10]);
        temp[2] = _mm_aesenclast_si128(temp[2], mRoundKeysEnc[10]);
        temp[3] = _mm_aesenclast_si128(temp[3], mRoundKeysEnc[10]);
        temp[4] = _mm_aesenclast_si128(temp[4], mRoundKeysEnc[10]);
        temp[5] = _mm_aesenclast_si128(temp[5], mRoundKeysEnc[10]);
        temp[6] = _mm_aesenclast_si128(temp[6], mRoundKeysEnc[10]);
        temp[7] = _mm_aesenclast_si128(temp[7], mRoundKeysEnc[10]);

    	ciphertexts[idx + 0] = _mm_xor_si128(temp[0], plaintexts[0]);
    	ciphertexts[idx + 1] = _mm_xor_si128(temp[1], plaintexts[1]);
    	ciphertexts[idx + 2] = _mm_xor_si128(temp[2], plaintexts[2]);
    	ciphertexts[idx + 3] = _mm_xor_si128(temp[3], plaintexts[3]);
    	ciphertexts[idx + 4] = _mm_xor_si128(temp[4], plaintexts[4]);
    	ciphertexts[idx + 5] = _mm_xor_si128(temp[5], plaintexts[5]);
    	ciphertexts[idx + 6] = _mm_xor_si128(temp[6], plaintexts[6]);
    	ciphertexts[idx + 7] = _mm_xor_si128(temp[7], plaintexts[7]);
    }

    for (; idx < blockLength; ++idx)
    {
        ciphertexts[idx] = _mm_xor_si128(plaintexts[idx], mRoundKeysEnc[0]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[1]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[2]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[3]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[4]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[5]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[6]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[7]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[8]);
        ciphertexts[idx] = _mm_aesenc_si128(ciphertexts[idx], mRoundKeysEnc[9]);
        ciphertexts[idx] = _mm_aesenclast_si128(ciphertexts[idx], mRoundKeysEnc[10]);
        ciphertexts[idx] = _mm_xor_si128(plaintexts[idx], ciphertexts[idx]);
    }
}

void AES::encryptCTR(uint64_t baseIdx, uint64_t blockLength, block * ciphertext) const {

    const uint64_t step = 8;
    uint64_t idx = 0;
    uint64_t length = blockLength - blockLength % step;

    //std::array<block, step> temp;
    block temp[step];

    for (; idx < length; idx += step, baseIdx += step)
    {
        temp[0] = _mm_xor_si128(_mm_set1_epi64x(baseIdx + 0), mRoundKeysEnc[0]);
        temp[1] = _mm_xor_si128(_mm_set1_epi64x(baseIdx + 1), mRoundKeysEnc[0]);
        temp[2] = _mm_xor_si128(_mm_set1_epi64x(baseIdx + 2), mRoundKeysEnc[0]);
        temp[3] = _mm_xor_si128(_mm_set1_epi64x(baseIdx + 3), mRoundKeysEnc[0]);
        temp[4] = _mm_xor_si128(_mm_set1_epi64x(baseIdx + 4), mRoundKeysEnc[0]);
        temp[5] = _mm_xor_si128(_mm_set1_epi64x(baseIdx + 5), mRoundKeysEnc[0]);
        temp[6] = _mm_xor_si128(_mm_set1_epi64x(baseIdx + 6), mRoundKeysEnc[0]);
        temp[7] = _mm_xor_si128(_mm_set1_epi64x(baseIdx + 7), mRoundKeysEnc[0]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[1]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[1]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[1]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[1]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[1]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[1]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[1]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[1]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[2]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[2]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[2]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[2]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[2]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[2]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[2]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[2]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[3]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[3]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[3]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[3]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[3]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[3]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[3]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[3]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[4]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[4]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[4]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[4]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[4]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[4]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[4]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[4]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[5]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[5]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[5]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[5]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[5]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[5]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[5]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[5]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[6]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[6]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[6]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[6]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[6]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[6]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[6]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[6]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[7]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[7]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[7]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[7]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[7]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[7]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[7]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[7]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[8]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[8]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[8]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[8]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[8]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[8]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[8]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[8]);

        temp[0] = _mm_aesenc_si128(temp[0], mRoundKeysEnc[9]);
        temp[1] = _mm_aesenc_si128(temp[1], mRoundKeysEnc[9]);
        temp[2] = _mm_aesenc_si128(temp[2], mRoundKeysEnc[9]);
        temp[3] = _mm_aesenc_si128(temp[3], mRoundKeysEnc[9]);
        temp[4] = _mm_aesenc_si128(temp[4], mRoundKeysEnc[9]);
        temp[5] = _mm_aesenc_si128(temp[5], mRoundKeysEnc[9]);
        temp[6] = _mm_aesenc_si128(temp[6], mRoundKeysEnc[9]);
        temp[7] = _mm_aesenc_si128(temp[7], mRoundKeysEnc[9]);

        ciphertext[idx + 0] = _mm_aesenclast_si128(temp[0], mRoundKeysEnc[10]);
        ciphertext[idx + 1] = _mm_aesenclast_si128(temp[1], mRoundKeysEnc[10]);
        ciphertext[idx + 2] = _mm_aesenclast_si128(temp[2], mRoundKeysEnc[10]);
        ciphertext[idx + 3] = _mm_aesenclast_si128(temp[3], mRoundKeysEnc[10]);
        ciphertext[idx + 4] = _mm_aesenclast_si128(temp[4], mRoundKeysEnc[10]);
        ciphertext[idx + 5] = _mm_aesenclast_si128(temp[5], mRoundKeysEnc[10]);
        ciphertext[idx + 6] = _mm_aesenclast_si128(temp[6], mRoundKeysEnc[10]);
        ciphertext[idx + 7] = _mm_aesenclast_si128(temp[7], mRoundKeysEnc[10]);
    }

    for (; idx < blockLength; ++idx, ++baseIdx)
    {
        ciphertext[idx] = _mm_xor_si128(_mm_set1_epi64x(baseIdx), mRoundKeysEnc[0]);
        ciphertext[idx] = _mm_aesenc_si128(ciphertext[idx], mRoundKeysEnc[1]);
        ciphertext[idx] = _mm_aesenc_si128(ciphertext[idx], mRoundKeysEnc[2]);
        ciphertext[idx] = _mm_aesenc_si128(ciphertext[idx], mRoundKeysEnc[3]);
        ciphertext[idx] = _mm_aesenc_si128(ciphertext[idx], mRoundKeysEnc[4]);
        ciphertext[idx] = _mm_aesenc_si128(ciphertext[idx], mRoundKeysEnc[5]);
        ciphertext[idx] = _mm_aesenc_si128(ciphertext[idx], mRoundKeysEnc[6]);
        ciphertext[idx] = _mm_aesenc_si128(ciphertext[idx], mRoundKeysEnc[7]);
        ciphertext[idx] = _mm_aesenc_si128(ciphertext[idx], mRoundKeysEnc[8]);
        ciphertext[idx] = _mm_aesenc_si128(ciphertext[idx], mRoundKeysEnc[9]);
        ciphertext[idx] = _mm_aesenclast_si128(ciphertext[idx], mRoundKeysEnc[10]);
    }
}
