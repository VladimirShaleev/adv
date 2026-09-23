file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../vcpkg.json" ADV_VCPKG_JSON)

string(REGEX MATCH "\"version\"[ \t\n\r]*:[ \t\n\r]*\"[^\"]*\"" ADV_TMP_VERSION_PAIR ${ADV_VCPKG_JSON})
string(REGEX REPLACE "\"version\"[ \t\n\r]*:[ \t\n\r]*\"([^\"]*)\"" "\\1" ADV_VERSION ${ADV_TMP_VERSION_PAIR})
string(REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)" "\\1" ADV_VERSION_MAJOR ${ADV_VERSION})
string(REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)" "\\2" ADV_VERSION_MINOR ${ADV_VERSION})
string(REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)" "\\3" ADV_VERSION_MICRO ${ADV_VERSION})

unset(ADV_TMP_VERSION_PAIR)
unset(ADV_VCPKG_JSON)
