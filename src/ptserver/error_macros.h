#define PT_EXIT(evalue)                                 \
        {                                               \
           osi_audit (PTS_ExitEvent, evalue, AUD_END); \
	   exit(evalue);                                \
	} 
				  
				   
