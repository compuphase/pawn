main()
    {
    new HandOfCards[10]
    FillRandom(HandOfCards, 52)

    print "A draw of 10 numbers from a range of 0 to 51 " ...
          "(inclusive) without duplicates:\n"
    for (new i = 0; i < sizeof HandOfCards; i++)
        printf "%d ", HandOfCards[i]
    }

FillRandom(Series[], Range, Number = sizeof Series)
    {
    assert Range >= Number      /* cannot select 50 values
                                 * without duplicates in the
                                 * range 0..40, for example */
    new Index = 0
    for (new Seq = Range - Number; Seq < Range; Seq++)
        {
        new Val = random(Seq + 1)
        new Pos = InSeries(Series, Val, Index)
        if (Pos >= 0)
            {
            Series[Index] = Series[Pos]
            Series[Pos] = Seq
            }
        else
            Series[Index] = Val
        Index++
        }
    }

InSeries(Series[], Value, Top = sizeof Series)
    {
    for (new i = 0; i < Top; i++)
        if (Series[i] == Value)
            return i
    return -1
    }
