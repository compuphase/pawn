#include <fixed>

main()
    {
    /* rounding due to conversion */
    new Fixed: value
    value = strfixed(''3.1415926535897932384626433832795'')
    printf(''Value is %q (should be 3.142)\n'', value)

    /* rounding in division */
    new Fixed: a, Fixed: b
    a = 1       /* assignment operator */
    b = 3
    printf(''%q / %q = %q (should be 1.000 / 3.000 = 0.333)\n'', a, b, a/b)
    a = 2
    printf(''%q / %q = %q (should be 2.000 / 3.000 = 0.667)\n'', a, b, a/b)

    /* mixing cells with fixed point */
    printf(''2.0 / %q = %q\n'', b, 2.0/b)
    printf(''2 / %q = %q\n'', b, 2/b)
    printf(''%q / 3 = %q\n'', a, a/3)

    /* rounding in multiplication */
    a = 3.297
    b = 2.426
    printf(''%q * %q = %q (should be 3.297 / 2.426 = 7.999)\n'', a, b, a*b)

    /* raising to the power */
    printf(''%q ^ 2 = %q (should be 3.142 ^ 2 = 9.872)\n'', value, fpower(value, 2))
    printf(''%q ^ 3 = %q (should be 3.142 ^ 3 = 31.018)\n'', value, fpower(value, 3))
    printf(''%q ^ 4 = %q (should be 3.142 ^ 4 = 97.460)\n'', value, fpower(value, 4))
    printf(''%q ^ 0 = %q (should be 3.142 ^ 0 = 1.000)\n'', value, fpower(value, 0))
    printf(''%q ^ -1 = %q (should be 3.142 ^ -1 = 0.318)\n'', value, fpower(value, -1))
    printf(''%q ^ -2 = %q (should be 3.142 ^ -2 = 0.101)\n'', value, fpower(value, -2))

    /* square root */
    printf(''sqroot(2) = %q (should be 1.414)\n'', fsqroot(2))
    printf(''sqroot(3) = %q (should be 1.732)\n'', fsqroot(3))
    printf(''sqroot(7) = %q (should be 2.646)\n'', fsqroot(7))
    }

