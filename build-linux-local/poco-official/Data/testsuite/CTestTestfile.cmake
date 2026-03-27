# CMake generated Testfile for 
# Source directory: /home/ph/work/myiot/build-linux-local/deps/Poco/Data/testsuite
# Build directory: /home/ph/work/myiot/build-linux-local/poco-official/Data/testsuite
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(Data "/home/ph/work/myiot/build-linux-local/bin/Data-testrunner" "-ignore" "/home/ph/work/myiot/cppignore.lnx" "-all")
set_tests_properties(Data PROPERTIES  ENVIRONMENT "POCO_BASE=/home/ph/work/myiot" WORKING_DIRECTORY "/home/ph/work/myiot/build-linux-local/poco-official/Data/testsuite" _BACKTRACE_TRIPLES "/home/ph/work/myiot/build-linux-local/deps/Poco/Data/testsuite/CMakeLists.txt;30;add_test;/home/ph/work/myiot/build-linux-local/deps/Poco/Data/testsuite/CMakeLists.txt;0;")
