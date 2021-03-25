#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <netinet/tcp.h>
#include "lora_client.h"
#include "lora_data_list.h"
/*****************************GLOBAL DEFINE******************************/
#define SOCKET_FILE ("/opt/idc/lora_sock")
sem_t recv_sem, send_sem;
sem_t r_break_sem, s_break_sem;
static int lora_fd;
static Data_list_t *list_send_head = NULL;
static Data_list_t *list_recv_head = NULL;
/*****************************FUNCTION DECLARATION******************************/
static void *receive_data_thread(void *param);
static void *keep_connect_thread(void *param);
static void *send_data_thread(void *param);
static void *recv_data_handler_thread(void *param);
static int check_socket_status(void);
static int connect_lora_server(void);
/****************************FUNCTION DEFINITION*************************/
void init_client(void){
    pthread_t pid[4];

    /*初始话数据链表*/
    list_send_head = create_head_node();
    if (list_send_head == NULL) {
        printf("Create list list_send_head failed.\n");
        exit(-1);
    }
    list_recv_head = create_head_node();
    if (list_recv_head == NULL) {
        printf("Create list list_recv_head failed.\n");
        exit(-1);
    }
    /*初始化信号量*/
    sem_init(&recv_sem, 0, 0);
    sem_init(&send_sem, 0, 0);
    sem_init(&r_break_sem, 0, 1);
    sem_init(&s_break_sem, 0, 1);
    /*创建线程*/
    pthread_create(&pid[0], NULL, keep_connect_thread, NULL);
    pthread_create(&pid[1], NULL, receive_data_thread, NULL);
    pthread_create(&pid[2], NULL, send_data_thread, NULL);
    pthread_create(&pid[3], NULL, recv_data_handler_thread, NULL);
}

void insert_data_to_list(uint8_t *data, uint16_t data_len) {
    Data_list_t *node = create_list_node(data, data_len);
    if (node != NULL) {
        insert_data_node(node, list_send_head);
    }
}


static int connect_lora_server(void){
    int fd;

    fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd > 0) {
        struct sockaddr_un addr;
        addr.sun_family = AF_UNIX;
        memset(addr.sun_path, 0, sizeof(addr.sun_path));
        strncpy(addr.sun_path, SOCKET_FILE, sizeof(addr.sun_path));
        if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) == 0) {
            return fd;
        }
    }

    return -1;
}

static int check_socket_status(void)
{
    struct tcp_info info;
    int len = sizeof(info);
    getsockopt(lora_fd, IPPROTO_TCP, TCP_INFO, &info, (socklen_t *)&len);

    if ((info.tcpi_state == TCP_ESTABLISHED) && (lora_fd > 0))
    {
        /*连接正常*/
        return 0;
    } else {
        return -1;
    }
}

/*
 * 保持连接处理线程
 * */
static void *keep_connect_thread(void *param)
{
    int cnt_status = 0;

    while (1) {
        if (cnt_status == 0) {
            lora_fd = connect_lora_server();
            if (lora_fd <= 0) {
                printf("Connect error.\n");
                sleep(5);
                continue;
            }
            cnt_status = 1;
            sem_post(&recv_sem);
            sem_post(&send_sem);
            printf("Connect succeed, fd = %d\n", lora_fd);
        }
        if (check_socket_status() != 0) {
            printf("Lora fd stutas error.\n");
            shutdown(lora_fd, SHUT_RDWR);
            close(lora_fd);
            cnt_status = 0;
            sem_post(&r_break_sem);
            sem_post(&s_break_sem);
            continue;
        }

        sleep(10);
    }
}

/*
 * 数据接收线程
 * */
static void *receive_data_thread(void *param)
{
    uint8_t buff[1500];
    ssize_t read_len = 0;
    uint16_t head = 0;
    uint16_t data_len = 0;
    int copy_len = 0;

    while (1) {
        if (sem_trywait(&r_break_sem) == 0) {
            printf("Get recv break sem.\n");
            sem_wait(&recv_sem);
        }

        data_len = 0;
        head = 0;
        errno = 0;

        read_len = read(lora_fd, &head, sizeof(head));
        if (read_len < sizeof(head)) {
            perror("1-Lora client recv");
            sleep(1);
            continue;
        }
        //head = *(uint16_t *) tmp;
        if (head != 0x5AA5) {
            printf("Illegal frame list_send_head.\n");
            continue;
        }
        read_len = read(lora_fd, &data_len, sizeof(data_len));
        if (read_len == -1) {
            perror("2-Lora client recv");
            sleep(1);
            continue;
        } else {
            printf("data_len = %u\n", data_len);
        }
        memset(buff, 0, sizeof(buff));
        memcpy(buff, &head, sizeof(head));
        memcpy(buff + sizeof(head), &data_len, sizeof(data_len));
        data_len += 23; /*剩余帧长度*/
        copy_len = sizeof(head) + sizeof(data_len);
        while (data_len != 0) {
            read_len = read(lora_fd, buff + copy_len, data_len);
            if (read_len == -1) {
                perror("3-Lora client recv");
                sleep(1);
                continue;
            }
            copy_len += read_len;
            data_len -= read_len;
        }

        /*将数据存入链表*/
        Data_list_t *node = create_list_node(buff, copy_len);
        if (node != NULL) {
            insert_data_node(node, list_recv_head);
        }

        usleep(10000);
    }
}

/**
 * 数据发送线程
 * */
static void *send_data_thread(void *param)
{
    Data_list_t *node = NULL;
    while (1) {
        if (sem_trywait(&s_break_sem) == 0) {
            printf("Get send break sem.\n");
            sem_wait(&send_sem);
        }
        uint8_t *buffer;
        uint16_t data_len;
        /*发送数据到服务器*/
        errno = 0;
        ssize_t ret, send_len = 0;

        node = get_first_node(list_send_head); /*获取数据节点*/
        if (node != NULL) {
            data_len = node->data_len;
            buffer = node->data;
            while (data_len != 0 && (ret = write(lora_fd, buffer, data_len)) != 0) {
                if (ret == -1) {
                    if (errno == EINTR) {
                        continue;
                    }
                    perror("Tcp write:");
                    break;
                }
                data_len -= ret;
                buffer += ret;
                send_len += ret;
            }
            printf("Send %u data to lora.\n", send_len);
            delete_data_node(node);     /*释放节点*/
        }
        usleep(10000);
    }
}

/*
 * 收到的lora数据处理线程
 * */
static void *recv_data_handler_thread(void *param) {
    Data_list_t *node = NULL;

    /*TODO 完善处理功能*/
    while (1) {
        node = get_first_node(list_recv_head);
        if (node != NULL) {
            printf("Get recv data:");
            for (int i = 0; i < node->data_len; ++i) {
                printf("%02X ", node->data[i]);
            }
            printf("\n");




            /*删除节点*/
            delete_data_node(node);
        }

        usleep(10000);
    }
}
