/*
 *  main.h
 *  github-recommend
 *
 *  Copyright 2009 Alexis Sellier
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

#define WATCH_SIZE 1024
#define RECOMMEND_SIZE 10

/* Speed vs Accuracy controls */
#define REPO_WATCHER_TRESHOLD 1
#define REPO_MATCH_TRESHOLD 0.1
#define USER_MAX_WATCH 80
#define USER_MIN_WATCH 2

#define BASE_WEIGHT 10

#define TRUE 1
#define FALSE 0

#define P_SIZE sizeof(char*)

#define LANGS 40

struct Stats {
    int user_count;
    int repo_count;
    int input_count;
    int watch_count;
    int max_watchers;
    int last_user;
    int last_repo;
    int filtered_user_count;
    int filtered_watch_count;
    int newest_repo;
} stats;

struct lang {
    int id;
    int weight;
};

struct repo {
    int id;
    char name[96];
    char owner_name[64];
    int age;
    struct repo *fork;
    struct user *owner;
    struct lang langs[6];
    int lang_count;
    int watchers;
};

struct match {
    struct repo *repo;
    double weight;
};

struct user {
    int id;
    int hip_factor;
    struct repo *watch[WATCH_SIZE];
    int watch_count;
    struct match *recommend;
    struct lang langs[LANGS];
    int lang_count;
};

typedef struct user User;
typedef struct repo Repo;
typedef struct lang Lang;
typedef struct match Match;

struct repo ** repos_array;
pid_t pid;

struct user ** load_files();
struct user ** filter();
struct user ** analyze(struct user ** test, struct user ** set, int from, int to);
void results(struct user ** set, int from, int to);
double normalize(int n, int max, int norm);
double normalize_tapered(int x, int max, int norm, int v);
char find(void *needle, void *haystack, size_t size);
int compare_langs(const void*, const void*);
int compare_repos(const void*, const void*);
