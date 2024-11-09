#include <stdio.h>
#include <stdlib.h>

typedef struct free_block_t {
    int size;
    struct free_block_t* next;
} free_block_t;

// 创建一个新的节点
free_block_t* createfree_block_t(int size) {
    free_block_t* newfree_block_t = (free_block_t*)malloc(sizeof(free_block_t));
    newfree_block_t->size = size;
    newfree_block_t->next = NULL;
    return newfree_block_t;
}

// 合并两个已排序的链表
free_block_t* merge(free_block_t* list1, free_block_t* list2) {
    if (!list1) return list2;
    if (!list2) return list1;

    if (list1->size > list2->size) {
        list1->next = merge(list1->next, list2);
        return list1;
    } else {
        list2->next = merge(list1, list2->next);
        return list2;
    }
}

// 分割链表为两半
void split(free_block_t* head, free_block_t** front, free_block_t** back) {
    free_block_t* fast;
    free_block_t* slow;
    slow = head;
    fast = head->next;

    while (fast != NULL) {
        fast = fast->next;
        if (fast != NULL) {
            slow = slow->next;
            fast = fast->next;
        }
    }
    *front = head;
    *back = slow->next;
    slow->next = NULL;
}

// 归并排序
void mergeSort(free_block_t** headRef) {
    free_block_t* head = *headRef;
    free_block_t* a;
    free_block_t* b;

    if (head == NULL || head->next == NULL) {
        return;
    }

    split(head, &a, &b);
    mergeSort(&a);
    mergeSort(&b);

    *headRef = merge(a, b);
}

// 打印链表
void printList(free_block_t* free_block_t) {
    while (free_block_t != NULL) {
        printf("%d -> ", free_block_t->size);
        free_block_t = free_block_t->next;
    }
    printf("NULL\n");
}

// 示例
int main() {
    free_block_t* head = createfree_block_t(3);
    head->next = createfree_block_t(1);


    printf("原链表: ");
    printList(head);

    mergeSort(&head);

    printf("排序后的链表: ");
    printList(head);
  
    return 0;
}