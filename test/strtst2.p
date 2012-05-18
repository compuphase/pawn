#include <string>

main()
    {
    new i
    new s[20]

    i = strval(''0'')
    printf ''%d (should be 0)\n'', i
    i = strval(''9'')
    printf ''%d (should be 9)\n'', i
    i = strval(''10'')
    printf ''%d (should be 10)\n'', i
    i = strval(''123'')
    printf ''%d (should be 123)\n'', i
    i = strval(''-9'')
    printf ''%d (should be -9)\n'', i
    i = strval(''-10'')
    printf ''%d (should be -10)\n'', i
    i = strval(''-123'')
    printf ''%d (should be -123)\n'', i

    valstr s, 0
    printf ''%s (should be 0)\n'', s
    valstr s, 9
    printf ''%s (should be 9)\n'', s
    valstr s, 10
    printf ''%s (should be 10)\n'', s
    valstr s, 123
    printf ''%s (should be 123)\n'', s
    valstr s, -9
    printf ''%s (should be -9)\n'', s
    valstr s, -10
    printf ''%s (should be -10)\n'', s
    valstr s, -123
    printf ''%s (should be -123)\n'', s
    }
