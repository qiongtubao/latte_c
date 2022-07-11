

# 问题 
* jemalloc 碎片化怎么扫描数据？

# 优化
    ？？

# 功能
封装各种内存库（jemalloc, libc, tcmalloc) 统一函数申请内存
# API
* zmalloc (申请内存)
* zfree (释放内存)
