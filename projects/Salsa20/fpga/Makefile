TARGET := hw
PLATFORM := xilinx_u250_gen3x16_xdma_4_1_202210_1

TEMP_DIR := ./_x.$(TARGET).$(PLATFORM)
BUILD_DIR := ./build_dir.$(TARGET).$(PLATFORM)

LINK_OUTPUT := $(BUILD_DIR)/$(HW_FILE_NAME).link.xclbin
PACKAGE_OUT = ./package.$(TARGET)

CXXFLAGS += -I$(XILINX_XRT)/include -I$(XILINX_VIVADO)/include -Wall -O0 -g -std=c++1y
LDFLAGS += -L$(XILINX_XRT)/lib -pthread -lOpenCL

PROJECT_NAME = salsa20
HW_FILE_NAME = salsa20
KERNEL_NAME = salsa20

############################## Setting up Host Variables ##############################
#Include Required Host Source Files
CXXFLAGS += -I ../../../xcl2
HOST_SRCS += ../../../xcl2/xcl2.cpp ./src/host.cpp ./src/utils.cpp
# Host compiler global settings
CXXFLAGS += -fmessage-length=0
LDFLAGS += -lrt -lstdc++ 

RMDIR = rm -rf

############################## Setting up Kernel Variables ##############################
# Kernel compiler global settings
VPP_FLAGS += --save-temps 


EXECUTABLE = ./$(PROJECT_NAME)
EMCONFIG_DIR = $(TEMP_DIR)

#Checks for XILINX_VITIS
check-vitis:
ifndef XILINX_VITIS
	$(module load xilinx/vivado/2022.2)
endif

#Checks for XILINX_XRT
check-xrt:
ifndef XILINX_XRT
	$(module load xilinx/vivado/2022.2)
endif

############################## Setting Targets ##############################
.PHONY: all clean cleanall docs emconfig
all: check-vitis host $(BUILD_DIR)/$(HW_FILE_NAME).xclbin emconfig

.PHONY: host
host: $(EXECUTABLE)

.PHONY: build
build: check-vitis $(BUILD_DIR)/$(HW_FILE_NAME).xclbin

############################## Setting Rules for Binary Containers (Building Kernels) ##############################
$(TEMP_DIR)/$(HW_FILE_NAME).xo: src/$(HW_FILE_NAME).cpp
	mkdir -p $(TEMP_DIR)
	v++ -c $(VPP_FLAGS) -t $(TARGET) --platform $(PLATFORM) -k $(KERNEL_NAME) --temp_dir $(TEMP_DIR) -I'$(<D)' -o'$@' '$<'

$(BUILD_DIR)/$(HW_FILE_NAME).xclbin: $(TEMP_DIR)/$(HW_FILE_NAME).xo
	mkdir -p $(BUILD_DIR)
	v++ -l $(VPP_FLAGS) -t $(TARGET) --platform $(PLATFORM) --temp_dir $(TEMP_DIR) -o'$(LINK_OUTPUT)' $(+)
	v++ -p $(LINK_OUTPUT) $(VPP_FLAGS) -t $(TARGET) --platform $(PLATFORM) --package.out_dir $(PACKAGE_OUT) -o $(BUILD_DIR)/$(HW_FILE_NAME).xclbin

############################## Setting Rules for Host (Building Host Executable) ##############################

$(EXECUTABLE): $(HOST_SRCS) | check-xrt
	g++ -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

clean:
	-$(RMDIR) $(EXECUTABLE)

cleanhw:
	-$(RMDIR) *v++* *.log
	-$(RMDIR) _x* package.* .Xil .ipcache