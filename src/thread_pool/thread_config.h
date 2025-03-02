


#ifndef __LATTE_THREAD_CONFIG_H
#define __LATTE_THREAD_CONFIG_H


/* Define for latte_set_thread_title */
#ifdef __linux__
#define latte_set_thread_title(name) pthread_setname_np(pthread_self(), name)
#else
#if (defined __FreeBSD__ || defined __OpenBSD__)
#include <pthread_np.h>
#define latte_set_thread_title(name) pthread_set_name_np(pthread_self(), name)
#elif defined __NetBSD__
#include <pthread.h>
#define latte_set_thread_title(name) pthread_setname_np(pthread_self(), "%s", name)
#elif defined __HAIKU__
#include <kernel/OS.h>
#define latte_set_thread_title(name) rename_thread(find_thread(0), name)
#else
#if (defined __APPLE__ && defined(MAC_OS_X_VERSION_10_7))
int pthread_setname_np(const char *name);
#include <pthread.h>
#define latte_set_thread_title(name) pthread_setname_np(name)
#else
#define latte_set_thread_title(name)
#endif
#endif
#endif

#endif