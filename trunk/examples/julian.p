/* calculate Julian Day number from a date, and vice versa */

main()
    {
    new d, m, y, jdn

    print "Give a date (dd-mm-yyyy): "
    d = getvalue(_, '-', '/')
    m = getvalue(_, '-', '/')
    y = getvalue()

    jdn = DateToJulian(d, m, y)
    printf "Date %d/%d/%d = %d JD\n", d, m, y, jdn

    print "Give a Julian Day Number: "
    jdn = getvalue()
    JulianToDate jdn, d, m, y
    printf "%d JD = %d/%d/%d\n", jdn, d, m, y
    }

DateToJulian(day, month, year)
    {
    /* The first year is 1. Year 0 does not exist: it is 1 BC (or -1) */
    assert year != 0
    if (year < 0)
        year++

    /* move January and February to the end of the previous year */
    if (month <= 2)
        year--, month += 12
    new jdn = 365*year + year/4 - year/100 + year/400
              + (153*month - 457) / 5
              + day + 1721119
    return jdn
    }

JulianToDate(jdn, &day, &month, &year)
    {
    jdn -= 1721119

    /* approximate year, then adjust in a loop */
    year = (400 * jdn) / 146097
    while (365*year + year/4 - year/100 + year/400 < jdn)
        year++
    year--

    /* determine month */
    jdn -= 365*year + year/4 - year/100 + year/400
    month = (5*jdn + 457) / 153

    /* determine day */
    day = jdn - (153*month - 457) / 5

    /* move January and February to start of the year */
    if (month > 12)
        month -= 12, year++

    /* adjust negative years (year 0 must become 1 BC, or -1) */
    if (year <= 0)
        year--
    }
