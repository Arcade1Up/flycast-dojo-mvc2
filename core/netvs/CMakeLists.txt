target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/audio_dsp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lowpass2.cpp

#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/common/k_framecache.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/icrc32.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/ikcp.c
#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/imd5.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/mempage.cpp

#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/netvs_main.cpp

#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/netvs_autogame.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/netvs_connect.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/netvs_nat_type.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/netvs_record.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/netvs_rollback_state.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/netvs_run_frame.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/netvs_tcp_watch.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/okaiclient.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/p2p_rudp_api.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/p2p_tcp_api.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/netvs/p2p_tcp_exchange.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/emulator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/emulator_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/input_thread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ioapi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rijndael_alg_fst.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ticks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unzip.c)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/netvs)

#add_subdirectory(netvs)
#target_link_libraries(${PROJECT_NAME} PRIVATE netvs-enc)

if (UNIX AND NOT ANDROID)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/netvs/linux/libnetvs-enc.a)
elseif(ANDROID)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/netvs/android/libnetvs-enc.a)
endif()