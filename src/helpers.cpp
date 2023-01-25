#include "helpers.h"

float randomFloat()
{
    return (float)(rand()) / (float)(RAND_MAX);
}

float randomFloat(int a, int b)
{
    if (a > b)
        return randomFloat(b, a);
    if (a == b)
        return a;

    return (float)randomInt(a, b) + randomFloat();
}

int randomInt(int a, int b)
{
    if (a > b)
        return randomInt(b, a);
    if (a == b)
        return a;

    return a + (rand() % (b - a));
}