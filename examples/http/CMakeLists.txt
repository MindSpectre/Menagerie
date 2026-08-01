##############################################################################
# Minimal Http server (standalone path) - living documentation
# for the public <menagerie/http> API. Successor of the pre-redesign manual
# handler example.
##############################################################################
add_executable(${EXAMPLES}.Http.MinimalServer
        minimal_http_server.cpp
)
target_link_libraries(${EXAMPLES}.Http.MinimalServer
        PRIVATE
        Menagerie::Component::Http
)
##############################################################################
