/////////////////////////
// instance configuration

// memory allocation
#define MAX_NAME_SIZE 20  // for variables and functions
#define JSON_TOKENS 128   // in input configuration, more detail in jsmn documentation
#define PMVM_HEAP_SIZE 1024  // memory for mpvm internal configuration

// fuction and event configuration in pmvm_instance.c


/////////////////////////
// #define DEBUG

#ifdef DEBUG
#include <stdio.h>
#define dprintf(fmt, ...) printf((fmt), ##__VA_ARGS__)
#endif

#ifndef DEBUG
#define dprintf(fmt, ...)
#endif


/////////////////////////
// exports
struct State {
    int events;
    int *regs;
};

struct Conf {
    struct Pattern *patterns;
    int patterns_len;
    struct Reg *regs;
    int regs_len;

    struct State state;

    void *vm_heap;
    int vm_heap_offset;
    int vm_heap_size;
};

int set_event(char *tag, struct Conf *conf);
int set_regs(char *tag, int value, struct Conf *conf);
int get_regs(char *tag, int *value, struct Conf *conf);
int unsafe_get_regs(char *tag, struct Conf *conf);
int mk_state(struct Conf * conf);
int mk_dispatcher(struct Conf *conf, const char *dump);
int dispatch(struct Pattern * patterns, int count, struct State *st);

typedef int(* FunctionPtr)(int, int);

struct FunctionTag {
    FunctionPtr fun;
    char tag[MAX_NAME_SIZE];
};

struct EventTag {
    int code;
    char tag[MAX_NAME_SIZE];
};
