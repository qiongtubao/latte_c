
BUILD_DIR?=../out

latte_sds:
	cd src && $(MAKE) BUILD_DIR=$(BUILD_DIR) MALLOC=$(MALLOC) USE_SDS=yes all

latte_sds_test:latte_sds
	cd src && $(MAKE) BUILD_DIR=$(BUILD_DIR) MALLOC=$(MALLOC) USE_SDS=yes sds_test

latte_zmalloc_test:all
	cd src && $(MAKE) BUILD_DIR=$(BUILD_DIR) MALLOC=$(MALLOC) zmalloc_test

all:
	cd src && $(MAKE) BUILD_DIR=$(BUILD_DIR) MALLOC=$(MALLOC) USE_SDS=yes  $@

test: all
	cd src && $(MAKE) BUILD_DIR=$(BUILD_DIR) USE_SDS=yes test

clean:
	cd src/util && $(MAKE) clean
	cd src/zmalloc && $(MAKE) clean
	cd src/sds && $(MAKE) clean
	rm -rf out