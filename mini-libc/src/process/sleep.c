
#include <time.h>
#include <errno.h>

unsigned int sleep(int seconds) {
    struct timespec t2, t1 = { seconds, 0 };

    int ret = nanosleep(&t1, &t2);

    if (ret < 0) {
        errno = -ret;
        return -1;
    }

    return ret;
}
