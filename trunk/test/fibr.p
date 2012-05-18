/* Calculation of Fibonacci numbers by recursion */
#include <console>

fibonacci(n)
    {
    if (n <= 2)
        return 1
    return fibonacci(n - 1) + fibonacci(n - 2)
    }

main()
    {
    print(''Enter a value: '')
    new v = getvalue()
    printf(''The value of Fibonacci number %d is %d\n'',
           v, fibonacci(v) )
    }
