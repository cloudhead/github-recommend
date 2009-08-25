/*
 *  recommend.c
 *  github-recommend
 *
 *  Copyright 2009 Alexis Sellier
 *
 */

#include "main.h"
#include "recommend.h"

#define CUT_OFF 70

int compare_matches(const void *, const void *);
void result(User *, size_t, FILE *);

void recommend(User ** input, User ** set, int from, int to, size_t n, int(*algorithm)(User *, User **))
{
    User *user;
    int votes;
    char progress = '.';
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
        if(i > 0 && i % (stats.input_count / 100) == 0) write(1, &progress, 1);
        
        user = input[i];
        user->recommend = calloc(stats.repo_count, sizeof(Match));
        
        /* Run algorithm and get number of votes */
        votes = algorithm(user, set);
        
        if(votes) qsort(user->recommend, votes, sizeof(Match), &compare_matches);
        
        result(user,  n, fresults);
        free(user->recommend);
    }
    
    fclose(fresults);
}

void result(User *user, size_t n, FILE *fp)
{
    fprintf(fp, "%d:", user->id);
   
    for(int i = 0; i < n; i++) {
        if(user->recommend[i].weight) {
            if(i) fprintf(fp, ",");
            fprintf(fp, "%d", user->recommend[i].repo->id); 
        }
    }
    fprintf(fp, "\n");
}

int compare_matches(const void *a, const void *b)
{
    struct match *x, *y;
    double result;
    x = (struct match*)a;
    y = (struct match*)b;
    
    result = y->weight - x->weight;
    
    if     (result > 0) return  1;
    else if(result < 0) return -1;
    else                return  0;
}

inline char find(void *needle, void *haystack, size_t size)
{
    for(int i = 0; i < size; i++) if(*((void**)haystack + i) == needle) return TRUE;
    return FALSE;
}

Match* find_match(Repo *repo, Match *matches, size_t size) {
    for(int i = 0; i < size; i++) if(matches[i].repo == repo) return matches + i;
    return NULL;
}

inline double normalize(int n, int max, int norm) {
    return (double)n / (double)max * (double)norm;
}

inline double normalize_tapered(int x, int max, int norm, int v) {
    return normalize(x, max, norm) * (1 + ((max - x) / (double)max) * v);
}

int by_owner(User *user, User ** set) {
    Repo *repo;
    Repo *owner[512];
    int owner_count = 0, i, j, votes = 0;
            
    for(i = 0; i < user->watch_count; i++) {
        owner_count = 0;
        memset(owner, 0, 512 * P_SIZE);
        for(j = 0; j < stats.repo_count; j++) {
            repo = repos_array[j];
            
            if(!strcmp(user->watch[i]->owner_name, repo->owner_name)
               && !find(repo, user->watch, user->watch_count)) {
                owner[owner_count++] = repo;
            }
        }
        qsort(owner, owner_count, P_SIZE, &compare_repos);
        j = 0;
        while(owner[j]) {
            user->recommend[votes].repo = owner[j];
            user->recommend[votes++].weight = owner[j]->watchers;
            j++;
        }
    }
    return votes;
}

int best_friend(User *user, User ** set) {
    int best_vote = 0, temp_vote, owner_count = 0;
    char best_friend[128];
    char temp_friend[128];
    
    Repo *repo;
    Repo *owner[512];
    
    for(int i = 0; i < user->watch_count; i++) {
        strcpy(temp_friend, user->watch[i]->owner_name);
        temp_vote = 0;
        for(int j = 0; j < user->watch_count; j++) {
            if(j == i) continue;
            if(!strcmp(temp_friend, user->watch[j]->owner_name)) temp_vote++;
        }
        if(temp_vote > best_vote) {
            best_vote = temp_vote;
            strcpy(best_friend, temp_friend);
        }
    }
    
    /* Get best friend's repos */
    if(best_vote >= user->watch_count / 2 && user->watch_count > 4
        || best_vote >= 2
        || user->watch_count < 5) {
        for(int j = 0; j < stats.repo_count; j++) {
            repo = repos_array[j];
            
            if(!strcmp(best_friend, repo->owner_name)
               && !find(repo, user->watch, user->watch_count)) {
                owner[owner_count++] = repo;
            }
        }
        qsort(owner, owner_count, P_SIZE, &compare_repos);

        for(int j = 0; j < owner_count; j++) {
            user->recommend[j].repo = owner[j];
            user->recommend[j].weight = normalize(best_vote, user->watch_count, 100);
            j++;
        }
    }
    return owner_count;
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
    
    if(user->watch_count <= 1) {
        for(int i = 0; i < 40; i++) {
            if(!find(repos_array[k], user->watch, user->watch_count)) {
                user->recommend[i].repo = repos_array[k];
                user->recommend[i].weight = repos_array[k]->watchers;
                k++;
            }
        } 
    }
    return votes;
}

int nearest_neighbour(User *user, User ** set, int langs)
{
    int i, k;
    double vote;
    Repo *repo, *candidates[WATCH_SIZE];
    Match *match;
    int votes = 0;
    int candidate_count;
    int matches;
    
    if(langs) get_langs(user);

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
                
                if(langs) vote += match_langs(user->langs, repo->langs, user->lang_count, repo->lang_count);

                /* repo is already in recommendation list, add a vote */
                if(match = find_match(repo, user->recommend, votes)) match->weight += vote;
                else {
                    user->recommend[votes].repo = repo;
                    user->recommend[votes++].weight = vote
                        + normalize(repo->watchers, stats.max_watchers, 1);
                }
            }
        }
    }
    return votes;
}

int nn(User *user, User **set) {
    return nearest_neighbour(user, set, FALSE);
}

int nn_langs(User *user, User **set) {
    return nearest_neighbour(user, set, TRUE);
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

double match_langs(Lang *user_langs, Lang *repo_langs, int ul_count, int rl_count) {
    double vote = 0;
    
    for(int i = 0; i < ul_count; i++)
        for(int j = 0; j < rl_count; j++)
            if(user_langs[i].id == repo_langs[j].id)
                vote += sqrt(pow((ul_count - i), 2) + pow((rl_count - j), 2));
    
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