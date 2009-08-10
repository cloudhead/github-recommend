/*
 *  main.c
 *  github-recommend
 *
 *  Copyright 2009 Alexis Sellier
 *
 */

#include "main.h"
#define SPLIT 1500

User **users;
Repo **repos;
pid_t pid;

void print(int n) { printf("%d\n", n); }

int main(int argc, const char *argv[])
{
    User **test, **filtered_users;
    int t, status;
        
    printf("parsing data...\n");
    test = load_files();
    printf("filtering...\n");
    filtered_users = filter();
    printf("sorting repos...\n");
    
    qsort(repos_array, stats.repo_count, P_SIZE, &compare_repos);
    stats.max_watchers = repos_array[0]->watchers;
    
    printf("analyzing...\n");
    
    /* Print progress bar */
    printf("0 [");
    for(int i = 0; i < 50; i++) printf("-");
    printf("|");
    for(int i = 0; i < 50; i++) printf("-");
    printf("] 100\n   ");
    fflush(stdout);
    
    pid = fork();
    
    if(pid > 0) {
        t = time(NULL);
        test = recommend(test, filtered_users, 0, SPLIT);
        wait(&status);
        printf("\n");
        printf("total time: %lds\n", time(NULL) - t);
        printf("printing results to results.txt\n");
        results(test, 0, SPLIT);
    }
    else if(pid == 0) {
        test = recommend(test, filtered_users, SPLIT, stats.test_count);
        printf("*"); fflush(stdout);
        results(test, SPLIT, stats.test_count);
    }
    else {
        fprintf(stderr, "couldn't fork!");
        exit(1);
    }
    
    /* Free the memory */
    for(int i = 0; i < stats.last_user;  i++) if(users[i]) free(users[i]);
    for(int i = 0; i < stats.repo_count; i++) free(repos_array[i]);
    
    free(users); free(repos_array);
    free(repos); free(test);
    free(filtered_users);
         
    /* Exit child procress */
    if(!pid) _exit(0);
    
    return 0;
}

User** load_files()
{
    FILE *fdata  = NULL, 
         *frepos = NULL, 
         *flang  = NULL,
         *ftest  = NULL;
    
    char line[512];
    
    users = calloc(64000, P_SIZE);
    repos = calloc(128000, P_SIZE);
    repos_array = calloc(128000, P_SIZE);

    fdata  = fopen("github_resys/data/training_data.txt", "r");
    ftest  = fopen("github_resys/data/local_test.txt", "r");
    //fdata  = fopen("data/data.txt", "r");
    //ftest  = fopen("data/test.txt", "r");
    frepos = fopen("data/repos.txt", "r");
    flang  = fopen("data/lang.txt", "r");
    
    if(fdata == NULL || frepos == NULL || 
       flang  == NULL || ftest == NULL) {
        printf("Couldn't read data files");
        exit(1);
    }
    
    /*
     *  repos
     */
    int repo_count = 0, fork = 0, repo = 0;
    char *date, owner_name[64], name[96], *ptr;
    struct tm tp;
    
    while( fgets(line, sizeof(line), frepos) != NULL ) {
        if( line[strlen(line) - 1] == '\n' ) line[strlen(line) - 1] = 0;
        
        repo = strtoul(strtok(line, ":"), NULL, 0);
        sscanf(strtok(NULL, ","), "%[^/]/%s", owner_name, name);
        date = strtok(NULL, ",");
        ptr = strtok(NULL, ",");
        
        if(ptr) fork = strtoul(ptr, NULL, 0);
        else fork = 0;

        repos[repo] = malloc(sizeof(Repo));
        repos[repo]->id = repo;
        repos[repo]->watchers = 0;

        strptime(date, "%F", &tp);
        repos[repo]->age = tp.tm_year * 365 + tp.tm_yday;
        
        if(repos[repo]->age > stats.newest_repo) 
            stats.newest_repo = repos[repo]->age;
                
        repos_array[repo_count++] = repos[repo];
        memcpy(repos[repo]->owner_name, owner_name, strlen(owner_name));
        memcpy(repos[repo]->name, name, strlen(name));
        
        if(fork > 0) {
            repos[repo]->fork = repos[fork];
            fork = 0;
        }
        else repos[repo]->fork = 0;
    }
    stats.last_repo = repo;

    /*
     *  Data
     */
    int user_count = 0, watch_count = 0, user;
    
    while( fgets(line, sizeof(line), fdata) != NULL ) {        
        sscanf(line, "%d:%d\n", &user, &repo);
        
        /* Allocate a new user, and set his watch end pointer */
        if(users[user] == NULL) {
            users[user] = malloc(sizeof(User));
            users[user]->id = user;
            users[user]->watch_count = 0;
            user_count++;
        }
        repos[repo]->watchers++;
                
        /* Add repo to watch-list */
        users[user]->watch[ users[user]->watch_count ] = repos[repo];
        users[user]->watch_count++;
        watch_count++;
    }

    stats.last_user = user;
    
    
    /*
     *  Languages
     */
    char lang_str[512];
    struct lang langs[16];
    char *tok;
    int lang_id;
    int loc, lang_count;
    
    while( fgets(line, sizeof(line), flang) != NULL ) {
        sscanf(line, "%d:%s\n", &repo, lang_str);
        tok = strtok(lang_str, ",");
        memset(langs, 0, sizeof(struct lang) * 16);
        
        for(lang_count = 0; tok; lang_count++) {
            sscanf(tok, "%d;%d", &lang_id, &loc);
            langs[lang_count].id = lang_id;
            langs[lang_count].weight = loc;
            tok = strtok(NULL, ",");
        }
        qsort(langs, lang_count, sizeof(struct lang), &compare_langs);
        
        if(repos[repo]) {
            memcpy(repos[repo]->langs, langs, 6 * sizeof(struct lang));
            repos[repo]->lang_count = lang_count;
        }
    }
    
    /*
     *  Test Data
     */
    int test_count = 0;
    User **test = calloc(64000, P_SIZE);

    while( fgets(line, sizeof(line), ftest) != NULL ) {
        sscanf(line, "%d\n", &user);
        if(users[user]) {
            test[test_count] = malloc(sizeof(User));
            memcpy(test[test_count++], users[user], sizeof(User));
        }
    }
    
    /* Save some stats */
    stats.repo_count  = repo_count;
    stats.user_count  = user_count;
    stats.test_count  = test_count;
    stats.watch_count = watch_count;
    
    /* Reallocate the pointer arrays to fit more tightly */
    realloc(repos_array, repo_count * P_SIZE);
    realloc(test,  test_count * P_SIZE);
    realloc(users, (stats.last_user + 1) * P_SIZE);
    realloc(repos, (stats.last_repo + 1) * P_SIZE);
    
    printf("found %d repositories.\n", repo_count);
    printf("found %d users.\n", user_count);
    printf("found %d watches.\n", watch_count);
    printf("found %d test users.\n", test_count);
    
    fclose(frepos); fclose(fdata);
    fclose(flang);  fclose(ftest);
            
    return test;
}


void results(User **set, int from, int to)
{
    FILE *fresults;
    char mode[1];
    
    if(pid > 0) mode[0] = 'a';
    else mode[0] = 'w';
    
    if((fresults = fopen("data/results.txt", mode)) == NULL) {
        printf("can't create results file.");
        exit(1);
    }
    for(int i = from; i < to; i++) {
        fprintf(fresults, "%d:", set[i]->id);
        for(int j = 0; j < RECOMMEND_SIZE; j++) {
            if(set[i]->recommend[j].repo) {
                if(j) fprintf(fresults, ",");
                fprintf(fresults, "%d", set[i]->recommend[j].repo->id); 
            }
        }
        fprintf(fresults, "\n");
    }
    
    fclose(fresults);
}

User **filter()
{    
    /* Filter users */
    User **filtered_users = calloc(64000, P_SIZE);
    Repo **filtered_repos;
    int i, j = 0, k;
    
    stats.filtered_watch_count = 0;
    
    for(i = 0; i < stats.last_user + 1; i++) {
        if( users[i] &&
            USER_MIN_WATCH <= users[i]->watch_count && 
            USER_MAX_WATCH >= users[i]->watch_count ) {
            filtered_users[j++] = users[i];
        }
    }
    stats.filtered_user_count = j;
    realloc(filtered_users, j * P_SIZE);
    
    /* Filter repos */    
    for(i = 0; i < stats.filtered_user_count; i++) {
        
        filtered_repos = calloc(WATCH_SIZE, P_SIZE);
        
        for(j = k = 0; j < filtered_users[i]->watch_count; j++) {
            if(filtered_users[i]->watch[j]->watchers >= REPO_WATCHER_TRESHOLD) {
                filtered_repos[k++] = filtered_users[i]->watch[j];
            }
        }
        filtered_users[i]->watch_count = k;
        stats.filtered_watch_count += k;
        memcpy(filtered_users[i]->watch, filtered_repos, WATCH_SIZE * P_SIZE);
        free(filtered_repos);
    }
    
    printf("new user count: %d\n", stats.filtered_user_count);
    printf("new watch count: %d\n", stats.filtered_watch_count);
    
    return filtered_users;
}

int compare_repos(const void *a, const void *b)
{
    Repo *x, *y;
    x = *(void **)a;
    y = *(void **)b;
    
    return y->watchers - x->watchers;
}

int compare_langs(const void *a, const void *b)
{
    struct lang *x, *y;
    x = (struct lang*)a;
    y = (struct lang*)b;
    
    return y->weight - x->weight;
}

