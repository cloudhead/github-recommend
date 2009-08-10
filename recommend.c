/*
 *  recommend.c
 *  github-recommend
 *
 *  Created by Alexis Sellier on 07/08/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "recommend.h"

void recommend() {
    
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

struct match* find_match(Repo *repo, struct match *matches, size_t size) {
    for(int i = 0; i < size; i++) if(matches[i].repo == repo) return matches + i;
    return NULL;
}

inline int normalize(int n, int max, int norm) {
    return (int)(n / (float)max * norm);
}

inline int normalize_tapered(int x, int max, int norm, int v) {
    return ceil((normalize(x, max, norm) * (1 + ((max - x) / (float)max) * v)));
}

void masters_from_forks(User *user, struct match *vote, int *vote_count) {
    Repo *fork;
    for(int i = 0; i < user->watch_count; i++) {
        fork = user->watch[i]->fork;
        
        if(fork && !find(fork, user->watch, user->watch_count)) {
            *vote_count = *vote_count + 1;
            vote->weight = BASE_WEIGHT * 10000;
            vote->repo = fork;
        }
    }
}

User ** by_owner(User ** test, User ** set, int from, int to) {
    Repo *repo;
    Repo *owner[512];
    User *user;
    Match votes[stats.repo_count];
    int owner_count = 0, i, j, vote_count;
    
    for(int k = from; k < to; k++) {
        user = test[k];
        
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
            
            votes[vote_count].repo = owner[0];
            votes[vote_count++].weight = 8000;
            
            //        for(int j = 0; j < 3; j++) {
            //            if(owner[j]) {
            //                votes[*vote_count].repo = owner[j];
            //                votes[*vote_count].weight = owner[j]->watchers;   
            //                *vote_count = *vote_count + 1;
            //            }
            //            else break;
            //        }     
        }
    }
    return test;
}

struct lang *find_lang(int id, struct lang *langs) {
    for(int i = 0; i < LANGS; i++) {
        if(langs[i].id == id) return langs + i;
    }
    return NULL;
}

User ** forks(User ** test, User ** set, int from, int to) {
    Repo *fork;
    User *user;
    Match votes[stats.repo_count];
    int vote_count = 0;
    
    for(int k = from; k < to; k++) {
        user = test[k];
        for(int i = 0; i < user->watch_count; i++) {
            fork = user->watch[i]->fork;
            
            if(fork && !find(fork, user->watch, user->watch_count)) {
                votes[vote_count].weight = fork->watchers;
                votes[vote_count++].repo = fork;
            }
        }
    }
    return test;
}

User ** nearest_neighbour(User ** test, User ** set, int from, int to)
{
    int i, j, k, m, vote;
    Repo *repo, *candidates[WATCH_SIZE];
    register User *current;
    struct match votes[stats.repo_count], *match;
    int vote_count, candidate_count;
    int matches;
    char progress[] = ".";
    
    /* Each test-case */
    for(i = from; i < to; i++) {
        current = test[i]; vote_count = 0;
        memset(votes, 0, stats.repo_count * sizeof(struct match));
        
        /* Print progress */
        if(i > 0 && i % (stats.test_count / 100) == 0) {
            write(1, progress, 1);
        }
        
        /* Each user in the set */
        for(j = 0; j < stats.filtered_user_count; j++) {
            if(current == set[j]) continue;
            
            matches = candidate_count = 0;
                        
            /* Get number of matches, and store the rest in candidates[] */
            for(k = 0; k < set[j]->watch_count; k++) {
                repo = set[j]->watch[k];
                if(find(repo, current->watch, current->watch_count)) matches++;
                else candidates[candidate_count++] = repo;
            }
            
            /* If the watch-list is similar enough (lots of matches) */
            if(matches > (current->watch_count * REPO_MATCH_TRESHOLD)) {
                
                vote = normalize(matches, set[j]->watch_count, 100);// - abs(current->watch_count - set[j]->watch_count);
                //if(vote <= 0) vote = 1;
                
                /* Go through candidates, and add them to recommendations */
                for(k = 0; k < candidate_count; k++) {
                    repo = candidates[k];
                                        
                    /* repo is already in recommendation list, add a vote */
                    if(match = find_match(repo, votes, vote_count)) match->weight += vote;
                    else {
                        votes[vote_count].repo = repo;
                        votes[vote_count++].weight = vote;
                    }
                }
            }
        }
        /* Sort votes by weight and copy them in current->recommend */
        qsort(votes, vote_count, sizeof(struct match), &compare_matches);
        memcpy(current->recommend, votes, RECOMMEND_SIZE * sizeof(struct match));
        
        k = 0;
        
        /* Fill the rest with most popular repos */
        for(m = 0; m < RECOMMEND_SIZE; m++) {
            if(!current->recommend[m].repo) {
                //while(k < stats.repo_count) {
                    //print(m);
                    //if(repos_array[k]->langs[0].id == current->langs[0].id) {
                        current->recommend[m].repo = repos_array[k];
                        current->recommend[m].weight = BASE_WEIGHT;
                        k++;
                      //  break;
                    //}
                   // else k++;
                //}

            }
        }
        // printf("%d > %d > %d | ", votes[0].weight,votes[1].weight,current->recommend[2].weight) ;
        
    }
    return test;
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