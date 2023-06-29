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

    return (char*)old_break+sizeof(MallocMetadata);
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

void sfree(void* p) {
    if (p==NULL)
        return;
    
    MallocMetadata* metadata=(MallocMetadata*)((char*)p-sizeof(MallocMetadata));
    if (metadata->is_free==true)
        return;
    metadata->prev = NULL;
    metadata->next = NULL;
    MallocMetadata* current = free_list;
    //empty
    num_of_free_blocks++;
    num_of_free_bytes+=metadata->size;
    if(free_list==NULL){
        free_list=metadata;
        return;
    }
    if(free_list>metadata){
        metadata->next = free_list;
        free_list->prev = metadata;
        free_list = metadata;
        return;
    }
    while(current->next!=NULL){
        if(current<p && current->next>p){
            MallocMetadata* temp=current->next;
            current->next=metadata;
            metadata->next=temp;
            temp->prev=metadata;
            metadata->prev=current;
            return;
        }
        current = current->next;
    }
    current->next=metadata;
    metadata->prev=current;
}

void* srealloc(void* oldp, size_t size) {
    if(size==0||size>MAX_SMALLOC){
        return NULL;
    }
    if (oldp==NULL) {
        return smalloc(size);
    }
    MallocMetadata* metadata=(MallocMetadata*)((char*)oldp-sizeof(MallocMetadata));
    if (size <= (metadata->size))
        return oldp;
    void* addr=smalloc(size);
    if(addr==NULL){
        return NULL;
    }
    memmove(addr, oldp, metadata->size);
    sfree(oldp);
    return addr;
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
