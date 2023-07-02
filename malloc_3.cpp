#include <unistd.h>
#include <cstring>
#include <sys/mman.h>
#include <math.h>
#include <time.h>

#define MAX_ORDER 10
#define MAX_HEAP_BLOCK_SIZE (128 * 1024)
#define START_BLOCK_AMOUNT 32
#define BLOCK_SIZE(order) (2<<(order))*64
struct MallocMetadata
{
    int cookie;
    size_t size;
    bool is_free;
    MallocMetadata *next;
    MallocMetadata *prev;
};

//MallocMetadata *free_list;
size_t num_of_free_blocks = 0;
size_t num_of_free_bytes = 0;
size_t num_of_allocated_blocks = 0;
size_t num_of_allocated_bytes = 0;
size_t num_of_metadata_bytes = 0;
int global_cookie_value = 0;

class MetaList {
    MallocMetadata* head;
    public:
    MetaList();
    void insert(MallocMetadata* data);
    MallocMetadata* remove();
    MallocMetadata* select();
    bool is_empty() const;
};

MetaList::MetaList() {
    head=NULL;
}

bool MetaList::is_empty() const {
    return head==NULL;
}

// Insert a MallocMetadata to the list, sorted
void MetaList::insert(MallocMetadata* metadata) {
    if (metadata->cookie != global_cookie_value)
        exit(0xdeadbeef);
    if (head == NULL) {
        head = metadata;
        metadata->next=NULL;
        metadata->prev=NULL;
        return;
    }

    if (head > metadata)
    {
        metadata->next = head;
        metadata->prev=NULL;
        head->prev = metadata;
        head = metadata;
        return;
    }
    MallocMetadata* current=head;
    if(current->cookie!=global_cookie_value)
        exit(0xdeadbeef);
    while (current->next != NULL)
    {
        if(current->cookie!=global_cookie_value)
            exit(0xdeadbeef);
        if (current < metadata && current->next > metadata)
        {
            MallocMetadata *temp = current->next;
            current->next = metadata;
            metadata->next = temp;
            temp->prev = metadata;
            metadata->prev = current;
            return;
        }
        current = current->next;
    }
    current->next = metadata;
    metadata->prev = current;
    metadata->next=NULL;
}

MallocMetadata* MetaList::remove()
{
    MallocMetadata* result=head;
    if (result->cookie != global_cookie_value)
        exit(0xdeadbeef);
    if (result!=NULL) {
        head=result->next;
        result->next = NULL;
        result->prev = NULL;
    }
    return result;
}

MallocMetadata* MetaList::select()
{
    MallocMetadata* result = remove();
    if (result->cookie != global_cookie_value)
        exit(0xdeadbeef);
    if(result!=NULL){
        result->is_free = false;
        --num_of_free_blocks;
        num_of_free_bytes-=result->size;
    }
        
    return result;
}

class BuddyChan
{
    private:
        MetaList arr[MAX_ORDER+1];
        BuddyChan();
        int tight(size_t size) const;

    public:
        static BuddyChan &getInstance() {
            static BuddyChan instance;
            return instance;
        }
        static MallocMetadata *use_block(size_t size);
        static void insert_chan(MallocMetadata* to_insert);
        static bool possible_combine(MallocMetadata* metadata, size_t requested_size);
        static MallocMetadata* merge(MallocMetadata* metadata, size_t requested_size);
};

BuddyChan::BuddyChan()
{
    srand(time(0));
    global_cookie_value= (rand());
    void *current = sbrk(0);
    size_t alignment = MAX_HEAP_BLOCK_SIZE * START_BLOCK_AMOUNT;
    size_t remainder = alignment - ((size_t)current % alignment);
    void *after_alignment = sbrk(remainder);
    void *addr = sbrk(MAX_HEAP_BLOCK_SIZE * START_BLOCK_AMOUNT);
    
    MallocMetadata *first = (MallocMetadata *)addr;
    first->size = alignment / START_BLOCK_AMOUNT;
    first->is_free = true;
    arr[MAX_ORDER].insert(first);
    MallocMetadata *prev = first;
    for (int i = 1; i < START_BLOCK_AMOUNT; i++)
    {
        MallocMetadata *current = (MallocMetadata *)((char *)first + (i*128 * 1024));
        current->size = alignment / START_BLOCK_AMOUNT;
        current->is_free = true;
        current->cookie = global_cookie_value;
        arr[MAX_ORDER].insert(current);
    }
    num_of_free_blocks = START_BLOCK_AMOUNT;
    num_of_free_bytes = START_BLOCK_AMOUNT * (MAX_HEAP_BLOCK_SIZE - sizeof(MallocMetadata));
    num_of_allocated_blocks = START_BLOCK_AMOUNT;
    num_of_allocated_bytes = START_BLOCK_AMOUNT * (MAX_HEAP_BLOCK_SIZE - sizeof(MallocMetadata));
}

// Returns the order of the tightest block available. Returns -1 if not found.
int BuddyChan::tight(size_t size) const
{
    int tight=-1;
    for (int order = 0; order<=MAX_ORDER; order++)
    {
        if (BLOCK_SIZE(order) < size)
            continue;
        if (arr[order].is_empty())
            continue;
        tight=order;
    }
    return tight;
}

MallocMetadata *BuddyChan::use_block(size_t size)
{
    BuddyChan &buddy = getInstance();
    
    int order=buddy.tight(size);
    if(order==-1) 
        return NULL;
    while ((BLOCK_SIZE(order-1) >= size)&&(order != 0))
    {
        ++num_of_free_blocks;
        ++num_of_allocated_blocks;
        num_of_metadata_bytes+=sizeof(MallocMetadata);
        num_of_free_bytes-=sizeof(MallocMetadata);
        num_of_allocated_bytes-=sizeof(MallocMetadata);

        MallocMetadata* metadata1=buddy.arr[order].remove();
        if (metadata1->cookie != global_cookie_value)
            exit(0xdeadbeef);
        (metadata1->size)/=2;
        metadata1->is_free=true;
        // split into two blocks, sized order/2.
        // change size of metadata, create a new empty one.
        buddy.arr[order-1].insert(metadata1);
        MallocMetadata* metadata2=(MallocMetadata*)((char*)metadata1 + metadata1->size+sizeof(MallocMetadata));
        metadata2->size=metadata1->size;
        metadata2->is_free=true;
        metadata2->cookie = global_cookie_value;
        buddy.arr[order-1].insert(metadata2);
        order--;
    }
    return buddy.arr[order].select(); // set the matadate to not be free anymore, remove from list. finish! :)
}

// Insert a free block to the Allocator, combining buddies if needed
void BuddyChan::insert_chan(MallocMetadata* to_insert) {
    if(to_insert->cookie!=global_cookie_value)
        exit(0xdeadbeef);
    BuddyChan& chan = BuddyChan::getInstance();
    int order=log(to_insert->size)/log(2) - 7;
    MallocMetadata* buddy;
    num_of_free_blocks++;
    num_of_free_bytes+=to_insert->size-sizeof(MallocMetadata);
    bool first_time = true;
    if(order==MAX_ORDER){
        chan.arr[order].insert(to_insert);
        return;
    }
        
    while (order!=MAX_ORDER) {
        buddy=(MallocMetadata*)(((size_t)to_insert)^(to_insert->size));
        if(buddy->cookie!=global_cookie_value)
            exit(0xdeadbeef);
        if (!buddy->is_free) {
            chan.arr[order].insert(to_insert);
            break;
        }
        // Combine two buddies
        if (buddy->prev!=NULL)
            buddy->prev->next=buddy->next;
        if(buddy->next!=NULL)
            buddy->next->prev = buddy->prev;
        buddy->next=NULL;
        buddy->prev=NULL;
        if (to_insert > buddy)
            to_insert=buddy;
        if(to_insert->cookie!=global_cookie_value)
            exit(0xdeadbeef);
        if(!first_time){
            num_of_free_bytes+=to_insert->size-sizeof(MallocMetadata);
        }
        first_time = false;
        num_of_allocated_blocks--;
        num_of_allocated_bytes+=sizeof(MallocMetadata);
        (to_insert->size)*=2;
        chan.arr[order+1].insert(to_insert);
        num_of_metadata_bytes-=sizeof(MallocMetadata);
        --num_of_free_blocks;
        ++order;
    }
}

bool BuddyChan::possible_combine(MallocMetadata* metadata, size_t requested_size)
{
    BuddyChan& chan = BuddyChan::getInstance();
    int order=log(metadata->size)/log(2) - 7;
    MallocMetadata* buddy;
    if(order==MAX_ORDER){
        return false;
    }
    int curr_size = metadata->size;
    while (order!=MAX_ORDER) {
        buddy=(MallocMetadata*)(((size_t)metadata)^(curr_size));
        if (buddy->cookie != global_cookie_value)
            exit(0xdeadbeef);
        if(metadata->cookie!=global_cookie_value)
            exit(0xdeadbeef);
        if (!buddy->is_free) {
            return false;
        }
        if (metadata > buddy)
            metadata=buddy;
        curr_size*=2;
        if(curr_size>=requested_size)
            return true;
        ++order;
    }
    return false;
}

MallocMetadata* BuddyChan::merge(MallocMetadata* metadata, size_t requested_size)
{
    BuddyChan& chan = BuddyChan::getInstance();
    int order=log(metadata->size)/log(2) - 7;
    MallocMetadata* buddy;
    while (order!=MAX_ORDER) {
        buddy=(MallocMetadata*)(((size_t)metadata)^(metadata->size));
        if (buddy->cookie != global_cookie_value)
            exit(0xdeadbeef);
        if(metadata->cookie!=global_cookie_value)
            exit(0xdeadbeef);
        if (buddy->prev!=NULL)
            buddy->prev->next=buddy->next;
        if(buddy->next!=NULL)
            buddy->next->prev = buddy->prev;
        buddy->next=NULL;
        buddy->prev=NULL;
        if (metadata > buddy)
            metadata=buddy;
        num_of_allocated_blocks--;
        num_of_allocated_bytes+=sizeof(MallocMetadata);
        num_of_free_bytes-=metadata->size-sizeof(MallocMetadata);
        (metadata->size)*=2;
        if(metadata->size>=requested_size){
            return metadata;
        }
        num_of_metadata_bytes-=sizeof(MallocMetadata);
        --num_of_free_blocks;
        ++order;
    }
    return NULL;
}

void *smalloc(size_t size)
{
    if (size == 0)
    {
        return NULL;
    }
    void* ret_addr = NULL;
    if (size + sizeof(MallocMetadata) <= MAX_HEAP_BLOCK_SIZE)
    {
        ret_addr = BuddyChan::use_block(size+sizeof(MallocMetadata));
    }
    else
    {
        ret_addr = mmap(NULL, size+sizeof(MallocMetadata), PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
        if(ret_addr==MAP_FAILED)
            return NULL;
        ++num_of_allocated_blocks;
        num_of_allocated_bytes+=size;
        num_of_metadata_bytes+=sizeof(MallocMetadata);
    }
    return (char*)ret_addr + sizeof(MallocMetadata);
}

void *scalloc(size_t num, size_t size)
{
    if (size == 0 || num == 0)
        return NULL;
    void *addr = smalloc(num * size);
    if (addr == NULL)
        return NULL;
    memset(addr, 0, size * num);
    return addr;
}

void sfree(void *p)
{
    if (p == NULL)
        return;
    MallocMetadata *metadata = (MallocMetadata *)((char *)p - sizeof(MallocMetadata));
    if(metadata->cookie!=global_cookie_value)
        exit(0xdeadbeef);
    if (metadata->is_free == true)
        return;
    if(metadata->size<=MAX_HEAP_BLOCK_SIZE)
        BuddyChan::insert_chan(metadata);
    else{
        munmap(metadata, metadata->size); // error? if so, also add perror to mmap
        num_of_allocated_bytes-=metadata->size-sizeof(MallocMetadata);
        num_of_metadata_bytes-=sizeof(MallocMetadata);
        num_of_allocated_blocks--;
    }
}

void *srealloc(void *oldp, size_t size)
{
    if (size == 0)
    {
        return NULL;
    }
    if (oldp == NULL)
    {
        return smalloc(size);
    }
    MallocMetadata *metadata = (MallocMetadata *)((char *)oldp - sizeof(MallocMetadata));
    if(metadata->cookie!=global_cookie_value)
        exit(0xdeadbeef);
    
    if(metadata->size<=MAX_HEAP_BLOCK_SIZE)
    {
        if (size+sizeof(MallocMetadata) <= (metadata->size))
            return oldp;   
        if(BuddyChan::possible_combine(metadata, size+sizeof(MallocMetadata))){
            MallocMetadata* addr=BuddyChan::merge(metadata, size+sizeof(MallocMetadata));
            memmove((char*)addr+sizeof(MallocMetadata), oldp, ((MallocMetadata*)oldp)->size-sizeof(MallocMetadata));
            return (char*)addr+sizeof(MallocMetadata);
        }
    }
    else{
        if(metadata->size==size+sizeof(MallocMetadata))
            return oldp;
    }
    void *addr = smalloc(size);
    if (addr == NULL)
    {
        return NULL;
    }
    memmove(addr, oldp, metadata->size-sizeof(MallocMetadata));
    sfree(oldp);
    return addr;if (size+sizeof(MallocMetadata) <= (metadata->size))
        return oldp;   
}

size_t _num_free_blocks()
{
    return num_of_free_blocks;
}

size_t _num_free_bytes()
{
    return num_of_free_bytes;
}

size_t _num_allocated_blocks()
{
    return num_of_allocated_blocks;
}

size_t _num_allocated_bytes()
{
    return num_of_allocated_bytes;
}

size_t _num_meta_data_bytes()
{
    return num_of_metadata_bytes;
}

size_t _size_meta_data()
{
    return sizeof(MallocMetadata);
}
