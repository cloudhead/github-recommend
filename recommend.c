/*
 *  recommend.c
 *  github-recommend
 *
 *  Copyright 2009 Alexis Sellier
 *
 */

#include "main.h"
#include "recommend.h"

int compare_matches(const void *, const void *);
void result(User *, size_t, FILE *);

void recommend(User ** input, User ** set, int from, int to, size_t n, int(*algorithm)(User *, User **))
{
    User *user;
    int votes;
    char progress[] = ".";
    char path[32];
    
    FILE *fresults;
    
    if(pid > 0) sprintf(path, "data/results.txt");
    else        sprintf(path, "data/results-1.txt");

    if((fresults = fopen(path, "w")) == NULL) {
        printf("can't create results file.");
        exit(1);
    }
    
    for(int i = from; i < to; i++) {
        /* Print progress */
        if(i > 0 && i % (stats.input_count / 100) == 0) write(1, progress, 1);
        
        user = input[i];
        user->recommend = calloc(stats.repo_count, sizeof(Match));
        
        /* Run algorithm and get number of votes */
        votes = algorithm(user, set);
        
        if(votes) {
            qsort(user->recommend, votes, sizeof(Match), &compare_matches);
            result(user, n, fresults);
        }
        else free(user->recommend);
    }
    
    fclose(fresults);
}

void result(User *user, size_t n, FILE *fp)
{
    fprintf(fp, "%d:", user->id);
   
    for(int i = 0; i < n; i++) {
        if(user->recommend + i && user->recommend[i].repo) {
            if(i) fprintf(fp, ",");
            fprintf(fp, "%d", user->recommend[i].repo->id); 
        }
    }
    fprintf(fp, "\n");
}

int compare_matches(const void *a, const void *b)
{
    struct match *x, *y;
    x = (struct match*)a;
    y = (struct match*)b;
    
    return y->weight - x->weight;
}

inline char find(void *needle, void *haystack, size_t size)
{
    for(int i = 0; i < size; i++) if(*((void**)haystack + i) == needle) return TRUE;
    return FALSE;
}

inline Match* find_match(Repo *repo, Match *matches, size_t size) {
    for(int i = 0; i < size; i++) if(matches[i].repo == repo) return matches + i;
    return NULL;
}

inline int normalize(int n, int max, int norm) {
    return (int)(n / (float)max * norm);
}

inline int normalize_tapered(int x, int max, int norm, int v) {
    return ceil((normalize(x, max, norm) * (1 + ((max - x) / (float)max) * v)));
}

int by_owner(User *user, User ** set) {
    Repo *repo;
    Repo *owner[512];
    int owner_count = 0, i, j, votes = 0;
            
    for(i = 0; i < user->watch_count; i++) {
        owner_count = 0;
        for(j = 0; j < stats.repo_count; j++) {
            repo = repos_array[j];
            
            if(!strcmp(user->watch[i]->owner_name, repo->owner_name)
               && !find(repo, user->watch, user->watch_count)) {
                owner[owner_count++] = repo;
            }
        }
        qsort(owner, owner_count, P_SIZE, &compare_repos);
                
        for(int j = 0; owner[j]; j++) {
            user->recommend[votes].repo = owner[j];
            user->recommend[votes++].weight = owner[i]->watchers;
        }
    }
    return votes;
}

struct lang *find_lang(int id, struct lang *langs) {
    for(int i = 0; i < LANGS; i++) {
        if(langs[i].id == id) return langs + i;
    }
    return NULL;
}

int forks(User *user, User ** set) {
    Repo *fork;
    int votes = 0;
    
    for(int i = 0; i < user->watch_count; i++) {
        fork = user->watch[i]->fork;
        
        if(fork && !find(fork, user->watch, user->watch_count)) {
            user->recommend[votes].weight = fork->watchers;
            user->recommend[votes++].repo = fork;
        }
    }
    return votes;
}

int popular(User *user, User ** set) {
    int k = 0, votes = 0;
    
    /* Fill the rest with most popular repos */
    for(int m = 0; m < RECOMMEND_SIZE; m++) {
        if(!user->recommend[m].repo) {
            user->recommend[m].repo = repos_array[k];
            user->recommend[m].weight = BASE_WEIGHT;
            k++;
        }
    } 
    return votes;
}

int nearest_neighbour(User *user, User ** set)
{
    int i, k, vote;
    Repo *repo, *candidates[WATCH_SIZE];
    Match *match;
    int votes = 0, candidate_count;
    int matches;
    
    /* Each user in the set */
    for(i = 0; i < stats.filtered_user_count; i++) {
        if(user == set[i]) continue;
        
        matches = candidate_count = 0;
        
        /* Get number of matches, and store the rest in candidates[] */
        for(k = 0; k < set[i]->watch_count; k++) {
            repo = set[i]->watch[k];
            if(find(repo, user->watch, user->watch_count)) matches++;
            else candidates[candidate_count++] = repo;
        }
        
        /* If the watch-list is similar enough (lots of matches) */
        if(matches > (user->watch_count * REPO_MATCH_TRESHOLD)) {
            
            vote = normalize(matches, set[i]->watch_count, 100);
            
            /* Go through candidates, and add them to recommendations */
            for(k = 0; k < candidate_count; k++) {
                repo = candidates[k];
                
                /* repo is already in recommendation list, add a vote */
                if(match = find_match(repo, user->recommend, votes)) match->weight += vote;
                else {
                    user->recommend[votes].repo = repo;
                    user->recommend[votes++].weight = vote
                        + normalize(repo->watchers, stats.max_watchers, 10);
                }
            }
        }
    }
    return votes;
}

void get_langs(User *user) {
    struct lang *ptr;
    struct lang *lang;
    int vote, i, j;
    
    for(i = 0; i < user->watch_count; i++) {
        for(j = 0; j < user->watch[i]->lang_count; j++) {
            
            lang = user->watch[i]->langs + j;
            vote = lang->weight;
            
            /* If lang isn't in user list */
            if((ptr = find_lang(lang->id, user->langs)) == NULL) {
                ptr = user->langs + user->lang_count;
                user->lang_count++;
            }
            
            ptr->weight += vote;
            ptr->id = lang->id;
        }
    }
    
    qsort(user->langs, LANGS, sizeof(struct lang), &compare_langs);
}

int match_langs(Lang *user_langs, Lang *repo_langs, int ul_count, int rl_count) {
    int vote = 0;
    
    for(int i = 0; i < ul_count; i++)
        for(int j = 0; j < rl_count; j++)
            if(user_langs[i].id == repo_langs[j].id)
                vote += (ul_count + rl_count) - (i + j);
    
    return vote;
}

/* Computer average watchers of watched repos 
 to find out if the user is more interested in
 popular or obscure repos */
void average_watchers(User *user) {
    int popular = 1;
    int normal = 1;
    
    if(!user->hip_factor) {
        for(int i = 0; i < user->watch_count; i++) {
            if(user->watch[i]->watchers > 50) normal++;
            if(normalize_tapered(user->watch[i]->watchers, stats.max_watchers, 100, 1) > 50) popular++;
        }
        user->hip_factor = popular * 100 / normal;
    }
}