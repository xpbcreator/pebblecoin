add_definitions(-DSTATICLIB)

file(GLOB_RECURSE COMMON common/*)
LIST(APPEND COMMON cryptonote_genesis_config.cpp)
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

source_group(common FILES ${COMMON})
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

add_library(common ${COMMON})
add_library(crypto ${CRYPTO})
add_library(cryptonote_core ${CRYPTONOTE_CORE})
add_executable(daemon ${DAEMON} ${P2P} ${CRYPTONOTE_PROTOCOL})
add_executable(connectivity_tool ${CONN_TOOL})
add_executable(simpleminer ${MINER})
target_link_libraries(daemon rpc cryptonote_core crypto common upnpc-static ${Boost_LIBRARIES})
target_link_libraries(connectivity_tool cryptonote_core crypto common ${Boost_LIBRARIES})
target_link_libraries(simpleminer cryptonote_core crypto common ${Boost_LIBRARIES})
add_library(rpc ${RPC})
add_library(wallet ${WALLET})
add_executable(simplewallet ${SIMPLEWALLET} cryptonote_core/miner_opt.cpp)
target_link_libraries(simplewallet wallet rpc cryptonote_core crypto common upnpc-static ${Boost_LIBRARIES})
add_dependencies(daemon version)
add_dependencies(rpc version)
add_dependencies(simplewallet version)

set_property(TARGET common crypto cryptonote_core rpc wallet PROPERTY FOLDER "libs")
set_property(TARGET daemon simplewallet connectivity_tool simpleminer PROPERTY FOLDER "prog")
set_property(TARGET daemon PROPERTY OUTPUT_NAME "pebblecoind")

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

  add_executable(qtwallet ${QTWALLET} ${QTWALLET_FORM_HEADERS} ${QTWALLET_PROTO_SOURCES} ${QTWALLET_PROTO_HEADERS} ${QTWALLET_QRC_SOURCES} ${QTWALLET_LANG_SOURCES} ${QTWALLET_TRANS_OUTFILE})
  target_link_libraries(qtwallet wallet rpc cryptonote_core crypto common upnpc-static ${Boost_LIBRARIES} ${QT_LIBRARIES} ${PROTOBUF_LIBRARY} ${OPENSSL_LIBRARIES})
  set_property(TARGET qtwallet PROPERTY OUTPUT_NAME "pebblecoin-qt")
  
  if (WIN32)
    TARGET_LINK_LIBRARIES(qtwallet shlwapi.lib) #for PathRemoveFileSpec()
  endif()
endif()
