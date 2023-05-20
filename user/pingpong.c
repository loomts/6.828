#include "kernel/types.h"
#include "user/user.h"
int main(int argc, char const* argv[]) {
  int pid;
  int p[2];
  pipe(p);
  if (fork() == 0) {
    // child pong
    pid = getpid();
    char buf[2];
    if (read(p[0], buf, 1) != 1) {
      fprintf(2, "<%d>: failed to read in child", pid);
      exit(1);
    }
    close(p[0]);
    printf("%d: received ping\n", pid);
    if (write(p[1], buf, 1) != 1) {
      fprintf(2, "<%d>: failed to write in child", pid);
      exit(1);
    }
    close(p[1]);
    exit(0);
  } else {
    pid = getpid();
    char buf[2];
    buf[1] = 0;
    if (write(p[1], "1", 1) != 1) {
      fprintf(2, "<%d>: failed to write in parent", pid);
      exit(1);
    }
    close(p[1]);
    wait(0);
    if (read(p[0], buf, 1) != 1) {
      fprintf(2, "<%d>: failed to read in parent", pid);
      exit(1);
    }
    printf("%d: received pong\n", pid);
    close(p[0]);
    exit(0);
  }
}