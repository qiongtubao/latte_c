

# $(CC)=gcc
# $(CXX)=g++
LATTE_CC=$(QUIET_CC)$(CC) $(FINAL_CC_CFLAGS)
LATTE_CXX=$(CXX) $(FINAL_CXX_CFLAGS)
LATTE_LD=$(QUIET_LINK)$(CC) $(FINAL_CC_LDFLAGS)
LATTE_CXX_LD=$(CXX) $(FINAL_CXX_CFLAGS)

LATTE_INSTALL=$(QUIET_INSTALL)$(INSTALL)
# BUILD_OBJS=$(subst $(space),,$(subst $(comma),$(space),$(BUILD_OBJ)))
 


.make-prerequisites:
	@touch $@

%.o: %.c .make-prerequisites
	$(LATTE_CC) $(DEBUG) -MMD -o $@ -c $< $(FINAL_CC_LIBS) 

%.xo: %.cc .make-prerequisites
	$(LATTE_CXX) $(DEBUG) -MMD -o $@ -c $<  $(FINAL_CXX_LIBS) 

gtest: 
	$(MAKE) $(BUILD_OBJ) $(LIB_OBJ) LATTE_CFLAGS=$(LATTE_CFLAGS)"-fprofile-arcs -ftest-coverage"
	$(MAKE) $(TEST_MAIN).xo LATTE_CFLAGS=$(LATTE_CFLAGS)"-fprofile-arcs -ftest-coverage -I$(LATTE_LIB_WORKSPACE)/deps/googletest/googletest/include"
	$(LATTE_CXX)  -fprofile-arcs -ftest-coverage $(DEBUG) -o $(TEST_MAIN) $(TEST_MAIN).xo $(BUILD_OBJ) $(FINAL_CXX_LIBS) -I$(LATTE_LIB_WORKSPACE)/deps/googletest/googletest/include $(LATTE_LIB_WORKSPACE)/deps/googletest/lib/libgtest.a $(LATTE_LIB_WORKSPACE)/deps/googletest/lib/libgtest_main.a
	./$(TEST_MAIN)
	$(MAKE) latte_lcov
	$(MAKE) latte_genhtml

build_test:
	$(LATTE_CC)  -fprofile-arcs -ftest-coverage $(DEBUG) -o $(TEST_MAIN) $(TEST_MAIN).o $(BUILD_OBJ) $(LIB_OBJ) $(FINAL_CC_LIBS) 


test: 
	$(MAKE) $(BUILD_OBJ) $(LIB_OBJ) LATTE_CFLAGS=$(LATTE_CFLAGS)" -fprofile-arcs -ftest-coverage" 
	$(MAKE) $(TEST_MAIN).o LATTE_CFLAGS=$(LATTE_CFLAGS)" -fprofile-arcs -ftest-coverage" 
	$(MAKE) build_test
	./$(TEST_MAIN)
	$(MAKE) latte_lcov
	$(MAKE) latte_genhtml

# 基准测试。刻意不带 -fprofile-arcs / -ftest-coverage:覆盖率插桩会让
# 纳秒级的测量结果失真好几倍,所以 bench 和 test 用两套独立的编译产物,
# 各自 make 前先清掉对方的 .o(见 bench 目标里的 clean_obj)。
# 模块只要定义 BENCH_MAIN 就能用,例如 BENCH_MAIN?=cuckoo_bench。
# 传参:make bench BENCH_ARGS="5000000 big"
BENCH_ARGS?=
# 注意必须连 $(TEST_MAIN).o 一起删。只删 *.gcno 而留下插桩过的 test 对象,
# 会让 .o 和它的 .gcno 配对断裂:test 二进制照样能跑并写出 .gcda,
# 但 lcov 找不到对应的 .gcno,直接报错退出。
clean_obj:
	rm -rf $(BUILD_OBJ) $(LIB_OBJ) $(BENCH_MAIN).o $(TEST_MAIN).o *.gcno *.gcda

build_bench:
	$(LATTE_CC) $(DEBUG) -o $(BENCH_MAIN) $(BENCH_MAIN).o $(BUILD_OBJ) $(LIB_OBJ) $(FINAL_CC_LIBS) -lm

bench:
	@test -n "$(BENCH_MAIN)" || (echo "该模块未定义 BENCH_MAIN,无法运行 bench" && false)
	$(MAKE) clean_obj
	$(MAKE) $(BUILD_OBJ) $(LIB_OBJ)
	$(MAKE) $(BENCH_MAIN).o
	$(MAKE) build_bench
	./$(BENCH_MAIN) $(BENCH_ARGS)
	@# 跑完清掉这批"无插桩"对象:bench 和 test 共用同名 .o,
	@# 留着会让随后的 make test 直接复用它们,导致 lcov 采不到覆盖率数据。
	$(MAKE) clean_obj

mac_test:
	$(MAKE) $(BUILD_OBJ) $(LIB_OBJ) LATTE_CFLAGS=$(LATTE_CFLAGS)" -fprofile-arcs -ftest-coverage" 
	$(MAKE) $(TEST_MAIN).o LATTE_CFLAGS=$(LATTE_CFLAGS)" -fprofile-arcs -ftest-coverage" 
	$(MAKE) build_test
	leaks --atExit -- ./$(TEST_MAIN)
	$(MAKE) latte_lcov
	$(MAKE) latte_genhtml


asan_test:
	$(MAKE) $(BUILD_OBJ) SANITIZER=address $(LIB_OBJ) LATTE_CFLAGS=$(LATTE_CFLAGS)" -fprofile-arcs -ftest-coverage" 
	$(MAKE) $(TEST_MAIN).o SANITIZER=address LATTE_CFLAGS=$(LATTE_CFLAGS)" -fprofile-arcs -ftest-coverage" 
	$(MAKE) SANITIZER=address build_test
	./$(TEST_MAIN)
	$(MAKE) latte_lcov
	$(MAKE) latte_genhtml

test_lib: $(TEST_MAIN).o
	$(LATTE_CC)  -fprofile-arcs -ftest-coverage $(DEBUG) -o $(TEST_MAIN) $(TEST_MAIN).o $(BUILD_DIR)/lib/liblatte.a -lm -ldl -fno-omit-frame-pointer $(FINAL_CC_LIBS)
	./$(TEST_MAIN)
	
latte_lcov:
	lcov --capture --directory . \
		--output-file lcov.info \
		--test-name latte_lcov \
		--no-external 
latte_genhtml:
	genhtml lcov.info \
		--output-directory lcov_output \
		--title "latte LCOV" \
		--show-details \
		--legend

clean:
	rm -rf *.o *.d *.xo .make-prerequisites
	rm -rf $(TEST_MAIN)
	rm -rf $(BENCH_MAIN)
	rm -rf lcov_output *.gcno *.gcda lcov.info
	rm -rf $(LIB_OBJ)
	rm -rf $(BUILD_OBJ)
	
distclean: clean
	rm -f .make-*

install_lib: $(BUILD_OBJ) $(LIB_OBJ)
	$(foreach var,$(subst $(space),,$(subst $(comma),$(space),$(BUILD_OBJ))), $(shell if [ "$(var)" != "$(findstring $(var),$(shell sh -c 'cat $(BUILD_DIR)/objs.list'))" ]; then echo $(MAKE) SANITIZER=$(SANITIZER) install_o INSTALL_O="$(var)" BUILD_DIR="$(BUILD_DIR);";fi))
	@echo "install_lib $(BUILD_OBJ)"
	$(foreach var,$(LIB_MODULES),cd $(LATTE_LIB_WORKSPACE)/src/$(var) && $(MAKE) SANITIZER=$(SANITIZER) install_lib && cd ../$(MODULE);)


install_o:
	cp -rf $(INSTALL_O) $(BUILD_DIR);
	mkdir -p $(BUILD_DIR)/include/$(MODULE);
	cp -rf $(BUILD_INCLUDE) $(BUILD_DIR)/include/$(MODULE);
	echo "$(INSTALL_O)" >> $(BUILD_DIR)/objs.list;



