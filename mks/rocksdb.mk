

rocksdb:
	cd ../deps && $(MAKE) rocksdb

DEPENDENCY_TARGETS+= rocksdb

FINAL_CC_CFLAGS+= -DUSE_ROCKSDB -I../../deps/rocksdb/include
FINAL_CXX_CFLAGS+= -DUSE_ROCKSDB -I../../deps/rocksdb/include
FINAL_CC_LIBS := ../../deps/rocksdb/librocksdb.a -lpthread -ldl -lsnappy -lz -lstdc++ $(FINAL_CC_LIBS)
FINAL_CXX_LIBS := ../../deps/rocksdb/librocksdb.a -lpthread -ldl -lsnappy -lz $(FINAL_CXX_LIBS)