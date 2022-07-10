

# $(CC)=gcc
# $(CXX)=g++

LATTE_CC=$(QUIET_CC)$(CC) $(FINAL_CC_CFLAGS)
LATTE_CXX=$(CXX) $(FINAL_CXX_CLFAGS)
LATTE_LD=$(QUIET_LINK)$(CC) $(FINAL_CC_LDFLAGS)
LATTE_CXX_LD=$(CXX) $(FINAL_CXX_CLFAGS)

LATTE_INSTALL=$(QUIET_INSTALL)$(INSTALL)




.make-prerequisites:
	@touch $@

%.o: %.c .make-prerequisites
	$(LATTE_CC) $(DEBUG) -MMD -o $@ -c $<  $(FINAL_CC_LIBS) 

test: $(BUILD_OBJ) 
	$(MAKE) $(BUILD_OBJ)  LATTE_CFLAGS="-fprofile-arcs -ftest-coverage"
	$(MAKE) $(TEST_MAIN).o LATTE_CFLAGS="-fprofile-arcs -ftest-coverage"
	$(LATTE_CC)  -fprofile-arcs -ftest-coverage $(DEBUG) -o $(TEST_MAIN) $(TEST_MAIN).o $(BUILD_OBJ) $(FINAL_CC_LIBS)
	./$(TEST_MAIN)
	$(MAKE) latte_lcov
	$(MAKE) latte_genhtml
	
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
	rm -rf *.o *.d
	rm -rf $(TEST_MAIN)
	rm -rf lcov_output *.gcno *.gcda lcov.info
	
distclean: clean
	rm -f .make-*