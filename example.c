#include "pmvm.h"
#include <stdlib.h>
#include <stdio.h>

const char *input =
"{                                                                 \n"
"   defaults: {                                                    \n"
"       gps_step: 0,                                               \n"
"       priority: 42,                                              \n"
"   },                                                             \n"
"   rules:                                                         \n"
"      [                                                           \n"
"        {                                                         \n"
"           events: [COMPLETE],                                    \n"
"           update: {priority: 0},                                 \n"
"        },                                                        \n"
"        {                                                         \n"
"           events: [GPS1],                                        \n"
"           guard: [&& [=, gps_step, 0], [<, priority, 10]],       \n"
"           update: {gps_step: 1},                                 \n"
"           signal: 1,                                             \n"
"        },                                                        \n"
"        {                                                         \n"
"           events: [GPS1],                                        \n"
"           guard: [=, gps_step, 1],                               \n"
"           update: {gps_step: 0},                                 \n"
"           signal: 2,                                             \n"
"        },                                                        \n"
"        {                                                         \n"
"           events: [GPS1],                                        \n"
"           guard: [>=, priority, 1],                              \n"
"        },                                                        \n"
"      ]                                                           \n"
"}                                                                 \n";

int main(int argc, char** argv) {
    printf("Json input:\n%s\n", input);

    // initialize pmvm configuration
    struct Conf conf = {
        .vm_heap = malloc(PMVM_HEAP_SIZE),
        .vm_heap_offset = 0,
        .vm_heap_size = PMVM_HEAP_SIZE,
    };
    if (mk_dispatcher(&conf, input) == -1) { printf("Bad configuration!\n"); free(conf.vm_heap); return -1; }
    if (mk_state(&conf) == -1) { printf("Can't make state!\n"); free(conf.vm_heap); return -1; }

    printf("========================================================\n");
    for (int i = 0; i < conf.regs_len; ++i) printf("%08x ", conf.state.regs[i]); printf("\n");

    printf(">> get GSP1        // skip by priority\n");
    set_event("GPS1", &conf);
    printf("send signal: %d\n", dispatch(conf.patterns, conf.patterns_len, &(conf.state)));
    for (int i = 0; i < conf.regs_len; ++i) printf("%08x ", conf.state.regs[i]); printf("\n");

    printf(">> get COMPLETE\n");
    set_event("COMPLETE", &conf);
    printf("send signal: %d\n", dispatch(conf.patterns, conf.patterns_len, &(conf.state)));
    for (int i = 0; i < conf.regs_len; ++i) printf("%08x ", conf.state.regs[i]); printf("\n");

    printf(">> get GSP1        // catch\n");
    set_event("GPS1", &conf);
    printf("send signal: %d\n", dispatch(conf.patterns, conf.patterns_len, &(conf.state)));
    for (int i = 0; i < conf.regs_len; ++i) printf("%08x ", conf.state.regs[i]); printf("\n");

    printf(">> get GSP1        // catch\n");
    set_event("GPS1", &conf);
    printf("send signal: %d\n", dispatch(conf.patterns, conf.patterns_len, &(conf.state)));
    for (int i = 0; i < conf.regs_len; ++i) printf("%08x ", conf.state.regs[i]); printf("\n");

    printf(">> get GSP1        // catch\n");
    set_event("GPS1", &conf);
    printf("send signal: %d\n", dispatch(conf.patterns, conf.patterns_len, &(conf.state)));
    for (int i = 0; i < conf.regs_len; ++i) printf("%08x ", conf.state.regs[i]); printf("\n");

    free(conf.vm_heap);
    return 0;
}
