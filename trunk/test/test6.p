#include <console>

forward @testfunc(a);
@testfunc(a)
    return a+1;

#if 0
main()
{
    @testfunc(1);
}

