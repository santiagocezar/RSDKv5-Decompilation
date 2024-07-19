#include <gsKit.h>
#include <dmaKit.h>

using ShaderEntry = ShaderEntryBase;

class RenderDevice : public RenderDeviceBase
{
public:
    struct WindowInfo {
        typedef struct {
            uint32 width;
            uint32 height;
            uint32 refresh_rate;
        } BasicWindowInfo;
        union {
            struct {
                uint32 width;
                uint32 height;
                uint32 refresh_rate;
            };
            BasicWindowInfo internal;
        } * displays;
    };

    static WindowInfo displayInfo;

    static bool Init();
    static void CopyFrameBuffer();
    static void FlipScreen();
    static void Release(bool32 isRefresh);

    static void RefreshWindow();
    static void GetWindowSize(int32 *width, int32 *height);

    static void SetupImageTexture(int32 width, int32 height, uint8 *imagePixels);
    static void SetupVideoTexture_YUV420(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                         int32 strideV);
    static void SetupVideoTexture_YUV422(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                         int32 strideV);
    static void SetupVideoTexture_YUV444(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                         int32 strideV);

    static bool ProcessEvents();

    static void InitFPSCap();
    static bool CheckFPSCap();
    static void UpdateFPSCap();

    static bool InitShaders();
    static void LoadShader(const char *fileName, bool32 linear);

    static inline void ShowCursor(bool32 shown) { return; }
    static inline bool GetCursorPos(Vector2 *pos)
    {
        return false;
    };

    static inline void SetWindowTitle() { return; };

    static GSGLOBAL *gsGlobal;
    static GSTEXTURE screenTexture[SCREEN_COUNT];

private:
    static bool SetupRendering();
    static void InitVertexBuffer();
    static bool InitGraphicsAPI();

    static void GetDisplays();

    static void DrawScreenGeometry(GSTEXTURE* texture, int32 startVert, bool video);
    // static void ProcessEvent(SDL_Event event);

    // static uint32 displayModeIndex;
    // static int32 displayModeCount;

    static unsigned long long targetFreq;
    static unsigned long long curTicks;
    static unsigned long long prevTicks;

    static RenderVertex vertexBuffer[!RETRO_REV02 ? 24 : 60];

    // thingo majigo for handling video/image swapping
    // static uint8 lastTextureFormat;
};
