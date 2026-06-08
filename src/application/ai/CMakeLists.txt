target_sources(application_core
    PRIVATE GemmaInferenceEngine.cpp
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/GemmaInferenceEngine.hpp>
            $<INSTALL_INTERFACE:GemmaInferenceEngine.hpp>)
