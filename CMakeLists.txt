cmake_minimum_required(VERSION 3.20)

project(FlowField_TextureBuilder VERSION 0.1.0 LANGUAGES CXX)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_20)

option(FLOW_FIELD_BUILD_CLI "Build the JSON CLI executable" ON)

add_subdirectory(modules/quikgrid)

find_package(Stb REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(PROJ CONFIG REQUIRED)
find_package(GDAL CONFIG REQUIRED)

option(FLOW_FIELD_BUILD_PYTHON "Build Python extension module" ON)
if(FLOW_FIELD_BUILD_PYTHON)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
endif()

add_library(flow_field_texture_builder_core)
set_target_properties(flow_field_texture_builder_core PROPERTIES
        NO_SYSTEM_FROM_IMPORTED ON
        POSITION_INDEPENDENT_CODE ON
)
target_sources(flow_field_texture_builder_core PRIVATE
        src/flow_field.cpp
        src/flow_field_builder.cpp
        src/cartesian3.cpp
        src/geo_data_source.cpp
)
target_include_directories(flow_field_texture_builder_core PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
)
target_include_directories(flow_field_texture_builder_core PRIVATE
        "${Stb_INCLUDE_DIR}"
)
if(GDAL_INCLUDE_DIR)
        # Keep GDAL headers from the discovered package ahead of compiler-default installs.
        target_include_directories(flow_field_texture_builder_core BEFORE PRIVATE "${GDAL_INCLUDE_DIR}")
endif()
target_link_libraries(flow_field_texture_builder_core PUBLIC
        project_options
        quikgrid::quikgrid
        nlohmann_json::nlohmann_json
        PROJ::proj
        GDAL::GDAL
)

if(FLOW_FIELD_BUILD_CLI)
        add_executable(${PROJECT_NAME}
                src/main.cpp
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE
                flow_field_texture_builder_core
        )
endif()

if(FLOW_FIELD_BUILD_PYTHON)
        add_library(_flow_field_texture_builder_c SHARED
                src/python_binding/c_api.cpp
        )
        target_link_libraries(_flow_field_texture_builder_c PRIVATE
                flow_field_texture_builder_core
        )
        set_target_properties(_flow_field_texture_builder_c PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python/flow_field_texture_builder"
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python/flow_field_texture_builder"
                ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python/flow_field_texture_builder"
        )
        add_custom_command(TARGET _flow_field_texture_builder_c POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/python/flow_field_texture_builder"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "${PROJECT_SOURCE_DIR}/python/flow_field_texture_builder/__init__.py"
                        "${CMAKE_BINARY_DIR}/python/flow_field_texture_builder/__init__.py"
        )
        install(TARGETS _flow_field_texture_builder_c
                LIBRARY DESTINATION flow_field_texture_builder
                RUNTIME DESTINATION flow_field_texture_builder
        )
        install(FILES python/flow_field_texture_builder/__init__.py
                DESTINATION flow_field_texture_builder
        )
endif()

include(CTest)
if(BUILD_TESTING)
        add_test(NAME cmake_auto_triplet_test
                COMMAND "${CMAKE_COMMAND}" -P "${PROJECT_SOURCE_DIR}/tests/cmake_auto_triplet_test.cmake"
        )

        if(FLOW_FIELD_BUILD_PYTHON)
                add_test(NAME uv_project_config_test
                        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/uv_project_config_test.py"
                        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
                )
        endif()

        add_executable(quikgrid_context_test
                tests/quikgrid_context_test.cpp
        )
        target_link_libraries(quikgrid_context_test PRIVATE
                project_options
                quikgrid::quikgrid
        )
        add_test(NAME quikgrid_context_test COMMAND quikgrid_context_test)

        add_executable(flow_field_builder_test
                tests/flow_field_builder_test.cpp
        )
        target_link_libraries(flow_field_builder_test PRIVATE
                flow_field_texture_builder_core
        )
        target_include_directories(flow_field_builder_test PRIVATE
                "${Stb_INCLUDE_DIR}"
        )
        add_test(NAME flow_field_builder_test COMMAND flow_field_builder_test)

        add_executable(flow_field_cli_adapter_test
                tests/flow_field_cli_adapter_test.cpp
        )
        target_link_libraries(flow_field_cli_adapter_test PRIVATE
                flow_field_texture_builder_core
        )
        target_include_directories(flow_field_cli_adapter_test PRIVATE
                "${Stb_INCLUDE_DIR}"
        )
        add_test(NAME flow_field_cli_adapter_test COMMAND flow_field_cli_adapter_test)

        if(FLOW_FIELD_BUILD_PYTHON)
                add_test(NAME python_binding_smoke
                        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/python_binding_smoke.py"
                )
                set_tests_properties(python_binding_smoke PROPERTIES
                        ENVIRONMENT "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
                )
        endif()
endif()
