# Plugins directory
# Contains optional plugin modules that extend LogViewer functionality

if (USE_QT)

    # Field converter example plugin - demonstrates plugin system
    add_subdirectory(field_converter_example)

    # Event metrics analyzer plugin - frequency analysis
    add_subdirectory(event_metrics)

    add_subdirectory(ssh_parser)


    # Custom target to build all plugins
    add_custom_target(plugins_all
        COMMENT "Building all plugins"
    )

    # Add dependencies to all plugin targets
    add_dependencies(plugins_all 
        field_converter_example
        event_metrics
        ssh_parser_plugin
    )
endif()
