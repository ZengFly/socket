#ifndef ITC_LORA_DATA_LIST_H
#define ITC_LORA_DATA_LIST_H

#include <stdint.h>
#include <pthread.h>

/**链表节点定义**/
struct data_node {
    uint16_t data_len;          /*数据长度*/
    uint8_t data[1500];         /*存储数据*/
    pthread_mutex_t mutex;      /*互斥锁*/
    struct data_node *next;
};
typedef struct data_node Data_list_t;

/**链表操作函数**/
/*创建头节点*/
Data_list_t *create_head_node(void);

/*创建数据节点*/
Data_list_t *create_list_node(const uint8_t *data, uint16_t data_len);

/*插入节点*/
int insert_data_node(Data_list_t *node, Data_list_t *head);

/*删除数据节点*/
int delete_data_node(Data_list_t *node);

/*取出数据节点*/
Data_list_t *get_first_node(Data_list_t *head);

#endif //ITC_LORA_DATA_LIST_H
