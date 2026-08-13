#pragma once
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif


union u_entry;
typedef union u_entry entry;

struct s_amtentry;
typedef struct s_amtentry amtentry;

enum e_sortres;
typedef enum e_sortres sortres;

union u_list;
typedef union u_list List; 
typedef struct s_amtlist amtlist;
typedef struct s_blocklist blocklist;
typedef struct s_header header;


typedef unsigned char int8;
typedef unsigned short int16;
typedef unsigned int int32;
typedef unsigned long long int64;

typedef bool (*filterfunc)(int16, amtentry*, void*, void*);
typedef sortres (*sortfunc)(amtentry, amtentry);

#define NoMatch 0x1000 
#define Version 1
#define BlockSize 0xffff
#define MaxCap ((0xffffffff/8) - BlockSize - 1)
#define MAX_FILE_SIZE 33556

#define $c (char*)
#define $i (int)
#define $v (void*)
#define $1 (int8*)
#define $2 (int16*)
#define $4 (int32*)
#define $8 (int64*)

#define alloc(x)      malloc($i (x))
#define destroy(x)    free(x)
#define ralloc(x, y)  realloc($v (x), $i (y))
#define show(x)      _Generic((x), \
    amtentry:         showamtentry, \
    blocklist*:        show_blocklist, \
    amtlist*:          showamtlist) \
    ((# x), (x)) \

#define swap(x, y)    _Generic((x), \
    amtentry*:          amtswap) \
    ((x), (y))

#define copy(x)    _Generic((x), \
    amtlist*:          amtlistcopy) \
    ((x))

#define listeq(x, y)    _Generic((x), \
    amtlist*:          amtlisteq) \
    ((x), (y))
#if 0
    #define uniq(i, v)   amount((i), (v), $v 1, $v 0)
#endif

struct s_amtentry{
    int32 block;
    int32 amt;
};

struct s_amtlist{
    int32 capacity;
    int32 length;
    amtentry data[];
};

struct s_blocklist{
    int16 length;
    int32 blocks[];
};

struct s_header{
    int8 magic[3];
    int8 version;
    int32 filesz;
    int16 blocklist;
    int16 uniqlist;
};

union u_entry{
    amtentry amt;
};

union u_list{
    amtlist* amt;
};

enum e_sortres{
    MoreThan=0,
    Equal,
    LessThan
};

//contructors
amtlist* make_amtlist(void);
header* mk_header(int32, int16, int16);

void showamtlist(const char*, amtlist*);
void showamtentry(const char *identifier, amtentry e);
void show_blocklist(const char *identifier, blocklist *list);

bool add_amt(amtlist**, amtentry);
bool increase(amtlist**);
void amtswap(amtentry*, amtentry*);

amtlist* zipsort_(amtlist*, sortfunc);
amtlist* zipsort(amtlist*, sortfunc);
amtlist* amtlistcopy(amtlist* list);
bool amtlisteq(amtlist* l1, amtlist* l2);

sortres amtsort(amtentry e1, amtentry e2);
amtentry *amtsearch(amtlist* list, int32); 
void zero(int8 *a, int32 n);
int8* read_file(int8*);
amtlist* read_blocks(int8* contents, int32 filesz); 
bool head(int16, amtentry*, void*, void*);
bool tail(int16, amtentry*, void*, void* );
bool repeated(int16, amtentry*, void*, void*);
bool nonrepeated(int16, amtentry*, void*, void*);
blocklist* filter_blocks(amtlist*, filterfunc, void*, void*);
int8* empty(int32);
void copy_bytes(int8*, int8*, int32);
int32 copy_headers(int8*, header*, blocklist*, blocklist*);
int16 block_search(blocklist*, int32);
int32 parse_file(int8*, int8*, header*, blocklist*, blocklist*);
bool write_file(int8*, int8*, int32);

int main(int, char**);
int main1(void);
int main2(void);
int main3(int8*, int8*);
