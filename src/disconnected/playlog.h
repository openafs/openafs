
typedef struct repl_stats {
	log_ops_t	op;
	long		opno;	
	struct timeval	start_time;
	struct timeval	replstart_time;
	struct timeval	finish_time;
	int status;
} repl_stats_t;


typedef struct final_stats {
	float	min_time;
	float	max_time;
	float	total_time;
	int	num_ops;
} final_stats_t;
	
