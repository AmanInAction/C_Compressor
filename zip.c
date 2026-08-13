#include "zipzip.h"

int32 FILESZ;

int8* empty(int32 filesz){
    int32 size;
    int8* p;
    size = (sizeof(struct s_header) + (sizeof(int32) * 4096) + (sizeof(int32) * 4096) + (filesz / 2) + 4);

    p = (int8*)alloc(size);
    if(!p){
        fprintf(stderr, "Failed to allocate memory for empty space\n");
        return 0;
    }
    return p;
}
bool write_file(int8* filename, int8* memspace, int32 size){
    int32 fd;
    signed int ret;
    int32 n;
    int8* p;

    ret = open($c filename, O_WRONLY | O_CREAT, 00644);    
    if(ret < 1){
        return false;
    } else{
        fd = (int32)ret;
    }

    for(p=memspace, n=size; n; p++, n--){
        ret = write($i fd, $c p, 1);
        if(ret != 1){
            close($i fd);
            return false;
        }
    }
    close($i fd);

    return true;    
}
int32 parse_file(int8* dst, int8* src, header* hdr, blocklist* repeated, blocklist* remaining){
    int32 integer, offset, n, nextint;
    int32 *intptr;
    int8* dstptr, *srcptr;
    int16 idx, output;
    int16 repeats;
    int16* int16ptr;

    offset = 0;
    dstptr = dst;
    srcptr = src;

    if(!dst || !src || !hdr || !repeated || !remaining){
        fprintf(stderr, "Cannot parse file: invalid arguments\n");
        return 0;
    }

    for(n = hdr->filesz; n >= 4; dstptr += 2, offset += 2){
        intptr = (int32*)srcptr;
        integer = *intptr;

        idx = block_search(repeated, integer);

        if(idx != NoMatch){
            repeats = 1;

            while(repeats < 15 && n >= ((int32)(repeats + 1) * 4)){
                intptr = (int32*)(srcptr + ((int32)repeats * 4));
                nextint = *intptr;

                if(nextint != integer){
                    break;
                }

                repeats++;
            }

            if(idx > 0x0fff){
                return 0;
            }

            output = (int16)((idx & 0x0fff) | ((repeats & 0x0f) << 12));
            int16ptr = (int16 *)dstptr;
            *int16ptr = output;

            srcptr += ((int32)repeats * 4);
            n -= ((int32)repeats * 4);
            continue;
        }

        idx = block_search(remaining, integer);

        if(idx == NoMatch){
           return 0;
        }
        int16ptr = (int16*)dstptr;
        *int16ptr = idx;
        srcptr += 4;
        n -= 4;
    }

    return offset;
}

int16 block_search(blocklist* haystack, int32 needle){
    int16 n;
    bool found;

    if(!haystack){
        return NoMatch;
    }

    found = false;

    for(n = 0; n < haystack->length; n++){
        if(haystack->blocks[n] == needle){
            found = true;
            break;
        }
    }

    return found ? n : NoMatch; 
}
void copy_bytes(int8* dst, int8* src, int32 size){
    int8 *dptr, *sptr;
    int32 n;
    if(size == 0){
        return;
    }
    if(!dst || !src){
        fprintf(stderr, "Cannot copy bytes: invalid arguments\n");
        return;
    }
    for(n = size, dptr = dst, sptr = src; n; n--, dptr++, sptr++){
        *dptr = *sptr;
    }
}

int32 copy_headers(int8* memspace, header* hdr, blocklist* repeated, blocklist* remaining){
    int32 n, size;
    int8* p;

    if(!memspace || !hdr || !repeated || !remaining){
        fprintf(stderr, "Cannot copy headers: invalid arguments\n");
        return -1;
    }
    n = 0;
    p = memspace;
    size = sizeof(struct s_header);
    copy_bytes(p, $1 hdr, size);

    p += size;
    n += size;

    size = (sizeof(int32) * repeated->length);
    copy_bytes(p, $1 repeated->blocks, size);
    
    p += size;
    n += size;

    size = (sizeof(int32) * remaining->length);
    copy_bytes(p, $1 remaining->blocks, size);
    n += size;

    return n;
}

header* mk_header(int32 filesz, int16 blocklist, int16 uniqlist){
    header *p;
    int16 size;
    size = sizeof(struct s_header);
    p = (header*)alloc(size);
    assert(p);
    zero($1 p, size);

    p->magic[0] = 'Z';
    p->magic[1] = 'Z';
    p->magic[2] = 0;

    p->version = Version;
    p->filesz = filesz;
    p->blocklist = blocklist;
    p->uniqlist = uniqlist;
    
    return p;
}

bool head(int16 idx, amtentry* e, void* arg1, void* arg2){
    int32 max;

    if(!e){
        fprintf(stderr, "Cannot apply head filter: entry is NULL\n");
        return false;
    }
    max = *(int32*)arg2;
    return (idx > max) ? false : true;
}

bool tail(int16 idx, amtentry* e, void* arg1, void* arg2){
    int32 min;

    if(!e){
        fprintf(stderr, "Cannot apply tail filter: entry is NULL\n");
        return false;
    }
    min = *(int32*)arg2;
    return (idx >= min) ? true : false;
}

bool repeated(int16 idx, amtentry* e, void* arg1, void* arg2){
    (void)idx;
    (void)arg1;
    (void)arg2;

    if(!e){
        fprintf(stderr, "Cannot apply repeated filter: entry is NULL\n");
        return false;
    }

    return e->amt > 1;
}

bool nonrepeated(int16 idx, amtentry* e, void* arg1, void* arg2){
    (void)idx;
    (void)arg1;
    (void)arg2;

    if(!e){
        fprintf(stderr, "Cannot apply nonrepeated filter: entry is NULL\n");
        return false;
    }

    return e->amt <= 1;
}

blocklist* filter_blocks(amtlist* list, filterfunc func, void* arg1, void* arg2){

    blocklist* blist;
    int32 idx;
    int32 size;

    if(!list || !func){
        fprintf(stderr, "Cannot filter blocks: invalid arguments\n");
        return (blocklist*)0;
    }
    
    size = sizeof(struct s_blocklist) + (sizeof(int32) * list->length);

    blist = (blocklist*)alloc(size);
    if(!blist){
        fprintf(stderr, "Failed to allocate memory for blocklist\n");
        return (blocklist*)0;
    }
    zero($1 blist, size);

    for(idx=0; idx < list->length; idx++){
        if(func(idx, &list->data[idx], arg1, arg2)){
            blist->blocks[blist->length++] = list->data[idx].block;
        }
    }

    size = sizeof(struct s_blocklist) + (sizeof(int32) * blist->length);
    blist = (blocklist*)ralloc(blist, size);
    if(!blist){
        fprintf(stderr, "Failed to reallocate memory for blocklist\n");
        return (blocklist*)0;
    }
    
    return blist;
}

amtlist* read_blocks(int8* contents, int32 size_){
    int32 *intptr;
    int32 integer;
    int8* p;
    int32 n;
    int32 size;

    if(!contents || size_ <= 0){
        fprintf(stderr, "Cannot read blocks: contents is NULL or size is invalid\n");
        return (amtlist*)0;
    }
    amtlist* list;
    amtentry* e;
    size = size_;
    list = make_amtlist();

    if(!list){
        fprintf(stderr, "Failed to create amtlist for reading blocks\n");
        return list;
    }

    for(p=contents, n=size; n >= 4; p+=4, n-=4){
        intptr = (int32*)p;
        integer = *intptr;
        e = amtsearch(list, integer);

        if(!e){
            add_amt(&list, (amtentry){.block=integer, .amt=1});
        } else {
            e->amt++;
        }
    }

    return list;
}

void amtswap(amtentry* e1, amtentry* e2){
    if(!e1 || !e2){
        return;
    }

    amtentry tmp;

    tmp = *e1;
    *e1 = *e2;
    *e2 = tmp;
}

amtentry *amtsearch(amtlist* haystack, int32 needle){
    if(!haystack){
        fprintf(stderr, "Cannot search: list is NULL\n");
        return NULL;
    }
    for(int32 n = 0; n < haystack->length; n++){
        if(haystack->data[n].block == needle){
            return &haystack->data[n];
        } 
    }
    return NULL;
}

sortres amtsort(amtentry e1, amtentry e2){
    if(e1.amt > e2.amt){
        return LessThan;
    } else if(e1.amt < e2.amt){
        return MoreThan;
    } else {
        return Equal;
    }
}

amtlist* amtlistcopy(amtlist* list){
    if(!list){
        fprintf(stderr, "Cannot copy: list is NULL\n");
        return NULL;
    }
    int32 size;
    amtlist* newlist;

    size = sizeof(struct s_amtlist) + (sizeof(amtentry) * list->capacity);
    newlist = (amtlist*)alloc(size);
    if(!newlist){
        fprintf(stderr, "Failed to allocate memory for copy of amtlist\n");
        return (amtlist*)0;
    }
    zero($1 newlist, size);

    newlist->capacity = list->capacity;
    newlist->length = list->length;
    for(int32 n = 0; n < list->length; n++){
        newlist->data[n] = list->data[n];
    }

    return newlist;
}

bool amtlisteq(amtlist* l1, amtlist* l2){
    if(!l1 || !l2){
        fprintf(stderr, "Cannot compare: one or both lists are NULL\n");
        return false;
    }
    if(l1->length != l2->length){
        return false;
    }
    for(int32 n = 0; n < l1->length; n++){
        if(l1->data[n].block != l2->data[n].block || l1->data[n].amt != l2->data[n].amt){
            return false;
        }
    }
    return true;
}

amtlist* zipsort(amtlist* old, sortfunc func){
    amtlist* new;

    if(!old || !func){
        fprintf(stderr, "Cannot sort: invalid arguments\n");
        return (amtlist*)0;
    }

    new = (amtlist*)zipsort_(old, func);
    if(!new){
        fprintf(stderr, "Failed to sort list\n");
        return old;
    }

    return new;
}
amtlist* zipsort_(amtlist* list_, sortfunc func){
    if(!list_ || !func){
        fprintf(stderr, "Cannot sort: invalid arguments\n");
        return (amtlist*)0;
    }
    int32 len, n;
    sortres res;
    bool swapped;
    amtlist* newlist;

    newlist = amtlistcopy(list_);
    if(!newlist){
        fprintf(stderr, "Failed to copy list for sorting\n");
        return NULL;
    }

    len = newlist->length;
    if(len < 2){
        return newlist;
    }

    do{
        swapped = false;
        for(n = 0; n + 1 < len; n++){
            res = func(newlist->data[n], newlist->data[n + 1]);
            if(res == MoreThan){
                swap(&newlist->data[n], &newlist->data[n + 1]);
                swapped = true;
            }
        }
        len--;
    } while(swapped && len > 1);

    return newlist;
}

bool increase(amtlist **list){
    amtlist* p;
    int64 size;
    int32 len;

    if(!*list || (*list)->capacity >= MaxCap){
        fprintf(stderr, "Cannot increase capacity: list is NULL or already at maximum capacity\n");
        return false;
    }

    len = (*list)->capacity + BlockSize;
    size = sizeof(struct s_amtlist) + (sizeof(amtentry) * len);
    
    p = (amtlist*)ralloc(*list, size);

    if(!p){
        fprintf(stderr, "Failed to allocate memory for increased amtlist\n");
        exit(EXIT_FAILURE);
    }
    p->capacity = len;
    *list = p;
    return true;
}

bool add_amt(amtlist **list, amtentry entry){
    if(!*list){
        fprintf(stderr, "Cannot add entry: list is NULL\n");
        return false;
    }
    if((*list)->length == (*list)->capacity){
        if((*list)->capacity >= MaxCap){
        fprintf(stderr, "Cannot add entry: list is full\n");
            return false;
        } else if(!increase(list)){
            return false;
        }
    }


    (*list)->data[(*list)->length++] = entry;
    return true;
}

void showamtlist(const char *identifier, amtlist *list){
    int32 n;
    if(!identifier || !list){
        fprintf(stderr, "Cannot show list: list is NULL\n");
        return;
    }
    printf("(amtlist* ) %s\n", identifier);
    printf("Capacity: %d, Length: %d\n", list->capacity, list->length);

    if(list->length > 0){
        printf("Entries:\n");
        for(n=0; n < list->length && n < 8; n++){
            //printf("\tBlock: 0x%08x, Amt: %d\n", list->data[n].block, list->data[n].amt);
            show(list->data[n]);
        }
        if(list->length > 8){
            printf("\t... (and %d more entries)\n\n", list->length - 8);
        }
    }
    return;
}

void show_blocklist(const char *identifier, blocklist *list){
    int32 n;
    if(!identifier || !list){
        fprintf(stderr, "Cannot show blocklist: list is NULL\n");
        return;
    }
    printf("(blocklist* ) %s\n", identifier);
    printf("Length: %d\n", list->length);

    if(list->length > 0){
        printf("Blocks:\n");
        for(n=0; n < list->length && n < 8; n++){
            printf("\t0x%08x\n", list->blocks[n]);
        }
        if(list->length > 8){
            printf("\t... (and %d more blocks)\n\n", list->length - 8);
        }
    }
    return;
}

void showamtentry(const char *identifier, amtentry e){
    if(!identifier){
        fprintf(stderr, "Cannot show entry: identifier is NULL\n");
        return;
    }

    printf("(amtentry):%s={0x%.08x, %d}\n", identifier, e.block, e.amt);
    return;
}

amtlist* make_amtlist(void){
    int32 size;
    amtlist *list;
    size = sizeof(struct s_amtlist) + (sizeof(amtentry) * BlockSize);

    list = (amtlist*)alloc(size);
    if(!list){
        fprintf(stderr, "Failed to allocate memory for amtlist\n");
        exit(EXIT_FAILURE);
    }
    zero($1 list, size);

    list->length = 0;
    list->capacity = BlockSize;

    return list;
}


void zero(int8 *dst, int32 size){
    int8 *ptr;
    int32 n;
    for(ptr=dst, n=size; n; ptr++, n--){
        *ptr = 0;
    }
}

int main1(void){
    amtlist *list;
    amtlist *sorted;
    int32 n;

    list = make_amtlist();

    if(!list){
        fprintf(stderr, "Failed to create amtlist\n");
        return EXIT_FAILURE;
    }

    showamtlist("list", list);

    for(n=0; n<0x11000; n++){
        add_amt(&list, (amtentry){0x10000000 + n, n});
    }

    swap(&list->data[1], &list->data[3]);
    show(list);

    sorted = zipsort(list, amtsort);
    
    if(!sorted){
        fprintf(stderr, "Failed to sort list\n");
        destroy(list);
        return EXIT_FAILURE;
    }

    show(sorted);

    destroy(sorted);
    destroy(list);

    return 0;
}

int main2(void){
    amtlist *list;
    int32 n;
    amtentry* e;

    list = make_amtlist();

    if(!list){
        fprintf(stderr, "Failed to create amtlist\n");
        return EXIT_FAILURE;
    }

    show(list);

    for(n=0; n<0x11000; n++){
        add_amt(&list, (amtentry){0x10000000 + n, n});
    }

    e = amtsearch(list, 0x10000004);

    if(e){
        showamtentry("found entry", *e);
    } else {
        printf("Entry not found\n");
    }

    return 0;
}
int main3(int8* file, int8* out_file){
    int8* contents;
    int8* memspace;
    amtlist *list, *sorted;
    blocklist *repeated_blocks, *remaining_blocks;
    int32 ret, offset;
    header* hdr;
    int8* p;

    if(!file){
        fprintf(stderr, "No such file!");
        return EXIT_FAILURE;
    }

    contents = read_file(file);
    if(!contents){
        fprintf(stderr, "Unable to read file contents!\n");
        return EXIT_FAILURE;
    }

    list = read_blocks(contents, FILESZ);
    if(!list){
        destroy(contents);
        fprintf(stderr, "Cannot read the blocks properly!");
        return 1;
    }

    show(list);
    sorted = (amtlist*)zipsort((amtlist*)list, &amtsort);
    if(!sorted){
        fprintf(stderr, "Unable to sort the blocks!");
            destroy(list);
            destroy(contents);
        return 1;
    }

    repeated_blocks = filter_blocks(sorted, &repeated, NULL, NULL);
    if(!repeated_blocks){
        fprintf(stderr, "Failed to filter blocks!\n");
        destroy(sorted);
        destroy(list);
        destroy(contents);
        return 1;
    }

    remaining_blocks = filter_blocks(sorted, &nonrepeated, NULL, NULL);

    if(!remaining_blocks){
        fprintf(stderr, "Failed to filter blocks!\n");
        destroy(repeated_blocks);
        destroy(sorted);
        destroy(list);
        destroy(contents);
        return 1;
    }

    show(repeated_blocks);
    show(remaining_blocks);

    hdr = mk_header(FILESZ, repeated_blocks->length, remaining_blocks->length);

    if(!hdr){
        fprintf(stderr, "Failed to create header!\n");
        destroy(sorted);
        destroy(list);
        destroy(contents);
        destroy(repeated_blocks);
        destroy(remaining_blocks);
        return 1;
    }

    memspace = empty(FILESZ);
    if(!memspace){
        destroy(sorted);
        destroy(list);
        destroy(contents);
        destroy(repeated_blocks);
        destroy(remaining_blocks);

        fprintf(stderr, "alloc(): Memory error");
        return 1;
    }

    offset = copy_headers(memspace, hdr, repeated_blocks, remaining_blocks);
    p = memspace + offset;

    ret = parse_file(p, contents, hdr, repeated_blocks, remaining_blocks);
    if(ret <= 0){
        destroy(sorted);
        destroy(list);
        destroy(contents);
        destroy(repeated_blocks);
        destroy(remaining_blocks);
        destroy(hdr);
        destroy(memspace);
        
        fprintf(stderr, "Failed to parse file contents!\n");
        return 1;
    }
    offset += ret;
    memspace = (int8*)ralloc(memspace, offset);
    if(!memspace){
        fprintf(stderr, "Failed to reallocate memory for final output\n");
        destroy(sorted);
        destroy(list);
        destroy(contents);
        destroy(repeated_blocks);
        destroy(remaining_blocks);
        destroy(hdr);
        destroy(memspace);
        return 1;
    }

    if(!write_file(out_file, memspace, offset)){
        destroy(sorted);
        destroy(list);
        destroy(contents);
        destroy(repeated_blocks);
        destroy(remaining_blocks);
        destroy(hdr);
        destroy(memspace);
        fprintf(stderr, "Failed to write output file!\n");
        
        return 1;
    }

    destroy(sorted);
    destroy(list);
    destroy(contents);
    destroy(repeated_blocks);
    destroy(remaining_blocks);    
    destroy(hdr);
    destroy(memspace);  
    return 0;
}
int8 *read_file(int8* filename){
    int32 fd, filesz, size;
    struct stat sbuf;
    ssize_t ret;
    int8* contents, *p;
    int32 remaining;

    ret = open($c filename, O_RDONLY | O_BINARY);

    if(ret < 0){
        fprintf(stderr, "Failed to open file: %s\n", strerror(errno));
        return NULL;
    } else {
        fd = ret;
    }

    ret = fstat(fd, &sbuf);
    if(ret){
        fprintf(stderr, "Failed to get file status: %s\n", strerror(errno));
        close(fd);
        return NULL;
    }

    filesz = sbuf.st_size;

    if(filesz > MAX_FILE_SIZE){
        close(fd);
        fprintf(stderr, "File is too big!\n");
        return NULL;
    }
    printf("Filename: %s, Size: %d\n\n", filename, filesz);
    size = filesz + 4;
    contents = malloc(size);
    if(!contents){
        close(fd);
        fprintf(stderr, "Memory Error\n");
        return NULL;
    }

    zero(contents, size);
    p = contents;
    remaining = filesz;

    while(remaining > 0){
        int32 chunk;

        chunk = remaining < 512 ? remaining : 512;
        ret = read(fd, p, chunk);
        if(ret > 0){
            p += ret;
            remaining -= (int32)ret;
        } else if(ret < 0){
            close(fd);
            fprintf(stderr, "Failed to read contents from file %s: %s\n", filename, strerror(errno));
            destroy(contents);
            return NULL;
        } else {
            close(fd);
            fprintf(stderr, "Unexpected end of file while reading %s\n", filename);
            destroy(contents);
            return NULL;
        }
    }
    

    close(fd);
    FILESZ = filesz;
    return contents;
}
int main(int argc, char **argv){
    int8* filename;
    int8* output_filename;
    int16 size;
    int rc;

    if(argc < 2){
        fprintf(stderr, "Usage: %s <file>\n", *argv);
        return EXIT_FAILURE;
    }else{
        filename = (int8*)argv[1];
    }

    size = strlen($c filename) + 5;
    output_filename = $1 alloc(size);
    assert(output_filename);
    zero(output_filename, size);

    snprintf($c output_filename, (size - 1), "%s.zz", $c filename);
    
    rc = main3(filename, output_filename);
    destroy(output_filename);
    return rc;
}
