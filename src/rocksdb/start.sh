


nohup make test &
sleep 5
pid=`ps aux | grep latte_rocksdb_test | grep -v grep | awk '{print $2}'`
./cgroup_v1.sh  test $pid 650 