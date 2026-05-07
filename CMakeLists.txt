cmake_minimum_required(VERSION 3.20)

project(FlowField_TextureBuilder VERSION 0.1.0 LANGUAGES CXX)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_20)

add_subdirectory(modules/quikgrid)

find_package(Stb REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(PROJ CONFIG REQUIRED)
find_package(GDAL CONFIG REQUIRED)

add_executable(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES
        NO_SYSTEM_FROM_IMPORTED ON
)
target_sources(${PROJECT_NAME} PRIVATE
        src/flow_field.cpp
        src/cartesian3.cpp
        src/geo_data_source.cpp
        src/main.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
)
target_include_directories(${PROJECT_NAME} PRIVATE
        "${Stb_INCLUDE_DIR}"
)
if(GDAL_INCLUDE_DIR)
        # Keep GDAL headers from the discovered package ahead of compiler-default installs.
        target_include_directories(${PROJECT_NAME} BEFORE PRIVATE "${GDAL_INCLUDE_DIR}")
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE
        project_options
        quikgrid::quikgrid
        nlohmann_json::nlohmann_json
        PROJ::proj
        GDAL::GDAL
)
