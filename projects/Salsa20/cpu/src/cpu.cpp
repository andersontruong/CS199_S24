#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <chrono>

#define ROUNDS 20
#define ROTL32(v, c) (((v) << (c) | ((v) >> (32 - (c)))))
#define BUFFER_SIZE 16

// Nothing-up-my-sleeve number (little-endian)
// [expa](0x65, 0x78, 0x70, 0x61)
// [nd 3](0x6e, 0x64, 0x20, 0x33)
// [2-by](0x32, 0x2d, 0x62, 0x79)
// [te k](0x74, 0x65, 0x20, 0x6b)
const uint32_t SALSA20_INITIAL_WORDS[4] = { 0x61707865, 0x3320646e, 0x79622d32, 0x6b206574 };

class Salsa20 {
    public:
        uint32_t _state[BUFFER_SIZE];
        uint32_t _hash[BUFFER_SIZE];
        uint8_t _block_index;
        Salsa20(uint32_t* key, uint32_t* nonce): _block_index{0} {
            // Initialize constant words (on matrix diagonal)
            _state[0]  = SALSA20_INITIAL_WORDS[0]; // expa [0]
            _state[5]  = SALSA20_INITIAL_WORDS[1]; // nd 3 [5]
            _state[10] = SALSA20_INITIAL_WORDS[2]; // 2-by [10]
            _state[15] = SALSA20_INITIAL_WORDS[3]; // te k [15]

            // Set key at word positions: [1-4], [11-14]
            for (int i = 0; i < 4; ++i) {
                _state[i + 1]  = key[i];
                _state[i + 11] = key[i + 4];
            }

            // Set 2-word (8-byte/64-bit) nonce at positions [6-7]
            _state[6] = nonce[0];
            _state[7] = nonce[1];

            // Initialize block index at positions: [8-9]
            _state[8] = 0x00000000;
            _state[9] = 0x00000000;

            _gen_hash();
        }

        void encrypt_block(uint32_t* in_block, uint32_t* out_block) {
            for (int i = 0; i < BUFFER_SIZE; ++i) {
                if (_block_index == BUFFER_SIZE) {
                    _block_index = 0;
                    _gen_hash();
                }

                out_block[i] = in_block[i] ^ _hash[_block_index++];
            }
        }

        void reset_block_counter() {
            _state[8] = 0x00000000;
            _state[9] = 0x00000000;

            _gen_hash();
        }

        void reset_hash_position() {
            _block_index = 0;
        }

        void reset() {
            reset_block_counter();
            reset_hash_position();
        }

    private:

        // Quarter-Round
        void _QR(uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
            *b ^= ROTL32(*a + *d, 7);
            *c ^= ROTL32(*b + *a, 9);
            *d ^= ROTL32(*c + *b, 13);
            *a ^= ROTL32(*d + *c, 18);
        }
        
        // Double Round: Column-Round followed by Row-Round
        // Modifies `_hash` state in-place
        void _DR() {
            // Column-Round (odd)
            _QR(&_hash[0],  &_hash[4],  &_hash[8],  &_hash[12]);
            _QR(&_hash[5],  &_hash[9],  &_hash[13], &_hash[1]);
            _QR(&_hash[10], &_hash[14], &_hash[2],  &_hash[6]);
            _QR(&_hash[15], &_hash[3],  &_hash[7],  &_hash[11]);

            // Row-Round (even)
            _QR(&_hash[0],  &_hash[1],  &_hash[2],  &_hash[3]);
            _QR(&_hash[5],  &_hash[6],  &_hash[7],  &_hash[4]);
            _QR(&_hash[10], &_hash[11], &_hash[8],  &_hash[9]);
            _QR(&_hash[15], &_hash[12], &_hash[13], &_hash[14]);
        }

        // Generates 64B hash from current state
        // Stores in private variable `_hash`
        void _gen_hash() {
            int i = 0;
            memcpy(_hash, _state, BUFFER_SIZE*4);

            for (i = 0; i < ROUNDS; i += 2) {
                _DR();
            }

            for (i = 0; i < 16; ++i)
                _hash[i] += _state[i];

            // Increment block counter
            _state[8] += 1;
            if (!_state[8]) {
                _state[9] += 1;
            }
        }
};

void read_keyfile(char* filename, uint32_t* key, uint32_t* nonce) {
    std::ifstream keyfileStream(filename);
    char* keyReadBuffer = (char*)key;
    char* nonceReadBuffer = (char*) nonce;
    
    // Bytes 0-31 is the key
    keyfileStream.read(keyReadBuffer, 32);

    // Bytes 33-41 is the nonce (skipping newline char)
    keyfileStream.seekg(33);
    keyfileStream.read(nonceReadBuffer, 8);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <KEYFILE>" << std::endl;
        std::cout << "\tKeyfile has the first line as the 32-byte key (256 bits)" << std::endl;
        std::cout << "\tand the second line as the 8-byte none (64 bits)." << std::endl;
        return EXIT_FAILURE;
    }

    uint32_t key[8];
    uint32_t nonce[2];
    read_keyfile(argv[1], key, nonce);

    Salsa20 cipher(key, nonce);

    // for (int i = 0; i < 16; ++i) {
    //     std::cout << "[" << i << "]: 0x" << std::hex << cipher._hash[i] << std::endl;
    // }

    /*
        Test encryption
    */
    uint32_t input_buffer[BUFFER_SIZE*2];
    uint32_t output_buffer[BUFFER_SIZE*2];
    unsigned int bytes_read = read(0, input_buffer, BUFFER_SIZE*4*2);
    unsigned int block_count = bytes_read / 64 + (bytes_read % 64 != 0);

    const auto start = std::chrono::steady_clock::now();
    for (unsigned int i = 0; i < block_count; ++i)
        cipher.encrypt_block(input_buffer + (BUFFER_SIZE*i), output_buffer + (BUFFER_SIZE*i));
    const auto end = std::chrono::steady_clock::now();

    for (int i = 0; i < BUFFER_SIZE*2; ++i) {
        std::cout << "[" << i << "]: 0x" << std::hex << output_buffer[i] << std::endl;
    }

    const std::chrono::steady_clock::duration time_span = end - start;

    double nseconds = double(time_span.count()) * std::chrono::steady_clock::period::num / std::chrono::steady_clock::period::den;

    std::cout << nseconds << std::endl;

    // const int buffer_size = 64;
    // uint8_t input_buffer[64];
    // uint8_t out_byte;
    // ssize_t bytesRead;
    // while ((bytesRead = read(0, input_buffer, buffer_size)) > 0) {
    //     for (int i = 0; i < bytesRead; ++i) {
    //         cipher.encrypt_byte(input_buffer + i, &out_byte);
    //         std::cout << std::hex << (unsigned int) out_byte << std::endl;
    //     }
    // }
}