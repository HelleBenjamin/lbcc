#include "lbcc.h"

// This pass generates LanCode assembly from IR.

char *regs[] = {"r0", "r1", "r2"};

int num_regs = sizeof(regs) / sizeof(*regs);

static char *argregs[] = {"r3", "r4"};

__attribute__((format(printf, 1, 2))) static void p(char *fmt, ...);
__attribute__((format(printf, 1, 2))) static void emit(char *fmt, ...);

static void p(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  if (output) { // Check if output is not null
    vfprintf(output, fmt, ap);
    fprintf(output, "\n");
  }
  va_end(ap);
}

static void emit(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  if (output) {
    fprintf(output, "\t");
    vfprintf(output, fmt, ap);
    fprintf(output, "\n");
  }
  va_end(ap);
}

static void emit_cmp(char *insn, IR *ir) {
  int r0 = ir->r0->rn;
  int r1 = ir->r1->rn;
  int r2 = ir->r2->rn;

  emit("cmp %s, %s", regs[r1], regs[r2]);
  emit("%s %s", insn, regs[r0]);
}

static char *argreg(int r, int size) {
  return argregs[r];
}

static void emit_ir(IR *ir, char *ret) {
  int r0 = ir->r0 ? ir->r0->rn : 0;
  int r1 = ir->r1 ? ir->r1->rn : 0;
  int r2 = ir->r2 ? ir->r2->rn : 0;

  switch (ir->op) {
  case IR_IMM:
    emit("ld %s, %d", regs[r0], ir->imm);
    break;
  case IR_BPREL:
    emit("ld %s, [bp%s%d]", regs[r0], ir->var->offset == 0 ? "+" : "", ir->var->offset);
    break;
  case IR_MOV:
    emit("ld %s, %s", regs[r0], regs[r2]);
    break;
  case IR_RETURN:
    emit("ld r0, %s", regs[r2]);
    emit("jmp %s", ret);
    break;
  case IR_CALL:
    for (int i = 0; i < ir->nargs; i++)
      emit("ld %s, %s", argregs[i], regs[ir->args[i]->rn]);

    emit("push r3");
    emit("push r4");
    emit("ld r0, 0");
    emit("call %s", ir->name);
    emit("pop r4");
    emit("pop r3");
    break;
  case IR_LABEL_ADDR:
    emit("ld %s, %s", regs[r0], ir->name);
    break;
  case IR_EQ: // TODO: re-implement IR_EQ-IR_LE
    emit_cmp("setz", ir);
    break;
  case IR_NE:
    emit_cmp("setnz", ir);
    break;
  case IR_LT:
    emit_cmp("setl", ir);
    break;
  case IR_LE:
    emit_cmp("setle", ir);
    break;
  case IR_AND:
    emit("and %s, %s", regs[r0], regs[r2]);
    break;
  case IR_OR:
    emit("or %s, %s", regs[r0], regs[r2]);
    break;
  case IR_XOR:
    emit("xor %s, %s", regs[r0], regs[r2]);
    break;
  case IR_SHL: // TODO: re-implement IR_SHL and IR_SHR
    emit("mov cl, %s", regs[r2]);
    emit("shl %s, cl", regs[r0]);
    break;
  case IR_SHR:
    emit("mov cl, %s", regs[r2]);
    emit("shr %s, cl", regs[r0]);
    break;
  case IR_JMP:
    if (ir->bbarg)
      emit("ld %s, %s", regs[ir->bb1->param->rn], regs[ir->bbarg->rn]);
    emit("jmp .L%d", ir->bb1->label);
    break;
  case IR_BR:
    emit("cmp %s, 0", regs[r2]);
    emit("jnz .L%d", ir->bb1->label);
    emit("jmp .L%d", ir->bb2->label);
    break;
  case IR_LOAD:
    emit("push r4");
    emit("ld r4, %s", regs[r2]);
    emit("ld %s, [r4]", regs[r0]);
    emit("pop r4");
    break;
  case IR_LOAD_SPILL:
    emit("ld %s, [bp%s%d]", regs[r0], ir->var->offset == 0 ? "+" : "", ir->var->offset);
    break;
  case IR_STORE:
    emit("push r3");
    emit("ld r3, %s", regs[r1]);
    emit("ld [r3], %s", regs[r2]);
    emit("pop r3");
    break;
  case IR_STORE_ARG:
    emit("ld [bp%s%d], %s", ir->var->offset == 0 ? "+" : "", ir->var->offset, argreg(ir->imm, ir->size));
    break;
  case IR_STORE_SPILL:
    emit("ld [bp%s%d], %s", ir->var->offset == 0 ? "+" : "", ir->var->offset, regs[r1]);
    break;
  case IR_ADD:
    emit("add %s, %s", regs[r0], regs[r2]);
    break;
  case IR_SUB:
    emit("sub %s, %s", regs[r0], regs[r2]);
    break;
  case IR_MUL:
    emit("mul %s, %s", regs[r2], regs[r0]);
    break;
  case IR_DIV:
    emit("div %s, %s", regs[r2], regs[r0]);
    break;
  case IR_MOD:
    // TODO
    break;
    emit("mov rax, %s", regs[r0]);
    emit("cqo");
    emit("idiv %s", regs[r2]);
    emit("mov %s, rdx", regs[r0]);
    break;
  case IR_NOP:
    break;
  case IR_ASM:
    emit("%s", ir->name);
    break;
  case IR_VMEXIT:
    emit("vmexit %d", ir->imm);
    break;
  default:
    assert(0 && "unknown operator");
  }
}

void emit_code(Function *fn) {
  // Assign an offset from BP to each local variable.
  int off = 0;
  for (int i = 0; i < fn->lvars->len; i++) {
    Var *var = fn->lvars->data[i];
    off += var->ty->size;
    off = roundup(off, var->ty->align);
    var->offset = -off;
  }

  // Emit assembly
  char *ret = format(".Lend%d", nlabel++);

  p("%s:", fn->name);
  emit("push bp");
  emit("ld bp, sp");
  emit("sub sp, %d", off);

  for (int i = 0; i < fn->bbs->len; i++) {
    BB *bb = fn->bbs->data[i];
    p(".L%d:", bb->label);
    for (int i = 0; i < bb->ir->len; i++) {
      IR *ir = bb->ir->data[i];
      emit_ir(ir, ret);
    }
  }

  p("%s:", ret);
  emit("ld sp, bp");
  emit("pop bp");
  emit("ret");
}

static char *backslash_escape(char *s, int len) {
  static char escaped[256] = {
          ['\b'] = 'b', ['\f'] = 'f',  ['\n'] = 'n',  ['\r'] = 'r',
          ['\t'] = 't', ['\\'] = '\\', ['\''] = '\'', ['"'] = '"',
  };

  StringBuilder *sb = new_sb();
  for (int i = 0; i < len; i++) {
    uint8_t c = s[i];
    char esc = escaped[c];
    if (esc) {
      sb_add(sb, '\\');
      sb_add(sb, esc);
    } else if (isgraph(c) || c == ' ') {
      sb_add(sb, c);
    } else {
      sb_append(sb, format("\\%03o", c));
    }
  }
  return sb_get(sb);
}


// TODO: Implement data section in the LASM
static void emit_data(Var *var) {
  if (var->data) {
    p(".data");
    p("%s:", var->name);
    emit(".ascii \"%s\"", backslash_escape(var->data, var->ty->size));
    return;
  }

  p(".bss");
  p("%s:", var->name);
  emit(".zero %d", var->ty->size);
}

void gen_lancode(Program *prog) {

  for (int i = 0; i < prog->funcs->len; i++)
    emit_code(prog->funcs->data[i]);
  //for (int i = 0; i < prog->gvars->len; i++)
  //  emit_data(prog->gvars->data[i]);

}
