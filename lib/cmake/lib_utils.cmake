function(declare_cpp_library project standart lib_sources)
	
	project (${project})

	set(CMAKE_CXX_STANDARD ${standart})
	set(CMAKE_CXX_STANDARD_REQUIRED ON)

	add_library(${project} ${lib_sources})

	target_include_directories(${project} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

endfunction()