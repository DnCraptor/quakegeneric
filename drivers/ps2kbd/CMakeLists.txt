add_library(ps2kbd STATIC)

set_target_properties(ps2kbd PROPERTIES LINKER_LANGUAGE CXX)

target_sources(ps2kbd PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/ps2kbd_mrmltr.cpp
	${CMAKE_CURRENT_LIST_DIR}/ps2kbd_mrmltr.h
)

target_link_libraries(ps2kbd PUBLIC hardware_pio hardware_clocks)

target_include_directories(ps2kbd PUBLIC
	${CMAKE_CURRENT_LIST_DIR}
)


pico_generate_pio_header(ps2kbd 
	${CMAKE_CURRENT_LIST_DIR}/ps2kbd_mrmltr.pio
)

pico_generate_pio_header(ps2kbd 
	${CMAKE_CURRENT_LIST_DIR}/ps2kbd_mrmltr2.pio
)
