target_sources(picoprobe++ PRIVATE
        Logic_Analyzer.cpp
        protocol.cpp
        _capture.cpp
        LA_log.cpp
)

target_include_directories(picoprobe++ PRIVATE
        .
)

# generate PIO header file
generate_pio_header(picoprobe++
        ${CMAKE_CURRENT_LIST_DIR}/pio/capture.pio
        OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/pio)
