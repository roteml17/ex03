cmake_minimum_required(VERSION 3.10) 
project(funCoin) 
set(CMAKE_CXX_STANDARD 11)
#Add the source files
set(SOURCES server.cpp)
#Add the header files
#Create the executable
add_executable(funCoin ${SOURCES} ${HEADERS})
#Link the zlib library
find_package(ZLIB REQUIRED) 
target_link_libraries(funCoin ${ZLIB_LIBRARIES})