#ifndef SPOTIFS_MINUNIT_H
#define SPOTIFS_MINUNIT_H

#define mu_assert(message, test) do { if (!(test)) return message; } while (0)
#define mu_run_test(test) do { char *message = test(); tests_run++; \
                                if (message) return message; } while (0)
extern int tests_run;

#endif //SPOTIFS_MINUNIT_H
