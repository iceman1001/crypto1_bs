#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/sysinfo.h>
#include "craptev1.h"

#define rev32(word) (((word & 0xff) << 24) | (((word >> 8) & 0xff) << 16) | (((word >> 16) & 0xff) << 8) | (((word >> 24) & 0xff)))

uint64_t split(uint8_t p){
    return (((p & 0x8) >>3 )| ((p & 0x4) >> 2) << 8 | ((p & 0x2) >> 1) << 16 | (p & 0x1) << 24 );
}

uint32_t uid;
uint64_t *readnonces(char* fname){
    int i;
    FILE *f = fopen(fname, "r");
    uint64_t *nonces = malloc(sizeof (uint64_t) <<  24);
    if(fread(&uid, 1, 4, f)){
        uid = rev32(uid);
    }
    fseek(f, 6, SEEK_SET);
    i = 0;
    while(!feof(f)){
        uint32_t nt_enc1, nt_enc2;
        uint8_t par_enc;
        if(fread(&nt_enc1, 1, 4, f) && fread(&nt_enc2, 1, 4, f) && fread(&par_enc, 1, 1, f)){
            nonces[i  ] = split(~(par_enc >>   4)) << 32 | nt_enc1;
            nonces[i+1] = split(~(par_enc & 0xff)) << 32 | nt_enc2;
            i += 2;
        }
    }
    nonces[i] = -1;
    fclose(f);
    return nonces;
}

uint32_t **space;
size_t thread_count;
size_t states_tested = 0;
size_t total_states;
size_t keys_found = 0;

void* crack_states_thread(void* x){
    const size_t thread_id = (size_t)x;
    int j;
    for(j = thread_id; space[j * 5]; j += thread_count) {
        uint64_t key = craptev1_search_partition(space + j * 5);
        states_tested = total_states - craptev1_sizeof_space(space+j*5);
        printf("Cracking... %6.02f%%\n", (100.0*states_tested/(total_states)));
        if(key != -1){
            printf("Found key: %012lx\n", key);
            exit(0);
        }
    }
    return NULL;
}

int main(int argc, char* argv[]){
    if(argc != 2){
        printf("Usage: %s <nonces.bin>\n", argv[0]);
        return -1;
    }
    uint64_t *nonces = readnonces(argv[1]);
    space = craptev1_get_space(nonces, 95, uid);
    total_states = craptev1_sizeof_space(space);

    thread_count = get_nprocs_conf();
    pthread_t threads[thread_count];
    printf("Starting %lu threads to test %lu states\n", thread_count, total_states);
    size_t i;
    states_tested = 0;
    for(i = 0; i < thread_count; i++){
        pthread_create(&threads[i], NULL, crack_states_thread, (void*) i);
    }
    for(i = 0; i < thread_count; i++){
        pthread_join(threads[i], 0);
    }
    printf("Tested %lu states\n", states_tested);

    craptev1_destroy_space(space);
    return 0;
}
