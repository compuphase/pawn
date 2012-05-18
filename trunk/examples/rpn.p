/* a simple RPN calculator */
#include strtok
#include stack
#include rpnparse

main()
    {
    print "Type expressions in Reverse Polish Notation " ...
          "(or an empty line to quit)\n"
    new string{100}
    while (getstring(string, .pack = true))
        rpncalc string
    }
