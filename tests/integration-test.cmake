# Check that we have the variables we need.
foreach(VAR NETWORK_MONITOR_EXE TEST_CLIENT_EXE)
    if(NOT DEFINED ${VAR})
        message(FATAL_ERROR "Undefined: ${VAR}")
    endif()
    message("Exe: ${${VAR}}")
endforeach()

# Execute the network monitor and the test client executables in parallel.
# execute_process pipes the stdout of the first command into the stdin of the
# next one. We have setup TEST_CLIENT_EXE to echo every stdin message it
# receives.
execute_process(
    COMMAND "${NETWORK_MONITOR_EXE}"
    COMMAND "${TEST_CLIENT_EXE}"
    RESULTS_VARIABLE RESULTS
)

# Check the test exit values.
foreach(RES ${RESULTS})
    if(RES)
        message(FATAL_ERROR "Test failed: RES = ${RES}")
    endif()
endforeach() 