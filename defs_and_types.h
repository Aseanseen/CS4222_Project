#include "lib/memb.h"

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
typedef struct {
  unsigned long src_id;
} data_packet_struct;
/*---------------------------------------------------------------------------*/
#define HASH_TABLE_SIZE 5

struct TokenData {
   signed short rssi_sum;
   int rssi_count;
   int consec;
   int state_flag;
   int key;
   int detect_to_absent_ts;
   int absent_to_detect_ts;
};

struct TokenData* hashArray[HASH_TABLE_SIZE];
struct TokenData* _dummyToken;

int hashCode(int key) {
   return key % HASH_TABLE_SIZE;
}

struct TokenData *search(int key) {
   //get the hash 
   int hashIndex = hashCode(key);  
	
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

struct TokenData *insert(struct memb tmp, int key,signed short rssi_sum,int rssi_count,int consec,int state_flag) {
   struct TokenData *item = memb_alloc(&tmp);
   item->rssi_sum = rssi_sum;
   item->rssi_count = rssi_count;
   item->consec = consec;
   item->state_flag = state_flag;
   item->key = key;
   item->detect_to_absent_ts = 0;
   item->absent_to_detect_ts = 0;

   //get the hash 
   int hashIndex = hashCode(key);

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

struct TokenData* delete(struct TokenData* item){
   int key = item->key;
   //get the hash
   int hashIndex = hashCode(key);
   //move in array until an empty
   while(hashArray[hashIndex] != NULL){
      if(hashArray[hashIndex]->key == key){
         struct TokenData* temp = hashArray[hashIndex];
         //assign a dummy item at deleted position
         hashArray[hashIndex] = _dummyToken;
         return temp;
      }
      //go to next cell
      ++hashIndex;
      //wrap around the table
      hashIndex %= HASH_TABLE_SIZE;
   }
   return NULL;
}
/*---------------------------------------------------------------------------*/
