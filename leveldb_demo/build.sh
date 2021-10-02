rm -rf libcleveldb.a  
rm -rf test_level_db.o 
g++ -c -o test_level_db.o test_level_db.cc -I ./include -std=c++11
ar rvs libcleveldb.a test_level_db.o
gcc -o test test.c -L. -lcleveldb -lleveldb -lstdc++ zmalloc.o sds.o  