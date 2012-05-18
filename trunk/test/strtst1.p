#include <string>

main()
    {
    new pstr{40}, ustr[40]

    //----- strpack() and strunpack()
    strpack ustr, "Hello"
    strpack pstr, " world"
    strcat ustr, pstr
    printf '' 1. %s\n'', ustr

    strpack pstr, ''Hello''
    strunpack ustr, " world"
    strcat pstr, ustr
    printf '' 2. %s\n'', pstr

    strunpack ustr, "Hello"
    strpack pstr, '' world''
    strcat ustr, pstr
    printf '' 3. %s\n'', ustr

    //----- strmid()
    ustr = ''I said Hello, so you say world''
    strmid pstr, ustr, 7, 12
    printf '' 4. %s'', pstr
    strmid pstr, ustr, 24, 30
    printf ''%s\n'', pstr

    pstr = "I said Hello, so you say world"
    strmid ustr, pstr, 7, 12
    printf '' 5. %s'', ustr
    strmid ustr, pstr, 24, 30
    printf ''%s\n'', ustr

    //----- strdel()
    ustr = ``LIST''
    strdel ustr, 0, 4

    ustr = ''Hello cruel world''
    strdel ustr, 5, 11
    printf '' 6. %s\n'', ustr

    pstr = "Hello cruel world"
    strdel pstr, 6, 12
    printf '' 7. %s\n'', pstr

    //----- strins()
    ustr = ''Held''
    pstr = "lo worl"
    strins ustr, pstr, 3
    printf '' 8. %s\n'', ustr

    ustr = ``Held''
    pstr = "lo worl"
    strins ustr, pstr, 3
    printf '' 9. %s\n'', ustr
    }

