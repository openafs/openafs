.inline MEMBAR_ALL, 0
	membar #StoreStore | #LoadLoad | #LoadStore | #StoreLoad
.end

.inline MEMBAR_STORESTORE, 0
	membar #StoreStore
.end

.inline MEMBAR_LOADLOAD, 0
	membar #LoadLoad
.end

.inline CASPO_internal
	casx [%o0], %o1, %o2
	mov %o2, %o0
.end

.inline CAS64O_internal
	casx [%o0], %o1, %o2
	mov %o2, %o0
.end

.inline CASIO_internal
	cas [%o0], %o1, %o2
	mov %o2, %o0
.end

.inline RDTICK
	rd %tick, %o0
.end
