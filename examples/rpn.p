/* a simple RPN calculator */
#include strtok
#include stack
#include rpnparse

main()
    {
    print "Type an expression in Reverse Polish Notation: "
    new string[100]
    getstring string, sizeof string
    rpncalc string
    }
