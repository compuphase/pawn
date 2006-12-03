#include <datagram>

@receivestring(const message[], const source[])
    printf "[%s] says: %s\n", source, message

@keypressed(key)
    {
    static string[100 char]
    static index

    if (key == '\e')
        exit                    /* quit on 'Esc' key */

    echo key
    if (key == '\r' || key == '\n' || index char == sizeof string)
        {
        string{index} = '\0'    /* terminate string */
        sendstring string
        index = 0
        string[index] = '\0'
        }
    else
        string{index++} = key
    }

echo(key)
    {
    new string[2 char] = { 0 }
    string{0} = key == '\r' ? '\n' : key
    printf string
    }
