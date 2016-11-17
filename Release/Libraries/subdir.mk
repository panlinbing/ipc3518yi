################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Libraries/aes.c 

CPP_SRCS += \
../Libraries/JsonCommand.cpp \
../Libraries/Locker.cpp \
../Libraries/Sclient.cpp \
../Libraries/jsoncpp.cpp 

OBJS += \
./Libraries/JsonCommand.o \
./Libraries/Locker.o \
./Libraries/Sclient.o \
./Libraries/aes.o \
./Libraries/jsoncpp.o 

C_DEPS += \
./Libraries/aes.d 

CPP_DEPS += \
./Libraries/JsonCommand.d \
./Libraries/Locker.d \
./Libraries/Sclient.d \
./Libraries/jsoncpp.d 


# Each subdirectory must supply rules for building sources it contributes
Libraries/%.o: ../Libraries/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	arm-hisiv100nptl-linux-g++ -Dhi3518 -DHICHIP=0x35180100 -DHI_RELEASE -DHI_XXXX -DISP_V1 -DSENSOR_TYPE=OMNI_OV9712_DC_720P_30FPS -I/home/hoang/camera_ip/mpp/extdrv/tw2865 -I"/home/hoang/workspaceCameraIP/ipc3518yi/include" -I/home/hoang/QRcode/zbar/zbar-0.10/build/include -I/home/hoang/camera_ip/mpp/include -I/home/hoang/http-post/curl-cam/curl/build/include -I"/home/hoang/workspaceCameraIP/ipc3518yi/common" -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

Libraries/%.o: ../Libraries/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-hisiv100nptl-linux-gcc -DSENSOR_TYPE=OMNI_OV9712_DC_720P_30FPS -DISP_V1 -DHI_XXXX -DHI_RELEASE -DHICHIP=0x35180100 -Dhi3518 -I/home/hoang/camera_ip/mpp/include -I"/home/hoang/workspaceCameraIP/ipc3518yi/include" -I"/home/hoang/workspaceCameraIP/ipc3518yi/common" -I/home/hoang/camera_ip/mpp/extdrv/tw2865 -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


