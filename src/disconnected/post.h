
struct op_no_ent {
	int	num;
	struct op_no_ent *next;
};

struct sort_ent {
	struct op_no_ent *se_nump;
	int 	se_num;
	int	se_op;
	char	se_op_name[MAX_NAME];
	char	se_data[MAX_NAME];
	int	fid;
};


