
macro(configure_files srcDir destDir)
    message(STATUS "Configuring directory ${destDir}")
    make_directory(${destDir})

    file(GLOB templateFiles RELATIVE ${srcDir} ${srcDir}/*)
    foreach(templateFile ${templateFiles})
        set(srcTemplatePath ${srcDir}/${templateFile})
        if(NOT IS_DIRECTORY ${srcTemplatePath})
            message(STATUS "Configuring file ${templateFile}")
            configure_file(
                    ${srcTemplatePath}
                    ${destDir}/${templateFile}
                    @ONLY)
        endif(NOT IS_DIRECTORY ${srcTemplatePath})
    endforeach(templateFile)
endmacro(configure_files)

macro(create_git_version)
    set(EXPLORER_SOURCE_SHA "" CACHE STRING "Immutable explorer source SHA")
    set(EXPLORER_SOURCE_DATE "" CACHE STRING "Explorer source date")
    set(EXPLORER_SOURCE_BRANCH "" CACHE STRING "Explorer source branch")
    set(QWC_SOURCE_SHA "unknown" CACHE STRING "Compatible Qwertycoin core source SHA")

    if(EXPLORER_SOURCE_SHA)
        set(GIT_COMMIT_HASH "${EXPLORER_SOURCE_SHA}")
    else()
        execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_COMMIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    if(EXPLORER_SOURCE_DATE)
        set(GIT_COMMIT_DATETIME "${EXPLORER_SOURCE_DATE}")
    else()
        execute_process(COMMAND git log -1 --format=%cd --date=short WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_COMMIT_DATETIME OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    if(EXPLORER_SOURCE_BRANCH)
        set(GIT_BRANCH_NAME "${EXPLORER_SOURCE_BRANCH}")
    else()
        execute_process(COMMAND git rev-parse --abbrev-ref HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_BRANCH_NAME OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    set(GIT_BRANCH "${GIT_BRANCH_NAME}")



    configure_file(
            ${CMAKE_SOURCE_DIR}/src/version.h.in
            ${CMAKE_BINARY_DIR}/gen/version.h
    )

    include_directories(${CMAKE_BINARY_DIR}/gen)

endmacro(create_git_version)
