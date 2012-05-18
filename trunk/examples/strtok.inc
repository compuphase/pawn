/* extract words from a string (words are separated by white space) */
#include <string>

strtok(const string{}, &index)
    {
    new length = strlen(string)

    /* skip leading white space */
    while (index < length && string{index} <= ' ')
        index++

    /* store the word letter for letter */
    new offset = index                /* save start position of token */
    const wordlength = 20             /* maximum word length */
    new result{wordlength}            /* string to store the word in */
    while (index < length
           && string{index} > ' '
           && index - offset < wordlength)
        {
        result{index - offset} = string{index}
        index++
        }
    result{index - offset} = EOS      /* zero-terminate the string */

    return result
    }
