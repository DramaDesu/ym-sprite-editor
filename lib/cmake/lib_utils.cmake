function(declare_cpp_library project standart)
	
	project (${project})

	set(CMAKE_CXX_STANDARD ${standart})
	set(CMAKE_CXX_STANDARD_REQUIRED ON)

	add_library(${project} ${ARGN})

	target_include_directories(${project} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

endfunction()