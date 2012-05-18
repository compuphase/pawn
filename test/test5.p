#include <console>

move(from, to, spare, numdisks)
{
    if (numdisks > 1)
        move(from, spare, to, numdisks-1);
    printf("Move disk from pillar %d to pillar %d\n", from, to);
    if (numdisks > 1)
        move(spare, to, from, numdisks-1);
}
main()
{
	new i=63;
	printf("*** for %d\n",i);
t1:
}

