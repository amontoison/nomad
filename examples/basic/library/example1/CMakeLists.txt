add_executable(example1_lib.exe example1_lib.cpp )

target_include_directories(example1_lib.exe PRIVATE
    ${CMAKE_SOURCE_DIR}/src)

set_target_properties(example1_lib.exe PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")


if(OpenMP_CXX_FOUND)
    target_link_libraries(example1_lib.exe PUBLIC nomadAlgos nomadUtils nomadEval OpenMP::OpenMP_CXX)
else()
    target_link_libraries(example1_lib.exe PUBLIC nomadAlgos nomadUtils nomadEval)
endif()

# installing executables and libraries
install(TARGETS example1_lib.exe
    RUNTIME DESTINATION ${CMAKE_CURRENT_SOURCE_DIR} )

# Add a test for this example
if(BUILD_TESTS MATCHES ON)
    message(STATUS "    Add example library test 1") 

    # Can run this test after install
    add_test(NAME Example1BasicLib
	COMMAND ${CMAKE_BINARY_DIR}/examples/runExampleTest.sh ./example1_lib.exe
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
    )
endif()
