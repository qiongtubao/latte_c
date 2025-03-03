
#!/bin/bash
TMP_PATH=/tmp/fbcode_builder_getdeps-ZhomeZdongZDocumentsZlatteZlatte_cZdepsZrocksdbZthird-partyZfollyZbuildZfbcode_builder
CP_PATH=../../deps/rocksdb/folly_libs
rm -rf $CP_PATH
mkdir  $CP_PATH
cp -RL  $TMP_PATH/installed/folly/ $CP_PATH/folly
cp -RL  $TMP_PATH/installed/boost-* $CP_PATH/boost
cp -RL  $TMP_PATH/installed/libevent-* $CP_PATH/libevent
cp -RL  $TMP_PATH/installed/libiberty-* $CP_PATH/libiberty
cp -RL  $TMP_PATH/installed/fmt-* $CP_PATH/fmt
cp -RL  $TMP_PATH/installed/double-conversion-* $CP_PATH/double-conversion
cp -RL $TMP_PATH/installed/glog-* $CP_PATH/glog
cp -RL $TMP_PATH/installed/gflags-* $CP_PATH/gflags
