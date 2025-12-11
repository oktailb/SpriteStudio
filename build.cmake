if(WIN32)
    set(CMAKE_PREFIX_PATH "./cots/")
    set(CMAKE_LIBRARY_PATH "./cots/")
endif(WIN32)

#set(CMAKE_BUILD_TYPE Release)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -W -Wall -pedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -W -Wall -pedantic")
set(QT_FORCE_CMP0156_TO_VALUE NEW)

if (WIN32)
    message(AUTHOR_WARNING "PLEASE CONFIGURE IT REGARDING YOUR CONFIG")
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH};C:\\Qt6\\6.10.1\\mingw_64\\lib\\cmake\\Qt6)
endif()

string(TIMESTAMP CMAKE_BUILD_DATE "%Y-%m-%d %H:%M:%S")

function(get_git_info)
    execute_process(
        COMMAND git rev-parse --is-inside-work-tree
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE IS_GIT_REPO
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(NOT IS_GIT_REPO STREQUAL "true")
        set(GIT_INFO_AVAILABLE false PARENT_SCOPE)
        set(GIT_BRANCH "N/A (not a git repository)" PARENT_SCOPE)
        set(GIT_COMMIT_HASH "N/A" PARENT_SCOPE)
        set(GIT_COMMIT_DATE "N/A" PARENT_SCOPE)
        set(GIT_LAST_AUTHOR "N/A" PARENT_SCOPE)
        set(GIT_AUTHORS "Aucune information Git disponible" PARENT_SCOPE)
        return()
    endif()

    set(GIT_INFO_AVAILABLE true PARENT_SCOPE)

    execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND git log -1 --format=%cd --date=short
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND git log -1 --format=%an
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_LAST_AUTHOR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND git log --format=%an
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE ALL_AUTHORS
    )

    if(ALL_AUTHORS)
        execute_process(
            COMMAND echo "${ALL_AUTHORS}"
            COMMAND sort
            COMMAND uniq
            COMMAND head -10
            COMMAND tr "\\n" ","
            COMMAND sed "s/,$//"
            COMMAND sed "s/^,//"
            OUTPUT_VARIABLE GIT_AUTHORS
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    else()
        set(GIT_AUTHORS "Aucun auteur trouvé")
    endif()

    execute_process(
        COMMAND git log -1 --format=%ae
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_LAST_EMAIL
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND git log --format=%ae
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE ALL_AUTHORS_MAIL
    )

    if(ALL_AUTHORS)
        execute_process(
            COMMAND echo "${ALL_AUTHORS_MAIL}"
            COMMAND sort
            COMMAND uniq
            COMMAND head -10
            COMMAND tr "\\n" ","
            COMMAND sed "s/,$//"
            COMMAND sed "s/^,//"
            OUTPUT_VARIABLE GIT_AUTHORS_MAIL
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    else()
        set(GIT_AUTHORS_MAIL "Aucun mail trouvé")
    endif()

    set(GIT_BRANCH ${GIT_BRANCH} PARENT_SCOPE)
    set(GIT_COMMIT_HASH ${GIT_COMMIT_HASH} PARENT_SCOPE)
    set(GIT_COMMIT_DATE ${GIT_COMMIT_DATE} PARENT_SCOPE)
    set(GIT_LAST_AUTHOR ${GIT_LAST_AUTHOR} PARENT_SCOPE)
    set(GIT_AUTHORS ${GIT_AUTHORS} PARENT_SCOPE)
    set(GIT_AUTHORS_MAIL ${GIT_AUTHORS_MAIL} PARENT_SCOPE)
    set(GIT_LAST_EMAIL ${GIT_LAST_EMAIL} PARENT_SCOPE)

endfunction()

get_git_info()

if(GIT_INFO_AVAILABLE)
    message(STATUS "Git branch: ${GIT_BRANCH}")
    message(STATUS "Git commit: ${GIT_COMMIT_HASH}")
    message(STATUS "Git authors: ${GIT_AUTHORS}")
    message(STATUS "Git mails: ${GIT_AUTHORS_MAIL}")
else()
    message(STATUS "No Git information available")
endif()
