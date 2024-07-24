cmake_minimum_required (VERSION 3.16) 

set(CMAKE_CXX_STANDARD 17)
set(HEADERS ttmlgen.hpp parsemp4.hpp extractcaptions.hpp ${HEADERS})
set(SOURCES main.cpp ttmlgen.cpp parsemp4.cpp extractcaptions.cpp ${SOURCES})

add_executable(ttml_gen ${HEADERS} ${SOURCES})
set(EXEC_NAME ttml_gen)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-multichar")
target_link_libraries(ttml_gen)


