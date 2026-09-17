add_subdirectory(led_interface)
add_subdirectory(usb_dap_device)
add_subdirectory(usb_uart_device)
add_subdirectory(logic_analyzer_module)

target_sources(picoprobe++ PRIVATE
        picoprobe++.cpp
)

target_include_directories(picoprobe++ PRIVATE
        .
)

