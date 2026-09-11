cmake_minimum_required(VERSION 3.5.2)

set(PROJECT_NAME
        qwertycoin-explorer)


project(${PROJECT_NAME})

include(CTest)

set(CMAKE_CXX_STANDARD 17)

if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mbig-obj -O3")
endif()


if (NOT MONERO_DIR)
    set(MONERO_DIR ~/qwertycoin)
endif()

message(STATUS MONERO_DIR ": ${MONERO_DIR}")

if (NOT MONERO_SOURCE_DIR)
    set(MONERO_SOURCE_DIR ${MONERO_DIR}
          CACHE PATH "Path to the root directory for Qwertycoin")
endif()

if (NOT MONERO_BUILD_DIR)
    # set location of Qwertycoin build tree
    set(MONERO_BUILD_DIR ${MONERO_SOURCE_DIR}/build/release/
          CACHE PATH "Path to the build directory for Qwertycoin")

   if (NOT EXISTS ${MONERO_BUILD_DIR})
        # try different location
        message(STATUS "Trying different folder for Qwertycoin libraries")
        set(MONERO_BUILD_DIR ${MONERO_SOURCE_DIR}/build/Linux/master/release/
            CACHE PATH "Path to the build directory for Qwertycoin" FORCE)
    endif()

endif()

if (NOT EXISTS ${MONERO_BUILD_DIR})
  message(FATAL_ERROR "Qwertycoin libraries not found in: ${MONERO_BUILD_DIR}")
endif()

set(MY_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}/cmake"
        CACHE PATH "The path to the cmake directory of the current project")

list(APPEND CMAKE_MODULE_PATH "${MY_CMAKE_DIR}")

set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${MONERO_BUILD_DIR}"
        CACHE PATH "Add Qwertycoin directory for library searching")
include(MyUtils)

find_package(Monero)

# find boost
find_package(Boost COMPONENTS
        system
        filesystem
        thread
        date_time
        chrono
        regex
        serialization
        program_options
        date_time
        REQUIRED)

#info https://github.com/arsenm/sanitizers-cmake
find_package(Sanitizers)

if(APPLE)
    find_package(OpenSSL REQUIRED)
    include_directories(${OPENSSL_INCLUDE_DIR})
endif()

MESSAGE(STATUS "Looking for libunbound") # FindUnbound.cmake from monero repo

FIND_PATH(UNBOUND_INCLUDE_DIR
  NAMES unbound.h
  PATH_SUFFIXES include/ include/unbound/
  PATHS "${PROJECT_SOURCE_DIR}"
  ${UNBOUND_ROOT}
  $ENV{UNBOUND_ROOT}
  /usr/local/
  /usr/
)

find_library (UNBOUND_LIBRARY unbound)
if (WIN32 OR (${UNBOUND_LIBRARY} STREQUAL "UNBOUND_LIBRARY-NOTFOUND"))
    add_library(unbound STATIC IMPORTED)
    set_property(TARGET unbound PROPERTY IMPORTED_LOCATION ${MONERO_BUILD_DIR}/external/unbound/libunbound.a)
endif()

if("${Xmr_WALLET-CRYPTO_LIBRARIES}" STREQUAL "Xmr_WALLET-CRYPTO_LIBRARY-NOTFOUND")
  set(WALLET_CRYPTO "")
else()
  set(WALLET_CRYPTO ${Xmr_WALLET-CRYPTO_LIBRARIES})
endif()

# include boost headers
include_directories(${Boost_INCLUDE_DIRS})

# include monero
include_directories(${MONERO_SOURCE_DIR}/build)

include_directories("ext/mstch/include")
include_directories("ext/mstch/include/src")
include_directories("ext/crow")

# add ext/ subfolder
add_subdirectory(ext/)

# add src/ subfolder
add_subdirectory(src/)


set(SOURCE_FILES
        main.cpp)

#ADD_CUSTOM_TARGET(driver DEPENDS src/templates/index.html)

add_executable(${PROJECT_NAME}
        ${SOURCE_FILES})

add_sanitizers(${PROJECT_NAME})

create_git_version()

if(BUILD_TESTING)
    add_executable(explorer_regression_tests tests/regression_tests.cpp)
    target_include_directories(explorer_regression_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME explorer_regression_tests COMMAND explorer_regression_tests)
    add_executable(transaction_semantics_tests tests/transaction_semantics_tests.cpp)
    target_include_directories(transaction_semantics_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME transaction_semantics_tests COMMAND transaction_semantics_tests)
    add_test(NAME privacy_routes
            COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/privacy_routes.cmake)
endif()

configure_files(${CMAKE_CURRENT_SOURCE_DIR}/src/templates ${CMAKE_CURRENT_BINARY_DIR}/templates)
configure_files(${CMAKE_CURRENT_SOURCE_DIR}/src/templates/assets ${CMAKE_CURRENT_BINARY_DIR}/templates/assets)
configure_files(${CMAKE_CURRENT_SOURCE_DIR}/src/templates/assets/fonts ${CMAKE_CURRENT_BINARY_DIR}/templates/assets/fonts)
configure_files(${CMAKE_CURRENT_SOURCE_DIR}/src/templates/css ${CMAKE_CURRENT_BINARY_DIR}/templates/css)
configure_files(${CMAKE_CURRENT_SOURCE_DIR}/src/templates/partials ${CMAKE_CURRENT_BINARY_DIR}/templates/partials)
configure_files(${CMAKE_CURRENT_SOURCE_DIR}/src/templates/js ${CMAKE_CURRENT_BINARY_DIR}/templates/js)

set(LIBRARIES
        myxrm
        myext
        mstch
        wallet
        cryptonote_core
        epose
        cryptonote_basic
        cryptonote_protocol
        cryptonote_format_utils_basic
        blockchain_db
        device
        ${WALLET_CRYPTO}
        multisig
        daemonizer
        blocks
        lmdb
        ringct
        ringct_basic
        common
        mnemonics
        easylogging
        checkpoints
        cncrypto
        miniupnpc
        version
        epee
        hardforks
        net
        randomx
        sodium
        ${Boost_LIBRARIES}
        pthread
        unbound
        crypto
        ssl)

if(APPLE)
    set(LIBRARIES ${LIBRARIES} "-framework IOKit -framework Foundation")
else()
    set(LIBRARIES ${LIBRARIES} atomic)
endif()

find_library(UNWIND_LIBRARY unwind)
if (${UNWIND_LIBRARY} STREQUAL "UNWIND_LIBRARY-NOTFOUND")
  message (STATUS "unwind library not found")
  set (UNWIND_LIBRARY "")
else ()
  message (STATUS "Found unwind library: ${UNWIND_LIBRARY}")
endif ()

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" AND NOT WIN32)
    set(LIBRARIES ${LIBRARIES} ${UNWIND_LIBRARY})
endif()

if (WIN32)
    set(LIBRARIES ${LIBRARIES}
            wsock32
            ntdll
            ws2_32
            Iphlpapi
            )
else()
    set(LIBRARIES ${LIBRARIES} dl)
endif()

find_library(LIBUSB_LIBRARY usb-1.0)
if (LIBUSB_LIBRARY)
    set(LIBRARIES ${LIBRARIES} ${LIBUSB_LIBRARY})
endif()

find_library(LIBLZMA_LIBRARY lzma)
if (LIBLZMA_LIBRARY)
    set(LIBRARIES ${LIBRARIES} ${LIBLZMA_LIBRARY})
endif()

find_package(HIDAPI)
set(LIBRARIES ${LIBRARIES} ${HIDAPI_LIBRARIES})

target_link_libraries(${PROJECT_NAME} ${LIBRARIES})
if(BUILD_TESTING)
    target_link_libraries(transaction_semantics_tests ${LIBRARIES})
endif()
