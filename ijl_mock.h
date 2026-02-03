#ifndef IJL_MOCK_H
#define IJL_MOCK_H

// Constants
#define IJL_JBUFF_READPARAMS 1
#define IJL_JBUFF_READWHOLEIMAGE 3

// Structures
typedef struct {
  unsigned int UseExternal; // 0
  // ... padding/unknown ...
  unsigned int JPGBytes;     // 0x18
  unsigned int JPGSizeBytes; // ?
  // ...
  unsigned int JPGWidth;    // 0x2C
  unsigned int JPGHeight;   // 0x30
  unsigned int JPGChannels; // 0x34
  // ...
  unsigned char *JPGColor;
  // ...
  unsigned char *DIBBytes;
  unsigned int DIBWidth;
  unsigned int DIBHeight;
  unsigned int DIBChannels;
  unsigned int DIBPadBytes; // ?
                            // ...
} JPEG_CORE_PROPERTIES;

// Functions
#ifdef __cplusplus
extern "C" {
#endif

int __stdcall ijlInit(JPEG_CORE_PROPERTIES *jcprops);
int __stdcall ijlFree(JPEG_CORE_PROPERTIES *jcprops);
int __stdcall ijlRead(JPEG_CORE_PROPERTIES *jcprops, int ioType);

#ifdef __cplusplus
}
#endif

#endif // IJL_MOCK_H
