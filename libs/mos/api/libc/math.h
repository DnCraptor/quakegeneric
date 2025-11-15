#ifndef _MATH_H
#define _MATH_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M_OS_API_SYS_TABLE_BASE
#define M_OS_API_SYS_TABLE_BASE ((void*)(0x10000000ul + (16 << 20) - (4 << 10)))
static const unsigned long * const _sys_table_ptrs = (const unsigned long * const)M_OS_API_SYS_TABLE_BASE;
#endif

inline static double trunc (double t) {
    typedef double (*fn_ptr_t)(double);
    return ((fn_ptr_t)_sys_table_ptrs[200])(t);
}

inline static double floor (double t) {
    typedef double (*fn_ptr_t)(double);
    return ((fn_ptr_t)_sys_table_ptrs[201])(t);
}

inline static double pow (double x, double y) {
    typedef double (*fn_ptr_t)(double, double);
    return ((fn_ptr_t)_sys_table_ptrs[202])(x, y);
}
inline static float powf(float x, float y) {
    typedef float (*fn)(float, float);
    return ((fn)_sys_table_ptrs[257])(x, y);
}

inline static double sqrt (double x) {
    typedef double (*fn_ptr_t)(double);
    return ((fn_ptr_t)_sys_table_ptrs[203])(x);
}

inline static double sin (double x) {
    typedef double (*fn_ptr_t)(double);
    return ((fn_ptr_t)_sys_table_ptrs[204])(x);
}

inline static double cos (double x) {
    typedef double (*fn_ptr_t)(double);
    return ((fn_ptr_t)_sys_table_ptrs[205])(x);
}

inline static double tan (double x) {
    typedef double (*fn_ptr_t)(double);
    return ((fn_ptr_t)_sys_table_ptrs[206])(x);
}

inline static double atan (double x) {
    typedef double (*fn_ptr_t)(double);
    return ((fn_ptr_t)_sys_table_ptrs[207])(x);
}

inline static double log (double x) {
    typedef double (*fn_ptr_t)(double);
    return ((fn_ptr_t)_sys_table_ptrs[208])(x);
}

inline static double exp (double x) {
    typedef double (*fn_ptr_t)(double);
    return ((fn_ptr_t)_sys_table_ptrs[209])(x);
}

/// TODO: better to use hardware
static unsigned ___srand___;
inline static void srand(unsigned x) {
    ___srand___ = x;
}
static int rand(void) {
	___srand___ = (31421 * ___srand___ + 6927) & 0xffff;
	return ___srand___ / 0x10000 + 1;
}

#ifdef __cplusplus
}
#endif

#endif
