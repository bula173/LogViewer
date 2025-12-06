# Qt6 GUI Framework
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

if(Qt6_FOUND)
    message(STATUS "Found Qt6 ${Qt6_VERSION}")
    
    # Enable automatic MOC, UIC, and RCC processing
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
    
    # Create Qt6 library target
    add_library(qtgui_lib STATIC)
    
    target_sources(qtgui_lib PRIVATE
        # Add your Qt-specific source files here
        # For now, we'll just have empty placeholder
    )
    
    target_include_directories(qtgui_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    target_link_libraries(qtgui_lib PUBLIC
        Qt6::Core
        Qt6::Widgets
    )
    
    # Set component for packaging
    set_target_properties(qtgui_lib PROPERTIES 
        CPACK_COMPONENT_NAME "qt_app"
    )
    
else()
    message(FATAL_ERROR "Qt6 not found!")
endif()
