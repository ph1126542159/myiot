# CMake generated Testfile for 
# Source directory: /home/ph/work/myiot/build-linux-local/deps/Poco/CppParser/testsuite
# Build directory: /home/ph/work/myiot/build-linux-local/poco-official/CppParser/testsuite
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(CppParser "/home/ph/work/myiot/build-linux-local/bin/CppParser-testrunner" "-ignore" "/home/ph/work/myiot/cppignore.lnx" "-all")
set_tests_properties(CppParser PROPERTIES  ENVIRONMENT "POCO_BASE=/home/ph/work/myiot" WORKING_DIRECTORY "/home/ph/work/myiot/build-linux-local/poco-official/CppParser/testsuite" _BACKTRACE_TRIPLES "/home/ph/work/myiot/build-linux-local/deps/Poco/CppParser/testsuite/CMakeLists.txt;21;add_test;/home/ph/work/myiot/build-linux-local/deps/Poco/CppParser/testsuite/CMakeLists.txt;0;")
