cmake_minimum_required(VERSION 3.20)

project(omarchy-ham VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core)

add_library(omarchy_ham STATIC
  include/omarchy_ham/profile_watcher.h
  src/profile.cpp
  src/profile_store.cpp
  src/profile_watcher.cpp
  src/maidenhead.cpp
  src/gps.cpp
  src/pipewire.cpp
)
target_include_directories(omarchy_ham
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(omarchy_ham PUBLIC Qt6::Core)
target_compile_features(omarchy_ham PUBLIC cxx_std_20)
set_target_properties(omarchy_ham PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(omarchy_ham PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(omarchy-ham cli/main.cpp)
target_link_libraries(omarchy-ham PRIVATE omarchy_ham)
target_compile_features(omarchy-ham PRIVATE cxx_std_20)
target_compile_definitions(omarchy-ham PRIVATE OMARCHY_HAM_VERSION="${PROJECT_VERSION}")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(omarchy-ham PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS omarchy_ham omarchy-ham
  EXPORT omarchy_hamTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES README.md SPEC.md GLOSSARY.md LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})

set(package_install_dir ${CMAKE_INSTALL_LIBDIR}/cmake/omarchy_ham)
configure_package_config_file(
  cmake/omarchy_ham-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/omarchy_ham-config.cmake
  INSTALL_DESTINATION ${package_install_dir}
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/omarchy_ham-config-version.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/omarchy_ham-config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/omarchy_ham-config-version.cmake
  DESTINATION ${package_install_dir}
)
install(EXPORT omarchy_hamTargets
  NAMESPACE omarchy_ham::
  DESTINATION ${package_install_dir}
)

configure_file(cmake/omarchy-ham.pc.in ${CMAKE_CURRENT_BINARY_DIR}/omarchy-ham.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/omarchy-ham.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

include(CTest)
if(BUILD_TESTING)
  add_executable(omarchy-ham-tests tests/test_main.cpp)
  target_link_libraries(omarchy-ham-tests PRIVATE omarchy_ham)
  target_compile_features(omarchy-ham-tests PRIVATE cxx_std_20)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(omarchy-ham-tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME unit COMMAND omarchy-ham-tests)

  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME cli
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.py
              $<TARGET_FILE:omarchy-ham>
    )
  endif()
endif()
