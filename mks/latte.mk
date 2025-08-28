

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



