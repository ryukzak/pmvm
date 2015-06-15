#include <string.h>  // strcmp, memcpy, strlen
#include <stdlib.h>  // strtol
#include "jsmn.h"
#include "pmvm.h"
#include "pmvm_instance.c"


enum ArgType {
    fromState = 1,
    constant = 2,
    function = 3,
};

union ArgValue {
    int index;
    int value;
    struct Function2 *fun;
};

struct Arg {
    enum ArgType type;
    union ArgValue data;
};

struct Function2 {
    int (*fun)(int, int);
    struct Arg *a, *b;
};

struct Update {
    int index;
    struct Arg * arg;
};

struct Pattern {
    int events_mask;
    struct Function2 *guard;
    int update_len;
    struct Update **update;
    int signal;
};

// memory

void * vm_malloc(size_t size, struct Conf *conf) {
    if (conf->vm_heap_size < conf->vm_heap_offset + size) return NULL;
    void *offset = conf->vm_heap + conf->vm_heap_offset;
    conf->vm_heap_offset += size;
    return offset;
};

void vm_free_all(struct Conf *conf) {
    conf->vm_heap_offset = 0;
}


static int jsoneq(const char *dump, jsmntok_t *tok, const char *s) {
    return strncmp(dump + tok->start, s, tok->end - tok->start) == 0;
}


FunctionPtr get_function(const char *dump, jsmntok_t *tok) {
    if (tok->type != JSMN_PRIMITIVE && tok->type != JSMN_STRING) return 0;
    for (int i = 0; i < sizeof(functions)/sizeof(functions[0]); ++i) {
        if (jsoneq(dump, tok, functions[i].tag)) return functions[i].fun;
    }
    return NULL;
}


struct Reg {
    char tag[MAX_NAME_SIZE];
    int value;
};

int get_reg_index(const char *dump, jsmntok_t *tok, struct Conf *conf) {
    if (tok->type != JSMN_PRIMITIVE && tok->type != JSMN_STRING) return 0;
    for (int i = 0; i < conf->regs_len; ++i) {
        dprintf("get_reg_index %.*s `jsoneq` %s\n", tok->end - tok->start, dump + tok->start, conf->regs[i].tag);
        if (jsoneq(dump, tok, conf->regs[i].tag)) return i;
    }
    return -1;
}

int get_reg_index_by_tag(const char *tag, struct Conf *conf) {
    for (int i = 0; i < conf->regs_len; ++i) {
        if (strcmp(tag, conf->regs[i].tag)) return i;
    }
    return -1;
}


struct Function2 * parse_function(const char * dump, jsmntok_t * t, int *i, struct Conf *conf);

struct Arg * parse_arg(const char * dump, jsmntok_t * t, int *i, struct Conf *conf) {
    char *end_ptr;
    struct Arg *result = vm_malloc(sizeof(struct Arg), conf);
    dprintf("        parse_arg %.*s\n", t[*i].end - t[*i].start, dump + t[*i].start);
    if (t[*i].type == JSMN_ARRAY){
        result->type = function;
        result->data.fun = parse_function(dump, t, i, conf);
    } else {
        int value = strtol(dump + t[*i].start, &end_ptr, 10);
        if (end_ptr == dump + t[*i].end) {
            dprintf("|          constant: %.*s\n", t[*i].end - t[*i].start, dump + t[*i].start);
            result->type = constant;
            result->data.value = value;
        } else {
            dprintf("|          fromState: %.*s\n", t[*i].end - t[*i].start, dump + t[*i].start);
            result->type = fromState;
            result->data.value = get_reg_index(dump, &(t[*i]), conf);
            if (result->data.value == -1) { dprintf("regs not exist!\n"); return NULL;}
        }
        (*i)++;
    }
    return result;
}

struct Function2 * parse_function(const char * dump, jsmntok_t * t, int *i, struct Conf *conf) {
    struct Function2 * result = vm_malloc(sizeof(struct Function2), conf);
    dprintf("        parse_function %.*s\n", t[*i].end - t[*i].start, dump + t[*i].start);
    if (t[*i].type != JSMN_ARRAY) { dprintf("function must be array!\n"); return NULL; }
    int fun_len = t[*i].size;
    (*i)++;
    if (fun_len != 3) { dprintf("function must have 3 args: fun, a, b\n"); return NULL; }

    dprintf("|          %.*s\n", t[*i].end - t[*i].start, dump + t[*i].start);
    FunctionPtr fun = get_function(dump, &(t[*i])); (*i)++;
    if (fun == NULL) { dprintf("function not exists!\n"); return NULL; }

    struct Arg * a = parse_arg(dump, t, i, conf);
    struct Arg * b = parse_arg(dump, t, i, conf);

    result->fun = fun;
    result->a = a;
    result->b = b;

    return result;
}


int eval(struct Function2 *g, struct State *st);

int get_arg(enum ArgType t, union ArgValue v, struct State *st) {
    switch (t) {
        case fromState:
            return st->regs[v.index];
        case constant:
            return v.value;
        case function:
            return eval(v.fun, st);
        default:
            return 0;
    }
}

int eval(struct Function2 *g, struct State *st) {
    int a_value = get_arg(g->a->type, g->a->data, st);
    int b_value = get_arg(g->b->type, g->b->data, st);
    return g->fun(a_value, b_value);
}

int dispatch(struct Pattern * patterns, int count, struct State *st) {
    for (int i = 0; i < count; ++i) {
        dprintf("check rule %d event: %08x events_mask: %08x signal: %d event_match: %d guard: %d\n",
            i, st->events, patterns[i].events_mask, patterns[i].signal,
            patterns[i].events_mask & st->events && (st->events & ~patterns[i].events_mask) == 0,
            patterns[i].guard == NULL || eval(patterns[i].guard, st));

        if (patterns[i].events_mask & st->events
            && (st->events & ~patterns[i].events_mask) == 0
            && (patterns[i].guard == NULL
                || eval(patterns[i].guard, st))) {
            dprintf("activate rule %d.\n", i);
            for (int j = 0; j < patterns[i].update_len; ++j) {
                int v = get_arg(patterns[i].update[j]->arg->type, patterns[i].update[j]->arg->data, st);
                dprintf("Update [%d/%d]: regs[%d] = %d\n", j, patterns[i].update_len,
                    patterns[i].update[j]->index, v);
                st->regs[patterns[i].update[j]->index] = v;
            }
            st->events = 0;  // случайно событие не сбросится, тк проверка строгая, но вероятен lock
            return patterns[i].signal;
        }
    }
    return -1;
}


int get_event_code(const char *dump, jsmntok_t *tok) {
    if (tok->type != JSMN_PRIMITIVE && tok->type != JSMN_STRING) return 0;
    for (int i = 0; i < sizeof(events) / sizeof(events[0]); ++i) {
        if (jsoneq(dump, tok, events[i].tag)) return events[i].code;
    }
    return 0;
}

int get_event_code_by_tag(const char *tag) {
    for (int i = 0; i < sizeof(events) / sizeof(events[0]); ++i) {
        dprintf("^^^^^^^^^^^^^%s == %s = %d\n", tag, events[i].tag, strcmp(tag, events[i].tag));
        if (strcmp(tag, events[i].tag) == 0) return events[i].code;
    }
    return 0;
}

int mk_state(struct Conf * conf) {
    conf->state.events = 0x00000000;
    conf->state.regs = vm_malloc(conf->regs_len * sizeof(int), conf);
    if (conf->state.regs == NULL) { return -1; }
    for (int i = 0; i < conf->regs_len; ++i) {
        conf->state.regs[i] = conf->regs[i].value;
    }
    return 0;
}

int mk_dispatcher_inner(struct Conf *conf, const char *dump);

int mk_dispatcher(struct Conf *conf, const char *dump) {
    int tmp = mk_dispatcher_inner(conf, dump);
    if (tmp == -1) vm_free_all(conf);
    return tmp;
}

int mk_dispatcher_inner(struct Conf *conf, const char *dump) {
    conf->regs = NULL;
    conf->patterns = NULL;

    jsmn_parser p;
    jsmntok_t t[JSON_TOKENS];
    char * end_ptr;

    jsmn_init(&p);
    int r = jsmn_parse(&p, dump, strlen(dump), t, sizeof(t)/sizeof(t[0]));

    if (r < 0) { dprintf("Failed to parse JSON: %d\n", r); return -1; }
    if (t[0].type != JSMN_OBJECT) { dprintf("Top level must be dict!"); return -1; }

    for (int i = 1; i < r;) {
        dprintf("| %.*s:\n", t[i].end - t[i].start, dump + t[i].start);
        if (jsoneq(dump, &t[i], "defaults")) {
            i++; // switch from key to value
            if (t[i].type != JSMN_OBJECT) { dprintf("defaults must be object!\n"); return -1; }
            conf->regs_len = t[i].size;
            i++; // switch to child
            if (conf->regs_len == 0) continue;

            conf->regs = vm_malloc(conf->regs_len * sizeof(struct Reg), conf);
            if (conf->regs == NULL) { dprintf("Malloc crash.\n"); return -1; }

            for (int regs_i = 0; regs_i < conf->regs_len; ++regs_i) {
                // dprintf("+%d / %d \n", j, len);
                jsmntok_t *k = &t[i], *v = &t[i + 1];
                i += 2; // pass key_valie pair

                if (k->end - k->start >= MAX_NAME_SIZE) { dprintf("Too big tag (%.*s)!", k->end - k->start, dump + k->start); return -1; }
                memcpy(conf->regs[regs_i].tag, dump + k->start, k->end - k->start);
                conf->regs[regs_i].tag[k->end - k->start + 1] = '\0';                
                // sprintf(conf->regs[regs_i].tag, "%.*s", k->end - k->start, dump + k->start);
                
                if (v->type != JSMN_PRIMITIVE) { dprintf("Default values must be primitive!"); return -1; }
                conf->regs[regs_i].value = strtol(dump + v->start, &end_ptr, 10);
                if (end_ptr != dump + v->end) { dprintf("Default values must be integer!"); return -1; }

                dprintf("|     %.*s: %.*s\n",
                    k->end - k->start, dump + k->start,
                    v->end - v->start, dump + v->start
                    );
            }
            // dprintf("---------------\n");
        } else if (jsoneq(dump, &t[i], "rules")) {
            i++; // switch from key to value
            if (t[i].type != JSMN_ARRAY) { dprintf("rules must be array!\n"); return -1; }
            conf->patterns_len = t[i].size;
            i++; // switch to child
            if (conf->patterns_len == 0) continue;

            conf->patterns = vm_malloc(conf->patterns_len * sizeof(struct Pattern), conf);
            if (conf->patterns == NULL) { dprintf("Malloc crash.\n"); return -1; }

            for (int patterns_i = 0; patterns_i < conf->patterns_len; ++patterns_i) {
                dprintf("|     rule[%d]:\n", patterns_i);
                if (t[0].type != JSMN_OBJECT) { dprintf("Rules value must be object!"); return -1; }
                conf->patterns[patterns_i].events_mask = 0;
                conf->patterns[patterns_i].guard = NULL;
                conf->patterns[patterns_i].update = NULL;
                conf->patterns[patterns_i].signal = -1;

                int rule_len = t[i].size;
                if (rule_len == 0) { dprintf("Empty rule!\n"); return -1; }
                i++;
                for (int j = 0; j < rule_len; ++j) {
                    jsmntok_t *k = &t[i];
                    i += 1; // pass key
                    if (jsoneq(dump, k, "events")) {
                        dprintf("|      events:\n");
                        if (t[i].type != JSMN_ARRAY) { dprintf("events must be array!\n"); return -1; }

                        int events_len = t[i].size;
                        i++;

                        int events_mask = 0;
                        for (int g = 0; g < events_len; ++g) {
                            dprintf("|        %.*s\n", t[i].end - t[i].start, dump + t[i].start);
                            int code = get_event_code(dump, &(t[i]));
                            if (code == 0) { dprintf("Wrong event tag.\n"); return -1; }
                            events_mask |= code;
                            i++;
                        }
                        conf->patterns[patterns_i].events_mask = events_mask;
                        dprintf("|        patterns[%d].events_mask = %x \n", patterns_i, conf->patterns[patterns_i].events_mask);
                    } else if (jsoneq(dump, k, "guards")) {
                        dprintf("|      guards:\n");
                        if (t[i].type != JSMN_ARRAY) { dprintf("guard must be array!\n"); return -1; }
                        conf->patterns[patterns_i].guard = parse_function(dump, t, &i, conf);
                        if (conf->patterns[patterns_i].guard == NULL) { dprintf("Wrong guard!\n"); return -1; }
                    } else if (jsoneq(dump, k, "update")) {
                        dprintf("|      update:\n");
                        if (t[i].type != JSMN_OBJECT) { dprintf("update must be object!\n"); return -1; }
                        conf->patterns[patterns_i].update_len = t[i].size;
                        i++; // switch to child
                        if (conf->patterns[patterns_i].update_len == 0) continue;

                        struct Update ** upd = vm_malloc(conf->patterns[patterns_i].update_len * sizeof(struct Update *), conf);
                        if (upd == NULL) { dprintf("Malloc crash.\n"); return -1; }

                        int upd_i = 0;
                        for (upd_i = 0; upd_i < conf->patterns[patterns_i].update_len; ++upd_i) {
                            upd[upd_i] = vm_malloc(sizeof(struct Update), conf);
                            jsmntok_t *k = &t[i];
                            i++;

                            upd[upd_i]->index = get_reg_index(dump, k, conf);
                            if (upd[upd_i]->index == -1) { dprintf("regs not exists!\n"); return -1; }

                            upd[upd_i]->arg = parse_arg(dump, t, &i, conf);
                            if (upd[upd_i]->arg == NULL) { dprintf("wrong updata value!\n"); return -1; }
                        }
                        conf->patterns[patterns_i].update = upd;
                        for (int o = 0; o < conf->patterns[patterns_i].update_len; ++o) {
                            dprintf("+++ patterns[%d].update[%d] = {.index=%d, .arg={.type=%d, .data.value=%08x}}\n",
                                patterns_i, o, conf->patterns[patterns_i].update[o]->index,
                                conf->patterns[patterns_i].update[o]->arg->type,
                                conf->patterns[patterns_i].update[o]->arg->data.value
                                );
                        }
                    } else if (jsoneq(dump, k, "signal")) {
                        dprintf("|      signal:\n");
                        dprintf("|          %.*s\n", t[i].end - t[i].start, dump + t[i].start);
                        conf->patterns[patterns_i].signal = strtol(dump + t[i].start, &end_ptr, 10);
                        i++;
                    } else {
                        dprintf("Unexpected key: %.*s\n", k->end - k->start, dump + k->start);
                        return -1;
                    }
                }
            }
        } else {
            dprintf("Unexpected key: %.*s\n", t[i].end - t[i].start, dump + t[i].start);
            return -1;
        }
    }
    return 0;
}


int set_event(char *tag, struct Conf *conf) {
    int v = get_event_code_by_tag(tag);
    if (v == 0) return -1;
    conf->state.events |= v;
    return 0;
}

int set_regs(char *tag, int value, struct Conf *conf) {
    int i = get_reg_index_by_tag(tag, conf);
    if (i < 0) return -1;
    conf->state.regs[i] = value;
    return -1;
}

int get_regs(char *tag, int *value, struct Conf *conf) {
    int i = get_reg_index_by_tag(tag, conf);
    if (i < 0) return -1;
    *value = conf->state.regs[i];
    return 0;
}

int unsafe_get_regs(char *tag, struct Conf *conf) {
    int v;
    get_regs(tag, &v, conf);
    return v;
}
