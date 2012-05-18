/* Set operations, using bit arithmetic */

const
    { A = 0b0000001,
      B = 0b0000010,
      C = 0b0000100,
      D = 0b0001000,
      E = 0b0010000,
      F = 0b0100000,
      G = 0b1000000
    }

main()
    {
    new nextstep[] =
        [ C | E,     /* A can reach C and E */
          D | E,     /* B  "    "   D and E */
          G,         /* C  "    "   G */
          C | F,     /* D  "    "   C and F */
          0,         /* E  "    "   none */
          0,         /* F  "    "   none */
          E | F,     /* G  "    "   E and F */
        ]

    print "The departure point: "
    new start = clamp( .value = toupper(getchar()) - 'A',
                       .min = 0,
                       .max = sizeof nextstep - 1
                     )

    print "\nThe number of steps: "
    new steps = getvalue()

    /* make the set */
    new result = findtargets(start, steps, nextstep)
    printf "The points in range of %c in %d steps: ", start + 'A', steps
    for (new i = 0; i < sizeof nextstep; i++)
        if (result & 1 << i)
            printf "%c ", i + 'A'
    }

findtargets(start, steps, nextstep[], numpoints = sizeof nextstep)
    {
    new result = 0
    new addedpoints = nextstep[start]
    while (steps-- > 0 && result != addedpoints)
        {
        result = addedpoints
        for (new i = 0; i < numpoints; i++)
            if (result & 1 << i)
                addedpoints |= nextstep[i]
        }
    return result
    }
