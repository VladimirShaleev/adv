set(LIB_PREFIX "VM")
set(HEADER_NAME "vm-version.h")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in" 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/vm/${HEADER_NAME}")

set(LIB_PREFIX "ADV")
set(HEADER_NAME "adv-version.h")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in" 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/adv/${HEADER_NAME}")
