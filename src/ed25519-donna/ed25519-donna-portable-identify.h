/* os */
#if defined(_win32)	|| defined(_win64) || defined(__tos_win__) || defined(__windows__)
	#define os_windows
#elif defined(sun) || defined(__sun) || defined(__svr4) || defined(__svr4__)
	#define os_solaris
#else
	#include <sys/param.h> /* need this to define bsd */
	#define os_nix
	#if defined(__linux__)
		#define os_linux
	#elif defined(bsd)
		#define os_bsd
		#if defined(macos_x) || (defined(__apple__) & defined(__mach__))
			#define os_osx
		#elif defined(macintosh) || defined(macintosh)
			#define os_mac
		#elif defined(__openbsd__)
			#define os_openbsd
		#endif
	#endif
#endif


/* compiler */
#if defined(_msc_ver)
	#define compiler_msvc
#endif
#if defined(__icc)
	#define compiler_intel
#endif
#if defined(__gnuc__)
	#if (__gnuc__ >= 3)
		#define compiler_gcc ((__gnuc__ * 10000) + (__gnuc_minor__ * 100) + (__gnuc_patchlevel__))
	#else
		#define compiler_gcc ((__gnuc__ * 10000) + (__gnuc_minor__ * 100)                        )
	#endif
#endif
#if defined(__pathcc__)
	#define compiler_pathcc
#endif
#if defined(__clang__)
	#define compiler_clang ((__clang_major__ * 10000) + (__clang_minor__ * 100) + (__clang_patchlevel__))
#endif



/* cpu */
#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__ ) || defined(_m_x64)
	#define cpu_x86_64
#elif defined(__i586__) || defined(__i686__) || (defined(_m_ix86) && (_m_ix86 >= 500))
	#define cpu_x86 500
#elif defined(__i486__) || (defined(_m_ix86) && (_m_ix86 >= 400))
	#define cpu_x86 400
#elif defined(__i386__) || (defined(_m_ix86) && (_m_ix86 >= 300)) || defined(__x86__) || defined(_x86_) || defined(__i86__)
	#define cpu_x86 300
#elif defined(__ia64__) || defined(_ia64) || defined(__ia64__) || defined(_m_ia64) || defined(__ia64)
	#define cpu_ia64
#endif

#if defined(__sparc__) || defined(__sparc) || defined(__sparcv9)
	#define cpu_sparc
	#if defined(__sparcv9)
		#define cpu_sparc64
	#endif
#endif

#if defined(powerpc) || defined(__ppc__) || defined(__ppc__) || defined(_arch_ppc) || defined(__powerpc__) || defined(__powerpc) || defined(powerpc) || defined(_m_ppc)
	#define cpu_ppc
	#if defined(_arch_pwr7)
		#define cpu_power7
	#elif defined(__64bit__)
		#define cpu_ppc64
	#else
		#define cpu_ppc32
	#endif
#endif

#if defined(__hppa__) || defined(__hppa)
	#define cpu_hppa
#endif

#if defined(__alpha__) || defined(__alpha) || defined(_m_alpha)
	#define cpu_alpha
#endif

/* 64 bit cpu */
#if defined(cpu_x86_64) || defined(cpu_ia64) || defined(cpu_sparc64) || defined(__64bit__) || defined(__lp64__) || defined(_lp64) || (defined(_mips_szlong) && (_mips_szlong == 64))
	#define cpu_64bits
#endif

#if defined(compiler_msvc)
	typedef signed char int8_t;
	typedef unsigned char uint8_t;
	typedef signed short int16_t;
	typedef unsigned short uint16_t;
	typedef signed int int32_t;
	typedef unsigned int uint32_t;
	typedef signed __int64 int64_t;
	typedef unsigned __int64 uint64_t;
#else
	#include <stdint.h>
#endif

