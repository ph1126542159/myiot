# CMake generated Testfile for 
# Source directory: /home/ph/work/myiot/build-linux-local/deps/Poco/Net/testsuite
# Build directory: /home/ph/work/myiot/build-linux-local/poco-official/Net/testsuite
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(Net "/home/ph/work/myiot/build-linux-local/bin/Net-testrunner" "-ignore" "/home/ph/work/myiot/cppignore.lnx" "-all")
set_tests_properties(Net PROPERTIES  ENVIRONMENT "POCO_BASE=/home/ph/work/myiot" WORKING_DIRECTORY "/home/ph/work/myiot/build-linux-local/poco-official/Net/testsuite" _BACKTRACE_TRIPLES "/home/ph/work/myiot/build-linux-local/deps/Poco/Net/testsuite/CMakeLists.txt;25;add_test;/home/ph/work/myiot/build-linux-local/deps/Poco/Net/testsuite/CMakeLists.txt;0;")
