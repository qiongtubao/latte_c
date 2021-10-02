
g++ -o  level_db.o -c level_db.cc  -pthread -L ../../deps/leveldb/build -lleveldb -std=c++11 -I ../../deps/leveldb/include
ar cr libredisleveldb.a level_db.o ../sds.o ../zmalloc.o
gcc test.c -o test -L. -lredisleveldb
# g++ ./tests/mydb.cc -o mydb -pthread ../../deps/leveldb/build/libleveldb.a -std=c++11 -I ./include
4503597479886921