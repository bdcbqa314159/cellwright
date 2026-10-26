#define STB_IMAGE_WRITE_IMPLEMENTATION
// stb_image_write.h is vendored third-party code: silence its diagnostics here
// rather than dropping -Wall/-Wextra for our own sources (OPEN.md #11).
#if defined(__clang__) || defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#  pragma warning(push, 0)
#endif
#include "util/stb_image_write.h"
#if defined(__clang__) || defined(__GNUC__)
#  pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#  pragma warning(pop)
#endif
