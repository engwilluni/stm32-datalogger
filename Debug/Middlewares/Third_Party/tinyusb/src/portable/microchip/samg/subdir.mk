################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/Third_Party/tinyusb/src/portable/microchip/samg/dcd_samg.c 

OBJS += \
./Middlewares/Third_Party/tinyusb/src/portable/microchip/samg/dcd_samg.o 

C_DEPS += \
./Middlewares/Third_Party/tinyusb/src/portable/microchip/samg/dcd_samg.d 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/Third_Party/tinyusb/src/portable/microchip/samg/%.o Middlewares/Third_Party/tinyusb/src/portable/microchip/samg/%.su Middlewares/Third_Party/tinyusb/src/portable/microchip/samg/%.cyclo: ../Middlewares/Third_Party/tinyusb/src/portable/microchip/samg/%.c Middlewares/Third_Party/tinyusb/src/portable/microchip/samg/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F107xC -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I../Middlewares/Third_Party/tinyusb/src -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Middlewares-2f-Third_Party-2f-tinyusb-2f-src-2f-portable-2f-microchip-2f-samg

clean-Middlewares-2f-Third_Party-2f-tinyusb-2f-src-2f-portable-2f-microchip-2f-samg:
	-$(RM) ./Middlewares/Third_Party/tinyusb/src/portable/microchip/samg/dcd_samg.cyclo ./Middlewares/Third_Party/tinyusb/src/portable/microchip/samg/dcd_samg.d ./Middlewares/Third_Party/tinyusb/src/portable/microchip/samg/dcd_samg.o ./Middlewares/Third_Party/tinyusb/src/portable/microchip/samg/dcd_samg.su

.PHONY: clean-Middlewares-2f-Third_Party-2f-tinyusb-2f-src-2f-portable-2f-microchip-2f-samg

