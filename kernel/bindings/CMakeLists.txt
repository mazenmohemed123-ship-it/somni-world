pybind11_add_module(somni_core somni_bindings.cpp)

target_link_libraries(somni_core PRIVATE SOMNI::kernel)

set_target_properties(somni_core PROPERTIES
    OUTPUT_NAME "somni_core"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/orchestrator"
)
