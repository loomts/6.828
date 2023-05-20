#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

void find(char const* path, char const* target) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }
  switch (st.type) {
    case T_DIR:
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf("find: path too long\n");
        break;
      }
      strcpy(buf, path);
      p = buf + strlen(buf);  // why
      *p++ = '/';
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0 || strcmp(de.name, ".") == 0 ||
            strcmp(de.name, "..") == 0)
          continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0) {
          printf("find: cannot stat %s\n", buf);
          continue;
        }
        if (st.type == T_DIR)
          find(buf, target);
        else if (st.type == T_FILE) {
          if (strcmp(de.name, target) == 0)
            printf("%s\n", buf);
        }
      }
      break;
    case T_FILE:
      fprintf(2, "Usage:find dir file\n");
      exit(1);
    default:
      break;
  }
}
int main(int argc, char const* argv[]) {
  if (argc != 3) {
    fprintf(2, "Usage: find dir file\n");
    exit(1);
  }
  char const* path = argv[1];
  char const* target = argv[2];
  find(path, target);
  exit(0);
}