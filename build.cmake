if(WIN32)
    # Check if Qt6 is already provided via CMAKE_PREFIX_PATH, Qt6_DIR, or environment
    if(NOT CMAKE_PREFIX_PATH AND NOT Qt6_DIR AND NOT DEFINED ENV{QTDIR} AND NOT DEFINED ENV{CMAKE_PREFIX_PATH})
        # Check standard default installation locations for Qt on Windows
        file(GLOB QT_CANDIDATES
            "C:/Qt/6.*/mingw_64"
            "C:/Qt6/6.*/mingw_64"
        )
        if(QT_CANDIDATES)
            list(SORT QT_CANDIDATES ORDER DESCENDING)
            list(GET QT_CANDIDATES 0 QT_DETECTED)
            message(STATUS "Auto-detected Qt6 at: ${QT_DETECTED}")
            list(APPEND CMAKE_PREFIX_PATH "${QT_DETECTED}")
        endif()
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -W -Wall -pedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -pedantic")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -ggdb")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
endif()

set(QT_FORCE_CMP0156_TO_VALUE NEW)

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
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(ALL_AUTHORS)
        string(REPLACE "\n" ";" AUTHOR_LIST "${ALL_AUTHORS}")
        list(REMOVE_DUPLICATES AUTHOR_LIST)
        list(LENGTH AUTHOR_LIST NUM_AUTHORS)
        if(NUM_AUTHORS GREATER 10)
            list(SUBLIST AUTHOR_LIST 0 10 AUTHOR_LIST)
        endif()
        string(JOIN ", " GIT_AUTHORS ${AUTHOR_LIST})
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
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(ALL_AUTHORS_MAIL)
        string(REPLACE "\n" ";" MAIL_LIST "${ALL_AUTHORS_MAIL}")
        list(REMOVE_DUPLICATES MAIL_LIST)
        list(LENGTH MAIL_LIST NUM_MAILS)
        if(NUM_MAILS GREATER 10)
            list(SUBLIST MAIL_LIST 0 10 MAIL_LIST)
        endif()
        string(JOIN ", " GIT_AUTHORS_MAIL ${MAIL_LIST})
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
