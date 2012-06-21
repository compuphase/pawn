/*
  The greatest common divisor of two values,
  using Euclides' algorithm.
*/

main()
    {
    print "Input two values\n"
    new a = getvalue()
    new b = getvalue()
    while (a != b)
        if (a > b)
            a = a - b
        else
            b = b - a
    printf "The greatest common divisor is %d\n", a
    }
