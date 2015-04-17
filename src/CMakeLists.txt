add_definitions(-DSTATICLIB)

file(GLOB_RECURSE EPEE ../contrib/epee/lib/* ../contrib/epee/include/*)
file(GLOB_RECURSE COMMON common/*)
LIST(APPEND COMMON cryptonote_genesis_config.cpp cryptonote_config.cpp)
file(GLOB_RECURSE CRYPTO_CORE crypto_core/*)
file(GLOB_RECURSE CRYPTO crypto/*)
file(GLOB_RECURSE CRYPTONOTE_CORE cryptonote_core/*)
file(GLOB_RECURSE CRYPTONOTE_PROTOCOL cryptonote_protocol/*)
file(GLOB_RECURSE DAEMON daemon/*)
file(GLOB_RECURSE P2P p2p/*)
file(GLOB_RECURSE RPC rpc/*)
file(GLOB_RECURSE SIMPLEWALLET simplewallet/*)
file(GLOB_RECURSE CONN_TOOL connectivity_tool/*)
file(GLOB_RECURSE WALLET wallet/*)
file(GLOB_RECURSE MINER miner/*)

source_group(epee FILES ${EPEE})
source_group(common FILES ${COMMON})
source_group(crypto_core FILES ${CRYPTO_CORE})
source_group(crypto FILES ${CRYPTO})
source_group(cryptonote_core FILES ${CRYPTONOTE_CORE})
source_group(cryptonote_protocol FILES ${CRYPTONOTE_PROTOCOL})
source_group(daemon FILES ${DAEMON})
source_group(p2p FILES ${P2P})
source_group(rpc FILES ${RPC})
source_group(simplewallet FILES ${SIMPLEWALLET})
source_group(connectivity-tool FILES ${CONN_TOOL})
source_group(wallet FILES ${WALLET})
source_group(simpleminer FILES ${MINER})

add_library(epee ${EPEE})
add_library(common ${COMMON})
add_library(crypto_core ${CRYPTO_CORE})
add_library(crypto ${CRYPTO})
add_library(cryptonote_core ${CRYPTONOTE_CORE})
add_library(rpc ${RPC})
add_library(wallet ${WALLET})
#target_link_libraries(cryptonote_core wallet)
target_link_libraries(wallet crypto_core crypto ${Boost_LIBRARIES})
add_executable(daemon ${DAEMON} ${P2P} ${CRYPTONOTE_PROTOCOL})
add_executable(connectivity_tool ${CONN_TOOL} ${P2P})
add_executable(simpleminer ${MINER})
add_executable(simplewallet ${SIMPLEWALLET})
target_link_libraries(daemon wallet rpc cryptonote_core wallet crypto common crypto_core epee upnpc-static ${Boost_LIBRARIES})
target_link_libraries(connectivity_tool cryptonote_core crypto common crypto_core epee ${Boost_LIBRARIES})
target_link_libraries(simpleminer cryptonote_core crypto common crypto_core epee ${Boost_LIBRARIES})
target_link_libraries(simplewallet wallet rpc cryptonote_core wallet crypto common crypto_core epee upnpc-static ${Boost_LIBRARIES})
add_dependencies(common version)
add_dependencies(daemon version)
add_dependencies(rpc version)
add_dependencies(simplewallet version)

set_property(TARGET common crypto_core crypto cryptonote_core rpc wallet PROPERTY FOLDER "libs")
set_property(TARGET daemon simplewallet connectivity_tool simpleminer PROPERTY FOLDER "prog")
set_property(TARGET daemon PROPERTY OUTPUT_NAME "pebblecoind")

set_property(SOURCE crypto_core/jh.c crypto_core/crypto-ops.c crypto_core/blake256.c crypto_core/hash-extra-skein.c PROPERTY COTIRE_EXCLUDED TRUE)
set_target_properties(daemon simplewallet PROPERTIES COTIRE_UNITY_LINK_LIBRARIES_INIT COPY_UNITY)
if (NOT USE_PCH)
  set_property(DIRECTORY . PROPERTY COTIRE_ENABLE_PRECOMPILED_HEADER FALSE)
endif()
set_property(TARGET common crypto crypto_core cryptonote_core rpc wallet daemon simplewallet PROPERTY
    COTIRE_PREFIX_HEADER_INCLUDE_PATH
    "${CMAKE_SOURCE_DIR}/contrib;${CMAKE_SOURCE_DIR}/external;${CMAKE_SOURCE_DIR}/include")
cotire(common crypto crypto_core cryptonote_core rpc wallet daemon simplewallet)
set_property(TARGET common_unity crypto_unity crypto_core_unity cryptonote_core_unity rpc_unity wallet_unity PROPERTY FOLDER "libs")
set_property(TARGET daemon_unity simplewallet_unity PROPERTY FOLDER "prog")
set_property(TARGET daemon_unity PROPERTY OUTPUT_NAME "pebblecoind")
add_dependencies(daemon_unity version)
add_dependencies(rpc_unity version)
add_dependencies(simplewallet_unity version)

# QT Wallet
find_package(Qt4 QUIET COMPONENTS QtCore QtGui QtNetwork QtTest)
if (QT4_FOUND)
  add_definitions( -DENABLE_WALLET )
  include(${QT_USE_FILE})
  add_definitions(${QT_DEFINITIONS})
  set(CMAKE_AUTOMOC TRUE)  

  include(FindProtobuf)
  find_package(Protobuf REQUIRED)
  include_directories(${PROTOBUF_INCLUDE_DIR})

  SET(OLD_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
  if(STATIC)
    SET(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
  endif()
  find_package(OpenSSL REQUIRED)
  SET(CMAKE_FIND_LIBRARY_SUFFIXES ${OLD_SUFFIXES})
  include_directories(${OPENSSL_INCLUDE_DIR})

  file(GLOB_RECURSE QTWALLET qtwallet/*)
  list(APPEND QTWALLET daemon/daemon_options.cpp)
  # Remove test files
  file(GLOB_RECURSE QTWALLET_TEST qtwallet/test/*)
  foreach(TEST_FILE ${QTWALLET_TEST})
    list(REMOVE_ITEM QTWALLET ${TEST_FILE})
  endforeach()

  # Conditionally remove mac-only files
  file(GLOB_RECURSE QTWALLET_MAC_ONLY qtwallet/mac*)
  if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    foreach(MAC_ONLY_FILE ${QTWALLET_MAC_ONLY})
      list(REMOVE_ITEM QTWALLET ${MAC_ONLY_FILE})
    endforeach()
  endif()

  source_group(qtwallet FILES ${QTWALLET})

  file(GLOB_RECURSE QTWALLET_FORM_FILES qtwallet/forms/*)
  file(GLOB_RECURSE QTWALLET_PROTO_FILES qtwallet/*.proto)
  #file(GLOB_RECURSE QTWALLET_QRC_FILES qtwallet/*.qrc)
  set(QTWALLET_QRC_FILES qtwallet/bitcoin.qrc)

  qt4_wrap_ui(QTWALLET_FORM_HEADERS ${QTWALLET_FORM_FILES})
  protobuf_generate_cpp(QTWALLET_PROTO_SOURCES QTWALLET_PROTO_HEADERS ${QTWALLET_PROTO_FILES})

  # Deal with the translations
  # Manually generate one qm file for each ts file
  file(GLOB_RECURSE QTWALLET_LANG_FILES qtwallet/locale/*.ts)
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/src/locale)
  set(QM_FILES)
  foreach(QTWALLET_LANG_FILE ${QTWALLET_LANG_FILES})
    get_filename_component(OUTPUT_FILE ${QTWALLET_LANG_FILE} NAME_WE)
    set(OUTPUT_FILE ${CMAKE_BINARY_DIR}/src/locale/${OUTPUT_FILE}.qm)
    #message( "Adding rule for ${OUTPUT_FILE}..." )
    add_custom_command(
      OUTPUT ${OUTPUT_FILE}
      COMMAND lrelease "${QTWALLET_LANG_FILE}" -qm "${OUTPUT_FILE}"
      MAIN_DEPENDENCY ${QTWALLET_LANG_FILE}
    )
    list(APPEND QM_FILES ${OUTPUT_FILE})
  endforeach()
  # Copy the qrc file containing above to the destination first
  set(QTWALLET_TRANS_STEM bitcoin_trans)
  set(QTWALLET_TRANS_SRCFILE ${CMAKE_CURRENT_SOURCE_DIR}/qtwallet/${QTWALLET_TRANS_STEM}.qrc)
  set(QTWALLET_TRANS_INFILE ${CMAKE_CURRENT_BINARY_DIR}/${QTWALLET_TRANS_STEM}.qrc)
  set(QTWALLET_TRANS_OUTFILE ${CMAKE_CURRENT_BINARY_DIR}/qrc_${QTWALLET_TRANS_STEM}.cxx)

  add_custom_command(
    OUTPUT ${QTWALLET_TRANS_INFILE}
    COMMAND ${CMAKE_COMMAND} -E copy ${QTWALLET_TRANS_SRCFILE} ${QTWALLET_TRANS_INFILE}
    MAIN_DEPENDENCY ${QTWALLET_TRANS_SRCFILE}
  )
  # Run the resource compiler on the dest file
  add_custom_command(
    OUTPUT ${QTWALLET_TRANS_OUTFILE}
    COMMAND ${QT_RCC_EXECUTABLE}
    ARGS ${rcc_options} -name ${QTWALLET_TRANS_STEM} -o ${QTWALLET_TRANS_OUTFILE} ${QTWALLET_TRANS_INFILE}
    MAIN_DEPENDENCY ${QTWALLET_TRANS_INFILE}
    DEPENDS ${QM_FILES}
  )

  qt4_add_resources(QTWALLET_QRC_SOURCES ${QTWALLET_QRC_FILES})
  include_directories(${CMAKE_CURRENT_BINARY_DIR}) # for the ui_*.h files
  include_directories(qtwallet) # for the ui_*.h files to include custom widgets
  include_directories(qtwallet/irc) # for the irc includes to work (was a separate module)

  add_executable(qtwallet ${QTWALLET} ${P2P} ${CRYPTONOTE_PROTOCOL} ${QTWALLET_FORM_HEADERS} ${QTWALLET_PROTO_SOURCES} ${QTWALLET_PROTO_HEADERS} ${QTWALLET_QRC_SOURCES} ${QTWALLET_LANG_SOURCES} ${QTWALLET_TRANS_OUTFILE})
  target_link_libraries(qtwallet wallet rpc cryptonote_core wallet crypto common crypto_core upnpc-static epee ${Boost_LIBRARIES} ${QT_LIBRARIES} ${PROTOBUF_LIBRARY} ${OPENSSL_LIBRARIES})
  set_property(TARGET qtwallet PROPERTY OUTPUT_NAME "pebblecoin-qt")
  
  if (WIN32)
    TARGET_LINK_LIBRARIES(qtwallet shlwapi.lib) #for PathRemoveFileSpec()
  endif()
  
  set_property(SOURCE ${QTWALLET_QRC_SOURCES} ${QTWALLET_TRANS_OUTFILE} qtwallet/irc/cv/gui/WindowManager.cpp PROPERTY COTIRE_EXCLUDED TRUE)
  set_target_properties(qtwallet PROPERTIES COTIRE_UNITY_LINK_LIBRARIES_INIT COPY_UNITY)
  cotire(qtwallet)
  add_dependencies(qtwallet_unity qtwallet_automoc)
endif()
