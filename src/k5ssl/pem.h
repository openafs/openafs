struct pemstate;
struct pemstate * pemopen(int (*)(), void *);
int pemwrite(struct pemstate *, void *, int);
int pemread(struct pemstate *, void *, int);
int pemclose(struct pemstate *);
