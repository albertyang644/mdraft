# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "CMakeFiles/mdraft_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/mdraft_autogen.dir/ParseCache.txt"
  "mdraft_autogen"
  )
endif()
