#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "lora_data_list.h"

/*创建头节点*/
Data_list_t *create_head_node(void){
    Data_list_t *node = (Data_list_t *) malloc(sizeof(Data_list_t));
    if (node == NULL) {
        return NULL;
    }
    memset(node->data, 0, sizeof(node->data));
    node->data_len = 0;
    pthread_mutex_init(&node->mutex, NULL);
    node->next = NULL;

    return node;
}

/*创建数据节点*/
Data_list_t *create_list_node(const uint8_t *data, uint16_t data_len) {
    if (data == NULL || data_len >= 1500) {
        return NULL;
    }

    Data_list_t *node = (Data_list_t *) malloc(sizeof(Data_list_t));
    if (node == NULL) {
        return NULL;
    }
    memset(node->data, 0, sizeof(node->data));
    memcpy(node->data, data, data_len);
    node->data_len = data_len;
    node->next = NULL;

    return node;
}


/*插入节点到尾部*/
int insert_data_node(Data_list_t *node, Data_list_t *head) {
    if (node == NULL || head == NULL) {
        return -1;
    }
    pthread_mutex_lock(&head->mutex);

    Data_list_t *last_node = head;
    while (last_node->next != NULL) {
        last_node = last_node->next;
    }
    node->next = last_node->next;
    last_node->next = node;

    pthread_mutex_unlock(&head->mutex);

    return 0;
}


/*删除数据节点*/
int delete_data_node(Data_list_t *node) {
    if (node == NULL) {
        return -1;
    }
    free(node);
    node = NULL;

    return 0;
}


/*取出数据节点*/
Data_list_t *get_first_node(Data_list_t *head) {
    if (head == NULL) {
        return NULL;
    }
    pthread_mutex_lock(&head->mutex);

    Data_list_t *temp = head;
    Data_list_t *node = NULL;
    if (temp->next != NULL) {
        node = temp->next;
        temp->next = node->next;
        node->next = NULL;
    }

    pthread_mutex_unlock(&head->mutex);

    return node;
}
