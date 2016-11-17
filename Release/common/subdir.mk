################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../common/loadbmp.c \
../common/sample_comm_audio.c \
../common/sample_comm_isp.c \
../common/sample_comm_sys.c \
../common/sample_comm_vda.c \
../common/sample_comm_venc.c \
../common/sample_comm_vi.c \
../common/sample_comm_vo.c \
../common/sample_comm_vpss.c 

OBJS += \
./common/loadbmp.o \
./common/sample_comm_audio.o \
./common/sample_comm_isp.o \
./common/sample_comm_sys.o \
./common/sample_comm_vda.o \
./common/sample_comm_venc.o \
./common/sample_comm_vi.o \
./common/sample_comm_vo.o \
./common/sample_comm_vpss.o 

C_DEPS += \
./common/loadbmp.d \
./common/sample_comm_audio.d \
./common/sample_comm_isp.d \
./common/sample_comm_sys.d \
./common/sample_comm_vda.d \
./common/sample_comm_venc.d \
./common/sample_comm_vi.d \
./common/sample_comm_vo.d \
./common/sample_comm_vpss.d 


# Each subdirectory must supply rules for building sources it contributes
common/%.o: ../common/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-hisiv100nptl-linux-gcc -DSENSOR_TYPE=OMNI_OV9712_DC_720P_30FPS -DISP_V1 -DHI_XXXX -DHI_RELEASE -DHICHIP=0x35180100 -Dhi3518 -I/home/hoang/camera_ip/mpp/include -I"/home/hoang/workspaceCameraIP/ipc3518yi/include" -I"/home/hoang/workspaceCameraIP/ipc3518yi/common" -I/home/hoang/camera_ip/mpp/extdrv/tw2865 -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


