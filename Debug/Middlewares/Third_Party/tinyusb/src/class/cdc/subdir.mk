################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_device.c \
../Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_host.c \
../Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_rndis_host.c 

OBJS += \
./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_device.o \
./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_host.o \
./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_rndis_host.o 

C_DEPS += \
./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_device.d \
./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_host.d \
./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_rndis_host.d 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/Third_Party/tinyusb/src/class/cdc/%.o Middlewares/Third_Party/tinyusb/src/class/cdc/%.su Middlewares/Third_Party/tinyusb/src/class/cdc/%.cyclo: ../Middlewares/Third_Party/tinyusb/src/class/cdc/%.c Middlewares/Third_Party/tinyusb/src/class/cdc/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F107xC -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I../Middlewares/Third_Party/tinyusb/src -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Middlewares-2f-Third_Party-2f-tinyusb-2f-src-2f-class-2f-cdc

clean-Middlewares-2f-Third_Party-2f-tinyusb-2f-src-2f-class-2f-cdc:
	-$(RM) ./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_device.cyclo ./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_device.d ./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_device.o ./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_device.su ./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_host.cyclo ./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_host.d ./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_host.o ./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_host.su ./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_rndis_host.cyclo ./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_rndis_host.d ./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_rndis_host.o ./Middlewares/Third_Party/tinyusb/src/class/cdc/cdc_rndis_host.su

.PHONY: clean-Middlewares-2f-Third_Party-2f-tinyusb-2f-src-2f-class-2f-cdc

