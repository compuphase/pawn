#include <console>

forward @testfunc(a);
@testfunc(a)
    return a+1;

/*
main()
{
    @testfunc(1);
}

