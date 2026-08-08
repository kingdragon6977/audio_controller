#include <sys/types.h>
#include <errno.h>
#include <stdint.h>
#include "stm32f10x.h"

extern char _end;
extern char _estack;

static char *heap_end;

caddr_t _sbrk(int incr)
{
    char *prev_heap_end;

    if (heap_end == 0)
        heap_end = &_end;

    prev_heap_end = heap_end;

    if (heap_end + incr > &_estack)
    {
        errno = ENOMEM;
        return (caddr_t)-1;
    }

    heap_end += incr;
    return (caddr_t)prev_heap_end;
}

int _write(int file, const void *ptr, size_t len)
{
    const uint8_t *data = (const uint8_t *)ptr;
    size_t i;

    if (file != 1 && file != 2)
    {
        errno = EBADF;
        return -1;
    }

    for (i = 0; i < len; ++i)
    {
        while ((USART2->SR & USART_SR_TXE) == 0) {}
        USART2->DR = data[i];
    }

    return (int)len;
}

int _close(int file)
{
    (void)file;
    return -1;
}

int _fstat(int file, void *st)
{
    (void)file;
    (void)st;
    return 0;
}

int _isatty(int file)
{
    return (file == 1 || file == 2) ? 1 : 0;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

int _read(int file, void *ptr, size_t len)
{
    (void)file;
    (void)ptr;
    (void)len;
    errno = ENOSYS;
    return -1;
}

void _exit(int status)
{
    (void)status;
    while (1) {}
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

int _getpid(void)
{
    return 1;
}
