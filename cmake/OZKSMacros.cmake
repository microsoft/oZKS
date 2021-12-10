

# Set the C++ language version; we require C++17
macro(ozks_set_language target)
    target_compile_features(${target} PUBLIC cxx_std_17)
endmacro()

# Include a file to fetch thirdparty content
macro(ozks_fetch_thirdparty_content content_file)
    set(OZKS_FETCHCONTENT_BASE_DIR_OLD ${FETCHCONTENT_BASE_DIR})
    set(FETCHCONTENT_BASE_DIR ${OZKS_THIRDPARTY_DIR} CACHE STRING "" FORCE)
    include(${content_file})
    set(FETCHCONTENT_BASE_DIR ${OZKS_FETCHCONTENT_BASE_DIR_OLD} CACHE STRING "" FORCE)
    unset(OZKS_FETCHCONTENT_BASE_DIR_OLD)
endmacro()

# Set the VERSION property
macro(ozks_set_version target)
    set_target_properties(${target} PROPERTIES VERSION ${OZKS_VERSION})
endmacro()

# Set the library filename to reflect version
macro(ozks_set_version_filename target)
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME ${target}-${OZKS_VERSION_MAJOR}.${OZKS_VERSION_MINOR})
endmacro()

# Set the SOVERSION property
macro(ozks_set_soversion target)
    set_target_properties(${target} PROPERTIES
        SOVERSION ${OZKS_VERSION_MAJOR}.${OZKS_VERSION_MINOR})
endmacro()

# Set include directories for build and install interfaces
macro(ozks_set_include_directories target)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${OZKS_INCLUDES_DIR}>
        $<INSTALL_INTERFACE:${OZKS_INCLUDES_INSTALL_DIR}>)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:${OZKS_INCLUDES_INSTALL_DIR}>)
endmacro()

# Link a thread library
macro(ozks_link_threads target)
    # Require thread library
    if(NOT TARGET Threads::Threads)
        set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
        set(THREADS_PREFER_PTHREAD_FLAG TRUE)
        find_package(Threads REQUIRED)
    endif()

    # Link Threads
    target_link_libraries(${target} PUBLIC Threads::Threads)
endmacro()

# Include target to given export
macro(ozks_install_target target export)
    install(TARGETS ${target} EXPORT ${export}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endmacro()

# Manually combine archives, using ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} to keep temporary files.
macro(ozks_combine_archives target dependency)
    if(MSVC)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND lib.exe /OUT:$<TARGET_FILE:${target}> $<TARGET_FILE:${target}> $<TARGET_FILE:${dependency}>
            DEPENDS $<TARGET_FILE:${target}> $<TARGET_FILE:${dependency}>
            WORKING_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
    else()
        if(CMAKE_HOST_WIN32)
            get_filename_component(CXX_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
            set(AR_CMD_PATH "${CXX_DIR}/llvm-ar.exe")
            file(TO_NATIVE_PATH "${AR_CMD_PATH}" AR_CMD_PATH)
            set(DEL_CMD "del")
            set(DEL_CMD_OPTS "")
        else()
            set(AR_CMD_PATH "ar")
            set(DEL_CMD "rm")
            set(DEL_CMD_OPTS "-rf")
        endif()
        if(EMSCRIPTEN)
            set(AR_CMD_PATH "emar")
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${AR_CMD_PATH}" x $<TARGET_FILE:${target}>
            COMMAND "${AR_CMD_PATH}" x $<TARGET_FILE:${dependency}>
            COMMAND "${AR_CMD_PATH}" rcs $<TARGET_FILE:${target}> *.o
            COMMAND ${DEL_CMD} ${DEL_CMD_OPTS} *.o
            WORKING_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
    endif()
endmacro()

# Add secure compile options
macro(ozks_set_secure_compile_options target scope)
    if(MSVC)
        # Build debug symbols for static analysis tools
        target_link_options(${target} ${scope} /DEBUG)

        # Control Flow Guard / Spectre
        target_compile_options(${target} ${scope} /guard:cf)
        target_compile_options(${target} ${scope} /Qspectre)
        target_link_options(${target} ${scope} /guard:cf)
        target_link_options(${target} ${scope} /DYNAMICBASE)
    endif()
endmacro()
