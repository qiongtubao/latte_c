
BUILD_DIR?=../out

all:
	cd src && $(MAKE) BUILD_DIR=$(BUILD_DIR) USE_SDS=yes USE_UTIL=yes $@

test: all
	cd src && $(MAKE) BUILD_DIR=$(BUILD_DIR) USE_SDS=yes USE_UTIL=yes test

clean:
	cd src/util && $(MAKE) clean
	cd src/zmalloc && $(MAKE) clean
	cd src/sds && $(MAKE) clean
	rm -rf out