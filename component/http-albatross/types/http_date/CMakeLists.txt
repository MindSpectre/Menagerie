##############################################################################
# Http Types — RFC 9110 IMF-fixdate rendering (per-second cached Date header)
##############################################################################
add_library(${Albatross}.Types.HttpDate STATIC http_date.cpp)

target_include_directories(${Albatross}.Types.HttpDate PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
# Renderer lives in chrono; only the .cpp needs it, so link PRIVATE.
target_link_libraries(${Albatross}.Types.HttpDate PRIVATE
        Menagerie::Common::Cuckoo
)
##############################################################################
