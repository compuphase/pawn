#include <rational>

#if defined TAGGED_SYMBOLIC_INDEX
static const Table[][Rational:.key, Rational:.value] =
[
  [1.0, 3.0],
  [2.0, 3.5],
  [3.0, 3.7],
  [4.0, 3.8],
  [5.0, 3.9]
];
#endif

main()
{
    #if defined TAGGED_SYMBOLIC_INDEX
        for (new i; i<sizeof(Table); i++)
	    printf("%d: %r, %r\n", i, Table[i].key, Table[i].value);
    #endif
}
