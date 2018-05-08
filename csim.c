/*
 *  Created by David Fletcher and Jake Masters.
 */

#include "cachelab.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// --- structs --- //
typedef int bool;
#define true 1
#define false 0

typedef struct {
    char valid;
    long tag;
} Line;

typedef Line* Set;

// --- helper functions --- //
void initialize_cache(Line * cache, int * lru, int assoc, int set_num) {
   for(int i = 0; i < set_num; i++) {
        for(int j = 0; j < assoc; j++) {
            cache[(i * assoc)+(j % assoc)].valid = 'f';
            cache[(i * assoc)+(j % assoc)].tag = 0; 
            lru[(i * assoc)+(j % assoc)] = 0;
        }
   }
}

int check_cache(Line * cache, int set_num, int tag_bits, int associativity) {
    bool is_miss = true;

    for(int i = 0; i < associativity; i++) {
        if(cache[(set_num * associativity) + (i % associativity)].tag == tag_bits && strcmp(&cache[(set_num * associativity)+(i % associativity)].valid, "t") == 0) {
            is_miss = false;
            break;
        } else {
            continue;
        }
    }

    if(is_miss == true) {
        return 2;
    } else {
        return 1;
    }
}

void decay(int * lru, int set_num, int assoc, int index) {
    for(int i = 0; i < assoc; i++) {
        if(i != index) {
            lru[(set_num * assoc) + (i % assoc)]++;
        } else {
            lru[(set_num * assoc) + (i % assoc)] = 0;
        }
    }
}

int evict(Line * cache, int * lru, int set_num, int associativity, int tag_bits){
    // first check all valid bits. If all are set, then cache is full.
    bool is_full = false;
    for (int i=0; i < associativity; ++i){
        if (strcmp(&cache[(set_num*associativity)+(i%associativity)].valid,"t") == 0){
            is_full = true;
        } else {
            is_full = false;
            break;
        }
    }

    int max = 0;
    int index = 0;
    for(int i = 0; i < associativity; ++i) {
        if(max < lru[(set_num*associativity)+(i%associativity)]) {
            max = lru[(set_num*associativity)+(i%associativity)];
            index = i;
        }
    }

    //evict max @ index
    cache[(set_num * associativity) + (index % associativity)].valid = 't';
    cache[(set_num * associativity) + (index % associativity)].tag = tag_bits;
    
    decay(lru, set_num, associativity, index);
    
    if(is_full) {
        //increment evictions
        return 1;
    } else {
        //don't increment evictions
        return 0;
    }
}

void view_cache(Line * cache, int set_bits, int associativity){
    int num_sets = pow(2, set_bits);
    for(int i=0; i < num_sets; ++i){
        printf("%s", "[");
        for (int j=0; j < associativity; ++j){
            printf(" %s:%ld", &cache[(i*associativity)+(j%associativity)].valid, cache[(i*associativity)+(j%associativity)].tag);
        }
        printf("%s", " ]\n");
    }
    printf("%s", "\n");
}


// --- main function --- //
int main(int argc, char *argv[])
{
    // I/O logic
    bool help = false;
    bool verbose = false;
    bool super_verbose = false;
    int set_bits;
    int sets;
    int associativity;
    int num_hits = 0, num_misses = 0, num_evictions = 0;
    int block_bits;
    char *file_name;

    for (int i=1; i<argc; ++i){
        if (strcmp(argv[i],"-h") == 0){
            help = true;
        }
        else if (strcmp(argv[i],"-v") == 0){
            verbose = true;
        }
        else if (strcmp(argv[i],"-V") == 0){
            super_verbose = true;
            verbose = true;
        }
        else if (strcmp(argv[i],"-hv") == 0){
            help = verbose = true;
        }
        else if (strcmp(argv[i],"-s") == 0){
            set_bits = atoi(argv[i+1]);
            sets = pow(2, set_bits);
        }
        else if (strcmp(argv[i],"-E") == 0){
            associativity = atoi(argv[i+1]);
        }
        else if (strcmp(argv[i],"-b") == 0){
            block_bits = atoi(argv[i+1]);
        }
        else if (strcmp(argv[i],"-t") == 0){
            file_name = argv[i+1];
        }
    }

   // if -h specified
   if (help == true){
       printf("Usage:\n ./csim [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n");
   }

   /*Set* cache = (Set*)malloc(associativity * sizeof(Line) * pow(2,set_bits));
   for(int i=0; i < set_bits; ++i){
       cache[i] = (Line*)malloc(sizeof(Line) * associativity);
   }*/

   Line * cache = (Line *) malloc (sizeof(Line) * associativity * sets);
   int * lru = (int *) malloc(sizeof(int) * associativity * sets);

   initialize_cache(cache, lru, associativity, sets);

   // --- setup complete --- //

   // --- open our stream --- //

    FILE *fp;
    char buffer [100];
    fp = fopen(file_name, "r");
    char result[100];
    while(fgets(buffer, 100, fp) != NULL) {
        result[0] = '\0';
        char * address_char;
        char * instruction;
        char * bytes;

        instruction = strtok(buffer, " ");

        if(strcmp(instruction, "I") == 0) {
            continue;
        }
        
        address_char = strtok(NULL, ",");

        // need to trim leading zeros off of address
        int n;
        if((n = strspn(address_char,"0"))!=0 && address_char[n]!='\0'){
            address_char = &address_char[n];
        }

        bytes = strtok(NULL, " \n");

        long address_hex = strtol(address_char, NULL, 16);

        int set_num = (address_hex >> block_bits) & (long) (pow(2, set_bits)-1);
        int tag_bits = address_hex >> block_bits >> set_bits;
         
        // Leaving this in just in case, but I implemented verbose mode below instead
        //printf("Address: %s, instruction: %s, set number: %d, tag bits: %d, bytes: %s\n", address_char, instruction, set_num, tag_bits, bytes);

        /* Instruction Legend
         * L = data load
         * S = data store
         * M = data modify (load -> store)
         */
        int prev_evicts = num_evictions;
        if(strcmp(instruction, "L") == 0 || strcmp(instruction, "S") == 0) {
            int status = check_cache(cache, set_num, tag_bits, associativity);
            if(status == 1) {
                num_hits++;
                strcat(result, "hit");
                evict(cache, lru, set_num, associativity, tag_bits);
            } else {
                num_misses++;
                strcat(result, "miss");
                num_evictions = num_evictions + evict(cache, lru, set_num, associativity, tag_bits);
            }
        }
        else if(strcmp(instruction, "M") == 0) {
            int status = check_cache(cache, set_num, tag_bits, associativity);
            if(status == 1) {
                num_hits++;
                strcat(result, "hit");
            } else {
                num_misses++;
                strcat(result, "miss");
                num_evictions = num_evictions + evict(cache, lru, set_num, associativity, tag_bits);
            }
            status = check_cache(cache, set_num, tag_bits, associativity);
            if(status == 1) {
                num_hits++;
                strcat(result, " hit");
            } else {
                num_misses++;
                num_evictions = num_evictions + evict(cache, lru, set_num, associativity, tag_bits);
                strcat(result, " miss");
            }
        }
        if (num_evictions > prev_evicts){ strcat(result, " eviction");}
        if (verbose == true){
            printf("%s %s,%s %s \n", instruction, address_char, bytes, result);
        }
    if (super_verbose == true){
        printf("Set: %d\nTag: %d\n", set_num, tag_bits);
        view_cache(cache, set_bits, associativity); 
    }
    }

    printSummary(num_hits, num_misses, num_evictions);
    return 0;
}
