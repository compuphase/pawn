/**
 * This program illustrates Zeller's congruence algorithm to calculate
 * the day of the week given a date.
 */

/**
 *  <summary>
 *    The main program: asks the user to input a date and prints on
 *    what day of the week that date falls.
 *  </summary>
 */
main()
    {
    new day, month, year
    if (readdate(day, month, year))
        {
        new wkday = weekday(day, month, year)
        printf "The date %d-%d-%d falls on a ", day, month, year
        switch (wkday)
            {
            case 0:
                print "Saturday"
            case 1:
                print "Sunday"
            case 2:
                print "Monday"
            case 3:
                print "Tuesday"
            case 4:
                print "Wednesday"
            case 5:
                print "Thursday"
            case 6:
                print "Friday"
            }
        }
    else
        print "Invalid date"

    print "\n"
    }

/**
 * <summary>
 *   The core function of Zeller's congruence algorithm. The function
 *   works for the Gregorian calender.
 * </summary>
 *
 * <param name="day">
 *   The day in the month, a value between 1 and 31.
 * </param>
 * <param name="month">
 *   The month: a value between 1 and 12.
 * </param>
 * <param name="year">
 *   The year in four digits.
 * </param>
 *
 * <returns>
 *   The day of the week, where 0 is Saturday and 6 is Friday.
 * </returns>
 *
 * <remarks>
 *   This function does not check the validity of the date; when the
 *   date in the parameters is invalid, the returned "day of the week"
 *   will hold an incorrect value.
 *   <p/>
 *   This equation fails in many programming languages, notably most
 *   implementations of C, C++ and Pascal, because these languages have
 *   a loosely defined "remainder" operator. Pawn, on the other hand,
 *   provides the true modulus operator, as defined in mathematical
 *   theory and as was intended by Zeller.
 * </remarks>
 */
weekday(day, month, year)
    {
    /**
     * <remarks>
     *   For Zeller's congruence algorithm, the months January and
     *   February are the 13th and 14th month of the <em>preceding</em>
     *   year. The idea is that the "difficult month" February (which
     *   has either 28 or 29 days) is moved to the end of the year.
     * </remarks>
     */
    if (month <= 2)
        month += 12, --year

    new j = year % 100
    new e = year / 100
    return (day + (month+1)*26/10 + j + j/4 + e/4 - 2*e) % 7
    }

/**
 * <summary>
 *   Reads a date and stores it in three separate fields.
 * </summary>
 *
 * <param name="day">
 *   Will hold the day number upon return.
 * </param>
 * <param name="month">
 *   Will hold the month number upon return.
 * </param>
 * <param name="year">
 *   Will hold the year number upon return.
 * </param>
 *
 * <returns>
 *   <em>true</em> if the date is valid, <em>false</em> otherwise;
 *   if the function returns <em>false</em>, the values of
 *   <paramref name="day"/>, <paramref name="month"/> and
 *   <paramref name="year"/> cannot be relied upon.
 * </returns>
 */
bool: readdate(&day, &month, &year)
    {
    print "Give a date (dd-mm-yyyy): "
    day = getvalue(_,'-','/')
    month = getvalue(_,'-','/')
    year = getvalue()
    return 1 <= month <= 12 && 1 <= day <= daysinmonth(month,year)
    }

/**
 * <summary>
 *   Returns whether a year is a leap year.
 * </summary>
 *
 * <param name="year">
 *   The year in 4 digits.
 * </param>
 *
 * <remarks>
 *   A year is a leap year:
 *   <ul>
 *     <li> if it is divisable by 4, </li>
 *     <li> but <strong>not</strong> if it is divisable by 100, </li>
 *     <li> but it <strong>is</strong> it is divisable by 400. </li>
 *   </ul>
 * </remarks>
 */
bool: isleapyear(year)
    return year % 400 == 0 || year % 100 != 0 && year % 4 == 0

/**
 * <summary>
 *   Returns the number of days in a month (the month is an integer
 *   in the range 1 .. 12). One needs to pass in the year as well,
 *   because the function takes leap years into account.
 * </summary>
 *
 * <param name="month">
 *   The month number, a value between 1 and 12.
 * </param>
 * <param name="year">
 *   The year in 4 digits.
 * </param>
 */
daysinmonth(month, year)
    {
    static daylist[] = [ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 ]
    assert 1 <= month <= 12
    return daylist[month-1] + _:(month == 2 && isleapyear(year))
    }

