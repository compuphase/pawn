
#if !defined PASS_LIT_ARRAY && !defined PASS_WRONG_LENGTH

forward testfunc(d[]);
public testfunc(d[])
    {
    new a[4];
    new b[4] = [ 5, 6, ... ];
    new c;

#if defined OKAY
    a = [ 1, 2, 3, 4 ];         /* ok */
    a = [ 4, 5, 6, 7 ];         /* ok */
    a = b;                      /* ok */
#endif

#if defined WRONG_LENGTH_1
    a = [ 1, 2, 3 ];            /* error */
    #pragma unused b, c, d
#endif

#if defined WRONG_LENGTH_2
    a = [ 1, 2, 3, 4, 5 ];      /* error */
    #pragma unused b, c, d
#endif

#if defined WRONG_LENGTH_3
    a = d;                      /* error */
    #pragma unused b, c
#endif

#if defined INDEXED
    a[2] = [ 1, 2, 3, 4 ];      /* error */
#endif

#if defined NEED_INDEX_1
    a = 2;                      /* error */
#endif

#if defined NEED_INDEX_2
    a = c;                      /* error */
#endif

#if defined NOT_ARRAY_1
    c = [ 1, 2, 3, 4 ];         /* error */
#endif

#if defined NOT_ARRAY_2
    c[1] = [ 1, 2, 3, 4 ];      /* error */
#endif

#if defined MIX_PACKED_UNPACKED
    a = "abcd" 	                /* warning */
    #pragma unused b, c, d
#endif
    }

#endif  // !defined PASS_LIT_ARRAY && !defined PASS_WRONG_LENGTH

#if defined PASS_LIT_ARRAY || defined PASS_WRONG_LENGTH
testfunc2(a[4])
    {
    a[0]=a[1]+a[2]+a[3];        // to avoid a "symbol never used" warning
    }
main()
    {
    #if defined PASS_LIT_ARRAY
        testfunc2(''ab'');      // okay
        testfunc2(''abc'');     // okay
        testfunc2([1,2,3,4]);   // okay
    #endif
    #if defined PASS_WRONG_LENGTH
        testfunc2(''abcd'');    // error
        testfunc2([1,2]);       // error
        testfunc2([1,2,3,4,5]); // error
    #endif
    }
#endif
