set(LLVM_LINK_COMPONENTS support)

add_clang_executable(structdump
  StructDump.cpp
  )
target_link_libraries(structdump
  PRIVATE
  clangTooling
  clangBasic
  clangASTMatchers
  )