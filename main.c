#include "lbcc.h"

FILE *output;

void usage() {
  error("Usage: lbcc <input file> <output file>\n");
}

int main(int argc, char **argv) {
  if (argc == 1)
    usage();

  if (argc == 2 && !strcmp(argv[1], "-test")) {
    util_test();
    return 0;
  }

  char *path = argv[1];
  if (argc == 3) output = fopen(argv[2], "w");

  // TODO: Implement IR dumps
  bool dump_ir1 = false;
  bool dump_ir2 = false;

  // Tokenize and parse.
  Vector *tokens = tokenize(path, true);
  Program *prog = parse(tokens);
  sema(prog);
  gen_ir(prog);

  if (dump_ir1)
    dump_ir(prog->funcs);

  optimize(prog);
  liveness(prog);
  alloc_regs(prog);

  if (dump_ir2)
    dump_ir(prog->funcs);

  gen_lancode(prog);

  if(output) fclose(output);

  return 0;
}
