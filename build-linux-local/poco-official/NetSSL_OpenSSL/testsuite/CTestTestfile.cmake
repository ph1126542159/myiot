# CMake generated Testfile for 
# Source directory: /home/ph/work/myiot/build-linux-local/deps/Poco/NetSSL_OpenSSL/testsuite
# Build directory: /home/ph/work/myiot/build-linux-local/poco-official/NetSSL_OpenSSL/testsuite
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(NetSSL "/home/ph/work/myiot/build-linux-local/bin/NetSSL-testrunner" "-ignore" "/home/ph/work/myiot/cppignore.lnx" "-all")
set_tests_properties(NetSSL PROPERTIES  ENVIRONMENT "POCO_BASE=/home/ph/work/myiot" WORKING_DIRECTORY "/home/ph/work/myiot/build-linux-local/bin" _BACKTRACE_TRIPLES "/home/ph/work/myiot/build-linux-local/deps/Poco/NetSSL_OpenSSL/testsuite/CMakeLists.txt;25;add_test;/home/ph/work/myiot/build-linux-local/deps/Poco/NetSSL_OpenSSL/testsuite/CMakeLists.txt;0;")
