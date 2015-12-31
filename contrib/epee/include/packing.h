#if defined(__clang__) || defined(__GNUC__)

#define PACK( __declaration__ ) __declaration__ __attribute__((__packed__ ));

#else

#define PACK( __declaration__ ) __pragma(pack(push, 1)) __declaration__; __pragma(pack(pop))

#endif
