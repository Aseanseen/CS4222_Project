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

struct TokenData dummyData = {
	.rssi_count = 0,
	.consec = 0,
	.state_flag = 0,
	.key = -1,
	.detect_to_absent_ts = 0,
	.absent_to_detect_ts = 0
};

int hashCode(int key) {
   return key % HASH_TABLE_SIZE;
}

struct TokenData *search(int key) {
   //get the hash 
   int hashIndex = hashCode(key);  
	
   
   //move in array until an empty 
   while(hashArray[hashIndex] != NULL ) {
	
      if(hashArray[hashIndex]->key == key || hashArray[hashIndex]->key == -1) {
	printf("SEARCH: hashIndex found %d\n", hashIndex);         
	return hashArray[hashIndex]; 
      } 
			
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
   while(hashArray[hashIndex] != NULL) {
      //go to next cell
      ++hashIndex;
		
      //wrap around the table
      hashIndex %= HASH_TABLE_SIZE;
   }

   printf("INSERT: HashIndex to insert = %d\n", hashIndex);	
   hashArray[hashIndex] = item;

   return item;
}

struct TokenData *overwrite(struct TokenData* item, int key, signed short rssi_sum,int rssi_count,int consec,int state_flag) { 
   item->rssi_sum = rssi_sum;
   item->rssi_count = rssi_count;
   item->consec = consec;
   item->state_flag = state_flag;
   item->detect_to_absent_ts = 0;
   item->absent_to_detect_ts = 0;
   item->key = key;
   return item;
}


void delete(struct TokenData* item){
   int key = item->key;
   //get the hash
   int hashIndex = hashCode(key);
   printf("START OF DELETE\n");
   //move in array until an empty
   while(hashArray[hashIndex] != NULL){
      if(hashArray[hashIndex]->key == key){
	printf("DELETE: deleting HashIndex %d\n", hashIndex);
         hashArray[hashIndex]->key = -1;
         printf("END OF DELETE\n");
	 return;
      }
      //go to next cell
      ++hashIndex;
      //wrap around the table
      hashIndex %= HASH_TABLE_SIZE;
	printf("fuck %d - %d\n", key, hashIndex);

   }
   printf("END OF DELETE\n");
   return;
}

void display() {
   int i = 0;
	
   for(i = 0; i<HASH_TABLE_SIZE; i++) {
	
      if(hashArray[i] != NULL)
         printf(" (%d)",hashArray[i]->key);
      else
         printf(" ~~ ");
   }
	
   printf("\n");
}
/*---------------------------------------------------------------------------*/
