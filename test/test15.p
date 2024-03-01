new gVar;

stock SomeFunction()
{
#if defined main
    gVar = (gVar ? 0 : 1);
#endif
    return 0;
}

main()
{
    return SomeFunction() ? 0 : 1;
}
