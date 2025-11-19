add_library(dvi_hstx INTERFACE)

target_sources(dvi_hstx INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/dvi.c
    ${CMAKE_CURRENT_LIST_DIR}/dvi_modes.c
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
