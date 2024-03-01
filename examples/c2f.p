#include <rational>

@start()
    {
    var Rational: Celsius
    var Rational: Fahrenheit

    print "Celsius\t Fahrenheit\n"
    for (Celsius = 5; Celsius <= 25; Celsius++)
        {
        Fahrenheit = (Celsius * 1.8) + 32
        printf "%r \t %r\n", Celsius, Fahrenheit
        }
    }
