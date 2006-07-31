#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <afs/param.h>
#include <afs/afsutil.h>

int main(int argc, char **argv) {
  lb64_string_t tmp;

  unsigned long vol;
  if (argc < 2) { fprintf(stderr, "Usage: nametodir vol\n"); exit(1); }
  vol=strtoul(argv[1], NULL, 0);
  (void)int32_to_flipbase64(tmp, (int64_t) (vol & 0xff));
  printf("Component is %s\n", tmp);
  (void)int32_to_flipbase64(tmp, (int64_t) vol);
  printf("Component is %s\n", tmp);

  exit(0);
}
