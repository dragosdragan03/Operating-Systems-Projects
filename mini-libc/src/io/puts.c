// SPDX-License-Identifier: BSD-3-Clause

#include <unistd.h>
#include <string.h>
#include <errno.h>

int puts(const char *s) {
    write(1, s, strlen(s));
    write(1, "\n", 1);

    return 1;
}
