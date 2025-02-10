# Generated by BoostInstall.cmake for boost_conversion-1.86.0

if(Boost_VERBOSE OR Boost_DEBUG)
  message(STATUS "Found boost_conversion ${boost_conversion_VERSION} at ${boost_conversion_DIR}")
endif()

include(CMakeFindDependencyMacro)

if(NOT boost_assert_FOUND)
  find_dependency(boost_assert 1.86.0 EXACT HINTS "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
if(NOT boost_config_FOUND)
  find_dependency(boost_config 1.86.0 EXACT HINTS "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
if(NOT boost_smart_ptr_FOUND)
  find_dependency(boost_smart_ptr 1.86.0 EXACT HINTS "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
if(NOT boost_throw_exception_FOUND)
  find_dependency(boost_throw_exception 1.86.0 EXACT HINTS "${CMAKE_CURRENT_LIST_DIR}/..")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/boost_conversion-targets.cmake")
