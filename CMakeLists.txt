# symploke: linked data — the terms of RDF, its documents (N-Triples, N-Quads, Turtle, TriG), a store indexed every way with
# changes that undo, queries in SPARQL's core, entailment by RDFS and part of OWL 2 RL with derivations kept, SHACL's shapes,
# and graphs compared up to their blank nodes.
#
# Every module is a header-only target symploke::<module>. symploke uses the targets of morphe (canonical forms), apodeixis
# (verdicts) and calx (its checks): inside the infrastructure, the siblings under foundations/ (each added here when no larger
# project has added it). Built on its own, symploke builds its checks (SYMPLOKE_CHECKS); added to a larger project, only when
# asked.

cmake_minimum_required(VERSION 3.20)
project(symploke VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(SYMPLOKE_TOP_LEVEL ON)
else()
  set(SYMPLOKE_TOP_LEVEL OFF)
endif()
option(SYMPLOKE_CHECKS "Build symploke's checks" ${SYMPLOKE_TOP_LEVEL})

if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# symploke_sibling(<directory> <a target it defines>): the sibling library, added once.
function(symploke_sibling directory target)
  if(NOT TARGET ${target})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../${directory}/CMakeLists.txt)
      add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../${directory} ${CMAKE_BINARY_DIR}/${directory})
    else()
      message(FATAL_ERROR "symploke needs ${directory}: build it inside the infrastructure (foundations/${directory}).")
    endif()
  endif()
endfunction()

symploke_sibling(calx calx::linalg)
symploke_sibling(morphe morphe::formal)
symploke_sibling(apodeixis apodeixis::kernel)
if(SYMPLOKE_CHECKS)
  enable_testing()
endif()

# symploke_module(<name> [LIBRARIES ...] [CHECK_LIBRARIES ...]): the target symploke::<name> and, with the checks,
# check_symploke_<name>, which may use more libraries than the module does.
function(symploke_module name)
  cmake_parse_arguments(ARG "" "" "LIBRARIES;CHECK_LIBRARIES" ${ARGN})
  add_library(symploke_${name} INTERFACE)
  add_library(symploke::${name} ALIAS symploke_${name})
  target_include_directories(symploke_${name} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/${name}/include)
  target_compile_features(symploke_${name} INTERFACE cxx_std_20)
  target_link_libraries(symploke_${name} INTERFACE ${ARG_LIBRARIES})
  if(SYMPLOKE_CHECKS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${name}/tests/check_${name}.cpp)
    calx_add_check(check_symploke_${name} SOURCES ${name}/tests/check_${name}.cpp LIBRARIES symploke::${name} calx::random ${ARG_CHECK_LIBRARIES})
  endif()
endfunction()

# The modules in their order (decision 0003).
symploke_module(terms)
symploke_module(syntax LIBRARIES symploke::terms)
symploke_module(store LIBRARIES symploke::terms CHECK_LIBRARIES symploke::syntax)
symploke_module(query LIBRARIES symploke::store symploke::syntax)
symploke_module(entailment LIBRARIES symploke::store CHECK_LIBRARIES symploke::syntax morphe::combinatorics)
symploke_module(shapes LIBRARIES symploke::query CHECK_LIBRARIES symploke::syntax)
symploke_module(canonical LIBRARIES symploke::terms morphe::combinatorics apodeixis::kernel CHECK_LIBRARIES symploke::syntax)
