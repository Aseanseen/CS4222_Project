#include "lib/memb.h"

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
typedef struct
{
   unsigned long src_id;
} data_packet_struct;
/*---------------------------------------------------------------------------*/
#define ARR_MAX_LEN 5

struct TokenData
{
   signed short rssi_sum;
   int rssi_count;
   int consec;
   int state_flag;
   int key;
   int detect_to_absent_ts;
   int absent_to_detect_ts;
};

struct TokenData nullToken = {0,0,0,0,-1,0,0};

void print_token_data(struct TokenData *token)
{
   printf(
       "%d - %d - %d - %d - %d - %d - %d\n",
       token->key,
       token->rssi_sum,
       token->rssi_count,
       token->consec,
       token->state_flag,
       token->detect_to_absent_ts,
       token->absent_to_detect_ts);
}

struct TokenDataList
{
   int max_size;
   struct TokenData *tk[ARR_MAX_LEN];
   int num_elem;
};

void map_insert(struct TokenDataList *tklist, int key, int rssi_sum, int rssi_count, int consec, int state_flag, int absent_to_detect_ts, int detect_to_absent_ts)
{
   int i = 0;
   while (tklist->tk[i]->key != -1 && tklist->tk[i]->key != key)
   {
      i++;
      if (i == ARR_MAX_LEN)
      {
         //printf("Not enough space!\n");
         return;
      }
   }
   tklist->tk[i]->key = key;
   tklist->tk[i]->rssi_sum = rssi_sum;
   tklist->tk[i]->rssi_count = rssi_count;
   tklist->tk[i]->consec = consec;
   tklist->tk[i]->state_flag = state_flag;
   tklist->tk[i]->absent_to_detect_ts = absent_to_detect_ts;
   tklist->tk[i]->detect_to_absent_ts = detect_to_absent_ts;

   tklist->num_elem++;
}

void map_init(struct memb tmp, struct TokenDataList *tklist)
{
   int i;
   for (i = 0; i < tklist->max_size; i++)
   {
      tklist->tk[i] = memb_alloc(&tmp);
      tklist->tk[i]->key = -1;
      tklist->tk[i]->rssi_sum = 0;
      tklist->tk[i]->rssi_count = 0;
      tklist->tk[i]->consec = 0;
      tklist->tk[i]->state_flag = 0;
      tklist->tk[i]->absent_to_detect_ts = 0;
      tklist->tk[i]->detect_to_absent_ts = 0;
   }
   //printf("init done\n");
}

struct TokenData *map_search(struct TokenDataList *tklist, int key)
{
   int i;
   for (i = 0; i < tklist->max_size; i++)
   {
      if (tklist->tk[i]->key == key)
      {
         return tklist->tk[i];
      }
   }
   //printf("token not found...\n");
   return &nullToken;
}

void map_remove(struct TokenDataList *tklist, struct TokenData *token)
{
   int i;
   for (i = 0; i < tklist->max_size; i++)
   {
      if (tklist->tk[i]->key == token->key)
      {
         tklist->tk[i]->key = -1;
         tklist->num_elem--;
         return;
      }
   }
   //printf("cannot find item\n");
   return;
}

void map_view(struct TokenDataList *tklist)
{
   int i;
   printf("VIEW ARRAYMAP BEGIN: %d\n", tklist->num_elem);
   for (i = 0; i < tklist->max_size; i++)
   {
      print_token_data(tklist->tk[i]);
   }
   printf("VIEW ARRAYMAP END\n");
}