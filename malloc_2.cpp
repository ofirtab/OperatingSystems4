#include <unistd.h>
#include <cstring>
#define MAX_SMALLOC 1e8

struct MallocMetadata{
    size_t size;       
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

MallocMetadata* free_list;
size_t num_of_free_blocks = 0;
size_t num_of_free_bytes = 0;
size_t num_of_allocated_blocks = 0;
size_t num_of_allocated_bytes = 0;
size_t num_of_metadata_bytes = 0;



void* smalloc(size_t size){
    if(size==0){
        return NULL;
    }
    if(size>MAX_SMALLOC){
        return NULL;
    }
    if(free_list!=NULL)
    {
        MallocMetadata* current=free_list;
        do{
            if (current->size >= size)
            {
                if (current->next != NULL)
                    (current->next)->prev=current->prev;
                if (current->prev != NULL)
                    (current->prev)->next=current->next;
                
                current->next=NULL;
                current->prev=NULL;
                current->is_free=false;
                num_of_free_blocks--;
                num_of_free_bytes-=current->size;

                return ((char*)current+sizeof(MallocMetadata));
            }
            current = current->next;
        } while(current!=NULL);
    }
    
    void* old_break=sbrk(size+sizeof(MallocMetadata));
        if (old_break==(void*)(-1))
            return NULL;
        ++num_of_allocated_blocks;
        num_of_allocated_bytes+=size;
        num_of_metadata_bytes+=sizeof(MallocMetadata);
        (*(MallocMetadata*)old_break).size = size;
        (*(MallocMetadata*)old_break).next = NULL;
        (*(MallocMetadata*)old_break).prev = NULL;
        (*(MallocMetadata*)old_break).is_free = false;
    
        return old_break;
}

void* scalloc(size_t num, size_t size) {
    if (size==0 || num==0)
        return NULL;
    if (num*size > MAX_SMALLOC)
        return NULL;
    void* addr=smalloc(num*size);
    if (addr==NULL)
        return NULL;
    memset(addr, 0, size*num); // check this.
    return addr;
}

void sfree(void* p)
{
    if (p==NULL||(*(MallocMetadata*)p).is_free==true)
        return;
    (*(MallocMetadata*)p).is_free=true;
    MallocMetadata* current = free_list;
    //empty
    
    if(free_list==NULL){
        free_list=(MallocMetadata*)p;
        num_of_free_blocks++;
        num_of_free_bytes+=((MallocMetadata*)p)->size;
        return;
    }
    while(free_list->next!=NULL){
        if(free_list<p && free_list->next>p){
            // add to list
            return;
        }
    }
    // two edge shit-fuck one put in the beginning one in the last one
}


size_t _num_free_blocks() {
    return num_of_free_blocks;
}

size_t _num_free_bytes() {
    return num_of_free_bytes;
}

size_t _num_allocated_blocks() {
    return num_of_allocated_blocks;
}

size_t _num_allocated_bytes() {
    return num_of_allocated_bytes;
}

size_t _num_meta_data_bytes() {
    return num_of_metadata_bytes;
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}
