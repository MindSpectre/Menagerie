##############################################################################
# Http Types — RFC 9110 IMF-fixdate rendering (per-second cached Date header)
##############################################################################
add_library(${Http}.Types.HttpDate STATIC http_date.cpp)

target_include_directories(${Http}.Types.HttpDate PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
# Renderer lives in chrono; only the .cpp needs it, so link PRIVATE.
target_link_libraries(${Http}.Types.HttpDate PRIVATE
        Menagerie::Common::Chrono
)
##############################################################################
