

asan_double_free:
	gcc -fsanitize=address -g asan_double_free_test.c -o asan_double_free_test
	./asan_double_free_test

asan_access_thread:
	gcc -fsanitize=address -g asan_double_free_thread_test.c -lpthread -o asan_double_free_thread_test
	./asan_double_free_thread_test

# 偶尔不会报错 (AddressSanitizer:DEADLYSIGNAL)
asan_non_mutex_address:
	gcc -fsanitize=address -g asan_non_mutex_test.c -lpthread -o asan_non_mutex_test
	./asan_non_mutex_test

# 会报错
asan_non_mutex_thread:
	gcc -fsanitize=thread -g asan_non_mutex_test.c -lpthread -o asan_non_mutex_test
	./asan_non_mutex_test

clean:
	rm -rf asan_non_mutex_test  asan_double_free_test asan_double_free_thread_test