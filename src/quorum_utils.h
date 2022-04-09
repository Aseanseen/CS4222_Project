/*
Populates an array with time slots to remain active for.
Suppose an example array of n = 3:

0 1 2
3 4 5
6 7 8

And row number, r is 1 and col number, c is 1

Then, rotation number is the number which the row and col intersects.
It is determined by formula n * r + c = 3 * 1 + 1 = 4.

We first find all col-wise numbers excluding rot number 4, and populate the arr as such:
[1, x, x, x, 7].

We reserve 3 slots between the col num before and after rot num for the row-wise numbers 3, 4, 5.

The final array is [1, 3, 4, 5, 7]. It is always sorted and is helpful in the 
revised send scheduler algorithm to determine when to turn on and off the radio.
*/
void set_active_slots(int *buf, int row_num, int col_num)
{
    int temp = 0, insert_index = 0;
    int j;

    int rotation_num = N * row_num + col_num;
    for (j = 0; j < N; j++)
    {
        temp = col_num + N * j;
        if (temp < rotation_num)
        {
            buf[j] = temp;
        }
        else if (temp > rotation_num)
        {
            buf[j + N - 1] = temp;
        }
        else
        {
            insert_index = j;
        }
    }

    for (j = 0; j < N; j++)
    {
        temp = row_num * N + j;
        buf[insert_index + j] = temp;
    }
}