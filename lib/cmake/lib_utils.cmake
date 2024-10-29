function(declare_cpp_library project standart)
	
	project (${project})

	set(CMAKE_CXX_STANDARD ${standart})
	set(CMAKE_CXX_STANDARD_REQUIRED ON)

	add_library(${project} ${ARGN})

	target_include_directories(${project} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	target_include_directories(${project} INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

    include(GNUInstallDirs)

    install(TARGETS ${project}
        EXPORT ${project}-targets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

    install(EXPORT ${project}-targets
        FILE ${project}-targets.cmake
        NAMESPACE ${project}::
        DESTINATION share/${project}
    )

    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    include(CMakePackageConfigHelpers)

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${project}-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${project}-config.cmake"
        @ONLY
    )

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${project}-config.cmake"
        DESTINATION share/${project}
    )

endfunction()