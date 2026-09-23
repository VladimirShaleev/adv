file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../vcpkg.json" ADV_VCPKG_JSON)

string(REGEX MATCH "\"version\"[ \t\n\r]*:[ \t\n\r]*\"[^\"]*\"" ADV_TMP_VERSION_PAIR ${ADV_VCPKG_JSON})
string(REGEX REPLACE "\"version\"[ \t\n\r]*:[ \t\n\r]*\"([^\"]*)\"" "\\1" ADV_VERSION ${ADV_TMP_VERSION_PAIR})
string(REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)" "\\1" ADV_VERSION_MAJOR ${ADV_VERSION})
string(REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)" "\\2" ADV_VERSION_MINOR ${ADV_VERSION})
string(REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)" "\\3" ADV_VERSION_MICRO ${ADV_VERSION})

string(REGEX MATCH "\"name\"[ \t\n\r]*:[ \t\n\r]*\"[^\"]*\"" ADV_TMP_NAME_PAIR ${ADV_VCPKG_JSON})
string(REGEX REPLACE "\"name\"[ \t\n\r]*:[ \t\n\r]*\"([^\"]*)\"" "\\1" ADV_NAME ${ADV_TMP_NAME_PAIR})
string(REGEX MATCH "\"description\"[ \t\n\r]*:[ \t\n\r]*\"[^\"]*\"" ADV_TMP_DESCRIPTION_PAIR ${ADV_VCPKG_JSON})
string(REGEX REPLACE "\"description\"[ \t\n\r]*:[ \t\n\r]*\"([^\"]*)\"" "\\1" ADV_DESCRIPTION ${ADV_TMP_DESCRIPTION_PAIR})

string(REGEX MATCH "\"homepage\"[ \t\n\r]*:[ \t\n\r]*\"[^\"]*\"" ADV_TMP_HOMEPAGE_PAIR ${ADV_VCPKG_JSON})
string(REGEX REPLACE "\"homepage\"[ \t\n\r]*:[ \t\n\r]*\"([^\"]*)\"" "\\1" ADV_HOMEPAGE ${ADV_TMP_HOMEPAGE_PAIR})

unset(ADV_TMP_VERSION_PAIR)
unset(ADV_VCPKG_JSON)
