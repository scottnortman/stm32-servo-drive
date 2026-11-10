# STM32 bare-metal toolchain (Arm GNU Toolchain)
#
# Set the ARM_TOOLCHAIN_BIN_DIR environment variable to point at the
# toolchain's bin/ directory (this is how CI selects its own install);
# otherwise falls back to the local macOS install path.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(DEFINED ENV{ARM_TOOLCHAIN_BIN_DIR})
    set(ARM_TOOLCHAIN_BIN_DIR "$ENV{ARM_TOOLCHAIN_BIN_DIR}")
else()
    set(ARM_TOOLCHAIN_BIN_DIR "/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi/bin")
endif()

# Parent of bin/ — also the root of the target sysroot (arm-none-eabi/lib/...)
get_filename_component(ARM_TOOLCHAIN_ROOT "${ARM_TOOLCHAIN_BIN_DIR}/.." ABSOLUTE)

# Resolve executable names explicitly. CMake does not add .exe when an
# absolute compiler path is supplied from a toolchain file on Windows.
find_program(ARM_GCC NAMES arm-none-eabi-gcc arm-none-eabi-gcc.exe
    PATHS "${ARM_TOOLCHAIN_BIN_DIR}" NO_DEFAULT_PATH REQUIRED)
find_program(ARM_OBJCOPY NAMES arm-none-eabi-objcopy arm-none-eabi-objcopy.exe
    PATHS "${ARM_TOOLCHAIN_BIN_DIR}" NO_DEFAULT_PATH REQUIRED)
find_program(ARM_SIZE NAMES arm-none-eabi-size arm-none-eabi-size.exe
    PATHS "${ARM_TOOLCHAIN_BIN_DIR}" NO_DEFAULT_PATH REQUIRED)

set(CMAKE_C_COMPILER   "${ARM_GCC}")
set(CMAKE_ASM_COMPILER "${ARM_GCC}")
set(CMAKE_OBJCOPY      "${ARM_OBJCOPY}")
set(CMAKE_SIZE         "${ARM_SIZE}")

# --- CRITICAL: prevent macOS host flags from being injected ---
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_ASM_COMPILER_WORKS 1)

set(CMAKE_OSX_ARCHITECTURES "" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "" CACHE STRING "" FORCE)
set(CMAKE_OSX_SYSROOT "" CACHE PATH "" FORCE)

# Also clear any cached initial flags that might contain -arch/-isysroot
set(CMAKE_C_FLAGS_INIT "" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS_INIT "" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_INIT "" CACHE STRING "" FORCE)


