set(CRYPTO ${COMMON}.Crypto)

##############################################################################
# Hash Utilities: SHA256
##############################################################################
add_library(${CRYPTO}.Encrypt STATIC
        encrypt/hashers/hasher.cpp
        encrypt/hashers/pbkdf2.hpp
        encrypt/hashers/sha_256.hpp
        encrypt/hashers/detail/hash_interface.hpp
        encrypt/salt/salt_generator.hpp
        encrypt/salt/salt_generator.cpp
)

target_include_directories(${CRYPTO}.Encrypt PUBLIC
        encrypt/salt
        encrypt/hashers
)

target_link_libraries(${CRYPTO}.Encrypt
        PUBLIC
        Menagerie::Common::Beavers
        PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
)
##############################################################################


##############################################################################
# Crypto utilities
##############################################################################
add_library(${CRYPTO}.Utilities INTERFACE
        utilities/security_utils.hpp
        utilities/compile_time_utils.hpp
)

target_include_directories(${CRYPTO}.Utilities
        INTERFACE
        utilities/
)
##############################################################################

##############################################################################
# Crypto exported library
##############################################################################
add_combined_library(${CRYPTO}
        DIRECTORIES
        export
        SOURCES
        export/menagerie/crypto
        LIBRARIES
        ${CRYPTO}.Utilities
        ${CRYPTO}.Encrypt
)

add_library(Menagerie::Common::Crypto ALIAS ${CRYPTO})
##############################################################################
