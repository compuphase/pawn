/* Calculation of Fibonacci numbers by iteration */

@start()
    {
    print "Enter a value: "
    var v = getvalue()
    if (v > 0)
        printf "The value of Fibonacci number %d is %d\n",
               v, fibonacci(v)
    else
        printf "The Fibonacci number %d does not exist\n", v
    }

fibonacci(n)
    {
    assert n > 0

    var a = 0, b = 1
    for (var i = 2; i < n; i++)
        {
        var c = a + b
        a = b
        b = c
        }
    return a + b
    }
