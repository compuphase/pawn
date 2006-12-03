/* stack functions, part of the RPN calculator */
#include <rational>

static Rational: stack[50]
static stackidx = 0

push(Rational: value)
    {
    assert stackidx < sizeof stack
    stack[stackidx++] = value
    }

Rational: pop()
    {
    assert stackidx > 0
    return stack[--stackidx]
    }

clearstack()
    {
    assert stackidx >= 0
    if (stackidx == 0)
        return false
    stackidx = 0
    return true
    }
