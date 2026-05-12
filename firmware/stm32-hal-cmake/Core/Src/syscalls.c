/* Minimal newlib stubs (GNU ARM embedded toolchain). */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

register char *stack_ptr asm("sp");

__attribute__((weak)) int __io_putchar(int ch)
{
    (void)ch;
    return 0;
}

char *__env[1] = { 0 };
char **environ = __env;

void initialise_monitor_handles(void)
{
}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

void _exit(int status)
{
    (void)status;
    while (1) {
    }
}

__attribute__((weak)) int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    return -1;
}

__attribute__((weak)) int _write(int file, char *ptr, int len)
{
    (void)file;
    for (int i = 0; i < len; i++)
        __io_putchar(ptr[i]);
    return len;
}

void *_sbrk(int incr)
{
    extern uint8_t end asm("end");
    static uint8_t *heap_end;
    uint8_t *prev_heap_end;

    if (heap_end == NULL)
        heap_end = &end;

    prev_heap_end = heap_end;
    if ((intptr_t)incr < 0 && heap_end + incr < &end) {
        errno = ENOMEM;
        return (void *)-1;
    }
    if (heap_end + incr > (uint8_t *)stack_ptr) {
        errno = ENOMEM;
        return (void *)-1;
    }

    heap_end += incr;
    return (void *)prev_heap_end;
}

int _close(int file)
{
    (void)file;
    return -1;
}

int _fstat(int file, struct stat *st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}
