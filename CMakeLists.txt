# Include dirs exposed unconditionally: the devicetree preprocessor needs
# headers at DTS-parse time, before Kconfig resolves symbols.
zephyr_include_directories(include)

if(CONFIG_RGB_PETRA)
  zephyr_library()
  zephyr_library_sources(src/rgb_petra.c)
  # Behavior compiled only when its DT node is present (canonical ZMK pattern)
  zephyr_library_sources_ifdef(CONFIG_ZMK_BEHAVIOR_RGB_PETRA src/behavior_rgb_petra.c)
endif()
