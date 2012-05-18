#include <console>
#include <float>

#if 0
native Float:operator*(Float:a, Float:b) = floatmul
native Float:operator/(Float:a, Float:b) = floatdiv
native Float:operator+(Float:a, Float:b) = floatadd
native Float:operator-(Float:a, Float:b) = floatsub

stock Float:operator==(Float:a, Float:b)
    return floatcmp(a, b) == 0
stock Float:operator!=(Float:a, Float:b)
    return floatcmp(a, b) != 0
stock Float:operator<(Float:a, Float:b)
    return floatcmp(a, b) < 0
stock Float:operator<=(Float:a, Float:b)
    return floatcmp(a, b) <= 0
stock Float:operator>(Float:a, Float:b)
    return floatcmp(a, b) > 0
stock Float:operator>=(Float:a, Float:b)
    return floatcmp(a, b) >= 0

forward Float:operator%(Float:a,Float:b)
forward Float:operator-(Float:a)
#endif

main()
    {
    #if defined USEROP_STOCK
        new Float:p1=3.1415
        new Float:p2=50.0e-1
        new Float:p3

        printf(''p1=%.6f  p2=%.6f  p3=%.6f\n'', p1, p2, p3)
        p3=p1+p2
        printf(''sum=%.6f\n'', p3)

        p3=p1*p2
        printf(''product=%.6f\n'', p3)

        p3=p1/p2
        printf(''quotient=%.6f\n'', p3)

        p3=-p3
        printf(''negated=%.6f\n'', p3)
    #endif

    #if defined USEROP_CONSTPARAMS
        new Float: f;
        f = 1.5 * 2.5;
        printf(''F=%f\r\n'',f);
    #endif

    #if defined USEROP_DEBUGINFO
        new Float:f1 = 30.0;
        new Float:f2 = 20.9;

        if (f1 > f2)
            printf (''Bigger\n'');
        if (f1 != f2)
            printf (''Unequal\n'');
    #endif

    #if defined USEROP_CHAINOP
        new a = 0
        if (-1 <= a <= 1)
            printf(''\''a\'' okay\n'')
        new Float:b = 0.0
        if (-1.0 <= b <= 1.0)
            printf(''\''b\'' okay\n'')
    #endif
    }
