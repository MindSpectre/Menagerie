# vcpkg bootstrap shim.
#
# Every preset points CMAKE_TOOLCHAIN_FILE at this file rather than directly at
# vcpkg/scripts/buildsystems/vcpkg.cmake, so a fresh clone configures without the
# user provisioning vcpkg by hand. This file resolves a vcpkg checkout, clones and
# bootstraps one if there is none, then chains to the real vcpkg toolchain.
#
# Resolution order:
#   1. <repo>/vcpkg          - a checkout already in the source tree wins
#   2. -DVCPKG_ROOT=<path>   - explicit override on the configure line
#   3. $ENV{VCPKG_ROOT}      - shared/system checkout (the toolchain image sets this)
#   4. otherwise             - clone + bootstrap into <repo>/vcpkg
#
# Knobs:
#   VCPKG_BOOTSTRAP_URL - clone source (default: upstream microsoft/vcpkg)
#   VCPKG_BOOTSTRAP_REF - commit/tag/branch to check out. Empty (the default)
#                         tracks upstream's default branch via a shallow clone,
#                         matching infrastructure/toolchain/Dockerfile. Setting it
#                         forces a full clone, since an arbitrary sha needs history.
#
# CMake re-reads the toolchain file for every try_compile, so everything below must
# stay cheap and idempotent: after the first pass resolution short-circuits on the
# step 1 existence check and nothing is cloned twice.

# Derived from this file's own location rather than CMAKE_SOURCE_DIR, which is not
# the repository root inside try_compile sub-projects.
get_filename_component(MENAGERIE_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(VCPKG_BOOTSTRAP_URL "https://github.com/microsoft/vcpkg" CACHE STRING
        "Repository the vcpkg bootstrap clones from")
set(VCPKG_BOOTSTRAP_REF "" CACHE STRING
        "vcpkg commit/tag/branch to check out; empty tracks the default branch")

set(_mv_root "")
set(_mv_origin "")

if (EXISTS "${MENAGERIE_SOURCE_ROOT}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(_mv_root "${MENAGERIE_SOURCE_ROOT}/vcpkg")
    set(_mv_origin "source tree")
elseif (DEFINED VCPKG_ROOT AND EXISTS "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(_mv_root "${VCPKG_ROOT}")
    set(_mv_origin "-DVCPKG_ROOT")
elseif (DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(_mv_root "$ENV{VCPKG_ROOT}")
    set(_mv_origin "VCPKG_ROOT environment variable")
endif ()

# ── Provision ────────────────────────────────────────────────────────────────

if (NOT _mv_root)
    set(_mv_dest "${MENAGERIE_SOURCE_ROOT}/vcpkg")

    find_program(MENAGERIE_GIT_EXECUTABLE git)
    if (NOT MENAGERIE_GIT_EXECUTABLE)
        message(FATAL_ERROR
                "No vcpkg checkout found and git is not installed, so one cannot be provisioned.\n"
                "Install git and re-run, or provide a vcpkg checkout yourself:\n"
                "  git clone ${VCPKG_BOOTSTRAP_URL} ${_mv_dest} && ${_mv_dest}/bootstrap-vcpkg.sh\n"
                "An existing checkout elsewhere can be reused with -DVCPKG_ROOT=<path>.")
    endif ()

    if (VCPKG_BOOTSTRAP_REF)
        # A full clone: an arbitrary commit is not reachable from a shallow one.
        set(_mv_clone_args "")
        message(STATUS "vcpkg: not found, cloning ${VCPKG_BOOTSTRAP_URL}@${VCPKG_BOOTSTRAP_REF} into ${_mv_dest}")
    else ()
        set(_mv_clone_args --depth 1)
        message(STATUS "vcpkg: not found, cloning ${VCPKG_BOOTSTRAP_URL} into ${_mv_dest}")
    endif ()

    execute_process(
            COMMAND "${MENAGERIE_GIT_EXECUTABLE}" clone ${_mv_clone_args} "${VCPKG_BOOTSTRAP_URL}" "${_mv_dest}"
            RESULT_VARIABLE _mv_rc
    )
    if (NOT _mv_rc EQUAL 0)
        # Leave no half-clone behind: it would satisfy the step 1 check on the next
        # configure and fail far less legibly than this does.
        file(REMOVE_RECURSE "${_mv_dest}")
        message(FATAL_ERROR "vcpkg: clone of ${VCPKG_BOOTSTRAP_URL} failed (exit ${_mv_rc}).")
    endif ()

    if (VCPKG_BOOTSTRAP_REF)
        execute_process(
                COMMAND "${MENAGERIE_GIT_EXECUTABLE}" -C "${_mv_dest}" checkout --quiet "${VCPKG_BOOTSTRAP_REF}"
                RESULT_VARIABLE _mv_rc
        )
        if (NOT _mv_rc EQUAL 0)
            file(REMOVE_RECURSE "${_mv_dest}")
            message(FATAL_ERROR "vcpkg: checkout of VCPKG_BOOTSTRAP_REF '${VCPKG_BOOTSTRAP_REF}' failed (exit ${_mv_rc}).")
        endif ()
    endif ()

    set(_mv_root "${_mv_dest}")
    set(_mv_origin "bootstrap clone")
endif ()

# ── Bootstrap the vcpkg executable ───────────────────────────────────────────
# Also covers a checkout that was cloned but never bootstrapped.

if (CMAKE_HOST_WIN32)
    set(_mv_exe "${_mv_root}/vcpkg.exe")
    set(_mv_bootstrap "${_mv_root}/bootstrap-vcpkg.bat")
else ()
    set(_mv_exe "${_mv_root}/vcpkg")
    set(_mv_bootstrap "${_mv_root}/bootstrap-vcpkg.sh")
endif ()

if (NOT EXISTS "${_mv_exe}")
    message(STATUS "vcpkg: bootstrapping ${_mv_bootstrap} (this takes a minute)")
    execute_process(
            COMMAND "${_mv_bootstrap}" -disableMetrics
            WORKING_DIRECTORY "${_mv_root}"
            RESULT_VARIABLE _mv_rc
    )
    if (NOT _mv_rc EQUAL 0)
        message(FATAL_ERROR "vcpkg: ${_mv_bootstrap} failed (exit ${_mv_rc}).")
    endif ()
endif ()

# ── Chain to the real toolchain ──────────────────────────────────────────────

# Cached so the top-level CMakeLists can report which checkout the build used;
# try_compile sub-projects re-resolve instead, which now lands on step 1.
set(MENAGERIE_VCPKG_ROOT "${_mv_root}" CACHE INTERNAL "Resolved vcpkg root")
set(MENAGERIE_VCPKG_ORIGIN "${_mv_origin}" CACHE INTERNAL "How the vcpkg root was resolved")

include("${_mv_root}/scripts/buildsystems/vcpkg.cmake")
