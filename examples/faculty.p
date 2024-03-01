/* Calculation of the faculty of a value */

@start()
    {
    print "Enter a value: "
    var v = getvalue()
    var f = faculty(v)
    printf "The faculty of %d is %d\n", v, f
    }

faculty(n)
    {
    assert n >= 0

    var result = 1
    while (n > 0)
        result *= n--

    return result
    }
