# CMake generated Testfile for 
# Source directory: /home/ph/work/myiot/build-linux-local/deps/Poco/JWT/testsuite
# Build directory: /home/ph/work/myiot/build-linux-local/poco-official/JWT/testsuite
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(JWT "/home/ph/work/myiot/build-linux-local/bin/JWT-testrunner" "-ignore" "/home/ph/work/myiot/cppignore.lnx" "-all")
set_tests_properties(JWT PROPERTIES  ENVIRONMENT "POCO_BASE=/home/ph/work/myiot" WORKING_DIRECTORY "/home/ph/work/myiot/build-linux-local/poco-official/JWT/testsuite" _BACKTRACE_TRIPLES "/home/ph/work/myiot/build-linux-local/deps/Poco/JWT/testsuite/CMakeLists.txt;25;add_test;/home/ph/work/myiot/build-linux-local/deps/Poco/JWT/testsuite/CMakeLists.txt;0;")
