#include "ed25519-donna-portable-identify.h"

/* ticks - not tested on anything other than x86 */
static uint64_t
get_ticks(void) {
#if defined(cpu_x86) || defined(cpu_x86_64)
	#if defined(compiler_intel)
		return _rdtsc();
	#elif defined(compiler_msvc)
		return __rdtsc();
	#elif defined(compiler_gcc)
		uint32_t lo, hi;
		__asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi));
		return ((uint64_t)lo | ((uint64_t)hi << 32));
	#else
		need rdtsc for this compiler
	#endif
#elif defined(os_solaris)
	return (uint64_t)gethrtime();
#elif defined(cpu_sparc) && !defined(os_openbsd)
	uint64_t t;
	__asm__ __volatile__("rd %%tick, %0" : "=r" (t));
	return t;
#elif defined(cpu_ppc)
	uint32_t lo = 0, hi = 0;
	__asm__ __volatile__("mftbu %0; mftb %1" : "=r" (hi), "=r" (lo));
	return ((uint64_t)lo | ((uint64_t)hi << 32));
#elif defined(cpu_ia64)
	uint64_t t;
	__asm__ __volatile__("mov %0=ar.itc" : "=r" (t));
	return t;
#elif defined(os_nix)
	timeval t2;
	gettimeofday(&t2, null);
	t = ((uint64_t)t2.tv_usec << 32) | (uint64_t)t2.tv_sec;
	return t;
#else
	need ticks for this platform
#endif
}

#define timeit(x,minvar)         \
	ticks = get_ticks();         \
 	x;                           \
	ticks = get_ticks() - ticks; \
	if (ticks < minvar)          \
		minvar = ticks;

#define maxticks 0xffffffffffffffffull

