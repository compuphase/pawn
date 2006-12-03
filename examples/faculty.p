/* Calculation of the faculty of a value */

main()
    {
    print "Enter a value: "
    new v = getvalue()
    new f = faculty(v)
    printf "The faculty of %d is %d\n", v, f
    }

faculty(n)
    {
    assert n >= 0

    new result = 1
    while (n > 0)
        result *= n--

    return result
    }
