/* The Towers of Hanoi, a game solved through recursion */

main()
    {
    print "How many disks: "
    new disks = getvalue()
    move 1, 3, 2, disks
    }

move(from, to, spare, numdisks)
    {
    if (numdisks > 1)
        move from, spare, to, numdisks-1
    printf "Move disk from pillar %d to pillar %d\n", from, to
    if (numdisks > 1)
        move spare, to, from, numdisks-1
    }
