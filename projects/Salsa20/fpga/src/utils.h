#ifndef UTILS_H
#define UTILS_H
#include <stdint.h>
#include <xcl2.hpp>

void read_keyfile(char* filename, uint32_t* key, uint32_t* nonce);

void setupOpenCL(
    const std::string& binaryFile,
    cl_int& err,
    cl::Context& context,
    cl::Kernel& krnl,
    cl::CommandQueue& q,
    cl::Program& program
);

#endif