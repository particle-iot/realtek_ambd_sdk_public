/**************************************************
 * malloc/free/realloc wrap for gcc compiler
 *
 **************************************************/
#if defined(__GNUC__)
#include "FreeRTOS.h"

//wrap malloc to pvPortMalloc
#if (defined(configUSE_PSRAM_FOR_HEAP_REGION) && ( configUSE_PSRAM_FOR_HEAP_REGION == 1 ))
void *__wrap_malloc(size_t size)
{
        return pvPortMalloc(size);
}

void *__wrap_realloc(void *p, size_t size)
{
        return pvPortReAlloc(p, size);
}

void __wrap_free(void *p)
{
        vPortFree(p);
}

void *__wrap_calloc(size_t xWantedCnt, size_t xWantedSize)
{
        return pvPortCalloc(xWantedCnt, xWantedSize);
}
#endif

#endif