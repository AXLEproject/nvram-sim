
#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>

#define MAX_TXIDS 64

enum {
  TX_FREE = 1,
  TX_STARTED = 2,
  TX_COMMITTING = 4,
  TX_ABORTING = 8,
};

#define W32 int32_t
#define U32 uint32_t
#define W64 int64_t
#define U64 uint64_t
#define Addr uint64_t

#define KB 1024LL
#define MB 1024LL*KB
#define GB 1024LL*MB
#define TB 1024LL*GB
#define K 1000LL
#define M 1000LL*K
#define G 1000LL*M

#define IS_WRITEBACK_CACHE true

const Addr addr_space = 64*GB;
const size_t mem_access_cost = 0;

const size_t L3_direct_entries = 16 * 1024;
//const size_t L3_associativity = 64;
const size_t L3_associativity = 64;
const size_t L3_line_size_bytes = 64;
const size_t L3_hit_cost_ticks = 100;

// IMPORTANT! 
// all private cache levels need to have the same line size!!!
//

// 1024x8 = 512 KB
const size_t L2_direct_entries = 2*1024;
//const size_t L2_associativity = 32;
const size_t L2_associativity = 8;
const size_t L2_line_size_bytes = 64;
const size_t L2_hit_cost_ticks = 10;

// 128x4 = 32 KB
// 256x4 = 64 KB
const size_t L1_direct_entries = 128;
//const size_t L1_associativity = 32;
const size_t L1_associativity = 4;
const size_t L1_line_size_bytes = 64;
const size_t L1_hit_cost_ticks = 2;

const size_t L1i_direct_entries = 128;
const size_t L1i_associativity = 4;
const size_t L1i_line_size_bytes = 64;
const size_t L1i_hit_cost_ticks = 1;

const size_t memdir_access_cycles = L3_hit_cost_ticks + L2_hit_cost_ticks + L1_hit_cost_ticks;

typedef bool Fault;
const bool NoFault=false;
const unsigned PHYSICAL         = 0x00200;
#define RETRY true
//bool rw_array_silent(uint64_t addr, uint32_t len, uint8_t *data, const bool is_write);

#include <string>
inline std::string nspaces(size_t number) {
  std::string s;
  s.resize(number, ' ');
  return s;
}

extern char *strbuf;
void nvlog(char *data, int size);
#ifdef DEBUG
	#ifndef TRACING_ON
	#define TRACING_ON 0
	#endif
	#define NVLOG(...) { int size = sprintf(strbuf, __VA_ARGS__); nvlog(strbuf, size); }
	#define NVLOG1(...) { int size = sprintf(strbuf, __VA_ARGS__); nvlog(strbuf, size); }
	//#define NVLOG1(...) /* nothing */
	#define NVLOG_ERROR(...) { int size = sprintf(strbuf, "*** ERROR: "  __VA_ARGS__); nvlog(strbuf, size); }
#else
	#define NVLOG(...) /* nothing */
	#define NVLOG1(...) /* nothing */
	#define NVLOG_ERROR(...) { sprintf(strbuf, "*** ERROR: "  __VA_ARGS__); fprintf(stderr, "%s\n", strbuf); }
#endif

template <int n> struct lg { static const int value = 1 + lg<n/2>::value; };
template <> struct lg<1> { static const int value = 0; };
#define log2compiletime(v) (lg<(v)>::value)

template <int n> struct lg10 { static const int value = 1 + lg10<n/10>::value; };
template <> struct lg10<1> { static const int value = 0; };
template <> struct lg10<0> { static const int value = 0; };
#define log10compiletime(v) (lg10<(v)>::value)


static inline unsigned int
ones32(register unsigned int x)
{
  /* 32-bit recursive reduction using SWAR...
	   but first step is mapping 2-bit values
	   into sum of 2 1-bit values in sneaky way
	*/
        x -= ((x >> 1) & 0x55555555);
        x = (((x >> 2) & 0x33333333) + (x & 0x33333333));
        x = (((x >> 4) + x) & 0x0f0f0f0f);
        x += (x >> 8);
        x += (x >> 16);
        return(x & 0x0000003f);
}
static inline unsigned int
ones64(register U64 x)
{
  // Algorithm : 64-bit recursive reduction using SWAR
  const U64 MaskMult = 0x0101010101010101; 
  const U64 mask1h = (~0UL) / 3 << 1;
  const U64 mask2l = (~0UL) / 5;
  const U64 mask4l = (~0UL) / 17;
  x -= (mask1h & x) >> 1;
  x = (x & mask2l) + ((x >> 2) & mask2l);
  x += x >> 4; 
  x &= mask4l; 
  // BitMask += BitMask >> 8; 
  // BitMask += BitMask >> 16; 
  // BitMask += BitMask >> 32; 
  // return (ushort)(BitMask & 0xff); 
  // Replacement for the >> 8 and >> 16 AND return (BitMask & 0xff)
  return (unsigned int)((x * MaskMult) >> 56);
}

// if LOG0UNDEFINED, this code returns -1 for log2(0);
// otherwise, it returns 0 for log2(0).
/*
// For a 32-bit value:
unsigned int
log2floor(register unsigned int x)
{
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >> 16);
#ifdef	LOG0UNDEFINED
  return(ones32(x) - 1);
#else
  return(ones32(x >> 1));
#endif
}
*/
// For a 64-bit value:
static inline U64
log2floor(register U64 x)
{
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >> 16);
  x |= (x >> 32);
#ifdef	LOG0UNDEFINED
  return(ones64(x) - 1);
#else
  return(ones64(x >> 1));
#endif
}

#if defined __GNUC__
static inline U64
log2power2(U64 v)
{// calling a GCC extension, which might use CPU assembly instruction for calculation
  return __builtin_ctzl(v);
}
#else
static const W64 log2power2mask[] = {
  0xAAAAAAAAAAAAAAAA,
  0xCCCCCCCCCCCCCCCC,
  0xF0F0F0F0F0F0F0F0,
  0xFF00FF00FF00FF00,
  0xFFFF0000FFFF0000,
  0xFFFFFFFF00000000
};
static inline U64
log2power2(register U64 v)
{
  register unsigned int r = (v & log2power2mask[0]) != 0;
  for (int i = 5; i > 0; i--) // unroll for speed...
  {
    r |= ((v & log2power2mask[i]) != 0) << i;
  }
  return r;
}
#endif

#define sqr(x) ((x)*(x))
#define cube(x) ((x)*(x)*(x))
#define bit(x, n) (((x) >> (n)) & 1)

#define bitmask(l) (((l) == 64) ? (U64)(-1LL) : ((1LL << (l))-1LL))
//#define bits(x, i, l) (((x) >> (i)) & bitmask(l))
#define lowbits(x, l) bits(x, 0, l)
#define setbit(x,i) ((x) |= (1LL << (i)))
#define clearbit(x, i) ((x) &= (U64)(~(1LL << (i))))
#define assignbit(x, i, v) ((x) = (((x) &= (U64)(~(1LL << (i)))) | (((U64)((bool)(v))) << i)));

/*
// This works, as well as the other version (down), that is currently used...
// code:
static const int MultiplyDeBruijnBitPosition[32] = 
{
  // precomputed lookup table
   0,  1, 28,  2, 29, 14, 24,  3, 30, 22, 20, 15, 25, 17,  4,  8, 
  31, 27, 13, 23, 21, 19, 16,  7, 26, 12, 18,  6, 11,  5, 10,  9
};
static inline int lowestBitSet32(register unsigned int x)
{
  if (x==0) return -1;
  // leave only lowest bit
  x  &= -(int)x;
  // DeBruijn constant
  x  *= 0x077CB531;
  // get upper 5 bits
  x >>= 27;
  // convert to actual position
  return MultiplyDeBruijnBitPosition[x]; 
}

static inline int lowestBitSet64(register U64 x)
{
  if (x==0) return -1;
  register int res = 0;
  res = lowestBitSet32((unsigned int)x);
  x >>= 31;
  if (x==0) return res;
  res = 31+lowestBitSet32((unsigned int)(x));
  return res;
}
*/

static inline int lowestBitSet32(register U32 x)
{
  if (x==0) return -1;
  x &= ~(x-1);
  return (((x & 0xAAAAAAAAUL) != 0)
    | ((x & 0xCCCCCCCCUL) != 0) << 1
    | ((x & 0xF0F0F0F0UL) != 0) << 2
    | ((x & 0xFF00FF00UL) != 0) << 3
    | ((x & 0xFFFF0000UL) != 0) << 4);
}

static inline int lowestBitSet64(register U64 x)
{
  if (x==0) return -1;
  x &= ~(x-1);
  return (((x & 0xAAAAAAAAAAAAAAAAULL) != 0)
  | ((x & 0xCCCCCCCCCCCCCCCCULL) != 0) << 1
  | ((x & 0xF0F0F0F0F0F0F0F0ULL) != 0) << 2
  | ((x & 0xFF00FF00FF00FF00ULL) != 0) << 3
  | ((x & 0xFFFF0000FFFF0000ULL) != 0) << 4
  | ((x & 0xFFFFFFFF00000000ULL) != 0) << 5 );
}

template <typename T, typename A> static inline T floor(T x, A a) { return (T)(((T)x) & ~((T)(a-1))); }
template <typename T, typename A> static inline T trunc(T x, A a) { return (T)(((T)x) & ~((T)(a-1))); }
template <typename T, typename A> static inline T ceil(T x, A a) { return (T)((((T)x) + ((T)(a-1))) & ~((T)(a-1))); }
template <typename T, typename A> static inline T mask_bits(T x, A a) { return (T)(((T)x) & ((T)(a-1))); }

#define is_power_of_2(_x) ((_x&(_x-1))==0)

#define MIN2(_x,_y) ((_x<_y)?_x:_y)
#define MAX2(_x,_y) ((_x>_y)?_x:_y)

#endif //__GLOBALS_H__
