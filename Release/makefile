################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include socket/subdir.mk
-include rtmp/subdir.mk
-include libjpeg/subdir.mk
-include highgui/subdir.mk
-include core/subdir.mk
-include common/subdir.mk
-include Libraries/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: ipc3518yi

# Tool invocations
ipc3518yi: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross G++ Linker'
	arm-hisiv100nptl-linux-g++ -L/home/hoang/QRcode/zbar/zbar-0.10/build/lib -L/home/hoang/http-post/curl-cam/curl/build/lib -Wl,-gc-sections -o  "ipc3518yi" $(OBJS) $(USER_OBJS) $(LIBS) /home/hoang/camera_ip/mpp/lib/libmpi.a /home/hoang/camera_ip/mpp/lib/libVoiceEngine.a /home/hoang/camera_ip/mpp/lib/libaec.a /home/hoang/camera_ip/mpp/lib/libresampler.a /home/hoang/camera_ip/mpp/lib/libanr.a /home/hoang/camera_ip/mpp/lib/libisp.a /home/hoang/camera_ip/mpp/lib/libsns_ov9712.a
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(CC_DEPS)$(C++_DEPS)$(EXECUTABLES)$(OBJS)$(C_UPPER_DEPS)$(CXX_DEPS)$(C_DEPS)$(CPP_DEPS) ipc3518yi
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
