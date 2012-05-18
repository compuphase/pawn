#if defined DIMxxx
  new a[4] = [1, 1, 1, 1]
  new aa[3][4] = [ [1, 1, 1, 1], [2, 2, 2, 2], [3, 3, 3, 3] ]
  new aaa[2][3][4] = [ [ [1, 1, 1, 1], [2, 2, 2, 2], [3, 3, 3, 3] ],
                       [ [10, 10, 10, 10], [20, 20, 20, 20], [30, 30, 30, 30] ] ]
#endif

#if defined DIMxx_
  new a[] = [1, 1, 1, 1]
  new aa[3][] = [ [1, 1, 1, 1], [2, 2, 2, 2], [3, 3, 3, 3] ]
  new aaa[2][3][] = [ [ [1, 1, 1, 1], [2, 2, 2, 2], [3, 3, 3, 3] ],
                      [ [10, 10, 10, 10], [20, 20, 20, 20], [30, 30, 30, 30] ] ]
#endif

#if defined DIMx__
  new a[] = [1, 1, 1, 1]
  new aa[][] = [ [1, 1, 1, 1], [2, 2, 2, 2], [3, 3, 3, 3] ]
  new aaa[2][][] = [ [ [1, 1, 1, 1], [2, 2, 2, 2], [3, 3, 3, 3] ],
                     [ [10, 10, 10, 10], [20, 20, 20, 20], [30, 30, 30, 30] ] ]
#endif

#if defined DIM___
  new a[] = [1, 1, 1, 1]
  new aa[][] = [ [1, 1, 1, 1], [2, 2, 2, 2], [3, 3, 3, 3] ]
  new aaa[][][] = [ [ [1, 1, 1, 1], [2, 2, 2, 2], [3, 3, 3, 3] ],
                    [ [10, 10, 10, 10], [20, 20, 20, 20], [30, 30, 30, 30] ] ]
#endif

#assert defined DIMxxx || defined DIMxx_ || defined DIMx__ || defined DIM___

main()
    {
    #assert sizeof a == 4
    #assert sizeof aa == 3
    #assert sizeof aaa == 2
    #assert sizeof aaa[] == 3

    #if defined DIMxxx
        #assert sizeof aa[] == 4
        #assert sizeof aaa[][] == 4
        const m_aa = sizeof aa[]
        const m_aaa = sizeof aaa[][]
    #else
        const m_aa = 4
        const m_aaa = 4
    #endif

    for (new x = 0; x < sizeof a; x++)
        {
        assert a[x] == 1
        printf ''%d%c'', a[x], (x == sizeof a - 1) ? '|' : ','
        }
    printf ''\n''

    for (new y = 0; y < sizeof aa; y++)
        for (new x = 0; x < m_aa; x++)
            {
            assert aa[y][x] == y+1
            printf ''%d%c'', aa[y][x], (x == m_aa - 1) ? '|' : ','
            }
    printf ''\n''

    for (new z = 0; z < sizeof aaa; z++)
        for (new y = 0; y < sizeof aaa[]; y++)
            for (new x = 0; x < m_aaa; x++)
                {
                assert aaa[z][y][x] == (y+1)*(1+9*z)
                printf ''%d%c'', aaa[z][y][x], (x == m_aaa - 1) ? '|' : ','
                }
    printf ''\n''
    }

