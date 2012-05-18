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
    }
