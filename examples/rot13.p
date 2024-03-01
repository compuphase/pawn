/* Simple encryption, using ROT13 */

@start()
    {
    printf "Please type the string to mangle: "

    var str[100]
    getstring str, sizeof str, .pack = false
    rot13 str

    printf "After mangling, the string is: \"%s\"\n", str
    }

rot13(string[])
    {
    for (var index = 0; string[index]; index++)
        if ('a' <= string[index] <= 'z')
            string[index] = (string[index] - 'a' + 13) % 26 + 'a'
        else if ('A' <= string[index] <= 'Z')
            string[index] = (string[index] - 'A' + 13) % 26 + 'A'
    }
