
ROCKSDB=glibc
CFLAGS += -msse2

# Backwards compatibility for selecting an allocator
ifeq ($(USE_ROCKSDB_TCMALLOC),yes)
	ROCKSDB=tcmalloc
endif

# Backwards compatibility for selecting an allocator
ifeq ($(USE_ROCKSDB_JEMALLOC),yes)
	ROCKSDB=jemalloc
endif



ifeq ($(ROCKSDB),glibc)
rocksdb:
	cd ../deps && $(MAKE) rocksdb
endif


DEPENDENCY_TARGETS+= rocksdb
FINAL_CC_CFLAGS+=  -I../../deps/rocksdb/include
FINAL_CXX_CFLAGS+= -I../../deps/rocksdb/include
FOLLY_LIBS_PATH=../../deps/rocksdb/folly_libs

FINAL_CC_LIBS := ../../deps/rocksdb/librocksdb.a $(FOLLY_LIBS_PATH)/folly/lib/libfolly.a $(FOLLY_LIBS_PATH)/boost/lib/libboost_context.a $(FOLLY_LIBS_PATH)/boost/lib/libboost_filesystem.a $(FOLLY_LIBS_PATH)/boost/lib/libboost_atomic.a $(FOLLY_LIBS_PATH)/boost/lib/libboost_program_options.a $(FOLLY_LIBS_PATH)/boost/lib/libboost_regex.a $(FOLLY_LIBS_PATH)/boost/lib/libboost_system.a $(FOLLY_LIBS_PATH)/boost/lib/libboost_thread.a $(FOLLY_LIBS_PATH)/double-conversion/lib/libdouble-conversion.a  $(FOLLY_LIBS_PATH)/fmt/lib/libfmt.a $(FOLLY_LIBS_PATH)/glog/lib/libglog.so.0 $(FOLLY_LIBS_PATH)/gflags/lib/libgflags.so.2.2  $(FOLLY_LIBS_PATH)/libevent/lib/libevent-2.1.so $(FOLLY_LIBS_PATH)/libiberty/lib/libiberty.a  -lstdc++ -lm -pthread -lz -lsnappy -luring $(FINAL_CC_LIBS)
FINAL_CXX_LIBS := ../../deps/rocksdb/librocksdb.a $(FINAL_CXX_LIBS)



# 1. make info USE_JEMALLOC=yes
# 2. make info USE_TCMALLOC_MINIMAL=yes
# 3. make info USE_TCMALLOC=yes
# 4. make info
