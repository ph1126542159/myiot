# CMake generated Testfile for 
# Source directory: /home/ph/work/myiot/build-linux-local/deps/Poco/JSON/testsuite
# Build directory: /home/ph/work/myiot/build-linux-local/poco-official/JSON/testsuite
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(JSON "/home/ph/work/myiot/build-linux-local/bin/JSON-testrunner" "-ignore" "/home/ph/work/myiot/cppignore.lnx" "-all")
set_tests_properties(JSON PROPERTIES  ENVIRONMENT "POCO_BASE=/home/ph/work/myiot" WORKING_DIRECTORY "/home/ph/work/myiot/build-linux-local/poco-official/JSON/testsuite" _BACKTRACE_TRIPLES "/home/ph/work/myiot/build-linux-local/deps/Poco/JSON/testsuite/CMakeLists.txt;25;add_test;/home/ph/work/myiot/build-linux-local/deps/Poco/JSON/testsuite/CMakeLists.txt;0;")
