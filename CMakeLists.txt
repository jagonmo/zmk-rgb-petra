# Include dirs exposed unconditionally: the devicetree preprocessor needs
# headers at DTS-parse time, before Kconfig resolves symbols.
zephyr_include_directories(include)
 
if(CONFIG_RGB_PETRA)
  zephyr_library()
  zephyr_library_sources(src/rgb_petra.c)
  # Behavior compiled only when its DT node is present (canonical ZMK pattern)
  zephyr_library_sources_ifdef(CONFIG_ZMK_BEHAVIOR_RGB_PETRA src/behavior_rgb_petra.c)
endif()
 
# ZMK's own headers (zmk/*.h and drivers/behavior.h) live in the ZMK app
# include tree. Locate the zmk project via west topdir and add its include
# dirs so this module can build against ZMK's public API.
zephyr_include_directories(${WEST_TOPDIR}/zmk/app/include)
zephyr_include_directories(${WEST_TOPDIR}/zmk/app/src)