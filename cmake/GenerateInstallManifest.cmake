# GenerateInstallManifest.cmake
# Generates a manifest file tracking installed libraries and versions
# This prevents overwriting up-to-date files during installation updates

function(generate_install_manifest)
    cmake_parse_arguments(MANIFEST "" "OUTPUT_FILE;INSTALL_PREFIX" "LIBRARIES" ${ARGN})
    
    if(NOT MANIFEST_OUTPUT_FILE)
        message(FATAL_ERROR "OUTPUT_FILE is required")
    endif()
    
    # Create manifest JSON content
    string(TIMESTAMP BUILD_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)
    
    # Start JSON object
    set(MANIFEST_JSON "{")
    string(APPEND MANIFEST_JSON "\n  \"version\": \"${PROJECT_VERSION}\",")
    string(APPEND MANIFEST_JSON "\n  \"installationDate\": \"${BUILD_TIME}\",")
    string(APPEND MANIFEST_JSON "\n  \"installationPrefix\": \"${MANIFEST_INSTALL_PREFIX}\",")
    
    # Add CMake version info
    string(APPEND MANIFEST_JSON "\n  \"buildInfo\": {")
    string(APPEND MANIFEST_JSON "\n    \"cmakeVersion\": \"${CMAKE_VERSION}\",")
    string(APPEND MANIFEST_JSON "\n    \"cmakeGenerator\": \"${CMAKE_GENERATOR}\",")
    string(APPEND MANIFEST_JSON "\n    \"buildType\": \"${CMAKE_BUILD_TYPE}\",")
    string(APPEND MANIFEST_JSON "\n    \"cxxStandard\": \"${CMAKE_CXX_STANDARD}\",")
    string(APPEND MANIFEST_JSON "\n    \"compiler\": \"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\"")
    string(APPEND MANIFEST_JSON "\n  },")
    
    # Add libraries information
    string(APPEND MANIFEST_JSON "\n  \"libraries\": {")
    
    list(LENGTH MANIFEST_LIBRARIES lib_count)
    set(lib_index 0)
    
    foreach(lib IN LISTS MANIFEST_LIBRARIES)
        string(APPEND MANIFEST_JSON "\n    \"${lib}\": {")
        
        # Try to find library version from common sources
        if(TARGET ${lib})
            get_target_property(lib_version ${lib} VERSION)
            if(lib_version)
                string(APPEND MANIFEST_JSON "\n      \"version\": \"${lib_version}\"")
            else()
                string(APPEND MANIFEST_JSON "\n      \"version\": \"unknown\"")
            endif()
        else()
            string(APPEND MANIFEST_JSON "\n      \"version\": \"unknown\"")
        endif()
        
        string(APPEND MANIFEST_JSON "\n    }")
        
        math(EXPR lib_index "${lib_index} + 1")
        if(NOT lib_index EQUAL lib_count)
            string(APPEND MANIFEST_JSON ",")
        endif()
    endforeach()
    
    string(APPEND MANIFEST_JSON "\n  },")
    
    # Add metadata
    string(APPEND MANIFEST_JSON "\n  \"metadata\": {")
    string(APPEND MANIFEST_JSON "\n    \"description\": \"LogViewer installation manifest - prevents overwriting during upgrades\",")
    string(APPEND MANIFEST_JSON "\n    \"format_version\": \"1.0\"")
    string(APPEND MANIFEST_JSON "\n  }")
    string(APPEND MANIFEST_JSON "\n}")
    
    # Write manifest file
    file(WRITE "${MANIFEST_OUTPUT_FILE}" "${MANIFEST_JSON}")
    message(STATUS "Generated installation manifest: ${MANIFEST_OUTPUT_FILE}")
endfunction()
