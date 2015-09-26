#
# use_cxx_standard(VERSIONS <version>...
#                  [REQUIRED])
#
# Find the first supported CXX version in the VERSIONS argument, and use it to
# initialize the global CMAKE_CXX_STANDARD variable.
#
include(CMakeParseArguments)
function(USE_CXX_STANDARD)
	cmake_parse_arguments(USE_CXX_STANDARD "REQUIRED" "" "VERSIONS" ${ARGN})

	if(NOT USE_CXX_STANDARD_VERSIONS)
		message(FATAL_ERROR "use_cxx_standard() missing VERSIONS argument")
	endif()

	foreach(VERSION IN LISTS USE_CXX_STANDARD_VERSIONS)
		check_cxx_standard(${VERSION} CXX_SUPPORTED)
		if(CXX_SUPPORTED)
			set(CMAKE_CXX_STANDARD ${VERSION} PARENT_SCOPE)
			return()
		endif()
	endforeach()

	if(NOT USE_CXX_STANDARD_REQUIRED)
		return()
	endif()

	string(REPLACE ";" " or " VERSIONS "${USE_CXX_STANDARD_VERSIONS}")
	message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER} does not support C++ versions ${VERSIONS}.")
endfunction()

#
# check_cxx_standard(VERSION VARIABLE)
#
# Try to compile with CMAKE_CXX_STANDARD=VERSION.
#
macro(CHECK_CXX_STANDARD VERSION VARIABLE)
	set(CHECK_CXX_STANDARD_MESSAGE "Detecting CXX${VERSION} support")
	message(STATUS ${CHECK_CXX_STANDARD_MESSAGE})
	try_compile(${VARIABLE}
		${CMAKE_BINARY_DIR}
		${CMAKE_ROOT}/Modules/DummyCXXFile.cxx
		CMAKE_FLAGS -DCMAKE_CXX_STANDARD=${VERSION} #-DCOMPILE_DEFINITIONS:STRING="-fpoo"
		OUTPUT_VARIABLE OUTPUT)
	if(${VARIABLE})
		message(STATUS "${CHECK_CXX_STANDARD_MESSAGE} - yes")
		file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
			"${CHECK_CXX_STANDARD_MESSAGE} passed with "
			"the following output:\n${OUTPUT}\n\n")
	else()
		message(STATUS "${CHECK_CXX_STANDARD_MESSAGE} - no")
		file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
			"${CHECK_CXX_STANDARD_MESSAGE} failed with "
			"the following output:\n${OUTPUT}\n\n")
	endif()
endmacro()
