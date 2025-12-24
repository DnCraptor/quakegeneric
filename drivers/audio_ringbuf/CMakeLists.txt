add_library(audio_i2s_pwm INTERFACE)

target_sources(audio_i2s_pwm INTERFACE
		${CMAKE_CURRENT_LIST_DIR}/audio.c
		${CMAKE_CURRENT_LIST_DIR}/audio.h
)

target_link_libraries(audio_i2s_pwm INTERFACE hardware_pio hardware_dma hardware_pwm hardware_clocks)

target_include_directories(audio_i2s_pwm INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)

pico_generate_pio_header(audio_i2s_pwm
  ${CMAKE_CURRENT_LIST_DIR}/audio_i2s.pio
)
