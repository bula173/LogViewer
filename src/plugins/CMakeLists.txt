# Plugins directory
# Contains optional plugin modules that extend LogViewer functionality

if (USE_QT)

    # Field converter example plugin - demonstrates plugin system
    add_subdirectory(field_converter_example)

    # Event metrics analyzer plugin - frequency analysis
    add_subdirectory(event_metrics)

    # AI provider plugin (built-in)
    add_subdirectory(ai)

    # Log anomaly metrics plugin - Eigen/Boost-based analyzer
    add_subdirectory(log_anomaly_metrics)

    # SSH parser plugin - optional (requires libssh)
    add_subdirectory(ssh_parser)


    # Custom target to build all plugins
    add_custom_target(plugins_all
        COMMENT "Building all plugins"
    )

    # Add dependencies to all plugin targets
    add_dependencies(plugins_all 
        field_converter_example
        event_metrics
        ai_provider
    )
    
    # Add optional plugins only if successfully created
    if(TARGET ssh_parser_plugin)
        add_dependencies(plugins_all ssh_parser_plugin)
    endif()

    if(TARGET log_anomaly_metrics)
        add_dependencies(plugins_all log_anomaly_metrics)
    endif()

    # Custom target to copy plugin zips to dist/plugins
    add_custom_target(copy_plugins_to_dist ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/dist/plugins"
        COMMAND ${CMAKE_COMMAND} -E echo "Copying plugin packages to dist/plugins..."
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
            "${CMAKE_BINARY_DIR}/plugins/field_converter_example.zip"
            "${CMAKE_BINARY_DIR}/plugins/event_metrics.zip"
            "${CMAKE_BINARY_DIR}/plugins/ai_provider.zip"
            "${CMAKE_SOURCE_DIR}/dist/plugins/"
        DEPENDS plugins_all
        COMMENT "Copying plugin packages for distribution"
        VERBATIM
    )
    
    # Copy optional plugins if they exist
    if(TARGET ssh_parser_plugin)
        add_custom_command(TARGET copy_plugins_to_dist POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different 
                "${CMAKE_BINARY_DIR}/plugins/ssh_parser_plugin.zip"
                "${CMAKE_SOURCE_DIR}/dist/plugins/"
            COMMENT "Copying ssh_parser_plugin.zip"
        )
    endif()
    
    if(TARGET log_anomaly_metrics)
        add_custom_command(TARGET copy_plugins_to_dist POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different 
                "${CMAKE_BINARY_DIR}/plugins/log_anomaly_metrics.zip"
                "${CMAKE_SOURCE_DIR}/dist/plugins/"
            COMMENT "Copying log_anomaly_metrics.zip"
        )
    endif()
endif()
