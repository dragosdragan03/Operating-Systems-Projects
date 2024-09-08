
#include <time.h>
#include <errno.h>
#include <internal/syscall.h>

int nanosleep(const struct timespec* t1, struct timespec* t2)
{
    int ret = syscall(35, t1, t2);

    if (ret < 0) {
        errno = -ret;
        return -1;
    }

    return ret;
}
