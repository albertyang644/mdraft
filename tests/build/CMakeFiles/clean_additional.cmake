# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "CMakeFiles/core_smoke_test_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/core_smoke_test_autogen.dir/ParseCache.txt"
  "core_smoke_test_autogen"
  )
endif()
