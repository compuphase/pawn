#include <string>

main()
    {
    new abc{8}

    strpack abc, "1"
    strcat abc, "abcdefghij"
    printf "%d: %s\n", strlen(abc), abc

    strpack abc, "12"
    strcat abc, "abcdefghij"
    printf "%d: %s\n", strlen(abc), abc

    strpack abc, "123"
    strcat abc, "abcdefghij"
    printf "%d: %s\n", strlen(abc), abc

    strpack abc, "1234"
    strcat abc, "abcdefghij"
    printf "%d: %s\n", strlen(abc), abc


    new def{80}

    strpack def, "1"
    strcat def, "abcdefghij"
    printf "%d: %s\n", strlen(def), def

    strpack def, "12"
    strcat def, "abcdefghij"
    printf "%d: %s\n", strlen(def), def

    strpack def, "123"
    strcat def, "abcdefghij"
    printf "%d: %s\n", strlen(def), def

    strpack def, "1234"
    strcat def, "abcdefghij"
    printf "%d: %s\n", strlen(def), def

    }
