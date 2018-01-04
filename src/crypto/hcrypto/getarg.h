/* Just disable the getarg functionality for now */

struct getargs{
    const char *long_name;
    char short_name;
    enum { arg_integer,
           arg_string,
           arg_flag,
           arg_negative_flag,
           arg_strings,
           arg_double,
           arg_collect,
           arg_counter
    } type;
    void *value;
    const char *help;
    const char *arg_help;
};

static inline int
getarg(struct getargs *args, size_t num_args,
       int argc, char **argv, int *goptind) {
  return 0;
}

static inline void
arg_printusage (struct getargs *args,
                size_t num_args,
                const char *progname,
                const char *extra_string) {
  return;
}

static inline void
rk_print_version(const char *str) {
  return;
}
