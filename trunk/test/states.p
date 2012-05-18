
#if defined NAMED_AUTOMATON
forward @keypressed(key);

main()
    state foo:plain

@keypressed(key) <foo:plain>
    {
    state (key == '/') foo:plain
    printf ''%c'', key
    }
#endif

