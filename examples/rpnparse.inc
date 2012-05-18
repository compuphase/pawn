/* main rpn parser and lexical analysis, part of the RPN calculator */
#include <rational>
#include <string>

#define Token [
              .type,          /* operator or token type */
    Rational: .value,         /* value, if t_type is "Number" */
              .word{20},      /* raw string */
    ]

const Number    = '0'
const EndOfExpr = '#'

rpncalc(const string{})
    {
    new index
    new field[Token]
    for ( ;; )
        {
        field = gettoken(string, index)
        switch (field.type)
            {
            case Number:
                push field.value
            case '+':
                push pop() + pop()
            case '-':
                push - pop() + pop()
            case '*':
                push pop() * pop()
            case '/', ':':
                push 1.0 / pop() * pop()
            case EndOfExpr:
                break   /* exit "for" loop */
            default:
                printf "Unknown operator '%s'\n", field.word
            }
        }
    printf "Result = %r\n", pop()
    if (clearstack())
        print "Stack not empty\n", red
    }

gettoken(const string{}, &index)
    {
    /* first get the next "word" from the string */
    new word{20}
    word = strtok(string, index)

    /* then parse it */
    new field[Token]
    field.word = word
    if (strlen(word) == 0)
        {
        field.type = EndOfExpr /* special "stop" symbol */
        field.value = 0
        }
    else if ('0' <= word{0} <= '9')
        {
        field.type = Number
        field.value = rval(word)
        }
    else
        {
        field.type = word{0}
        field.value = 0
        }

    return field
    }
