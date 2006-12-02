/* valid public variables */
public abc;
new @def;

/* valid public functions */
forward test1(arg);
public test1(arg)
    return arg+1;

forward @test2(arg);
@test2(arg)
    return arg-1;

/* invalid public variables */
#if defined INVALID_PUBVAR
    public native monkey;
#endif

#if defined INVALID_PUBLOCAL
    testlocal(@arg)
        return @arg+1;
#endif

/* invalid public functions */
#if defined INVALID_PUBFUNC1
    native @test3(arg)
        return arg*2;
#endif

#if defined INVALID_PUBFUNC2
    public native test4(arg)
        return arg/2;
#endif

main()
    {
    test1(abc);
    @test2(@def);

    #if defined INVALID_PUBLOCAL
        new @value = 1;
        testlocal(@value);
    #endif
    }
