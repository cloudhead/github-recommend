
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))

void recommend(User **, User **, int, int, size_t, int(*algorithm)(User *, User **));

/* Algorithms */
int nearest_neighbour(User *, User **, int);
int nn(User *, User **);
int nn_langs(User *, User **);

int popular(User *, User **);
int forks(User *, User **);
int by_owner(User *, User **);
int best_friend(User *, User **);

void get_langs(User*);
double match_langs(Lang*, Lang*, int, int);