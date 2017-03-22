#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <math.h>


/**********************
 * Constants and macros
 **********************/
#define MAXLINE     1024 /* max string size */
#define LONG_SIZE    64
#define MAXOPS     500000
/******************************
 * The key compound data types
 *****************************/
/* Characterizes a single trace operation */
typedef struct {
    enum {LOAD,STORE,MODIFY} type;
    unsigned long address;
    unsigned int size;
} traceop_t;

typedef struct{
    traceop_t *ops;
    int num_ops;
}trace_t;

typedef struct{
    int s;
    int E;
    int b;
}cache_t;

typedef struct line{
    char valid;
    unsigned long tag;
    struct line *pred;
    struct line *succ;
}line_t;

typedef struct{
    unsigned long address;
    line_t *set_start;
    line_t *set_end;
}set_t;
/********************
 * Global variables
 *******************/
int verbose = 0;        /* global flag for verbose output */
char msg[MAXLINE];      /* for whenever we need to compose an error message */
unsigned int s;
unsigned int E;
unsigned int b;
line_t *start=NULL;
unsigned int miss=0;
unsigned int hit=0;
unsigned int eviction=0;

/* Directory where default tracefiles are found */
static char tracedir[MAXLINE];

/*********************
 * Function prototypes
 *********************/
static trace_t *read_trace(char *tracedir, char *filename);
static void cache_p(traceop_t test,set_t sets[]);
static void initial(trace_t *trace);

/**************
 * Main routine
 **************/
int main(int argc, char **argv)
{
    char c;
    char *tracefiles = NULL;  /* null-terminated array of trace file names */
    /*
     * Read and interpret the command line arguments
     */
    while((c=getopt(argc,argv,"hvs:E:b:t:"))!=-1){
        switch(c){
            case 'v':
                verbose=1;
                break;
            case 's':
                s=atoi(optarg);
                break;
            case 'E':
                E=atoi(optarg);
                break;
            case 'b':
                b=atoi(optarg);
                break;
            case 't':
                strcpy(tracedir,"./");
                tracefiles=optarg;
                break;
        }
    }
    initial(read_trace(tracedir, tracefiles));
    printSummary(hit, miss, eviction);
    return 0;
}

/**********************************************
 * The following routines manipulate tracefiles
 *********************************************/

/*
 * read_trace - read a trace file and store it in memory
 */
static trace_t *read_trace(char *tracedir, char *filename)
{
    FILE *tracefile=NULL;
    char type[MAXLINE];
    unsigned int size,op_index;
    unsigned long address;
    trace_t *trace=NULL;
    char path[MAXLINE];
    op_index = 0;
    /* Allocate the trace record */
    if ((trace = (trace_t *) malloc(sizeof(trace_t))) == NULL)
        printf("trace failed in read_trance");
    
    /* read every request line in the trace file */
    strcpy(path, tracedir);
    strcat(path, filename);
    if ((tracefile = fopen(path, "r")) == NULL)
        sprintf(msg, "Could not open %s in read_trace", path);
    
    /* We'll store each request line in the trace in this array */
    if ((trace->ops =
         (traceop_t *)malloc(MAXOPS*sizeof(traceop_t))) == NULL)
        printf("trace operation failed in read_trace");
    
    while (fscanf(tracefile, "%s", type) != EOF) {
        switch(type[0]) {
            case 'I':
                fscanf(tracefile, "%lx,%u", &address, &size);
                break;
            case 'L':
                fscanf(tracefile, "%lx , %u", &address, &size);
                trace->ops[op_index].type = LOAD;
                trace->ops[op_index].address= address;
                trace->ops[op_index].size = size;
                op_index++;
                break;
            case 'S':
                fscanf(tracefile, "%lx,%u", &address, &size);
                trace->ops[op_index].type = STORE;
                trace->ops[op_index].address= address;
                trace->ops[op_index].size = size;
                op_index++;
                break;
            case 'M':
                fscanf(tracefile, "%lx,%u", &address, &size);
                trace->ops[op_index].type = MODIFY;
                trace->ops[op_index].address= address;
                trace->ops[op_index].size = size;
                op_index++;
                break;
            default:
                printf("Wrong input format");
                exit(1);
            }
        
    }
    fclose(tracefile);
    trace->num_ops=op_index;
    return trace;
}

/*
 * cache_p-process every trace operation
 */
static void cache_p(traceop_t test,set_t *sets){
    int times=0;
    long addr=test.address;
    int tmp=(1<<31)>>(31-s);
    unsigned int set_index=(addr>>b)&~tmp;
    unsigned long tag_addr=addr>>(s+b);
    line_t *set_start=(line_t *)sets[set_index].address;
    line_t *p=set_start-1;
    line_t *free=set_start;
    int i=0;
    for(;i<E;i++){
        p++;
        if(p->valid=='1'){
            if(tag_addr==p->tag){
                /* Hit */
                hit++;
                if(test.type==MODIFY)
                    hit++;
                /* Modify the double-directed list */
                if(p->succ==(line_t*)-1){
                    if(p->pred!=(line_t*)-1)
                        sets[set_index].set_end=p->pred;
                }
                else
                    if(p->pred!=(line_t*)-1)
                        p->succ->pred=p->pred;
                if(p->pred!=(line_t*)-1){
                    p->pred->succ=p->succ;
                    sets[set_index].set_start->pred=p;
                    p->pred=(line_t*)-1;
                    p->succ=sets[set_index].set_start;
                    sets[set_index].set_start=p;
                }
                break;
            }
        }
        else
            free=p;
        times++;
    }
    if(times==E){
        /* Miss */
        miss++;
        if(test.type==MODIFY)
            hit++;
        if((free!=set_start)||(free==set_start&&set_start->valid!='1')){
            free->valid='1';
            free->tag=tag_addr;
            /* Modify the double-directed list */
            if(free->succ==(line_t*)-1){
                if(free->pred!=(line_t*)-1)
                    sets[set_index].set_end=free->pred;
            }
            else
                if(free->pred!=(line_t*)-1)
                    free->succ->pred=free->pred;
            if(free->pred!=(line_t*)-1){
                free->pred->succ=free->succ;
                sets[set_index].set_start->pred=free;
                free->pred=(line_t*)-1;
                free->succ=sets[set_index].set_start;
                sets[set_index].set_start=free;
            }

        }
        else{
            /* Eviction */
            eviction++;
            p=sets[set_index].set_end;
            p->tag=tag_addr;
            /* Modify the double-directed list */
            if(p->pred!=(line_t*)-1){
                p->pred->succ=p->succ;
                sets[set_index].set_end=p->pred;
                sets[set_index].set_start->pred=p;
                p->pred=(line_t*)-1;
                p->succ=sets[set_index].set_start;
                sets[set_index].set_start=p;
            }
        }
    }
}

/*
 * initial-malloc and set the initial value
 */
static void initial(trace_t *trace){
    unsigned int S=0;
    line_t *p;
    int i;
    int j;
    S=pow(2,s);
    set_t *sets=malloc(sizeof(set_t)*S);
    start=malloc(sizeof(line_t)*S*E);
    p=start;
    i=sizeof(line_t);
    for(i=0;i<S;i++){
        sets[i].address=(unsigned long)start+i*E*sizeof(line_t);
        for(j=0;j<E;j++){
            if(j==0){
                p->pred=(line_t*)-1;
                sets[i].set_start=p;
            }
            else
                p->pred=p-1;
            if(j==E-1){
                p->succ=(line_t*)-1;
                sets[i].set_end=p;
            }
            else
                p->succ=p+1;
            p++;
        }
    }
    for(i=0;i<trace->num_ops;i++)
        cache_p(trace->ops[i],sets);
}






