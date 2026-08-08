set(SPIDER ${COMMON}.Spider)

##############################################################################
# Spider policies classes
##############################################################################
add_library(${SPIDER}.Policies INTERFACE
        policies/policies.hpp
)
target_include_directories(${SPIDER}.Policies INTERFACE
        policies/
)
##############################################################################

# Spider core files and structs
##############################################################################
add_library(${SPIDER}.Core STATIC
        core/spider.cpp
        core/spider.hpp
        core/detail/traits.hpp
        core/detail/details.hpp
        core/detail/spider.inl
)
target_include_directories(${SPIDER}.Core PUBLIC
        core/
)
target_link_libraries(${SPIDER}.Core
        PUBLIC
        ${SPIDER}.Policies
        PRIVATE
        Menagerie::Common::Chrono
)
##############################################################################


##############################################################################
# Spider exported library
##############################################################################
add_combined_library(${SPIDER}
        DIRECTORIES
        export
        SOURCES
        export/menagerie/spider
        LIBRARIES
        ${SPIDER}.Core
)
add_library(Menagerie::Common::Spider ALIAS ${SPIDER})
##############################################################################


