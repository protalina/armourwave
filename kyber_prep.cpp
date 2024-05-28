#include "kyber512_kem.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include "aes.h"
#include <bitset>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <span>
#include <vector>
#include <cmath>
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <thread>
#pragma comment(lib, "winmm.lib")

const int BUFFER_SIZE = 100000;
const int SAMPLE_RATE = 20000;

// Function to initialize audio playback
bool InitAudioPlayback(WAVEFORMATEX& wfx, HWAVEOUT& hWaveOut) {
    // Set up the WAVEFORMATEX structure
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2; // Stereo
    wfx.nSamplesPerSec = SAMPLE_RATE;
    wfx.wBitsPerSample = 16; // 16-bit samples
    wfx.nBlockAlign = (wfx.wBitsPerSample / 8) * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    // Open the audio device
    MMRESULT result = waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to open audio device" << std::endl;
        return false;
    }

    return true;
}

// Function to play audio from buffer
void PlayAudio(const short* buffer, const WAVEFORMATEX& wfx, HWAVEOUT& hWaveOut) {
    // Prepare WAVEHDR structure
    WAVEHDR waveHeader;
    ZeroMemory(&waveHeader, sizeof(WAVEHDR));
    waveHeader.lpData = reinterpret_cast<LPSTR>(const_cast<short*>(buffer));
    waveHeader.dwBufferLength = BUFFER_SIZE * sizeof(short);

    // Prepare the audio data for playback
    MMRESULT result = waveOutPrepareHeader(hWaveOut, &waveHeader, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to prepare audio header" << std::endl;
        return;
    }

    // Play the audio
    result = waveOutWrite(hWaveOut, &waveHeader, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to play audio" << std::endl;
        return;
    }

    // Wait for playback to finish
    while (waveOutUnprepareHeader(hWaveOut, &waveHeader, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
        //Sleep(10);
    }
}

int16_t binaryToDec(const std::bitset<16>& bits) {
    // Check if the most significant bit (sign bit) is set
    if (bits[15]) {
        // If MSB is set, this is a negative number
        // Convert to 2's complement
        std::bitset<16> complement = ~bits;
        // Convert to decimal and negate
        return -static_cast<int16_t>(complement.to_ulong() + 1);
    }
    else if (bits.to_ulong() == 0xFFFF) {
        // Special case: if all bits are set to 1, return -1
        return -1;
    }
    else {
        // Positive number, simply convert to decimal
        return static_cast<int16_t>(bits.to_ulong());
    }
}

std::vector<std::string> tokenizeString(const std::string& input) {
    std::vector<std::string> tokens;
    const int tokenLength = 32; // Token length is constant
    for (size_t i = 0; i < input.length(); i += tokenLength) {
        tokens.push_back(input.substr(i, tokenLength));
    }
    return tokens;
}

std::vector<std::string> tokenize(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0, end = 0;
    while ((end = str.find(delimiter, start)) != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
    }
    tokens.push_back(str.substr(start));
    return tokens;
}

// Function to play audio from buffer in a separate thread
void PlayAudioThread(const short* buffer, const WAVEFORMATEX& wfx, HWAVEOUT& hWaveOut) {
    PlayAudio(buffer, wfx, hWaveOut);
}

int main() {
    int fileCounter = 1;
    while (true) {
        std::cout << "THIS IS THE FILE COUNTER: " << fileCounter << std::endl << std::endl;
        std::string fileName = "data" + std::to_string(fileCounter) + ".txt";
        std::ifstream file(fileName);

        if (!file) {
            std::cout << "File not found: " << fileName << std::endl;
            fileCounter++;
            continue;
        }

        std::string line;
        std::string content;
        std::vector<std::string> tokens;

        // Read the entire file content into a string
        while (std::getline(file, line)) {
            content += line + "\n";
        }

        // Tokenize content between START and MIDDLE
        size_t start_pos = content.find("START");
        if (start_pos != std::string::npos) {
            size_t middle_pos = content.find("MIDDLE", start_pos);
            if (middle_pos != std::string::npos) {
                std::string token1 = content.substr(start_pos + 5, middle_pos - start_pos - 5);
                tokens.push_back(token1);
            }
        }

        // Tokenize content between MIDDLE and END
        size_t middle_pos = content.find("MIDDLE");
        if (middle_pos != std::string::npos) {
            size_t end_pos = content.find("END", middle_pos);
            if (end_pos != std::string::npos) {
                std::string token2 = content.substr(middle_pos + 6, end_pos - middle_pos - 6);
                tokens.push_back(token2);
            }
        }

        // Tokenize content between END and OVER
        size_t end_pos = content.find("END");
        if (end_pos != std::string::npos) {
            size_t over_pos = content.find("OVER", end_pos);
            if (over_pos != std::string::npos) {
                std::string token3 = content.substr(end_pos + 3, over_pos - end_pos - 3);
                tokens.push_back(token3);
            }
        }

        constexpr size_t KEY_LEN = 32;
        std::vector<uint8_t> shrd_key1(KEY_LEN, 0);
        auto _shrd_key1 = std::span<uint8_t, KEY_LEN>(shrd_key1);

        std::vector<uint8_t> skey(kyber512_kem::SKEY_LEN, 0);
        auto _skey = std::span<uint8_t, kyber512_kem::SKEY_LEN>(skey);

        std::vector<uint8_t> cipher(kyber512_kem::CIPHER_LEN, 0);
        auto _cipher = std::span<uint8_t, kyber512_kem::CIPHER_LEN>(cipher);

        // decapsulate cipher text and obtain KDF
        {
            using namespace kyber_utils;
            std::string test = tokens[0];
            std::string test2 = tokens[1];
            auto temp_skey = kyber_utils::from_hex<kyber512_kem::SKEY_LEN>(test);
            auto temp_cipher = kyber_utils::from_hex<kyber512_kem::CIPHER_LEN>(test2);

            // Copy data from temp_skey and temp_cipher to _skey and _cipher
            std::copy(temp_skey.begin(), temp_skey.end(), _skey.begin());
            std::copy(temp_cipher.begin(), temp_cipher.end(), _cipher.begin());

        }
        auto rkdf = kyber512_kem::decapsulate(_skey, _cipher);
        rkdf.squeeze(_shrd_key1);


        // OBTAIN ALL 100,000 SAMPLES AS 32 CHARACTERS
        std::string keyToCollect;
        {
            using namespace kyber_utils;
            keyToCollect = to_hex(_shrd_key1);
        }
        std::vector<std::string> obtainedEncryptedStrings = tokenizeString(tokens[2]);
        AES aes(AESKeyLength::AES_256);
        const unsigned int keyLen = 32;
        unsigned char key[keyLen];
        for (int j = 0; j < keyLen; j++) {
            key[j] = keyToCollect[j];
        }

        std::vector<short> bufferO;

        for (int k = 0; k < obtainedEncryptedStrings.size(); k++) {

            std::string resultToUse = obtainedEncryptedStrings[k];
            const int size = 16;
            unsigned char byteArr[16]; // Array to store bytes

            // Iterate through the string and convert pairs of characters to bytes
            for (int i = 0; i < 16; ++i) {
                std::string byteString = resultToUse.substr(i * 2, 2); // Extract pair of characters
                byteArr[i] = static_cast<unsigned char>(std::stoi(byteString, nullptr, 16)); // Convert pair to byte
            }
     
            unsigned char* decryptedBytes = aes.DecryptECB(byteArr, size, key);

            std::string decryptedString(reinterpret_cast<char*>(decryptedBytes), 16);
            std::bitset<16> decryptedBitset;
            for (size_t i = 0; i < 16; ++i) {
                if (i < decryptedString.size()) {
                    if (decryptedString[i] == '1') {
                        decryptedBitset.set(15 - i, true);
                    }
                    else {
                        decryptedBitset.set(15 - i, false);
                    }
                }
                else {
                    decryptedBitset.set(15 - i, false);
                }
            }
            std::cout << std::dec;
            int decimalValue = binaryToDec(decryptedBitset);
            delete[] decryptedBytes;
            bufferO.push_back(decimalValue);
        }

        // Initialize audio playback
        WAVEFORMATEX wfx;
        HWAVEOUT hWaveOut;
        if (!InitAudioPlayback(wfx, hWaveOut)) {
            return 1;
        }

        short buffer[BUFFER_SIZE];
        std::copy(bufferO.begin(), bufferO.begin() + BUFFER_SIZE, buffer);

        bufferO.clear();
        tokens.clear();

        // Play the audio in a separate thread
        std::thread audioThread(PlayAudioThread, buffer, std::ref(wfx), std::ref(hWaveOut));
        audioThread.detach(); // Detach the thread to allow it to run independently

        file.close();
        fileCounter++;
    }

    return 0;
}
