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
endif()
