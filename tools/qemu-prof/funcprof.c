/*
funcprof - QEMU TCG plugin that counts executed guest instructions per function.

when QEMU translates a block of guest code, the block's instructions are 
attributed to the function they belong to (looked up in the guest's binary's symbol table)
and an inline coutner add is attached to the block. Every time the block runs, its instruction 
count is added to that function's total. At exit, a table sorted by instruction count is written
to a file.

Instruction counts are exact for the emulated CPU, unlike QEMU wall clock time. Code outside the 
main binary (libc, libm, the dynamic loader) has no symbols in user mode and is grouped into one bucket.

Usage (QEMU 6.1 user mode):
    qemu-arm -d plugin -D /dev/null -plugin ./libfuncprof.so,arg=out=prof.txt ./prog ...

QEMU 6.1 user mode loads the quest's symbol table only when logging is enabled, hence 
"-d plugin -D /dev/null" (enable a log category, discard the output).
Arguments: out=FILE (default funcprof.txt), top=N (default 0 = all functions).
*/

#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

#define MAX_FUNCS 8192
#define OUTSIDE   "[shared libraries: libc, libm, ld.so]"

typedef struct {
    const char *name;
    uint64_t insns;   /* updated by inline ops: never move these structs */
} Func;

static Func funcs[MAX_FUNCS];   /* funcs[0] is the OUTSIDE bucket */
static size_t n_funcs;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static char *out_path;
static size_t top_n;          /* 0 = print every function */

/* Counter for a symbol name. Called at translation time only, not per execution. */
static Func *func_for(const char *name)
{
    if (name == NULL) {
        return &funcs[0];
    }
    for (size_t i = 1; i < n_funcs; i++) {
        if (strcmp(funcs[i].name, name) == 0) {
            return &funcs[i];
        }
    }
    if (n_funcs == MAX_FUNCS) {
        return &funcs[0];
    }
    funcs[n_funcs].name = strdup(name);
    return &funcs[n_funcs++];
}

static void tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    (void)id;
    size_t n = qemu_plugin_tb_n_insns(tb);
    const char *first = qemu_plugin_insn_symbol(qemu_plugin_tb_get_insn(tb, 0));
    bool one_func = true;

    for (size_t i = 1; i < n && one_func; i++) {
        one_func = qemu_plugin_insn_symbol(qemu_plugin_tb_get_insn(tb, i)) == first;
    }

    pthread_mutex_lock(&lock);
    if (one_func) {
        /* common case: one add of n per block execution */
        qemu_plugin_register_vcpu_tb_exec_inline(tb, QEMU_PLUGIN_INLINE_ADD_U64,
                                                 &func_for(first)->insns, n);
    } else {
        /* block spans two functions: count each instruction separately */
        for (size_t i = 0; i < n; i++) {
            struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
            qemu_plugin_register_vcpu_insn_exec_inline(
                insn, QEMU_PLUGIN_INLINE_ADD_U64,
                &func_for(qemu_plugin_insn_symbol(insn))->insns, 1);
        }
    }
    pthread_mutex_unlock(&lock);
}

static int by_insns_desc(const void *a, const void *b)
{
    uint64_t x = (*(Func *const *)a)->insns;
    uint64_t y = (*(Func *const *)b)->insns;
    return (x < y) - (x > y);
}

static void at_exit(qemu_plugin_id_t id, void *userdata)
{
    (void)id;
    (void)userdata;
    Func *sorted[MAX_FUNCS];
    uint64_t total = 0;
    double cum = 0;

    for (size_t i = 0; i < n_funcs; i++) {
        sorted[i] = &funcs[i];
        total += funcs[i].insns;
    }
    qsort(sorted, n_funcs, sizeof(sorted[0]), by_insns_desc);

    FILE *out = fopen(out_path, "w");
    if (out == NULL) {
        perror(out_path);
        return;
    }
    fprintf(out, "# funcprof: guest instructions executed per function\n");
    fprintf(out, "# total instructions: %" PRIu64 "\n", total);
    fprintf(out, "# %4s %16s %7s %7s  %s\n", "rank", "instructions", "%", "cum%", "function");
    for (size_t i = 0; i < n_funcs && (top_n == 0 || i < top_n) && sorted[i]->insns > 0; i++) {
        double pct = 100.0 * sorted[i]->insns / total;
        cum += pct;
        fprintf(out, "  %4zu %16" PRIu64 " %7.2f %7.2f  %s\n",
                i + 1, sorted[i]->insns, pct, cum, sorted[i]->name);
    }
    fclose(out);
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                                           int argc, char **argv)
{
    (void)info;
    /* argv is freed after install returns, so copy what we keep */
    out_path = strdup("funcprof.txt");
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "out=", 4) == 0) {
            free(out_path);
            out_path = strdup(argv[i] + 4);
        } else if (strncmp(argv[i], "top=", 4) == 0) {
            top_n = strtoul(argv[i] + 4, NULL, 10);
        } else {
            fprintf(stderr, "funcprof: unknown argument '%s' (use out=FILE, top=N)\n", argv[i]);
            return -1;
        }
    }
    funcs[n_funcs++].name = OUTSIDE;
    qemu_plugin_register_vcpu_tb_trans_cb(id, tb_trans);
    qemu_plugin_register_atexit_cb(id, at_exit, NULL);
    return 0;
}