


#ifndef __TESTASSERT_H
#define __TESTASSERT_H

#define assert(e) do {							\
	if (!(e)) {				\
		printf(						\
		    "%s:%d: Failed assertion: \"%s\"\n",	\
		    __FILE__, __LINE__, #e);				\
		return 0;						\
	}								\
} while (0)

#define assert_err(err, e) do {							\
	if (!(e)) {				\
		printf(						\
		    "%s:%d: Failed assertion: \"%s\" ,%s\n",	\
		    __FILE__, __LINE__, #e, err);				\
		return 0;						\
	}								\
} while (0)
#endif