
BUILD_DIR?=../out

latte_sds:
	cd src && $(MAKE) BUILD_DIR=$(BUILD_DIR) MALLOC=$(MALLOC) USE_VALGRIND=$(USE_VALGRIND) USE_SDS=yes all

latte_sds_test:latte_sds
	cd src && $(MAKE) BUILD_DIR=$(BUILD_DIR) MALLOC=$(MALLOC) USE_VALGRIND=$(USE_VALGRIND) USE_SDS=yes sds_test


latte_dict:
	cd src && $(MAKE) BUILD_DIR=$(BUILD_DIR) USE_DICT=yes MALLOC=$(MALLOC) USE_VALGRIND=$(USE_VALGRIND) USE_DICT=yes all 

latte_dict_test: latte_dict 
	cd src && $(MAKE) BUILD_DIR=$(BUILD_DIR) USE_DICT=yes MALLOC=$(MALLOC) USE_VALGRIND=$(USE_VALGRIND) dict_test

latte_zmalloc:
	cd src && $(MAKE) BUILD_DIR=$(BUILD_DIR) USE_DICT=yes MALLOC=$(MALLOC) USE_VALGRIND=$(USE_VALGRIND) all 

latte_zmalloc_test: latte_zmalloc 
	cd src && $(MAKE) BUILD_DIR=$(BUILD_DIR) MALLOC=$(MALLOC) USE_VALGRIND=$(USE_VALGRIND) zmalloc_test



all:
	cd src && $(MAKE) BUILD_DIR=$(BUILD_DIR) MALLOC=$(MALLOC) USE_SDS=yes  USE_DICT=yes $@

test: all
	cd src && $(MAKE) BUILD_DIR=$(BUILD_DIR) USE_SDS=yes test

clean:
	cd src/util && $(MAKE) clean
	cd src/zmalloc && $(MAKE) clean
	cd src/sds && $(MAKE) clean
	cd src/dict && $(MAKE) clean
	rm -rf out