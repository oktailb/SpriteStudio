if(WIN32)
    set(CMAKE_PREFIX_PATH "./cots/")
    set(CMAKE_LIBRARY_PATH "./cots/")
endif(WIN32)

#set(CMAKE_BUILD_TYPE Release)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O5 -W -Wall -pedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O5 -W -Wall -pedantic")
