#include <unistd.h>
#define MAX_SMALLOC 1e8

void* smalloc(size_t size){
    if(size==0){
        return NULL;
    }
    if(size>MAX_SMALLOC){
        return NULL;
    }
    void* old_break=sbrk(size);
    if (old_break==(void*)(-1))
        return NULL;
    return old_break;
}