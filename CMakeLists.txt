set(LLVM_LINK_COMPONENTS support)

add_clang_executable(coop Coop.cpp coop_utils.cpp Logger.cpp MatchCallbacks.cpp SystemStateInformation.cpp)

target_include_directories(coop PUBLIC include) 

target_link_libraries(coop PRIVATE clangTooling clangBasic clangASTMatchers)
