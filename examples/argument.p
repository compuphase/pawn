#include <args>

@start()
    {
    printf "Argument count = %d\n", argcount()

    var opt{100}
    for (var index = 0; argindex(index, opt); index++)
        printf "Argument %d = %s\n", index, opt
    }
