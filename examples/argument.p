#include <args>

main()
    {
    printf "Argument count = %d\n", argcount()

    new opt{100}
    for (new index = 0; argindex(index, opt); index++)
        printf "Argument %d = %s\n", index, opt
    }
