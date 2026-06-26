################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/board.c \
../Core/Src/freertos.c \
../Core/Src/main.c \
../Core/Src/proto.c \
../Core/Src/records.c \
../Core/Src/rtc_time.c \
../Core/Src/sd_spi.c \
../Core/Src/stm32f1xx_hal_msp.c \
../Core/Src/stm32f1xx_hal_timebase_tim.c \
../Core/Src/stm32f1xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f1xx.c \
../Core/Src/task_gpio.c \
../Core/Src/task_sampler.c \
../Core/Src/task_storage.c \
../Core/Src/task_usb.c \
../Core/Src/usb_descriptors.c \
../Core/Src/user_diskio.c 

OBJS += \
./Core/Src/board.o \
./Core/Src/freertos.o \
./Core/Src/main.o \
./Core/Src/proto.o \
./Core/Src/records.o \
./Core/Src/rtc_time.o \
./Core/Src/sd_spi.o \
./Core/Src/stm32f1xx_hal_msp.o \
./Core/Src/stm32f1xx_hal_timebase_tim.o \
./Core/Src/stm32f1xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f1xx.o \
./Core/Src/task_gpio.o \
./Core/Src/task_sampler.o \
./Core/Src/task_storage.o \
./Core/Src/task_usb.o \
./Core/Src/usb_descriptors.o \
./Core/Src/user_diskio.o 

C_DEPS += \
./Core/Src/board.d \
./Core/Src/freertos.d \
./Core/Src/main.d \
./Core/Src/proto.d \
./Core/Src/records.d \
./Core/Src/rtc_time.d \
./Core/Src/sd_spi.d \
./Core/Src/stm32f1xx_hal_msp.d \
./Core/Src/stm32f1xx_hal_timebase_tim.d \
./Core/Src/stm32f1xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f1xx.d \
./Core/Src/task_gpio.d \
./Core/Src/task_sampler.d \
./Core/Src/task_storage.d \
./Core/Src/task_usb.d \
./Core/Src/usb_descriptors.d \
./Core/Src/user_diskio.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F107xC -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I../Middlewares/Third_Party/tinyusb/src -I../Middlewares/Third_Party/FatFs/src -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/board.cyclo ./Core/Src/board.d ./Core/Src/board.o ./Core/Src/board.su ./Core/Src/freertos.cyclo ./Core/Src/freertos.d ./Core/Src/freertos.o ./Core/Src/freertos.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/proto.cyclo ./Core/Src/proto.d ./Core/Src/proto.o ./Core/Src/proto.su ./Core/Src/records.cyclo ./Core/Src/records.d ./Core/Src/records.o ./Core/Src/records.su ./Core/Src/rtc_time.cyclo ./Core/Src/rtc_time.d ./Core/Src/rtc_time.o ./Core/Src/rtc_time.su ./Core/Src/sd_spi.cyclo ./Core/Src/sd_spi.d ./Core/Src/sd_spi.o ./Core/Src/sd_spi.su ./Core/Src/stm32f1xx_hal_msp.cyclo ./Core/Src/stm32f1xx_hal_msp.d ./Core/Src/stm32f1xx_hal_msp.o ./Core/Src/stm32f1xx_hal_msp.su ./Core/Src/stm32f1xx_hal_timebase_tim.cyclo ./Core/Src/stm32f1xx_hal_timebase_tim.d ./Core/Src/stm32f1xx_hal_timebase_tim.o ./Core/Src/stm32f1xx_hal_timebase_tim.su ./Core/Src/stm32f1xx_it.cyclo ./Core/Src/stm32f1xx_it.d ./Core/Src/stm32f1xx_it.o ./Core/Src/stm32f1xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f1xx.cyclo ./Core/Src/system_stm32f1xx.d ./Core/Src/system_stm32f1xx.o ./Core/Src/system_stm32f1xx.su ./Core/Src/task_gpio.cyclo ./Core/Src/task_gpio.d ./Core/Src/task_gpio.o ./Core/Src/task_gpio.su ./Core/Src/task_sampler.cyclo ./Core/Src/task_sampler.d ./Core/Src/task_sampler.o ./Core/Src/task_sampler.su ./Core/Src/task_storage.cyclo ./Core/Src/task_storage.d ./Core/Src/task_storage.o ./Core/Src/task_storage.su ./Core/Src/task_usb.cyclo ./Core/Src/task_usb.d ./Core/Src/task_usb.o ./Core/Src/task_usb.su ./Core/Src/usb_descriptors.cyclo ./Core/Src/usb_descriptors.d ./Core/Src/usb_descriptors.o ./Core/Src/usb_descriptors.su ./Core/Src/user_diskio.cyclo ./Core/Src/user_diskio.d ./Core/Src/user_diskio.o ./Core/Src/user_diskio.su

.PHONY: clean-Core-2f-Src

