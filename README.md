# COOP
COOP (**C**ache friendly **O**bject-**o**riented **P**rogramming) is a source-to-source transformation tool, that attempts to automatically find and extract cold fields in a target OOP C++ code base, through static analysis.

It uses Clang's LibTooling environment to generate a project's ASTs and match predefined patterns against those. In order to do so it will create a coop_config.txt file that will be used to determine what files to include/exclude in the analysis. While COOP will look for a compilation database the config file will be used to declare additional files/dependencies or nonos.


