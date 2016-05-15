/* TEST11
 *
 * This file is for regression testing of the Pawn compiler and the abstract
 * machine. It contains conditionally compiled segments that must certify
 * the correct behaviour of the compiler or the abstract machine. Note that
 * "correct behaviour" may mean that the compiler or the abstract machine
 * aborts with an error.
 */

#define Rect[
    .left, .top,
    .right, .bottom
    ]

#define Vector[
    .orgx, .orgy,
    .dx, .dy
    ]

#if defined ARRAY_INDEX_TAG_ASSIGN2
#define token[
    .t_type,           /* operator or token type */
    .t_word[20],       /* raw string */
    ]
#endif

#if defined ARRAY_INDEX_TAG_DECL
forward fliprect(rect[Vector])
#endif

#if defined ARRAY_INDEX_TAG_DECL || defined ARRAY_INDEX_TAG_CALL
fliprect(rect[Rect])
    {
    new t

    t = rect[.left]
    rect[.left] = rect[.right]
    rect[.right] = t

    t = rect[.top]
    rect[.top] = rect[.bottom]
    rect[.bottom] = t
    }
#endif

#if defined CLEAR_STRING1
    clear(name{}, size=sizeof name)
        {
        name = ""
        return size
        }
#endif
#if defined CLEAR_STRING2
    clear(name[])
        {
        name = ``''
        }
#endif

main()
    {
    #if defined ARRAY_INDEX_TAG_ASSIGN
        new v[Vector]
        new r[Rect]
        v = r
    #endif

    #if defined ARRAY_INDEX_TAG_ASSIGN2
        new word[20] = ''123''
        new field[token]
        field[.t_word] = word
    #endif

    #if defined ARRAY_INDEX_TAG_CALL
        new v[Vector]
        fliprect(v)
    #endif

    #if defined ARRAY_INDEX_TAG_DECL
        new v[Vector]
        fliprect v      // just to avoid a spureous warning
    #endif

    #if defined PSEUDO_PACKED_ARRAY_PACKED
        new arr[.a, .b, .c{8}]
        arr.c{0} = 0
    #endif
    #if defined PSEUDO_PACKED_ARRAY_UNPACKED
        new arr[.a, .b, .c{8}]
        arr.c[0] = 0
    #endif

    #if defined CLEAR_STRING1 || defined CLEAR_STRING2
        new name{} = "monkey"
        clear name
    #endif
    }
