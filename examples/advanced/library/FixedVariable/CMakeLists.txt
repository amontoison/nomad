add_executable(fixedVariable.exe fixedVariable.cpp )

target_include_directories(fixedVariable.exe PRIVATE
    ${CMAKE_SOURCE_DIR}/src)

set_target_properties(fixedVariable.exe PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")


if(OpenMP_CXX_FOUND)
    target_link_libraries(fixedVariable.exe PUBLIC nomadAlgos nomadUtils nomadEval OpenMP::OpenMP_CXX)
else()
    target_link_libraries(fixedVariable.exe PUBLIC nomadAlgos nomadUtils nomadEval)
endif()

# installing executables and libraries
install(TARGETS fixedVariable.exe
    RUNTIME DESTINATION ${CMAKE_CURRENT_SOURCE_DIR} )

# Add a test for this example
if(BUILD_TESTS MATCHES ON)
    message(STATUS "    Add example test for fixed variables") 

    # Can run this test after install
    add_test(NAME ExampleAdvancedFixedVariables
	COMMAND ${CMAKE_BINARY_DIR}/examples/runExampleTest.sh ./fixedVariable.exe
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
    )
endif()
