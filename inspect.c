/*
 *  inspect.c
 *  github-recommend
 *
 */

#include "inspect.h"

void inspect_user(User *user)
{
    int i = 0;
    printf("\nuser\n====\n");
    printf("id: %d\n", user->id);
    printf("watches: ");
    while(i < user->watch_count) {
        printf("%d ", user->watch[i++]->id); 
    }
    printf(" (%d)\n", i);
    if(user->lang_count) {
        printf("langs: ");
        for(int i = 0; i < user->lang_count; i++)
            if(user->langs + i) printf("[%d: %d] ", user->langs[i].id, user->langs[i].weight);
        printf("\n");
    }
}

void inspect_repo(Repo *repo)
{    
    printf("\nrepo\n====\n");
    printf("id: %d\n", repo->id);
    printf("name: '%s'\n", repo->name);
    printf("owner: %s\n", repo->owner_name);
    printf("languages: ");
    
    for(int i = 0; i < repo->lang_count; i++)
        if(repo->langs[i].id) printf("[%d: %d]", repo->langs[i].id, repo->langs[i].weight);
    
    printf("(%d) \n", repo->lang_count);
    printf("watchers: %d\n", repo->watchers);
    printf("fork?: ");
    if(repo->fork) printf("%d\n", repo->fork->id);
    else printf("no\n");
}
