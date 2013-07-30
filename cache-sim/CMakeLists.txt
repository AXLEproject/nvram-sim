project (cache)

#add_subdirectory (Demo)

#add_library (Hello hello.cxx)

#include_directories (${HELLO_SOURCE_DIR}/Hello)
include_directories(${cache_SOURCE_DIR})

#link_directories (${HELLO_BINARY_DIR}/Hello)

SET(CMAKE_BUILD_TYPE Debug)
#SET(CMAKE_BUILD_TYPE Release)

#add_definitions("-fmudflap -funwind-tables -rdynamic") 
add_definitions("-Wall")
add_executable (cache main.cpp cache.cpp logger.cpp)
#target_link_libraries (cache dl)

