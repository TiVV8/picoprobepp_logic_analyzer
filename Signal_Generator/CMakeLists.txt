cmake_minimum_required(VERSION 3.20)

# Set the target board
set(YAHAL_BOARD "rp2350-launchpad")

# Locate the YAHAL library folder
include(YAHAL_import.cmake)

project(Signal_Generator)

add_executable(Signal_Generator)

# Optimize application for speed
target_compile_options(Signal_Generator
        PRIVATE -Ofast
)

# Add YAHAL
yahal_add_me(Signal_Generator)

# Set YAHAL system clock to 200MHz for RP2040
if (RP2040)
target_compile_definitions(YAHAL_Signal_Generator
        PRIVATE SYS_CLK_FREQ=200
)
endif()

# Optimize YAHAL for speed
target_compile_options(YAHAL_Signal_Generator
        PRIVATE -Ofast
)

# Add WS2812 driver if running on a Launchpad
if (YAHAL_BOARD STREQUAL "rp2040-launchpad" OR
    YAHAL_BOARD STREQUAL "rp2350-launchpad")
yahal_add_driver(Signal_Generator ${YAHAL_MCU}/ws2812)
endif()

yahal_add_custom_targets(Signal_Generator)
yahal_add_extra_outputs(Signal_Generator)

target_sources(Signal_Generator PRIVATE
        sig_gen.cpp
)