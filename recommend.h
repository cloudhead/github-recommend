
#define min(a,b) (((a) < (b)) ? (a) : (b))

void recommend(User **, User **, int, int, size_t, int(*algorithm)(User *, User **));

/* Algorithms */
int nearest_neighbour(User *, User **);
int popular(User *, User **);
int forks(User *, User **);
int by_owner(User *, User **);
