# DB Connection Pool JSON layer for web apps;

cmake_minimum_required (VERSION 2.6)

project (mpool)

set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set (CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

file (GLOB PROJECT_ALL_SRCS src/*.cpp)

add_executable (mpool src/main.cpp)

add_library (serverexception SHARED src/ServerException.cpp)
add_library (server SHARED src/Server.cpp)
add_library (client SHARED src/Client.cpp)
add_library (manager SHARED src/Manager.cpp)
add_library (dbpool SHARED src/DBPool.cpp)

set_target_properties(serverexception PROPERTIES VERSION 0.0.7)
set_target_properties(server PROPERTIES VERSION 0.0.7)
set_target_properties(client PROPERTIES VERSION 0.0.7)
set_target_properties(manager PROPERTIES VERSION 0.0.7)
set_target_properties(dbpool PROPERTIES VERSION 0.0.7)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_SOURCE_DIR}/cmake/Modules")

find_package (Threads REQUIRED)

if(CMAKE_THREAD_LIBS_INIT)
 	#target_link_libraries(mpool PUBLIC ${CMAKE_THREAD_LIBS_INIT})
	#target_link_libraries(client PUBLIC ${CMAKE_THREAD_LIBS_INIT})
	#target_link_libraries(server PUBLIC ${CMAKE_THREAD_LIBS_INIT})
	#target_link_libraries(manager PUBLIC ${CMAKE_THREAD_LIBS_INIT})
endif()

find_package (JSONCPP REQUIRED)

if (JSONCPP_FOUND)
	target_link_libraries (mpool ${JSONCPP_LIBRARY})
	target_link_libraries (client ${JSONCPP_LIBRARY})
	target_link_libraries (server ${JSONCPP_LIBRARY})
	target_link_libraries (manager ${JSONCPP_LIBRARY})
endif ()

find_package (MYSQL REQUIRED)

if (MySQL_FIND)
	target_include_directories (mpool PUBLIC ${MYSQL_INCLUDE_DIR})
	target_include_directories (client PUBLIC ${MYSQL_INCLUDE_DIR})
	target_include_directories (server PUBLIC ${MYSQL_INCLUDE_DIR})
	target_include_directories (manager PUBLIC ${MYSQL_INCLUDE_DIR})
	target_include_directories (dbpool PUBLIC ${MYSQL_INCLUDE_DIR})
	target_link_libraries (mpool ${MYSQL_LIB_DIR})
	target_link_libraries (client ${MYSQL_LIB_DIR})
	target_link_libraries (server ${MYSQL_LIB_DIR})
	target_link_libraries (manager ${MYSQL_LIB_DIR})
	target_link_libraries (dbpool ${MYSQL_LIB_DIR})
endif ()

target_link_libraries (dbpool ${MYSQL_CLIENT_LIBS})
target_link_libraries (server client)
target_link_libraries (mpool client server manager serverexception dbpool)

set (CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb -DDEBUG")  
set (CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall") 

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	set (CXXFLAGS ${CMAKE_CXX_FLAGS_DEBUG})
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
	set (CXXFLAGS ${CMAKE_CXX_FLAGS_RELEASE})
endif()

install(TARGETS mpool client server client manager dbpool serverexception 
	RUNTIME DESTINATION bin 
	LIBRARY DESTINATION lib)

install(DIRECTORY conf.d DESTINATION etc) 