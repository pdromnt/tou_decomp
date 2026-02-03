#ifndef IJL_MOCK_H
#define IJL_MOCK_H

// Constants
#define IJL_JBUFF_READPARAMS 1
#define IJL_JBUFF_READWHOLEIMAGE 3

// Structures
typedef struct {
  unsigned int UseWinRGB;  // 0
  void *DIBBuffer;         // 4
  int DIBWidth;            // 8
  int DIBHeight;           // 12
  int DIBPadBytes;         // 16
  int DIBChannels;         // 20
  int DIBColor;            // 24
  int pad[3];              // 28, 32, 36
  const char *JPGFileName; // 40
  int JPGWidth;            // 44
  int JPGHeight;           // 48
  int JPGChannels;         // 52
  int JPGColor;            // 56
  void *JPGBuffer;         // 60
  int JPGBufferSize;       // 64
  char pad2[1200];
} JPEG_CORE_PROPERTIES;

// Functions
#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllimport) int __stdcall ijlInit(JPEG_CORE_PROPERTIES *jcprops);
__declspec(dllimport) int __stdcall ijlFree(JPEG_CORE_PROPERTIES *jcprops);
__declspec(dllimport) int __stdcall ijlRead(JPEG_CORE_PROPERTIES *jcprops,
                                            int ioType);

#ifdef __cplusplus
}
#endif

#endif // IJL_MOCK_H
