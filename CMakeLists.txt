cmake_minimum_required(VERSION 3.13)  # CMake version check
project(PintOS)               # Create project "simple_example"
set(CMAKE_CXX_STANDARD 14)            # Enable c++14 standard

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32 -g -msoft-float -O -march=i686 -fno-stack-protector")
set(CMAKE_C_COMPILER "x86_64-elf-gcc")
set(CMAKE_LINKER "x86_64-elf-ld")
set(CMAKE_LINKER_FLAGS "-m elf_i386")

set(KERNEL_SOURCE_FILES
        src/threads/start.S
        src/threads/init.c
        src/threads/thread.c
        src/threads/switch.S
        src/threads/interrupt.c
        src/threads/intr-stubs.S
        src/threads/synch.c
        src/threads/palloc.c
        src/threads/malloc.c
        )

set(DRIVER_SOURCE_FILES
        src/devices/pit.c
        src/devices/timer.c
        src/devices/kbd.c
        src/devices/vga.c
        src/devices/serial.c
        src/devices/block.c
        src/devices/partition.c
        src/devices/ide.c
        src/devices/input.c
        src/devices/rtc.c
        src/devices/shutdown.c
        src/devices/speaker.c
        )
set(LIB_SOURCE_FILES
        src/lib/debug.c
        src/lib/random.c
        src/lib/stdio.c
        src/lib/stdlib.c
        src/lib/string.c
        src/lib/arithmetic.c
        src/lib/ustar.c
        )

set(KERNEL_LIB_SOURCE_FILES
        src/lib/kernel/debug.c
        src/lib/kernel/list.c
        src/lib/kernel/bitmap.c
        src/lib/kernel/hash.c
        src/lib/kernel/console.c
        )

set(USER_PROG_SOURCE_FILES
        src/userprog/process.c
        src/userprog/pagedir.c
        src/userprog/exception.c
        src/userprog/syscall.c
        src/userprog/gdt.c
        src/userprog/tss.c
        )

set(VIRT_MEMORY_SOURCE_FILES)

include_directories(src src/lib src/lib/kernel)

# Add executable target with source files listed in SOURCE_FILES variable
add_executable(dummy
        ${KERNEL_SOURCE_FILES}
        ${DRIVER_SOURCE_FILES}
        ${LIB_SOURCE_FILES}
        ${KERNEL_LIB_SOURCE_FILES}
        )
