#include <sys/stat.h>
#include <errno.h>
#include <stddef.h>

extern char end; // Defined by linker script
static char *heap_end = 0;

__attribute__((weak)) void *_sbrk(ptrdiff_t incr) {
  char *prev_heap_end;
  if (heap_end == 0) {
    heap_end = &end;
  }
  prev_heap_end = heap_end;
  heap_end += incr;
  return (void *)prev_heap_end;
}

__attribute__((weak)) int _close(int file) {
  (void)file;
  return -1;
}

__attribute__((weak)) int _fstat(int file, struct stat *st) {
  (void)file;
  st->st_mode = S_IFCHR;
  return 0;
}

__attribute__((weak)) int _isatty(int file) {
  (void)file;
  return 1;
}

__attribute__((weak)) int _lseek(int file, int ptr, int dir) {
  (void)file;
  (void)ptr;
  (void)dir;
  return 0;
}

__attribute__((weak)) int _read(int file, char *ptr, int len) {
  (void)file;
  (void)ptr;
  (void)len;
  return 0;
}

__attribute__((weak)) int _write(int file, char *ptr, int len) {
  (void)file;
  (void)ptr;
  (void)len;
  return len;
}
