new g[][] = [ ''Eenie'', ''Meenie'' ]

native printf(const format[], {Float,Fixed,_}:...)

main()
    {
    new l[]{} = [ "Meinie", "Moe" ]
    #assert sizeof g == 2
    printf ''%s, %s, '', g[0], g[1]
    #assert sizeof l == 2
    printf ''%s, %s\n'', l[0], l[1]
    arrayparam
    }

#if defined MIX_DIMENSIONS
arrayparam(a[2][5] = [ ''Out'', ''Goes'' ])
    {
    #assert sizeof a == 2
    printf ''%s %s You\n'', a[0], a[1]
    }
#else
arrayparam(a[][] = [ ''Out'', ''Goes'' ])
    {
    #assert sizeof a == 2
    printf ''%s %s You\n'', a[0], a[1]
    }
#endif
