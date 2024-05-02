#include "xcl2.hpp"
#include "utils.h"
#include <unistd.h>
#include <algorithm>
#include <memory>

#define BUFFER_SIZE 16
#define PAGE_SIZE 4096

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <XCLBIN File> <KEYFILE>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string binaryFile = argv[1];

    /* Extract key and nonce from KEYFILE */
    uint32_t *key;
    uint32_t *nonce;
    posix_memalign((void**)&key, PAGE_SIZE, 8*sizeof(uint32_t));
    posix_memalign((void**)&nonce, PAGE_SIZE, 2*sizeof(uint32_t));
    read_keyfile(argv[2], key, nonce);

    cl_int err;
    cl::Context context;
    cl::Kernel krnl_salsa20;
    cl::CommandQueue q;
    cl::Program program;

    uint32_t *plaintext_buffer, *ciphertext_buffer = NULL;
    posix_memalign((void**)&plaintext_buffer, PAGE_SIZE, 2*BUFFER_SIZE*sizeof(uint32_t));
    posix_memalign((void**)&ciphertext_buffer, PAGE_SIZE, 2*BUFFER_SIZE*sizeof(uint32_t));

    setupOpenCL(
        binaryFile,
        err,
        context,
        krnl_salsa20,
        q,
        program
    );

    /*
        Read plaintext from STDIN
    */
    unsigned int bytes_read = read(0, plaintext_buffer, BUFFER_SIZE*4*2);
    std::cout << "read: " << bytes_read << std::endl;

    unsigned int block_count = bytes_read / 64 + (bytes_read % 64 != 0);

    /*
        Allocate Buffer in Global Memory
    */
    printf("Using host pointer with MigrateMemObjects\n");
    OCL_CHECK(err, 
        cl::Buffer buffer_key(
            context,
            CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
            32,
            key,
            &err)
    );
    OCL_CHECK(err, 
        cl::Buffer buffer_nonce(
            context,
            CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
            8,
            nonce,
            &err)
    );
    OCL_CHECK(err, 
        cl::Buffer buffer_in(
            context,
            CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
            BUFFER_SIZE*4*2,
            plaintext_buffer,
            &err)
    );
    OCL_CHECK(err, 
        cl::Buffer buffer_out(
            context,
            CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
            BUFFER_SIZE*4*2,
            ciphertext_buffer,
            &err)
    );

    OCL_CHECK(err, err = krnl_salsa20.setArg(0, buffer_key));
    OCL_CHECK(err, err = krnl_salsa20.setArg(1, buffer_nonce));
    OCL_CHECK(err, err = krnl_salsa20.setArg(2, buffer_in));
    OCL_CHECK(err, err = krnl_salsa20.setArg(3, buffer_out));
    OCL_CHECK(err, err = krnl_salsa20.setArg(4, block_count));

    // clEnqueueMigrateMemObjects does immediate migration of data without
    // considering
    // the fact that data is actually needed or not by kernel operation
    // Copy input data to device global memory
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_key, buffer_nonce, buffer_in}, 0 /* 0 means from host*/));

    // Launch the Kernel
    // For HLS kernels global and local size is always (1,1,1). So, it is
    // recommended
    // to always use enqueueTask() for invoking HLS kernel
    OCL_CHECK(err, err = q.enqueueTask(krnl_salsa20));

    // Copy Result from Device Global Memory to Host Local Memory
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_out}, CL_MIGRATE_MEM_OBJECT_HOST));
    
    OCL_CHECK(err, err = q.finish());

    for (int i = 0; i < BUFFER_SIZE*2; ++i) {
        std::cout << "[" << i << "]: 0x" << std::hex << ciphertext_buffer[i] << std::endl;
    }

    return EXIT_SUCCESS;
}