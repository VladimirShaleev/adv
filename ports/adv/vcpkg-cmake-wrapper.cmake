find_program(ADVC_EXECUTABLE NAMES advc PATHS "${CMAKE_CURRENT_LIST_DIR}/../../../@HOST_TRIPLET@/tools/adv" NO_DEFAULT_PATH)

_find_package(${ARGS} CONFIG)
