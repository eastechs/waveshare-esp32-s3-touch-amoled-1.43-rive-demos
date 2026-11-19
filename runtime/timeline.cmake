# Build the pinned bridge with support for artboards authored with just a timeline.
# Only the generated translation unit is patched. The submodule stays pristine.
function(enable_rive_timelines target)
    get_target_property(sources "${target}" SOURCES)
    set(bridge "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../vendor/rive-thorvg/src/rive_thorvg.cpp")
    set(change "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/timeline.patch")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/rive_timeline.cpp")
    find_program(PATCH_PROGRAM patch REQUIRED)
    execute_process(COMMAND "${PATCH_PROGRAM}" -s -F 0 -o "${output}" "${bridge}" "${change}"
        RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "Unable to apply the timeline change to the pinned rive-thorvg bridge.")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${bridge}" "${change}")
    list(FILTER sources EXCLUDE REGEX "(^|/)rive_thorvg[.]cpp$")
    set_property(TARGET "${target}" PROPERTY SOURCES ${sources} "${output}")
endfunction()
