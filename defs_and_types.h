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

void print_token_data(struct TokenData *token)
{
   printf(
       "%d - %d - %d - %d - %d - %d\n",
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

void map_insert(struct TokenDataList *tklist, struct TokenData *input)
{
   int i = 0;
   while (tklist->tk[i]->key != -1 && tklist->tk[i]->key != input->key)
   {
      i++;
      if (i == ARR_MAX_LEN)
      {
         printf("Not enough space!\n");
         return;
      }
   }
   tklist->tk[i]->key = input->key;
   tklist->tk[i]->rssi_sum = input->rssi_sum;
   tklist->tk[i]->rssi_count = input->rssi_count;
   tklist->tk[i]->consec = input->consec;
   tklist->tk[i]->state_flag = input->state_flag;
   tklist->tk[i]->absent_to_detect_ts = input->absent_to_detect_ts;
   tklist->tk[i]->detect_to_absent_ts = input->detect_to_absent_ts;

   tklist->num_elem++;
}

void map_init(struct memb tmp, struct TokenDataList *tklist)
{
   for (int i = 0; i < tklist->max_size; i++)
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
}

struct TokenData *map_search(struct TokenDataList *tklist, int key)
{
   for (int i = 0; i < tklist->max_size; i++)
   {
      if (tklist->tk[i]->key == key)
      {
         return tklist->tk[i];
      }
   }
   printf("token not found...\n");
   return NULL;
}

void map_remove(struct TokenDataList *tklist, struct TokenData *token)
{
   for (int i = 0; i < tklist->max_size; i++)
   {
      if (tklist->tk[i]->key == token->key)
      {
         tklist->tk[i]->key = -1;
         tklist->num_elem--;
         return;
      }
   }
   printf("cannot find item\n");
   return;
}

void map_view(struct TokenDataList *tklist)
{
   printf("VIEW ARRAYMAP BEGIN: %d\n", tklist->num_elem);
   for (int i = 0; i < tklist->max_size; i++)
   {
      print_token_data(tklist->tk[i]);
   }
   printf("VIEW ARRAYMAP END\n");
}