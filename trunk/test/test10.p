#include <float>

#if defined INCOMPLETE_2D_ARRAY
const Float:PI = 3.1415693

new Float:values[11][8] =
[
  [ PI, PI, PI, PI, PI, PI, PI, PI],
//[ PI, PI, PI, PI, PI, PI, PI, PI],
//[ PI, PI, PI, PI, PI, PI, PI, PI],
//[ PI, PI, PI, PI, PI, PI, PI, PI],
//[ PI, PI, PI, PI, PI, PI, PI, PI],
//[ PI, PI, PI, PI, PI, PI, PI, PI],
  [ PI, PI, PI, PI, PI, PI, PI, PI],
  [ PI, PI, PI, PI, PI, PI, PI, PI],
  [ PI, PI, PI, PI, PI, PI, PI, PI],
  [ PI, PI, PI, PI, PI, PI, PI, PI],
  [ PI, PI, PI, PI, PI, PI, PI, PI]
];
#endif

main()
    {
    #if defined INCOMPLETE_2D_ARRAY
        values[1] = values[0]
    #endif
    }

