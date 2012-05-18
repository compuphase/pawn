#include <float>

#define E_path[
    Float: .speed,
    .from, .to
    ]
#define E_transform[
    .form,
    Float: .params[4]
    ]
#define E_anim[
    .type,
    .path[E_path],
    .transform[E_transform]
    ]

main()
    {
    new anim[E_anim]

    anim[.type] = 0

    anim[.path][.from] = 1
    anim[.path][.to] = 2
    anim[.path][.speed] = 3.3

    anim[.transform][.form] = 4
    anim[.transform][.params][0] = 5.5
    anim[.transform][.params][1] = 6.6
    anim[.transform][.params][2] = 7.7
    anim[.transform][.params][3] = 8.8

    /* below, there is an assignment in the expression, and the expression is
     * then passed passed by reference
     */
    printf ''%d\t(should be 0)\n'', anim[.type] = 0

    printf ''%d\t(should be 1)\n'', anim[.path][.from]
    printf ''%d\t(should be 2)\n'', anim[.path][.to]
    printf ''%r\t(should be 3.30000)\n'', anim[.path][.speed]

    printf ''%d\t(should be 4)\n'', anim[.transform][.form]
    printf ''%r\t(should be 5.50000)\n'', anim[.transform][.params][0]
    printf ''%r\t(should be 6.60000)\n'', anim[.transform][.params][1]
    printf ''%r\t(should be 7.70000)\n'', anim[.transform][.params][2]
    printf ''%r\t(should be 8.80000)\n'', anim[.transform][.params][3]

    #if defined OUT_OF_BOUNDS
        anim[.transform][.params][4] = 9.9
    #endif
    }
