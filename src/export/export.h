/*
 * $Locker:  $
 *
 * export.h -	definitions for the EXPORT kernel extension
 */

/*
 * EXPORT kernel extension configuration parameters
 */
struct k_conf {
	u_int	nsyms;			/* # of symbols			*/
	u_int	symt_sz;		/* size of symbol table		*/
	u_int	str_sz;			/* size of string table		*/
	caddr_t	symtab;			/* user address of symtab	*/
	caddr_t	strtab;			/* user address of string table	*/
};

/*
 * kernel function import
 */
struct k_func {
	void	*(**fpp)();	/* ^ to ^ to function we import	*/
	char	*name;		/* ^ to symbol name		*/
	u_int	fdesc[3];	/* function descriptor storage	*/
};

/*
 * kernel variable import
 */
struct k_var {
	void	*varp;		/* ^ to surrogate variable	*/
	char	*name;		/* ^ to symbol name		*/
};
