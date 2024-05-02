// extern "C" {
// void salsa20(unsigned int* buffer0, unsigned int* buffer1, unsigned int size) {
//     // Intentional empty kernel as this example doesn't require actual
//     // kernel to work.

//     // Auto-pipeline is going to apply pipeline to this loop
//     dummy:
//         for (unsigned int i = 0; i < size; i++) {
//             buffer0[i] = buffer1[i] + 1;
//         }
//     }
// }
#include <stdint.h>
#include <cstring>

#define ROUNDS 20
#define ROTL32(v, c) (((v) << (c) | ((v) >> (32 - (c)))))
#define BUFFER_SIZE 16
#define BYTES_PER_WORD 4

// Nothing-up-my-sleeve number (little-endian)
// [expa](0x65, 0x78, 0x70, 0x61)
// [nd 3](0x6e, 0x64, 0x20, 0x33)
// [2-by](0x32, 0x2d, 0x62, 0x79)
// [te k](0x74, 0x65, 0x20, 0x6b)
const uint32_t SALSA20_INITIAL_WORDS[4] = { 0x61707865, 0x3320646e, 0x79622d32, 0x6b206574 };

class Salsa20 {
    public:
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

        void encrypt_block(uint32_t* plaintext_buffer, uint32_t* ciphertext_buffer) {
            for (uint8_t i = 0; i < BUFFER_SIZE; ++i) {
                if (_block_index == BUFFER_SIZE) {
                    _block_index = 0;
                    _gen_hash();
                }

                ciphertext_buffer[i] = plaintext_buffer[i] ^ _hash[_block_index++];
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
        uint32_t _state[BUFFER_SIZE];
        uint32_t _hash[BUFFER_SIZE];
        uint8_t _block_index;

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
            memcpy(_hash, _state, 64);

            for (i = 0; i < ROUNDS; i += 2) {
                _DR();
            }

            for (i = 0; i < BUFFER_SIZE; ++i)
                _hash[i] += _state[i];

            // Increment block counter
            _state[8] += 1;
            if (!_state[8]) {
                _state[9] += 1;
            }
        }
};

extern "C" {
void salsa20(uint32_t* key, uint32_t* nonce, uint32_t* plaintext_buffer, uint32_t* ciphertext_buffer, uint32_t block_count) {
    Salsa20 cipher(key, nonce);

    for (uint32_t i = 0; i < block_count; ++i) {
        cipher.encrypt_block(plaintext_buffer + (BUFFER_SIZE*i), ciphertext_buffer + (BUFFER_SIZE*i));
    }
    }
}