# wxWidgets GUI Framework
find_package(wxWidgets REQUIRED COMPONENTS core base)

if(wxWidgets_FOUND)
    include(${wxWidgets_USE_FILE})
    message(STATUS "Found wxWidgets ${wxWidgets_VERSION}")
    
    # Create wxWidgets library target
    add_library(wxgui_lib STATIC)
    
    target_sources(wxgui_lib PRIVATE
        # Add your wxWidgets-specific source files here
        # For now, we'll just have empty placeholder
    )
    
    target_include_directories(wxgui_lib PUBLIC
        ${wxWidgets_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    target_link_libraries(wxgui_lib PUBLIC
        ${wxWidgets_LIBRARIES}
    )
    
    target_compile_definitions(wxgui_lib PUBLIC
        ${wxWidgets_DEFINITIONS}
    )
    
    # Set component for packaging
    set_target_properties(wxgui_lib PROPERTIES 
        CPACK_COMPONENT_NAME "wx_app"
    )
    
else()
    message(FATAL_ERROR "wxWidgets not found!")
endif()
