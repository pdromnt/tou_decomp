// stb_image_impl.c - STB Image implementation file
// This file must be compiled separately to avoid including the implementation
// in multiple TUs

#define STB_IMAGE_IMPLEMENTATION
// #define STBI_ONLY_JPEG // We only need JPEG support -> Disabled for TGA Fonts
// #define STBI_NO_STDIO   // We use standard IO
#include "stb_image.h"
