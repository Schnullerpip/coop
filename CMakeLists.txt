set(LLVM_LINK_COMPONENTS support)


add_clang_executable(coop Coop.cpp SystemStateInformation.hpp)

target_include_directories(coop PUBLIC include) 

target_link_libraries(coop PRIVATE clangTooling clangBasic clangASTMatchers)
