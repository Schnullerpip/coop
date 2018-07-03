set(LLVM_LINK_COMPONENTS support)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

add_clang_executable(coop Coop.cpp coop_utils.cpp Logger.cpp MatchCallbacks.cpp SystemStateInformation.cpp SourceModification.cpp InputArgs.cpp data.cpp naming.cpp)

target_include_directories(coop PUBLIC include) 

target_link_libraries(coop PRIVATE clangTooling clangBasic clangASTMatchers)
