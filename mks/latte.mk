

# $(CC)=gcc
# $(CXX)=g++

LATTE_CC=$(QUIET_CC)$(CC) $(FINAL_CC_CFLAGS)
LATTE_CXX=$(CXX) $(FINAL_CXX_CFLAGS)
LATTE_LD=$(QUIET_LINK)$(CC) $(FINAL_CC_LDFLAGS)
LATTE_CXX_LD=$(CXX) $(FINAL_CXX_CFLAGS)

LATTE_INSTALL=$(QUIET_INSTALL)$(INSTALL)




.make-prerequisites:
	@touch $@

%.o: %.c .make-prerequisites
	$(LATTE_CC) $(DEBUG) -MMD -o $@ -c $<  $(FINAL_CC_LIBS) 

%.xo: %.cc .make-prerequisites
	$(LATTE_CXX) $(DEBUG) -MMD -o $@ -c $<  $(FINAL_CXX_LIBS) 

gtest: 
	$(MAKE) $(BUILD_OBJ) $(LIB_OBJ) LATTE_CFLAGS="-fprofile-arcs -ftest-coverage"
	$(MAKE) $(TEST_MAIN).xo LATTE_CFLAGS="-fprofile-arcs -ftest-coverage -I$(WORKSPACE)/deps/googletest/googletest/include"
	$(LATTE_CXX)  -fprofile-arcs -ftest-coverage $(DEBUG) -o $(TEST_MAIN) $(TEST_MAIN).xo $(BUILD_OBJ) $(FINAL_CXX_LIBS) -I$(WORKSPACE)/deps/googletest/googletest/include $(WORKSPACE)/deps/googletest/lib/libgtest.a $(WORKSPACE)/deps/googletest/lib/libgtest_main.a
	./$(TEST_MAIN)
	$(MAKE) latte_lcov
	$(MAKE) latte_genhtml

test: 
	$(MAKE) $(BUILD_OBJ) $(LIB_OBJ) LATTE_CFLAGS="-fprofile-arcs -ftest-coverage"
	$(MAKE) $(TEST_MAIN).o LATTE_CFLAGS="-fprofile-arcs -ftest-coverage"
	$(LATTE_CC)  -fprofile-arcs -ftest-coverage $(DEBUG) -o $(TEST_MAIN) $(TEST_MAIN).o $(BUILD_OBJ) $(LIB_OBJ) $(FINAL_CC_LIBS)
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
	cp -rf $(BUILD_OBJ) $(BUILD_DIR)
	$(shell sh -c 'echo " $(BUILD_OBJ)" >> $(BUILD_DIR)/objs.list')



