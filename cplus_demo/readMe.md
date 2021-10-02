g++ -c -o testcplusplus.o testcplusplus.cc 
ar rvs libtest.a testcplusplus.o
gcc -o test test.c -L. -ltest -lstdc++