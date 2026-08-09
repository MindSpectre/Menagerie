##############################################################################
# Minimal Http server (standalone path) - living documentation
# for the public <menagerie/estuary> API. Successor of the pre-redesign manual
# handler example.
##############################################################################
add_executable(${EXAMPLES}.Estuary.MinimalServer
        minimal_http_server.cpp
)
target_link_libraries(${EXAMPLES}.Estuary.MinimalServer
        PRIVATE
        Menagerie::Component::Estuary
)
##############################################################################
