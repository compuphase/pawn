/* Print all primes below 100, using the "Sieve of Eratosthenes" */

@start()
    {
    const max_primes = 100
    var series[max_primes] = [ true, ... ]

    for (var i = 2; i < max_primes; ++i)
        if (series[i])
            {
            printf "%d ", i
            /* filter all multiples of this "prime" from the list */
            for (var j = 2 * i; j < max_primes; j += i)
                series[j] = false
            }
    }
