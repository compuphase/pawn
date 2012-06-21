#include <core>
#include <console>



#if defined ASSERT_LINENO
/* NOTE: make sure the function heading of ispacked stays at line 10, see
 *       test 3 in test.rexx (and ASSERT_LINENO below)
 */
bool: ispacked(string[])        /* line 10 */
    return bool: (string[0] > charmax);
#endif

#if defined PARTIAL_ARRAY_SIZE
  #include <float>
  new Array[0.5]
#endif
#if defined COUNT_ARRAY_SIZE
    new Array[][3] =
        [
        	[ 123, 456, 789 ],
        	[ 987, 654, 321 ]
        ]
#endif

#if defined DECLARATION_POSITION
  #include "menu.inc"
#endif

#if defined UNBALANCED_BRACES
  native clam();
#endif

#if defined REFERENCE_ARG
func(&va)
{
    printf("func1: %d\n",va);
    printf("func2: %d\n",va);
    va++;
}
#endif

#if defined DEF_ARRAY_ARG
DefArg(const cStruc[]='''')
    return cStruc[0]    /* just to avoid a warning */
#endif

#if defined DOUBLE_NATIVE_DECLARE
native printf(const format[], {Float,Fixed,_}:...)
#endif

#if defined CALL_BEFORE_DEF
Test()
    return 0
#endif

#if defined RETURN_WITH_INCR
increment(a)
    return ++a
#endif

#if defined CONST_WRITE_USAGE
const
    {
    PARAM_CELL=1,
    PARAM_CELL_REF,
    PARAM_STRING,
    PARAM_ARRAY
    };
native RegPublicFunction(ParameterCount, ...);
#endif

#if defined TAGOF_PARAM
    test({Sentient,Foo,_}:param,paramTag = tagof param)
    {
	if (paramTag == tagof Sentient:)
		printf("Passed a Sentient %d\n",_:param);
	else if (paramTag == tagof Foo:)
		printf("Passed a Foo %d\n",_:param);
	else
		printf("Passed something else %d\n",_:param);
    }
#endif

#if defined COND_EXP_ARRAY
    return_array(c)
        {
        new result[2]
        result[0] = '0' + c
        return result
        }
#endif

#if defined NATIVE_RET_ARRAY
    native netsocket[1](value);
#endif

#if defined STRINGIZE_OPER
    #define log(%1) "logging: " ... #%1 ... "\n"
#endif


#if defined PACKED_OPCODE_LIMITS
    printvalue(d, const s[])
        printf "%d (should be %s)\n", d, s
#endif


main()
    {
    #if defined ASSERT_LINENO
        assert ispacked(''unpacked'');
    #endif

    #if defined CASCADED_ASSIGN
        new a = 4, b = 5;
        a ^= b ^= a ^= b;
        printf(''a==%d, b==%d  (should be a==5, b==4)\n'', a, b);
    #endif

    #if defined LINE_CAT
    print(''hello'');     \
    print(''\n'');        // a somewhat long line
    #endif

    #if defined REFERENCE_ARG
        new test=40;
        printf(''before: %d\n'',test);
        func(test);
        printf(''after: %d\n'',test);
    #endif

    #if defined UNTERMINATED_STRING
        /* generated the error "missing terminating quote", but then stayed
         * in the loop for parsing parameters, because printf() has a
         * variable number of parameters and the closing ')' was not found
         * (the closing ')' was assumed to be part of the literal string)
         */
        printf(''Bye\n);
    #endif

    #if defined IMPLICIT_POSTFIX
        new a, b = 4
        a = 3
        ++b; ++a
        printf(''%d %d\n'', a, b)
    #endif

    #if defined REDUNDANT_TEST
        if (1==1)
            print("Hello ")
        print("world\n")
    #endif

    #if defined REDUNDANT_CODE
        if (1==2)
            print("NOT ")
        print("Okay\n")
    #endif

    #if defined COND_EXPR_CONST
        new str{} = "monkey"
        new index
        for (index = 0; str{index}; index += 1)
            if ('e' == str{index})
                break
        new result = str{index} ? index : -1
        printf("Result: %d\n", result)
    #endif

    #if defined DEF_ARRAY_ARG
        new cRef = DefArg()
        printf(''Result: %d\n'', cRef)
    #endif

    #if defined COND_OPER_ARRAY
      new a = 1
      new str[] = ''Second''
      printf(''result = %s\n'', a ? ''First'' : str)
      printf(''result = %s\n'', !a ? ''First'' : str)
    #endif

    #if defined POST_INCREMENT_REF
      new pos = 0
      printf(''Before: %d\n'', pos++ )
      printf(''After:  %d\n'', pos )
    #endif

    #if defined NEGATIVE_INDEX1
        new array[10] = [ 1, 2, ... ]
        new v = array[-1]
        v = v + 1       // to avoid a compiler warning
    #endif
    #if defined NEGATIVE_INDEX2
        new array[10] = [ 1, 2, ... ]
        printf(''field %d = %d\n'',-1,array[-1])
    #endif
    #if defined NEGATIVE_INDEX3
        new array[10] = [ 1, 2, ... ]
        new i = -1
        new v = array[i]
        v = v + 1       // to avoid a compiler warning
    #endif
    #if defined NEGATIVE_INDEX4
        new array[10] = [ 1, 2, ... ]
        new i = -1
        printf(''field %d = %d\n'',i,array[i])
    #endif

    #if defined DOUBLE_NATIVE_DECLARE
        printf ''Hello world\n''
    #endif

    #if defined CALL_BEFORE_DEF
        new x;
        switch(x)
            {
            case 1: x = Test()
            }
	printf(''bug'')
    #endif

    #if defined SWITCH_SIDE_EFFECT
        new a = 1
        switch (a++)
            {
            case 0:
                print ''zero''
            case 1:
                print ''one''
            case 2:
                print ''two''
            }
        print '' (should be one)\n''
        printf ''exit = %d (should be 2)\n'', a
    #endif

    #if defined CHAINED_ARRAY_ASSIGN
        new a[3]
        a[0] = a[1] = a[2] = 5
        printf ''%d %d %d\n'', a[0], a[1], a[2]
    #endif

    #if defined CONST_WRITE_USAGE
        RegPublicFunction(2, PARAM_CELL, PARAM_CELL);
    #endif

    #if defined SYMBOL_TOO_LONG_FOR_OPTIMIZER
        new desired_velocity_towards_A
        printf ''%d\n'', desired_velocity_towards_A
    #endif

    #if defined TAGOF_PARAM
	test(Sentient:2);
	test(Foo:1);
	test(0);
    #endif

    #if defined MACRO_PARM_INSTR
        #define TEST    ''%1test''
        print TEST
    #endif

    #if defined MACRO_PARM_UNKNOWN
        #define TEST    %1+4
        printf ''%d'', TEST
    #endif

    #if defined FLOORED_DIVISION
        new n = 8, q = 3, n2 = 9
        printf ''\t(%+d,%+d)  q=%+d r=%+d\n'', n, q, n / q, n % q
        printf ''\t(%+d,%+d)  q=%+d r=%+d\n'', n, -q, n / -q, n % -q
        printf ''\t(%+d,%+d)  q=%+d r=%+d\n'', -n, q, -n / q, -n % q
        printf ''\t(%+d,%+d)  q=%+d r=%+d\n'', -n, -q, -n / -q, -n % -q
        printf ''\t(%+d,%+d)  q=%+d r=%+d\n'', n2, q, n2 / q, n2 % q
        printf ''\t(%+d,%+d)  q=%+d r=%+d\n'', n2, -q, n2 / -q, n2 % -q
        printf ''\t(%+d,%+d)  q=%+d r=%+d\n'', -n2, q, -n2 / q, -n2 % q
        printf ''\t(%+d,%+d)  q=%+d r=%+d\n'', -n2, -q, -n2 / -q, -n2 % -q
    #endif

    #if defined RETURN_WITH_INCR
        new b
        b = increment(1)
        printf ''%d'', b
    #endif

    #if defined COND_EXP_ARRAY
        new b=0
        new other[]=''123''
        printf ''result=%s\n'', b ? return_array(1) : other
        b=1
        printf ''result=%s\n'', b ? return_array(1) : other
    #endif

    #if defined PRINTF_PCT
        new result = 15
        printf ''queue: %d%%\n'', result
        printf ''queue: %d\n''
    #endif

    #if defined PARTIAL_ARRAY_SIZE
        Array[Float:0] = 0  /* only to avoid a "symbol not used" warning */
    #endif
    #if defined COUNT_ARRAY_SIZE
        printf "size: %d x %d\n", sizeof Array, sizeof Array[]
        Array[0][0] = 0     /* only to avoid a "symbol not used" warning */
    #endif

    #if defined DECLARATION_POSITION
        printf ''hello...\n''
    #endif

    #if defined UNBALANCED_BRACES
        new b=1, c=0
    	if (b)
    	{
    	} else if (c)
    		clam();
    	}
    	new e = clam();
    #endif

    #if defined NATIVE_RET_ARRAY
        new addr[4]
        addr = netsocket(25)
        print addr
    #endif

    #if defined LIT_STRING_CAT
        print ''Eenie '' ... ''Meenie\n''
        print "Meinie " ... "Moe\n"
    #endif

    #if defined LIT_STRING_CAT_MIX
        print ''Eenie '' ... \''Meenie\n''
        print ''Meinie '' ... "Moe\n"
    #endif

    #if defined ENUM_ELLIPSIS
        #define Rect[ .left, .top, .right, .bottom ]
        new r[Rect] = [ -1, ... ]
        printf ''%d, %d, %d, %d\n'', r.left, r.top, r.right, r.bottom
    #endif

    #if defined STRINGIZE_OPER
        print log(test)
    #endif

    #if defined PACKED_OPCODE_LIMITS
        printvalue 32767, "32767"
        printvalue 32768, "32768"
        printvalue -32767, "-32767"
        printvalue -32768, "-32768"
        printvalue -32769, "-32769"
    #endif

    #if defined CONST_REL_CHAINED_OP
        printf ''Result = %d (should be 1)\n'', (0 < 1 <= 2)
    #endif

    #if defined MULTI_DIM_PARTIAL_COUNT
        new const seethis[][2]{8} = [
            ["bye", "hello"],
            ["bye", "hello"],
            ["bye", "hello"],
            ["bye", "hello"],
            ["bye", "hello"]
        ]

        print(seethis[0][1]);
    #endif

    #if defined SUB_CONST_EXPR
	new a = 40;
        new p = 70 - a
        new q = a - 10
	printf "p=%d, q=%d (should be p=30, q=30)\n", p, q;
        assert p == 30 && q == 30
    #endif
    }
