set(PUFFERFISH ${COMMON}.Pufferfish)

##############################################################################
# Hash Utilities: SHA256
##############################################################################
add_library(${PUFFERFISH}.Encrypt STATIC
        encrypt/hashers/hasher.cpp
        encrypt/hashers/pbkdf2.hpp
        encrypt/hashers/sha_256.hpp
        encrypt/hashers/detail/hash_interface.hpp
        encrypt/salt/salt_generator.hpp
        encrypt/salt/salt_generator.cpp
)

target_include_directories(${PUFFERFISH}.Encrypt PUBLIC
        encrypt/salt
        encrypt/hashers
)

target_link_libraries(${PUFFERFISH}.Encrypt
        PUBLIC
        Menagerie::Common::Beaver
        PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
)
##############################################################################


##############################################################################
# Pufferfish utilities
##############################################################################
add_library(${PUFFERFISH}.Utilities INTERFACE
        utilities/security_utils.hpp
        utilities/compile_time_utils.hpp
)

target_include_directories(${PUFFERFISH}.Utilities
        INTERFACE
        utilities/
)
##############################################################################

##############################################################################
# Pufferfish exported library
##############################################################################
add_combined_library(${PUFFERFISH}
        DIRECTORIES
        export
        SOURCES
        export/menagerie/pufferfish
        LIBRARIES
        ${PUFFERFISH}.Utilities
        ${PUFFERFISH}.Encrypt
)

add_library(Menagerie::Common::Pufferfish ALIAS ${PUFFERFISH})
##############################################################################
