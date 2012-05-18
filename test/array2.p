main()
    {
    #if defined ARRAY2DTO3D
        new thr[2][3][4] = [ [ [ 0x111, 0x112, 0x113, 0x114 ], [ 0x121, 0x122, 0x123, 0x124 ], [ 0x131, 0x132, 0x133, 0x134 ] ],
                             [ [ 0x211, 0x212, 0x213, 0x214 ], [ 0x221, 0x222, 0x223, 0x224 ], [ 0x231, 0x232, 0x233, 0x234 ] ] ]
        new two[3][4] = [ [ 0x11, 0x12, 0x13, 0x14 ], [ 0x21, 0x22, 0x23, 0x24 ], [ 0x31, 0x32, 0x33, 0x34 ] ]

        thr[1] = two

        for (new x = 0; x < sizeof thr; x++)
            {
            for (new y = 0; y < sizeof thr[]; y++)
                {
                for (new z = 0; z < sizeof thr[][]; z++)
                    printf ''%3x '', thr[x][y][z]
                printf ''- ''
                }
            printf ''\n''
            }
    #endif
    }
