# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

# @brief Downloads a third-party git dependency into
#  ${CMAKE_CURRENT_SOURCE_DIR}/<NAME>, unless that directory already exists (in which case the
#  function does nothing, so re-configuring does not re-download the dependency).
#
#  Either TAG or COMMIT must be provided, but not both:
#  - TAG performs a shallow `git clone --branch <TAG> --depth 1`, for repositories that expose
#    proper tags/branches.
#  - COMMIT performs an `init` + `fetch --depth 1` + `checkout`, for repositories that do not
#    expose tags and must be pinned to a specific commit.
#
# @param NAME       Name of the dependency. Used as the destination folder name and as the prefix
#                   of the resulting <NAME>_SOURCE_DIR variable.
# @param REPOSITORY Git repository URL to clone/fetch from.
# @param TAG        Git tag or branch to pin. Mutually exclusive with COMMIT.
# @param COMMIT     Git commit hash to pin. Mutually exclusive with TAG.
#
# @return Sets <NAME>_SOURCE_DIR in the caller's scope to the directory the dependency was
#         downloaded into (${CMAKE_CURRENT_SOURCE_DIR}/<NAME>).
function(tca_fetch_git_dependency)
    set(TCA_ENGINE_FGD_ONE_VALUE_ARGS NAME REPOSITORY TAG COMMIT)
    cmake_parse_arguments(TCA_ENGINE_FGD "" "${TCA_ENGINE_FGD_ONE_VALUE_ARGS}" "" ${ARGN})

    if(NOT TCA_ENGINE_FGD_NAME)
        message(FATAL_ERROR "tca_fetch_git_dependency: NAME is required")
    endif()

    if(NOT TCA_ENGINE_FGD_REPOSITORY)
        message(FATAL_ERROR "tca_fetch_git_dependency: REPOSITORY is required")
    endif()

    if(TCA_ENGINE_FGD_TAG AND TCA_ENGINE_FGD_COMMIT)
        message(FATAL_ERROR "tca_fetch_git_dependency: TAG and COMMIT are mutually exclusive")
    endif()

    if(NOT TCA_ENGINE_FGD_TAG AND NOT TCA_ENGINE_FGD_COMMIT)
        message(FATAL_ERROR "tca_fetch_git_dependency: either TAG or COMMIT is required")
    endif()

    set(TCA_ENGINE_FGD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${TCA_ENGINE_FGD_NAME})

    if(NOT EXISTS ${TCA_ENGINE_FGD_DIR})
        find_package(Git REQUIRED)

        if(TCA_ENGINE_FGD_TAG)
            message(STATUS "third-party: downloading ${TCA_ENGINE_FGD_NAME} ${TCA_ENGINE_FGD_TAG} into ${TCA_ENGINE_FGD_DIR}...")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} clone --branch ${TCA_ENGINE_FGD_TAG} --depth 1
                    ${TCA_ENGINE_FGD_REPOSITORY} ${TCA_ENGINE_FGD_DIR}
                RESULT_VARIABLE TCA_ENGINE_FGD_CLONE_RESULT
            )

            if(NOT TCA_ENGINE_FGD_CLONE_RESULT EQUAL 0)
                message(FATAL_ERROR "third-party: failed to download ${TCA_ENGINE_FGD_NAME} from ${TCA_ENGINE_FGD_REPOSITORY}")
            endif()
        else()
            message(STATUS "third-party: downloading ${TCA_ENGINE_FGD_NAME} (${TCA_ENGINE_FGD_COMMIT}) into ${TCA_ENGINE_FGD_DIR}...")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} init --quiet ${TCA_ENGINE_FGD_DIR}
                RESULT_VARIABLE TCA_ENGINE_FGD_INIT_RESULT
            )

            if(NOT TCA_ENGINE_FGD_INIT_RESULT EQUAL 0)
                message(FATAL_ERROR "third-party: failed to initialize ${TCA_ENGINE_FGD_NAME} repository at ${TCA_ENGINE_FGD_DIR}")
            endif()

            execute_process(
                COMMAND ${GIT_EXECUTABLE} fetch --depth 1 ${TCA_ENGINE_FGD_REPOSITORY} ${TCA_ENGINE_FGD_COMMIT}
                WORKING_DIRECTORY ${TCA_ENGINE_FGD_DIR}
                RESULT_VARIABLE TCA_ENGINE_FGD_FETCH_RESULT
            )

            if(NOT TCA_ENGINE_FGD_FETCH_RESULT EQUAL 0)
                message(FATAL_ERROR "third-party: failed to download ${TCA_ENGINE_FGD_NAME} from ${TCA_ENGINE_FGD_REPOSITORY}")
            endif()

            execute_process(
                COMMAND ${GIT_EXECUTABLE} checkout --quiet FETCH_HEAD
                WORKING_DIRECTORY ${TCA_ENGINE_FGD_DIR}
                RESULT_VARIABLE TCA_ENGINE_FGD_CHECKOUT_RESULT
            )

            if(NOT TCA_ENGINE_FGD_CHECKOUT_RESULT EQUAL 0)
                message(FATAL_ERROR "third-party: failed to checkout ${TCA_ENGINE_FGD_NAME} commit ${TCA_ENGINE_FGD_COMMIT}")
            endif()
        endif()
    endif()

    set(${TCA_ENGINE_FGD_NAME}_SOURCE_DIR ${TCA_ENGINE_FGD_DIR} PARENT_SCOPE)
endfunction()
