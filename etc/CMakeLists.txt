# Install files
if(APPLE)
    set(_etc_dest "${PROJECT_NAME}.app/Contents/Resources/etc")
else()
    set(_etc_dest "etc")
endif()

install(
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/field_dictionary.json
        ${CMAKE_CURRENT_SOURCE_DIR}/default_config.json
        ${CMAKE_CURRENT_SOURCE_DIR}/example_log_10k.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/qt.conf
    DESTINATION ${_etc_dest}
    COMPONENT application
)

# Install directory
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/prompts
    DESTINATION ${_etc_dest}
    COMPONENT application
)