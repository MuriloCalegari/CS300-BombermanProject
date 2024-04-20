#include "server_utils.h"
#include <stdio.h>

int check_bomb_linked_list_consistency(Match *match) {
    Bomb *current = match->bombs_head;

    if(match->bombs_head == NULL && match->bombs_tail != NULL) {
        printf("ERROR: Head is NULL but tail is not\n");
        return 0;
    }

    if(match->bombs_head != NULL && match->bombs_tail == NULL) {
        printf("ERROR: Head is not NULL but tail is\n");
        return 0;
    }

    while(current != NULL) {
        if(current->prev == NULL && current != match->bombs_head) {
            printf("ERROR: Current's prev is NULL but it is not head\n");
            return 0;
        }

        if(current->next == NULL && current != match->bombs_tail) {
            printf("ERROR: Current's next is NULL but it is not tail\n");
            return 0;
        }

        if(current->prev != NULL && current->prev->next != current) {
            printf("ERROR: Current's prev's next is not current\n");
            return 0;
        }

        if(current->next != NULL && current->next->prev != current) {
            printf("ERROR: Current's next's prev is not current\n");
            return 0;
        }
        current = current->next;
    }

    return 1;
}