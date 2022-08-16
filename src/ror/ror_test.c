

#include "test/testhelp.h"
#include "test/testassert.h"
#include "ror.h"
#include <stdio.h>
#include "zmalloc/zmalloc.h"


int test_sds() {
    char* rocksdb_stats = "** File Read Latency Histogram By Level [default] **\
2022/08/15-10:54:08.225330 7f6019ff3700 [/db_impl/db_impl.cc:945] ------- DUMPING STATS -------\
2022/08/15-10:54:08.225386 7f6019ff3700 [/db_impl/db_impl.cc:946]\
** DB Stats **\
Uptime(secs): 294000.2 total, 600.0 interval\
Cumulative writes: 285M writes, 556M keys, 283M commit groups, 1.0 writes per commit group, ingest: 83.45 GB, 0.29 MB/s\
Cumulative WAL: 0 writes, 0 syncs, 0.00 writes per sync, written: 0.00 GB, 0.00 MB/s\
Cumulative stall: 00:00:0.000 H:M:S, 0.0 percent\
Interval writes: 0 writes, 0 keys, 0 commit groups, 0.0 writes per commit group, ingest: 0.00 MB, 0.00 MB/s\
Interval WAL: 0 writes, 0 syncs, 0.00 writes per sync, written: 0.00 GB, 0.00 MB/s\
Interval stall: 00:00:0.000 H:M:S, 0.0 percent\
 \
** Compaction Stats [default] **\
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)\
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\
  L0      0/0    0.00 KB   0.0     36.0     0.0     36.0     110.0     74.0       0.0   1.5     53.8    164.6    684.42            665.60       904    0.757     19M    73K       0.0       0.0\
  L1      8/0   225.38 MB   0.9    138.5    74.0     64.5     131.4     66.9       0.0   1.8     97.1     92.1   1460.97           1435.20       245    5.963   2079M   294M       0.0       0.0\
  L2     95/0    2.49 GB   1.0    186.6    63.2    123.4     179.5     56.1       3.5   2.8    186.9    179.8   1022.20            987.13      1913    0.534    687M   200M       0.0       0.0\
  L3    858/0   24.99 GB   1.0    254.6    51.4    203.3     230.3     27.1       5.8   4.5    227.8    206.1   1144.60           1099.73      1528    0.749    248M    25M       0.0       0.0\
  L4    268/0    7.85 GB   0.0      0.0     0.0      0.0       0.0      0.0       7.8   0.0      0.0      0.0      0.00              0.00         0    0.000       0      0       0.0       0.0\
 Sum   1229/0   35.56 GB   0.0    615.7   188.6    427.1     651.3    224.1      17.1   8.8    146.2    154.7   4312.17           4187.66      4590    0.939   3034M   520M       0.0       0.0\
 Int      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.0      0.0       0.0   0.0      0.0      0.0      0.00              0.00         0    0.000       0      0       0.0       0.0\
 \
** Compaction Stats [default] **\
Priority    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)\
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\
 Low      0/0    0.00 KB   0.0    615.7   188.6    427.1     577.1    150.0       0.0   0.0    165.8    155.4   3802.83           3691.12      3763    1.011   3034M   520M       0.0       0.0\
High      0/0    0.00 KB   0.0      0.0     0.0      0.0      74.2     74.2       0.0   0.0      0.0    149.1    509.35            496.54       827    0.616       0      0       0.0       0.0\
 \
Blob file count: 0, total size: 0.0 GB\
 \
Uptime(secs): 294000.2 total, 600.0 interval\
Flush(GB): cumulative 74.185, interval 0.000\
AddFile(GB): cumulative 0.000, interval 0.000\
AddFile(Total Files): cumulative 0, interval 0\
AddFile(L0 Files): cumulative 0, interval 0\
AddFile(Keys): cumulative 0, interval 0\
Cumulative compaction: 651.26 GB write, 2.27 MB/s write, 615.71 GB read, 2.14 MB/s read, 4312.2 seconds\
Interval compaction: 0.00 GB write, 0.00 MB/s write, 0.00 GB read, 0.00 MB/s read, 0.0 seconds\
Stalls(count): 0 level0_slowdown, 0 level0_slowdown_with_compaction, 0 level0_numfiles, 0 level0_numfiles_with_compaction, 0 stop for pending_compaction_bytes, 0 slowdown for pending_compaction_bytes, 0 memtable_compaction, 0 memtable_slowdown, interval 0 total count\
Block cache LRUCache@0x1639ff0 capacity: 1.00 MB collections: 491 last_copies: 0 last_secs: 2.5e-05 secs_since: 0\
Block cache entry stats(count,size,portion): Misc(1,0.00 KB,0%)";
    db_stats_info(rocksdb_stats);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("test_sds function", 
            test_sds() == 1);
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}