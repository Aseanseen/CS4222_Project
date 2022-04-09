#ifndef __HASHARRAY_H__
#define __HASHARRAY_H__

#include "contiki.h"
#include "lib/memb.h"

typedef enum {
    CONTACT = 0x3,
    NOCONTACT_TOO_FAR = 0x1,
    NOCONTACT_DISCONNECT = 0x0
} STATE_CONN;

/*---------------------------------------------------------------------------*/
#define HASH_TABLE_SIZE 5

struct TokenData {
   // Sum of RSSI values within a eval cycle.
   signed short rssi_sum;
   // Num of RSSI values.
   int rssi_count;
   // Num of consecutive absent/present
   int consec;
   // Records the most recent absent/present
   STATE_CONN is_prev_detect;
   STATE_CONN is_curr_detect;
   // ID of the node.
   int key;

   int begin_timestamp_s;
};



struct TokenData* hashArray[HASH_TABLE_SIZE] = {NULL}; 

// Ensures index provided is always in range.
int hashArray_hashCode(int key) {
   return key % HASH_TABLE_SIZE;
}

// Returns token data given key.
struct TokenData *hashArray_search(int key) {
   //get the hash 
   int hashIndex = hashArray_hashCode(key);  
	
   //move in array until an empty 
   while(hashArray[hashIndex] != NULL) {
	
      if(hashArray[hashIndex]->key == key)
         return hashArray[hashIndex]; 
			
      //go to next cell
      ++hashIndex;
		
      //wrap around the table
      hashIndex %= HASH_TABLE_SIZE;
   }        
	
   return NULL;        
}

struct TokenData* hashArray_insert(struct memb tmp, int key,signed short rssi_sum,int rssi_count,int consec,bool is_detect) {

   struct TokenData *item = memb_alloc(&tmp);
   item->rssi_sum = rssi_sum;
   item->rssi_count = rssi_count;
   item->consec = consec;
   item->is_prev_detect = is_detect;
   item->key = key;

   //get the hash 
   int hashIndex = hashArray_hashCode(key);

   //move in array until an empty or deleted cell
   while(hashArray[hashIndex] != NULL && hashArray[hashIndex]->key != -1) {
      //go to next cell
      ++hashIndex;
		
      //wrap around the table
      hashIndex %= HASH_TABLE_SIZE;
   }
	
   hashArray[hashIndex] = item;
   return item;
}

#endif