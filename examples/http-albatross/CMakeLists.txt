##############################################################################
# Minimal Http server (standalone path) - living documentation
# for the public <menagerie/albatross> API. Successor of the pre-redesign manual
# handler example.
##############################################################################
add_executable(${EXAMPLES}.Albatross.MinimalServer
        minimal_http_server.cpp
)
target_link_libraries(${EXAMPLES}.Albatross.MinimalServer
        PRIVATE
        Menagerie::Component::Albatross
)
##############################################################################
