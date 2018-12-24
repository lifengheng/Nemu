#include "fs.h"

/* This is the information about all files in disk. */
static Finfo file_table[] __attribute__((used)) = {
  {"stdin", 0, 0, invalid_read, invalid_write},
  {"stdout", 0, 0, invalid_read, invalid_write},
  {"stderr", 0, 0, invalid_read, invalid_write},
#include "files.h"
};

size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

#define NR_FILES (sizeof(file_table) / sizeof(file_table[0]))

void init_fs() {
  // TODO: initialize the size of /dev/fb
  int i;
  for (i = 3; i < NR_FILES; ++i) {
    file_table[i].read = (ReadFn)fs_read;
    file_table[i].write = (WriteFn)fs_write;
    file_table[i].open_offset = 0;
  }
}

int fs_open(const char *pathname, int flags, int mode) {
  int i;
  for (i = 0; i < NR_FILES; ++i) {
    if (strcmp(pathname, file_table[i].name) == 0) {
      file_table[i].open_offset = 0;
      return i;
    }
  }
  panic("Unknown file %s\n", pathname);
}

ssize_t fs_read(int fd, void *buf, size_t len) {
  Log("fs_read: 0x%x length: 0x%x\n", file_table[fd].disk_offset + file_table[fd].open_offset, len);
  assert(file_table[fd].open_offset + len <= file_table[fd].size);
  ramdisk_read(buf, file_table[fd].disk_offset + file_table[fd].open_offset, len);
  file_table[fd].open_offset += len;
  return len; 
}

ssize_t fs_write(int fd, const void *buf, size_t len) {
  if (fd == 1 || fd == 2) {
    int i;
    for (i = 0; i < len; ++i) _putc(*(char *)buf + 1);
    return len;
  }
  Log("fs_write: 0x%x length: 0x%x\n", file_table[fd].disk_offset + file_table[fd].open_offset, len);
  assert(file_table[fd].open_offset + len <= file_table[fd].size);
  ramdisk_write(buf, file_table[fd].disk_offset + file_table[fd].open_offset, len);
  file_table[fd].open_offset += len;
  return len;
}

off_t fs_lseek(int fd, off_t offset, int whence) {
  switch(whence) {
    case SEEK_SET: assert(offset <= file_table[fd].size); file_table[fd].open_offset = offset; break;
    case SEEK_CUR: assert(file_table[fd].open_offset + offset <= file_table[fd].size); file_table[fd].open_offset += offset; break;
    case SEEK_END: assert(file_table[fd].open_offset <= 0); file_table[fd].open_offset = file_table[fd].size + offset; break;
    default: panic("Unkown whence.\n");
  }
  return file_table[fd].open_offset;
}

int fs_close(int fd) {
  fs_lseek(fd, 0, SEEK_SET);
  return 0;
}

size_t fs_filesz(int fd) {
  return file_table[fd].size;
}