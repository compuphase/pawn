@keypressed(key)
    {
    /* get current position */
    new x, y
    wherexy x, y

    /* determine how the update the current position */
    switch (key)
        {
        case 'u': y--   /* up */
        case 'd': y++   /* down */
        case 'l': x--   /* left */
        case 'r': x++   /* right */
        case '\e': exit /* Escape = exit */
        }

    /* adjust the cursor position and draw something */
    moveturtle x, y
    }

moveturtle(x, y)
    {
    gotoxy x, y
    print '*'
    gotoxy x, y
    }
