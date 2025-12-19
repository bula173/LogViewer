# Install files
install(
    FILES 
        ${CMAKE_CURRENT_SOURCE_DIR}/field_dictionary.json
        ${CMAKE_CURRENT_SOURCE_DIR}/default_config.json 
    DESTINATION etc
    COMPONENT application
)

# Install directory
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/prompts
    DESTINATION etc
    COMPONENT application
)