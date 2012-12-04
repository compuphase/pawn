/* word count: count words on a string that the user types */
#include <string>

main()
    {
    print "Please type a string: "
    new string[100]
    getstring string, sizeof string, false

    new count = 0

    new word[20]
    new index
    for ( ;; )
        {
        word = strtok(string, index)
        if (strlen(word) == 0)
            break
        count++
        printf "Word %d: '%s'\n", count, word
        }

    printf "\nNumber of words: %d\n", count
    }

strtok(const string[], &index)
    {
    new length = strlen(string)

    /* skip leading white space */
    while (index < length && string[index] <= ' ')
        index++

    /* store the word letter for letter */
    new offset = index                /* save start position of token */
    new result[20]                    /* string to store the word in */
    while (index < length
           && string[index] > ' '
           && index - offset < sizeof result - 1)
        {
        result[index - offset] = string[index]
        index++
        }
    result[index - offset] = EOS      /* zero-terminate the string */

    return result
    }
