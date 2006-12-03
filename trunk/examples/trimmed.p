#include <float>

public Float: CalculateMean(Float: values[], items)
    {
    /* return a "trimmed mean" by throwing out the minimum and
     * the maximum value and calculating the mean over the remaining
     * items
     */
    assert items >= 3    /* should receive at least three elements */

    new Float: minimum = values[0]
    new Float: maximum = values[0]
    new Float: sum = 0.0
    for (new i = 0; i < items; i++)
        {
        if (minimum > values[i])
            minimum = values[i]
        else if (maximum < values[i])
            maximum = values[i]
        sum += values[i]
        }

    return (sum - minimum - maximum) / (items - 2)
    }
