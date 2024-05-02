#include "utils.h"
#include <fstream>
#include <iostream>

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

void setupOpenCL(
    const std::string& binaryFile,
    cl_int& err,
    cl::Context& context,
    cl::Kernel& krnl,
    cl::CommandQueue& q,
    cl::Program& program
) {
    // OPENCL HOST CODE AREA START
    // get_xil_devices() is a utility API which will find the xilinx
    // platforms and will return list of devices connected to Xilinx platform
    auto devices = xcl::get_xil_devices();
    // read_binary_file() is a utility API which will load the binaryFile
    // and will return the pointer to file buffer.
    auto fileBuf = xcl::read_binary_file(binaryFile);
    cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};
    bool valid_device = false;

    for (unsigned int i = 0; i < devices.size(); i++) {
        // for (auto device : devices) {
        auto device = devices[i];
        if (device.getInfo<CL_DEVICE_NAME>().compare("xilinx_u250_gen3x16_xdma_shell_4_1") == 0) {
            // Creating Context and Command Queue for selected Device
            OCL_CHECK(err, context = cl::Context(device, nullptr, nullptr, nullptr, &err));
            OCL_CHECK(err, q = cl::CommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err));
            std::cout << "Trying to program device[" << i << "]: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;
            OCL_CHECK(err, program = cl::Program(context, {device}, bins, nullptr, &err));
            if (err != CL_SUCCESS) {
                std::cout << "Failed to program device[" << i << "] with xclbin file!\n";
            } else {
                std::cout << "Device[" << i << "]: program successful!\n";
                OCL_CHECK(err, krnl = cl::Kernel(program, "salsa20", &err));
                valid_device = true;
                break; // we break because we found a valid device
            }
        }
        else continue;
    }
    if (!valid_device) {
        std::cout << "Failed to program any device found, exit!\n";
        exit(EXIT_FAILURE);
    }
}