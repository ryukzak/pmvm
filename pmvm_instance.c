int g_eq(int a, int b) { return a == b; }
int g_l(int a, int b) { return a < b; }
int g_me(int a, int b) { return a >= b; }
int g_and(int a, int b) { return a && b; }
int g_or(int a, int b) { return a || b; }

const struct FunctionTag functions[] = {
    { g_eq,  "=="},
    { g_l,   "<" },
    { g_me,   ">=" },
    { g_and, "&&"},
    { g_or,  "||"},
};


const struct EventTag events[] = {
    { 0x00000001, "GPS1"},
    { 0x00000010, "BUTTON"},
    { 0x00000100, "COMPLETE"},
};
