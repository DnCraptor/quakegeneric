add_library(dvi_hstx INTERFACE)

option(VGA_HSTX "Enable VGA PWM driver" ON)
option(HDMI_HSTX "Enable HDMI data packets/audio support" ON)

target_sources(dvi_hstx INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/dvi.c
    ${CMAKE_CURRENT_LIST_DIR}/dvi_modes.c
    ${CMAKE_CURRENT_LIST_DIR}/data_packet.c
    ${CMAKE_CURRENT_LIST_DIR}/linebuf_cb/index8.S
)

target_include_directories(dvi_hstx INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(dvi_hstx INTERFACE
    hardware_dma
    hardware_pll
    hardware_gpio
)

if(VGA_HSTX)
    target_compile_definitions(dvi_hstx INTERFACE VGA_HSTX)
endif()

if(HDMI_HSTX)
    target_compile_definitions(dvi_hstx INTERFACE HDMI_HSTX)
endif()

