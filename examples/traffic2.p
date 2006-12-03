/* a more realistic traffic light synchronizer, including an
 * "override" for emergency vehicles
 */
#include <time>

main()
    state green_wait_interim

new bool: button_memo <red_wait, green_wait_interim, yellow_wait>

@keypressed(key)
    {
    switch (key)
        {
        case ' ': button_press
        case '*': mirt_detect
        }
    }

button_press() <green_wait>
    state yellow_wait

button_press() <red_wait, green_wait_interim>
    button_memo = true

button_press() <>               /* fallback */
    {}

mirt_detect()
    state mirt_override

@timer() <yellow_wait>
    state red_walk

@timer() <red_walk>
    state red_wait

@timer() <red_wait>
    state green_wait_interim

@timer() <green_wait_interim>
    {
    state (!button_memo) green_wait
    state (button_memo) yellow_wait
    }

@timer() <mirt_override>
    state green_wait

@timer() <>                     /* fallback */
    {}


entry() <green_wait_interim>
    {
    print "Green / Don't walk\n"
    settimer 5000
    }

entry() <yellow_wait>
    {
    print "Yellow / Don't walk\n"
    button_memo = false
    settimer 2000
    }

entry() <red_walk>
    {
    print "Red / Walk\n"
    settimer 5000
    }

entry() <red_wait>
    {
    print "Red / Don't walk\n"
    settimer 2000
    }

entry() <mirt_override>
    {
    print "Green / Don't walk\n"
    settimer 5000
    }
