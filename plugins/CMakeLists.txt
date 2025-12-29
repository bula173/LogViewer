# Plugins directory
# Contains optional plugin modules that extend LogViewer functionality

# AI provider plugin (built-in)
add_subdirectory(ai)


# Custom target to build all plugins
add_custom_target(plugins_all
    COMMENT "Building all plugins"
)

# Add dependencies to all plugin targets
add_dependencies(plugins_all 
    ai_provider
)

