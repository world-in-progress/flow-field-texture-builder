add_library(quikgrid STATIC)
add_library(quikgrid::quikgrid ALIAS quikgrid)
set_target_properties(quikgrid PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

target_sources(quikgrid PRIVATE
        contour.cpp
        gridxtyp.cpp
        scatdata.cpp
        surfgrid.cpp
        xpand.cpp
        xpand_context.cpp
)

target_include_directories(quikgrid SYSTEM PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(quikgrid PUBLIC project_options)
