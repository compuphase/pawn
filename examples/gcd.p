/*
  The greatest common divisor of two values,
  using Euclides' algorithm.
*/

@start()
    {
    print "Input two values\n"
    var a = getvalue()
    var b = getvalue()
    while (a != b)
        if (a > b)
            a = a - b
        else
            b = b - a
    printf "The greatest common divisor is %d\n", a
    }
