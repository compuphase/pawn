#include <string>

main()
    {
    new string[40], sub[40]
    string = ''Hello world''
    assert strcmp(string, ''Hello world'') == 0
    assert strcmp(string, ''hello World'') != 0
    assert strcmp(string, ''hello World'', true) == 0
    assert strcmp(string, ''Hello'', _, 5) == 0
    assert strcmp(string, ''Hello'', _, 6) != 0

    sub = ''wor''
    assert strfind(string, sub) == 6
    sub = ''WOR''
    assert strfind(string, sub) == -1
    assert strfind(string, sub, true) == 6
    }

