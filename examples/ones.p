forward ones: operator+(ones: a, ones: b)
forward ones: operator-(ones: a, ones: b)
forward ones: operator-(ones: a)

main()
    {
    new ones: chksum = ones: 0xffffffff
    print "Input values in hexadecimal, zero to exit\n"

    new ones: value
    do
        {
        print ">> "
        value = ones: getvalue(.base=16)
        chksum = chksum + value
        printf "Checksum = %x\n", chksum
        }
    while (value)
    }

stock ones: operator+(ones: a, ones: b)
    {
    const ones: mask =  ones: 0xffff    /* word mask */
    const ones: shift = ones: 16        /* word shift */

    /* add low words and high words separately */
    new ones: r1 = (a & mask) + (b & mask)
    new ones: r2 = (a >>> shift) + (b >>> shift)

    new ones: carry
    restart:            /* code label (goto target) */

    /* add carry of the new low word to the high word, then
     * strip it from the low word
     */
    carry = (r1 >>> shift)
    r2 += carry
    r1 &= mask

    /* add the carry from the new high word back to the low
     * word, then strip it from the high word
     */
    carry = (r2 >>> shift)
    r1 += carry
    r2 &= mask

    /* a carry from the high word injected back into the low
     * word may cause the new low to overflow, so restart in
     * that case
     */
    if (carry)
        goto restart

    return (r2 << shift) | r1
    }

stock ones: operator-(ones: a)
    return (a == ones: 0xffffffff) ? a : ~a

stock ones: operator-(ones: a, ones: b)
    return a + -b
