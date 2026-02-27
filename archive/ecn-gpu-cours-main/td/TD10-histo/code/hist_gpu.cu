
#include <stdio.h>
#include <string.h>
#include <iostream>

#include "text.h"

#define NB_ASCII_CHAR 128

void compute_histo_cpu(char * h_str, u_int * histo){
    for (int i = 0; i < len; i++){
            histo[h_str[i]]++;
    } 
}

int main( void ) {
    int len = strlen(h_str);
    printf("len:%d\n", len);
    int size = len*sizeof(char);

    u_int h_histo[NB_ASCII_CHAR] = {0};

    compute_histo_cpu(h_str, h_histo);

    for (int bean = 0; bean < NB_ASCII_CHAR; bean++) {
        std::cout << (char) bean << " : " << h_histo[bean] << std::endl;
    }
    return 0;
}
