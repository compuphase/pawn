/* TEST1
 *
 * This file is for regression testing of the Pawn compiler and the abstract
 * machine. It contains many conditionally compiled segments that must certify
 * the correct behaviour of the compiler or the abstract machine. Note that
 * "correct behaviour" may mean that the compiler or the abstract machine
 * aborts with an error.
 */

/* NOTES:
 * 1. this file should not contain native functions, see test 2 in TEST.BAT
 */


#if defined ELSEIF_EXPR
  #if ELSEIF_EXPR == 1
    // code
  #elseif ELSEIF_EXPR == 2
    // code
  #endif
#endif

#if defined RETURN_ARRAY
return_array()
    {
    new array[10];
    return array;
    }
#endif

#if defined UNINIT_ARRAY
    new array[32];
#endif

#if defined MIXED_CASE
    TestMixed() return;
#endif

#if defined PROTOTYPE_GOOD
forward ProtoFunc(Param1);

ProtoFunc(Param1)
    {
    return Param1 + 1;
    }
#endif

#if defined PROTOTYPE_BAD
forward ProtoFunc(Param1);

ProtoFunc(Param2)
    {
    return Param2 + 1;
    }
#endif

#if defined MISSING_PARM
/* expect 2 parameters of type array, later, only one is passed */
SetString(szName[] , szValue[])
    {
    szName[0] = szValue[0];
    }
#endif

#if defined REDECLARE_EXPAND
forward Test(a,b)

Test(a,b,c,d,e)
        return a+b+c+d+e+f
#endif

#if defined MISSING_UNUSED_FUNC
stock unused_func( a )
{
	return a + non_existing()
}
#endif

#if defined NO_RETURN
no_return(c)
    c = c + 10
#endif

#if defined REDECLARE_VAR_FUNC
new foo = 0;

foo(x,y)
{
  return x+y;
}
#endif

#if defined USELESS_DECLARE1
useless()
    new waste[100] = { 1 , 2 , ... }
#endif

#if defined PUBLIC_STATE_VAR
    public public_state_var <mystate>           // error 88
#endif

#if defined INITIALIZED_STATE_VAR
    new initialized_state_var <mystate> = 1

    set_state_var(value) <mystate>
        initialized_state_var = value
#endif

#if defined UNDECLARED_PUBLIC
    @something()
        return 1
#endif

#if defined PROTOTYPED_PUBLIC
    forward @proto1(abc, def);
    forward @proto2(bool:flg);
#endif

#if defined PUBLIC_WITH_PARAMETERS
    forward @test(bb);

    @test(bb)
        {
        return (bb);
    }
#endif

#if defined AMBIGUOUS_CALL /* this function is okay, the error is in the call */
increment(a)
    return a + 1
#endif

#if defined RECURSION_DETECTION
    recurse1(a)
        {
        if (a<=0)
            return 1
        return a + recurse1(a-1)
        }

    recurse2a(n)
        {
        if (n<=0)
            return 1
        return n * recurse2b(n-1)
        }

    recurse2b(n)
        {
        if (n<=0)
            return 1
        return n + recurse2a(n-1)
        }
#endif

#if defined CLEAR_STRING
    clear(name{}, size=sizeof name)
        {
        name = ""
        return size
        }
#endif

#if 0
#error This error message should never pop up
#endif

#if defined REDEFINE_CONSTANTS
const Left = 4
const
    {
    Left = 1,
    Top,
    Right,
    Bottom
    }
const Right = 4
#endif

#if defined ENUM_GOOD_DECL
const {  /* old way of declaring an enumeration */
    a = 1,
    b,
    c,
    }

const {  /* optional terminating commas */
    d = 4,    e
    f
    }

const {
    g = 7, h, i
    }
#endif

#if defined ENUM_BAD_DECL
const {
    j = 0, k l
    }
#endif


#if defined ARRAY2D_NO_COMMA
new Filenames[6]{} =
    [
    "alpha.mp3"
    "bravo.mp3",
    "charlie.mp3",
    "delta.mp3",
    "echo.mp3",
    "foxtrot.mp3"
    ]
#endif

#if defined INVALID_GLOBAL_DECL
    new port = readcfgvalue(.key = "proxy-port", .filename = config_network)
#endif

main()
    {
    #if defined UNDEF_FUNC_CALL
        /* calling an undefined function flagged an error in the compiler,
         * and then usually crashed.
         */
        undef();
    #endif
    #if defined RETURN_ARRAY
        /* must call the function, otherwise it is stripped off */
        return_array()
    #endif
    #if defined DANGLING_ELSE
        /* dangling-else problem should be signalled through a "loose
         * indentation" warning
         */
        new x = 5;
        if (x>=0)
            if ((x & 1))
                x += 1; /* move up to higher even number (if odd positive number) */
        else
            x = 0;      /* if x < 0, set it to 0 */
    #endif
    #if defined UNUSED_LOCAL
        /* unused & uninitialized local variable */
        new a;
    #endif
    #if defined MIXED_CASE
        TestMixed();
    #endif
    #if defined FOR_DEL_LOCALS
        /* The "for" loop removes all local symbols of the higher level
         * if the index is not declared in expr1 of the "for" loop.
         */
        new i;
        new a, b;

        for ( i = 0; i < 4; i++ )
          a = b + i;

        for ( i = 0; i < 4; i++ )       /* error: unknown variable "i" */
          b = b + i;
        b = a;          /* just to avoid the warning "variable 'a' is assigned
                         * a value that is never used" */
    #endif

    #if defined UNINIT_ARRAY
        array[0]=0;     /* just to avoid the warning "variable 'array' is
                         * assigned a value that is never used" */
    #endif

    #if defined PROTOTYPE_GOOD || defined PROTOTYPE_BAD
        ProtoFunc(10);
    #endif

    #if defined LVAL_IN_TEST
        new lLoop = 1;
        do
        {
          ++lLoop;
        }
        while (1 = lLoop);
    #endif

    #if defined MISSING_PARM
        SetString(''Value'');     /* function needs 2 parameters */
    #endif

    #if defined SWITCH_NO_COLONS
        new iWarns = 1
        switch(iWarns)
        {
        case 1
            for (new i=0;i<10;i++)
                iWarns++
        case 2
            iWarns--
        case 3
            iWarns=0
        }
    #endif

    #if defined FOR_BREAK_LCLVAR
        for(new x = 0; x < 10; x++)
            break;
    #endif

    #if defined NO_RETURN
        new v = no_return(10)
        v = v + 1       /* to avoid a "symbol is never used" warning */
    #endif

    #if defined REDECLARE_VAR_FUNC
        foo = foo + 1   /* to avoid a "symbol is never used" warning */
    #endif

    #if defined UNREACHABLE_CODE1
        new test = 0
        if (test)
        {
          return
          test++
        }
    #endif

    #if defined UNREACHABLE_CODE2
        new test = 0
        if (test)
            return 1
        else
            return 2
        return test
    #endif

    #if defined UNREACHABLE_CODE3
        new test = 0
        for (;;) {
            if (test > 10)
                return test
            test++
        } /* for */
        return test   /* endless loop, control never arrives here */
    #endif

    #if defined SELF_ASSIGNMENT
        new var = 0
        var = var

        new array[4] = [ 1, 2, ... ]
        array[2] = array[2]
    #endif

    #if defined NO_SELF_ASSIGNMENT
        new var = 0
        var = !var
    #endif

    #if defined USELESS_DECLARE1
        useless()
    #endif

    #if defined USELESS_DECLARE2
        new test = 1
        if (test < 10)
            new test2
    #endif

    #if defined OUT_OF_BOUNDS
        new array[10]
        new x = 10
        x = array[x]
        x = x + 1
    #endif

    #if defined PREPROCESS_DIRECTIVES
        #define TTESTT 15

        new v = 0
        #if defined TESTTT
            v = v + 2
        #endif
        #if TTESTT == 15
            v = v + 1
        #endif
    #endif

    #if defined TAGS_IN_COND_OPER_OK || defined TAGS_IN_COND_OPER_WRONG
        new test = 1
        new testt = 1
        new bool:test2
    #endif
    #if defined TAGS_IN_COND_OPER_OK
        test2 = (test==testt)?true:false        // okay
        test2 = (test==testt)?(bool:1):(bool:0) // okay
        test2 = (test==testt)?true:bool:0       // okay (but bad style)
        test2 = (test==testt)?(bool:1):bool:0   // okay (but bad style)
        test2 = test2 && false                  // to avoid a compiler warning
    #endif
    #if defined TAGS_IN_COND_OPER_WRONG
        test2 = (test==testt)?bool:1:bool:0     // warning 220
        test2 = test2 && false                  // to avoid a compiler warning
    #endif

    #if defined CARET_CTRL_CHAR
        new url{100} = "Link: ^"http://www.test.de^" foo"
        #pragma unused url
    #endif

    #if defined ARRAY_CELL_ASSIGN_NOSELF
        new g_Stalker[10]

        for (new i=1; i<sizeof g_Stalker; i++)
            if (g_Stalker[0] <= g_Stalker[i])
                g_Stalker[0] = g_Stalker[i]
    #endif

    #if defined MULTIDIM_ARRAY_VARDIM
        new bad[2][3][] =
            [
                [
                    [ 1, 2 ],
                    [ 3, 4, 5 ],
                    [ 6, 7, 8, 9 ]
                ],
                [
                    [ 1 ],
                    [ 2 ],
                    [ 3 ]
                ]
            ]

        assert(bad[0][0][0]==1);
        assert(bad[0][0][1]==2);
        assert(bad[0][1][0]==3);
        assert(bad[0][1][1]==4);
        assert(bad[0][1][2]==5);
        assert(bad[0][2][0]==6);
        assert(bad[0][2][1]==7);
        assert(bad[0][2][2]==8);
        assert(bad[0][2][3]==9);

        assert(bad[1][0][0]==1);
        assert(bad[1][1][0]==2); /* First error here */
        assert(bad[1][2][0]==3);
    #endif

    #if defined ARRAY_VARDIM_INCOMPLETE
        new iconfiles[5][] = [
            ''Image\\Bubbles\\32x32_dudaspray.tga'',
            ''Image\\Bubbles\\32x32_banan.tga'',
            ''Image\\Bubbles\\32x32_bananhej.tga'',
            ''Image\\Bubbles\\32x32_festekoldo.tga''
        ];
        iconfiles[0][0] = 0;    /* to avoid a warning, and verify that the array was properly declared */
    #endif

    #if defined ARRAY_VARDIM_WRONGBRACKETS
        new iconfiles[5][] = {
            ''Image\\Bubbles\\32x32_dudaspray.tga'',
            ''Image\\Bubbles\\32x32_banan.tga'',
            ''Image\\Bubbles\\32x32_bananhej.tga'',
            ''Image\\Bubbles\\32x32_festekoldo.tga''
        };
        iconfiles[0][0] = 0;    /* to avoid a warning, and verify that the array was properly declared */
    #endif

    #if defined ARRAY_VARDIM_NONINIT
        new eventname[1][]
        eventname[0][0] = 0;    /* just to avoid a warning */
    #endif

    #if defined LOCAL_STATE_VAR
        new local_state_var <mystate>           // error 88
        local_state_var = local_state_var + 1   // to avoid a compiler warning
    #endif
    #if defined PUBLIC_STATE_VAR
        public_state_var = 1                    // to avoid a compiler warning
    #endif
    #if defined INITIALIZED_STATE_VAR
        state mystate
        set_state_var 1
    #endif

    #if defined FOR_MIX_EXPR_VAR
        new c
        for (new a = 1, new b = 2; a < b; a++, b--)
            c = a + b + c
    #endif

    #if defined AMBIGUOUS_CALL
        new a, b
        increment (a > b) ? a : b
    #endif

    #if defined ARRAY_LOGIC_EXPR
        new array[32]

        if (array || 32)
            array[0] = 1;
    #endif

    #if defined RECURSION_DETECTION
        recurse1 10
        recurse2a 10
    #endif

    #if defined ARRAY_TOO_LARGE
        new big[1000][1000][2000]
        big[0][0][0] = 10
    #endif

    #if defined CHAINED_ASSIGN
        new bool: chglist = false
        new bool: to_parent = false
        if (!chglist)
            chglist = to_parent = true
        assert chglist
        assert to_parent
    #endif

    #if defined STRING_DBL_ESCAPE
        new name{10} = "\\"
        #pragma unused name
    #endif

    #if defined TERNARY_LIT_ARRAY
        new expression = 0
        new blah[8]
        blah = (expression) ? ''oh'' : ''22222222222222222''
    #endif

    #if defined TERNARY_1D_ARRAY
        new expression = 0
        new blah[8]
        new x[3]
        new y[9]
        blah = (expression) ? x : y
    #endif

    #if defined ARRAY2D_NO_COMMA
        Filenames[0][0] = 'b'   // to avoid warning 203 and the stripping
                                // of variable "Filenames"
    #endif

    #if defined SIZEOF_PSEUDO_ARRAY
        new array[.abc[4], .def]
        array.def = 0   /* to avoid a warning */
        #assert sizeof array == 5       /* 5 cells in the array definition */
        #assert sizeof array[.abc] == 4 /* 4 cells in the pseudo-array */
        #assert sizeof array.abc == 4   /* alternative syntax */
    #endif

    #if defined ELSE_NO_IF
        new a = 1, b = 2
        if (a > 1)
            {
            a = 0
            }
        else (a < b)
            {
            b = a
            }
        #endif

    #if defined INVALID_STATIC_2D_PACKED
        static monthnames[2]{4} = { "Jan", "Feb" }
        static confirmtext[2]{4} = { "Yes", "No " }
    #endif

    #if defined CLEAR_STRING
        new name{} = "monkey"
        clear name
    #endif

    #if defined PSEUDO_PACKED_ARRAY_PACKED
        new arr[.a, .b, .c{8}]
        arr.c{0} = 0
    #endif
    #if defined PSEUDO_PACKED_ARRAY_UNPACKED
        new arr[.a, .b, .c{8}]
        arr.c[0] = 0
    #endif
    }

#if defined LOCAL_SHADOWS
forward test_shadow(abc);
public test_shadow(abc)
    {
    new main = abc;     /* shadows a function */
    new abc = main;     /* shadows a parameter */
    return abc;         /* avoid warning "assigned a value that is not used" */
    }
#endif

#if defined LITERAL_QUEUE
new  b[4][5] = {
   { 1, 2, 1, 55, 5 },
   { 3, 4, 1, 24, 6 },
   { 5, 6, 1, 55, 7 },
   { 5, 6, 1, 55, 8}
}
#endif

#if defined PUBLIC_NOT_DECLARED
forward public dummyfunc()

dummyfunc()
    return 0
#endif

#if defined PUBLIC_NOT_FORWARDED
forward dummyfunc()

public dummyfunc()
    return 0
#endif
