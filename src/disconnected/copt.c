/* copt version 1.00 (C) Copyright Christopher W. Fraser 1984 */

#include <ctype.h>
#include <stdio.h>
#define HSIZE 107
#define MAXLINE 2000

int debug = 0;
int verbose = 0;

extern int optind;

char *next_component();

struct lnode {
	char *l_text;
	struct lnode *l_prev, *l_next;
};

struct onode {
	struct lnode *o_old, *o_new;
	struct node *cond;
	struct onode *o_next;
} *opts = 0;

#define MAX_VARS	20
/* These are the node operations */
#define	AND		1
#define OR		2
#define	LESS		3
#define	GREATER		4
#define NOT_EQUAL	5
#define	EQUAL		6
#define VAL		7
#define CONST		8

struct node {
	int	op;
	struct node *left;
	struct node *right;
	};

	
/* connect - connect p1 to p2 */
connect(p1, p2) struct lnode *p1, *p2; {
	if (p1 == 0 || p2 == 0)
		error("connect: can't happen\n");
	p1->l_next = p2;
	p2->l_prev = p1;
}


/* error - report error and quit */
error(s) char *s; {
	fputs(s, stderr);
	exit(1);
}

/* getlst - link lines from fp in between p1 and p2 */
getlst(fp, quit, p1, p2) FILE *fp; char *quit; struct lnode *p1, *p2; {
	char *install(), lin[MAXLINE];

	connect(p1, p2);
	while (fgets(lin, MAXLINE, fp) != NULL && strcmp(lin, quit)) {
	    if(*lin != '#') {
		insert(install(lin), p2);
		}
	    }
}
#define EQ(x, y)        (strcmp(x, y)==0) 

#define	OPERATION	1
#define VALUE		2
#define OPEN_P		3
#define CLOSE_P		4
#define	CMP_OP		5

int
get_type(str,op)
	char *str;
	int *op;
	{
	if(debug) printf("get_type: <%s> \n",str);
	if (EQ(str,"=")) {
		*op =EQUAL; 
		return CMP_OP;
		}
	else if (EQ(str,"!=")) {
		*op =NOT_EQUAL;
		return CMP_OP;
                }
	else if (EQ(str,"|")) {
		*op = OR;
		return OPERATION;
                }
	else if (EQ(str,"&")) {
		*op = AND;
		return OPERATION;
                }
	else if (EQ(str,"<")) {
		*op = LESS;
		return CMP_OP;
                }
	else if (EQ(str,">")) {
		*op = GREATER;
		return CMP_OP;
                }
	else if (*str == '%') {
		*op = VAL;
		return VALUE;
		}
	else if (*str == '(') {
		return OPEN_P;
		}
	else if (*str == ')') {
		return CLOSE_P;
		}
	else if (isdigit(*str)) {
		return CONST;
		}
	else  {
		printf("unknown element <%s> \n",str);
		exit(1);
		}

	}


/* process the conditional input line */
struct node *
process_conditional(str)
	char *str;
{
	struct node *left = 0;
	struct node *right = 0;
	struct node *cur_node = 0;
	char buffer[MAXLINE];
	char *current;
	int  type;
	int  operation;

	if(debug) printf("process cond <%s> \n",str);
	
	current = str;
	while (current =  next_component(current,buffer)) {
		type = get_type(buffer,&operation);
	
		if(type == OPERATION) {
			if(!left) {
				printf("parse error \n");
				exit(1);
			}

			cur_node = (struct node* ) malloc(sizeof(*cur_node));
			cur_node->op = operation;
			cur_node->left = left;
			right = process_conditional(current);

			if(!right) {
				printf("parse error \n");
				exit(1);
			}
			
			cur_node->right = right;
			return cur_node;
		} else if(type == CMP_OP) {
			if(!left) {
				printf("parse error \n");
				exit(1);
			    }

			cur_node = (struct node* ) malloc(sizeof(*cur_node));
			cur_node->op = operation;
			cur_node->left = left;
		} else if(type == VALUE) {
			int slot;
			char *component;

			if((left) && (!cur_node)) {
				printf("parse error \n");
				exit(1);
			}
			if(!left) {
				left = (struct node* ) malloc(sizeof(*left));
				left->op = VAL;
				left->right = (struct node *) 0;
				component = buffer;
				component++;
				slot = atoi(component);
				left->left = (struct node *) slot;   
			} else {
				right = (struct node* ) malloc(sizeof(*right));
				right->op = VAL;
				right->right = (struct node *) 0;
				component = buffer;
				component++;
				slot = atoi(component);
				right->left = (struct node *) slot;   
				cur_node->right = right;
				right = (struct node *) 0;	
				left = cur_node;
				cur_node = (struct node *) 0;	
			}
		} else if (type == CONST) {
			int value;

			if((left) && (!cur_node)) {
				printf("parse error \n");
				exit(1);
			}
			if(!left) {
				left = (struct node* ) malloc(sizeof(*left));
				left->op = CONST;
				left->right = (struct node *) 0;
				value = atoi(buffer);
				left->left = (struct node *) value;   
			} else {
				right = (struct node* ) malloc(sizeof(*right));
				right->op = CONST;
				right->right = (struct node *) 0;
				value = atoi(buffer);
				right->left = (struct node *) value;   
				cur_node->right = right;
				right = (struct node *) 0;	
				left = cur_node;
				cur_node = (struct node *) 0;	
			}
		} else if(type == OPEN_P) {
			left = process_conditional(current);
			if(debug) printf("after open_p left %x \n",left);
		} else if(type == CLOSE_P) {
			if(debug) printf("close_p left %x \n",left);
			return left;
		} else {
			printf("unknown type %d \n", type);
		}
	
	}

	/* If we made it to here, at most we have the left node defined */

	if(debug)
		printf("fall through, left %x \n",left);
	return left;
}


int
print_parse(parse_node,level)
	struct node *parse_node;
	int level;

{
	int value;
	if(debug) printf("parse node %x \n",parse_node);
   if(!parse_node) return;

	if(parse_node->op == VAL)  {
		value = (int) parse_node->left;
		if(debug) printf("VAL %d \n",value);
		return;
		}
		
	if(parse_node->left){
		print_parse(parse_node->left,level+1);
		}

	if(debug) printf("operation %d level %d\n",parse_node->op,level);

	if(parse_node->right){
		print_parse(parse_node->right,level+1);
		}
	
}

/* init - read patterns file */
init(fp) FILE *fp; {
	struct lnode head, tail;
	struct onode *p, **next;

        char buffer[MAXLINE];
	char *comp;
        char *temp_str;	
	struct node *parse_node;
	tail.l_text = NULL;
	head.l_text = NULL;
	
	next = &opts;
	while (*next)
		next = &((*next)->o_next);
	while (!feof(fp)) {
		p = (struct onode *) malloc((unsigned) sizeof(struct onode));

		/* read the old lines */
		getlst(fp, "=\n", &head, &tail);
		head.l_next->l_prev = 0;
		if (tail.l_prev)
			tail.l_prev->l_next = 0;
		p->o_old = tail.l_prev;
		
		/* read the new lines */
		getlst(fp, "=\n", &head, &tail);
		tail.l_prev->l_next = 0;
		if (head.l_next)
			head.l_next->l_prev = 0;
		p->o_new = head.l_next;

		*next = p;
		next = &p->o_next;

		/* read the condition lines */
		getlst(fp, "\n", &head, &tail);
		temp_str = head.l_next->l_text;
		while(temp_str = next_component(temp_str,buffer)) {
		    if(debug) printf("component <%s> string <%s> \n",buffer,temp_str);
		    }
		temp_str = head.l_next->l_text;

		parse_node = process_conditional(temp_str);

		if(parse_node) {
			if(debug) printf("after process node %x \n",parse_node);
			print_parse(parse_node,0);
			}
		
		/* link in the conditional tree */
		p->cond = parse_node;
	
			

	}
	*next = 0;
}

/* insert - insert a new node with text s before node p */
insert(s, p) char *s; struct lnode *p; {
	struct lnode *n;

	n = (struct lnode *) malloc(sizeof *n);
	n->l_text = s;
	connect(p->l_prev, n);
	connect(n, p);
}

/* Get the next component from the condition line */

char *next_component(str,buffer)
 	char *str;
	char *buffer;

 	{
	char *temp_str;
	char *new_str;
	temp_str = str;

	if(!str) return (char *) 0;

	/* pass all of the blanks */	
	while((*temp_str == ' ') || (*temp_str == '\t')) {
		temp_str ++;
	     	}

	/* if it's the end of the line return 0 */

	if(*temp_str == '\n') {
		return (char *)0;
		}

	new_str = buffer;

	while((*temp_str != ' ')&&(*temp_str != '\n') &&(*temp_str != '\t'))
	    {
	    *new_str++ = *temp_str++;

	    }

        *new_str = 0;
        return temp_str;
	}


/* install - install str in string table */
char *install(str) char *str; {
	register struct hnode *p;
	register char *p1, *p2, *s;
	register int i;
	static struct hnode {
		char *h_str;
		struct hnode *h_ptr;		  
	} *htab[HSIZE] = {0};
	
	s = str;
	for (i = 0; *s; i += *s++)
		;
	i %= HSIZE;

	for (p = htab[i]; p; p = p->h_ptr) 
		for (p1=str, p2=p->h_str; *p1++ == *p2++; )
			if (p1[-1] == '\0') 
				return (p->h_str);

	p = (struct hnode *) malloc(sizeof *p);
	p->h_str = (char *) malloc((s-str)+1);
	strcpy(p->h_str, str);
	p->h_ptr = htab[i];
	htab[i] = p;
	return (p->h_str);
} 


/* main - peephole optimizer */
main(argc, argv)
	int argc;
	char **argv;
{	FILE *fp;
	int i, c;
	struct lnode head, *p, *opt(), tail;

	while ((c = getopt(argc, argv, "dv")) != EOF) {
		switch (c) {
		case 'v':
			verbose++;
			break;
		case 'd':
			debug++;
			break;
		default:
			error("usage: copt [-d] [-v] patternfile ...\n");
		}
	}

	argc -= optind;
	argv += optind;

	for (i = 0; i < argc; i++)
		if ((fp = fopen(argv[i], "r")) == NULL) 
			error("copt: can't open patterns file\n");
		else {
			init(fp);
		    fclose(fp);
		}


	getlst(stdin, "", &head, &tail);
	head.l_text = tail.l_text = "";

	for (p = head.l_next; p != &tail; p = opt(p))
		;

	for (p = head.l_next; p != &tail; p = p->l_next)
		fputs(p->l_text, stdout);
}

/* match - match ins against pat and set vars */
int 
match(ins, pat, vars)
	char *ins, *pat, **vars;
{
	char *p, lin[MAXLINE];

	while (*ins && *pat)
		if (pat[0] == '%' && isdigit(pat[1])) {
			for (p = lin; *ins && *ins != pat[3];)
				*p++ = *ins++;
			*p = 0;
			p = install(lin);
			if (vars[atoi(&pat[1])] == 0)
				vars[atoi(&pat[1])] = p;
			else if (vars[atoi(&pat[1])] != p)
				return 0;
			pat += 3;
		}
		else if (*pat++ != *ins++)
			return 0;
	return *pat==*ins;
}


/*
**  Logical or of the left and right expressions
*/

int
and(nd,vars)
    struct node *nd;
    char *vars[MAX_VARS];

{
	return(eval_conditional(nd->left,vars) && 
		eval_conditional(nd->right,vars));
}

/*
**  Logical or of the left and right expressions
*/

int
or(nd,vars)
    struct node *nd;
    char *vars[MAX_VARS];

{
	return(eval_conditional(nd->left,vars) || 
		eval_conditional(nd->right,vars));
}

/*
**  Test to see if left and right substrings are equal
*/

int
equal(nd,vars)
    struct node *nd;
    char *vars[MAX_VARS];

{
	int left_ind;
	int right_ind;
	int rval, lval;
	int code;

	int eq;

	if(nd->left->op == VAL) {
		left_ind = (int) nd->left->left;
		lval = atoi(vars[left_ind]);
	} else if (nd->left->op == CONST) {
		lval = (int) nd->left->left;
		
	} else {
		printf("equal: left  bad parse tree \n");
		exit(1);
	}

	if(nd->right->op == VAL) {
		right_ind = (int) nd->right->left;
		rval = atoi(vars[right_ind]);
	} else if (nd->right->op == CONST) {
		rval = (int) nd->right->left;
	} else {
		printf("equal: right bad parse tree \n");
		exit(1);
	}


	code = (lval == rval);

	if(debug) {
		 printf("eq: lval %d rval %d code %d \n",lval, rval, code);
	}
	return (code);
}


/*
**  Test to see if left and right substrings are not_equal
*/

int
not_equal(nd,vars)
    struct node *nd;
    char *vars[MAX_VARS];

{

	return (!equal(nd,vars));
}

/* Test to see if left subr is less than right substr */

int
less(nd,vars)
    struct node *nd;
    char *vars[MAX_VARS];

{
	int left_ind;
	int right_ind;
	char *left_str;
	char *right_str;
	int rval, lval;
	int code;

	if(nd->left->op != VAL) {
		printf("less bad parse tree \n");
		exit(1);
	}

	if(nd->right->op != VAL) {
		printf("less bad parse tree \n");
		exit(1);
	}

	left_ind = (int) nd->left->left;
	right_ind = (int) nd->right->left;
	if(debug)
		printf("less: left_ind %d right_int %d \n",left_ind, right_ind);

	left_str = vars[left_ind];
	right_str = vars[right_ind];

	rval = atoi(right_str);
	lval = atoi(left_str);
	code = (lval < rval);

	if(debug) {
		 printf("less: left_str <%s> right <%s> \n",left_str,
			right_str);
		 printf("less: lval %d rval %d code %d \n",lval, rval, code);
	}
	return (code);
}

/*
**  Test to see if left and right substrings are not_equal
*/

int
greater(nd,vars)
    struct node *nd;
    char *vars[MAX_VARS];

    {
    int left_ind;
    int right_ind;
    char *left_str;
    char *right_str;
    int eq;
   
    if(nd->left->op != VAL) {
	printf(" greater bad parse tree \n");
	exit(1);
	}

    if(nd->right->op != VAL) {
	printf(" greater bad parse tree \n");
	exit(1);
	}

    left_ind = (int) nd->left->left;
    right_ind = (int) nd->right->left;

    left_str = vars[left_ind];
    right_str = vars[right_ind];

    if(debug) printf("greater: left_str <%s> right <%s> \n",left_str,right_str);

    eq = strcmp(left_str,right_str);

    if(debug) printf("greater: eq %d \n",eq);
 
    if(eq>0) return 1;
 	return 0;
	
    }




int
eval_conditional(cond,vars)
	struct node *cond;
	char *vars[MAX_VARS];
{
	int ret;

	if(!cond) {
		if(debug) printf("eval_conditional: no cond \n");
		return 1;
		}

	switch(cond->op) {
		case AND:
			ret = and(cond,vars);
			break;

		case OR:
			ret = or(cond,vars);
			break;

		case LESS:
			ret = less(cond,vars);
			break;

		case GREATER:
			ret = greater(cond,vars);
			break;

		case NOT_EQUAL:
			ret = not_equal(cond,vars);
			break;

		case EQUAL:
			ret = equal(cond,vars);
			break;

		default:
			printf("unkown op %d \n",cond->op);
			return 0;
		}

	return ret;

}
	



	



/* opt - replace instructions ending at r if possible */
struct lnode *
opt(r)
	struct lnode *r; 
{
	char *vars[MAX_VARS];
	int i;
	struct lnode *c, *p, *rep();
	struct onode *o;
	int    cond;

	for (o = opts; o; o = o->o_next) {
		c = r;
		p = o->o_old;
		for (i = 0; i < MAX_VARS; i++)
			vars[i] = 0;
		while (p && c && match(c->l_text, p->l_text, vars)) {
			p = p->l_prev;
			c = c->l_prev;
		}
			
		if (p == 0) {
		        /* check the conditional values */
			cond=eval_conditional(o->cond,vars);
			if(cond) {
				return rep(c, r->l_next, o->o_new, vars);
				}
			}
	}
	return r->l_next;
}

/* rep - substitute vars into new and replace lines between p1 and p2 */
struct lnode *
rep(p1, p2, new, vars) 
	struct lnode *p1, *p2, *new;
	char **vars; 
{
	char *args[MAX_VARS], *exec(), *subst(); 
	int i;
	struct bnode *b;
	struct lnode *p, *psav;

	for (p = p1->l_next; p != p2; p = psav) {
		psav = p->l_next;
		if (verbose)
			fputs(p->l_text, stderr);
		free(p);
	}
	connect(p1, p2);
	if (verbose)
		fputs("=\n", stderr);
	for (; new; new = new->l_next) {
		for (i = 0; i < MAX_VARS; i++)
			args[i] = 0;
		insert(subst(new->l_text, vars), p2);
		if (verbose)
			fputs(p2->l_prev->l_text, stderr);
	}
	if (verbose)
		putc('\n', stderr);
	return p1->l_next;
}

/* subst - return result of substituting vars into pat */
char *subst(pat, vars) char *pat, **vars; {
	char *install(), lin[MAXLINE], *s;
	int i;

	i = 0;
	for (;;)
		if (pat[0] == '%' && isdigit(pat[1])) {
			for (s = vars[atoi(&pat[1])]; i < MAXLINE &&
			    (lin[i] = *s++); i++)
				;
			pat += 3;
		}
		else if (i >= MAXLINE)
			error("line too long\n");
		else if (!(lin[i++] = *pat++))
			return install(lin);
}
