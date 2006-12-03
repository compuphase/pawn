bool: ispacked(string[])
    return bool: (string[0] > charmax)

my_strlen(string[])
    {
    new len = 0
    if (ispacked(string))
        while (string{len} != EOS)      /* get character from pack */
            ++len
    else
        while (string[len] != EOS)      /* get cell */
            ++len
    return len
    }

strupper(string[])
    {
    assert ispacked(string)

    for (new i=0; string{i} != EOS; ++i)
        string{i} = toupper(string{i})
    }

main()
    {
    new s[10]

    for (new i = 0; i < 5; i++)
        s{i}=i+'a'
    s{5}=EOS

    printf("String is %s\n", ispacked(s) ? "packed" : "unpacked")
    printf("String length is %d\n", my_strlen(s))
    printf("Original:   %s\n", s)
    strupper(s)
    printf("Upper case: %s\n", s)
    }
