#include <malloc.h>

GSGLOBAL *RenderDevice::gsGlobal = nullptr;
GSTEXTURE RenderDevice::screenTexture[SCREEN_COUNT];

// SDL_Texture *RenderDevice::imageTexture = nullptr;

// uint32 RenderDevice::displayModeIndex = 0;
// int32 RenderDevice::displayModeCount  = 0;

unsigned long long RenderDevice::targetFreq = 0;
unsigned long long RenderDevice::curTicks   = 0;
unsigned long long RenderDevice::prevTicks  = 0;

RenderVertex RenderDevice::vertexBuffer[!RETRO_REV02 ? 24 : 60];

// uint8 RenderDevice::lastTextureFormat = -1;

#define NORMALIZE(val, minVal, maxVal) ((float)(val) - (float)(minVal)) / ((float)(maxVal) - (float)(minVal))

bool RenderDevice::Init()
{

    gsGlobal = gsKit_init_global();


    gsGlobal->Mode            = GS_MODE_NTSC;
    // gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
    gsGlobal->PSM             = GS_PSM_CT24;
    gsGlobal->ZBuffering = GS_SETTING_OFF;
    gsGlobal->DoubleBuffering = GS_SETTING_OFF;
    // gsGlobal->Interlace = GS_NONINTERLACED;
    // gsGlobal->Field = GS_FRAME;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_vram_clear(gsGlobal);

    gsKit_init_screen(gsGlobal);

    gsKit_mode_switch(gsGlobal, GS_ONESHOT);

    videoSettings.windowed     = false;
    videoSettings.windowWidth  = gsGlobal->Width;
    videoSettings.windowHeight = gsGlobal->Height;

    PrintLog(PRINT_NORMAL, "w: %d h: %d windowed: %d", videoSettings.windowWidth, videoSettings.windowHeight, videoSettings.windowed);

    if (!SetupRendering() || !AudioDevice::Init())
        return false;

    InitInputDevices();
    return true;
}

void RenderDevice::CopyFrameBuffer()
{
    for (int32 s = 0; s < videoSettings.screenCount; ++s) {
        uint16 *pixels = (uint16*)screenTexture[s].Mem;
        uint16 *frameBuffer = screens[s].frameBuffer;
        for (int32 y = 0; y < SCREEN_YSIZE; ++y) {
            memcpy(pixels, frameBuffer, screens[s].size.x * sizeof(uint16));
            frameBuffer += screens[s].pitch;
            pixels += screenTexture[s].Width;
        }

        gsKit_texture_upload(gsGlobal, &screenTexture[s]);
    }
}

void RenderDevice::DrawScreenGeometry(GSTEXTURE* texture, int32 startVert, bool video = false) {
    float x1 = vertexBuffer[startVert].pos.x,
          y1 = vertexBuffer[startVert].pos.y,
          x2 = vertexBuffer[startVert + 2].pos.x,
          y2 = vertexBuffer[startVert + 2].pos.y,
          u1 = vertexBuffer[startVert].tex.x * textureSize.x,
          v1 = vertexBuffer[startVert].tex.y * textureSize.y,
          u2 = vertexBuffer[startVert + 2].tex.x * textureSize.x,
          v2 = vertexBuffer[startVert + 2].tex.y * textureSize.y;

    if (video){
        u2 = vertexBuffer[startVert + 2].tex.x * 1024;
        v2 = vertexBuffer[startVert + 2].tex.y * 512;
    }

    gsKit_prim_sprite_texture(gsGlobal, texture,
                              x1, y1, u1, v1,
                              x2, y2, u2, v2,
                              0, GS_SETREG_RGBA(0xff, 0xff, 0xff, 0xff));
}

void RenderDevice::FlipScreen()
{
    if (windowRefreshDelay > 0) {
        windowRefreshDelay--;
        if (!windowRefreshDelay)
            UpdateGameWindow();
        return;
    }

    float dimAmount = videoSettings.dimMax * videoSettings.dimPercent;

    // Clear the screen. This is needed to keep the
    // pillarboxes in fullscreen from displaying garbage data.
    gsKit_clear(gsGlobal, 0);

    int32 startVert = 0;

    switch (videoSettings.screenCount) {
        default:
        case 0:
            #if RETRO_REV02
            // DrawScreenGeometry(&imageTexture, 54, true);
            #else
            // DrawScreenGeometry(&imageTexture, 18, true);
            #endif
            break;

        case 1:
            DrawScreenGeometry(&screenTexture[0], 0);
            break;

        case 2:
            #if RETRO_REV02
            DrawScreenGeometry(&screenTexture[0], startVertex_2P[0]);
            DrawScreenGeometry(&screenTexture[1], startVertex_2P[1]);
            #else
            DrawScreenGeometry(&screenTexture[0], 6);
            DrawScreenGeometry(&screenTexture[1], 12);
            #endif

            #if RETRO_REV02
        case 3:
            DrawScreenGeometry(&screenTexture[0], startVertex_3P[0]);
            DrawScreenGeometry(&screenTexture[1], startVertex_3P[1]);
            DrawScreenGeometry(&screenTexture[2], startVertex_3P[2]);
            break;

        case 4:
            DrawScreenGeometry(&screenTexture[0], 30);
            DrawScreenGeometry(&screenTexture[1], 36);
            DrawScreenGeometry(&screenTexture[2], 42);
            DrawScreenGeometry(&screenTexture[3], 48);
            break;
            #endif
    }
    // if (dimAmount < 1.0f) {
    //     SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF - (dimAmount * 0xFF));
    //     SDL_RenderFillRect(renderer, NULL);
    // }
    // SDL_RenderPresent(renderer);

    gsKit_queue_exec(gsGlobal);
    gsKit_sync_flip(gsGlobal);
}

void RenderDevice::Release(bool32 isRefresh)
{
    for (int32 s = 0; s < SCREEN_COUNT; ++s) {
        free(screenTexture[s].Mem);
        screenTexture[s].Vram = 0;
    }

    // free(imageTexture);
    // imageTexture.Vram = 0;

    if (!isRefresh) {
        if (displayInfo.displays)
            free(displayInfo.displays);
        displayInfo.displays = NULL;
    }

    if (!isRefresh && gsGlobal) {
        gsKit_deinit_global(gsGlobal);
        gsGlobal = NULL;
    }

    if (!isRefresh) {
        if (scanlines)
            free(scanlines);
        scanlines = NULL;
    }
}

void RenderDevice::RefreshWindow()
{
#if 0
    videoSettings.windowState = WINDOWSTATE_UNINITIALIZED;

    Release(true);

    SDL_HideWindow(window);

    if (videoSettings.windowed && videoSettings.bordered)
        SDL_SetWindowBordered(window, SDL_TRUE);
    else
        SDL_SetWindowBordered(window, SDL_FALSE);

    GetDisplays();

    SDL_Rect winRect;
    winRect.x = SDL_WINDOWPOS_CENTERED;
    winRect.y = SDL_WINDOWPOS_CENTERED;
    if (videoSettings.windowed || !videoSettings.exclusiveFS) {
        int32 currentWindowDisplay = SDL_GetWindowDisplayIndex(window);
        SDL_DisplayMode displayMode;
        SDL_GetCurrentDisplayMode(currentWindowDisplay, &displayMode);

        if (videoSettings.windowed) {
            if (videoSettings.windowWidth >= displayMode.w || videoSettings.windowHeight >= displayMode.h) {
                videoSettings.windowWidth  = (displayMode.h / 480 * videoSettings.pixWidth);
                videoSettings.windowHeight = displayMode.h / 480 * videoSettings.pixHeight;
            }

            winRect.w = videoSettings.windowWidth;
            winRect.h = videoSettings.windowHeight;
            SDL_SetWindowFullscreen(window, SDL_FALSE);
            SDL_ShowCursor(SDL_FALSE);
        }
        else {
            winRect.w = displayMode.w;
            winRect.h = displayMode.h;
            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            SDL_ShowCursor(SDL_TRUE);
        }

        SDL_SetWindowSize(window, winRect.w, winRect.h);
        SDL_SetWindowPosition(window, winRect.x, winRect.y);
    }

    SDL_ShowWindow(window);

    if (!InitGraphicsAPI() || !InitShaders())
        return;

    videoSettings.windowState = WINDOWSTATE_ACTIVE;
#endif
}

void RenderDevice::InitFPSCap()
{
    // targetFreq = SDL_GetPerformanceFrequency() / videoSettings.refreshRate;
    // curTicks   = 0;
    // prevTicks  = 0;
}
bool RenderDevice::CheckFPSCap()
{
    // curTicks = SDL_GetPerformanceCounter();
    // if (curTicks >= prevTicks + targetFreq)
    //
    // return false;
    return true;
}
void RenderDevice::UpdateFPSCap() { prevTicks = curTicks; }

void RenderDevice::InitVertexBuffer()
{
    RenderVertex vertBuffer[sizeof(rsdkVertexBuffer) / sizeof(RenderVertex)];
    memcpy(vertBuffer, rsdkVertexBuffer, sizeof(rsdkVertexBuffer));

    // ignore the last 6 verts, they're scaled to the 1024x512 textures already!
    int32 vertCount = (RETRO_REV02 ? 60 : 24) - 6;

    // Regular in-game screen de-normalization stuff
    for (int32 v = 0; v < vertCount; ++v) {
        RenderVertex *vertex = &vertBuffer[v];
        vertex->pos.x        = NORMALIZE(vertex->pos.x, -1.0, 1.0) * videoSettings.pixWidth;
        vertex->pos.y        = (1.0 - NORMALIZE(vertex->pos.y, -1.0, 1.0)) * SCREEN_YSIZE;

        if (vertex->tex.x)
            vertex->tex.x = screens[0].size.x * (1.0 / textureSize.x);

        if (vertex->tex.y)
            vertex->tex.y = screens[0].size.y * (1.0 / textureSize.y);
    }

    // Fullscreen Image/Video de-normalization stuff
    for (int32 v = 0; v < 6; ++v) {
        RenderVertex *vertex = &vertBuffer[vertCount + v];
        vertex->pos.x        = NORMALIZE(vertex->pos.x, -1.0, 1.0) * videoSettings.pixWidth;
        vertex->pos.y        = (1.0 - NORMALIZE(vertex->pos.y, -1.0, 1.0)) * SCREEN_YSIZE;

        // Set the texture to fill the entire screen with all 1024x512 pixels
        if (vertex->tex.x)
            vertex->tex.x = 1.0f;

        if (vertex->tex.y)
            vertex->tex.y = 1.0f;
    }

    memcpy(vertexBuffer, vertBuffer, sizeof(vertBuffer));
}

bool RenderDevice::InitGraphicsAPI()
{
    videoSettings.shaderSupport = false;

    viewSize.x = 0;
    viewSize.y = 0;

    if (videoSettings.windowed || !videoSettings.exclusiveFS) {
        if (videoSettings.windowed) {
            viewSize.x = videoSettings.windowWidth;
            viewSize.y = videoSettings.windowHeight;
        }
        else {
            viewSize.x = displayWidth[0];
            viewSize.y = displayHeight[0];
        }
    }
    else {
        int32 bufferWidth  = videoSettings.fsWidth;
        int32 bufferHeight = videoSettings.fsHeight;
        if (videoSettings.fsWidth <= 0 || videoSettings.fsHeight <= 0) {
            bufferWidth  = displayWidth[0];
            bufferHeight = displayHeight[0];
        }

        viewSize.x = bufferWidth;
        viewSize.y = bufferHeight;
    }

    int32 maxPixHeight = 0;
#if !RETRO_USE_ORIGINAL_CODE
    int32 screenWidth = 0;
#endif
    for (int32 s = 0; s < 4; ++s) {
        if (videoSettings.pixHeight > maxPixHeight)
            maxPixHeight = videoSettings.pixHeight;

        screens[s].size.y = videoSettings.pixHeight;

        float viewAspect = viewSize.x / viewSize.y;
#if !RETRO_USE_ORIGINAL_CODE
        screenWidth = (int32)((viewAspect * videoSettings.pixHeight) + 3) & 0xFFFFFFFC;
#else
        int32 screenWidth = (int32)((viewAspect * videoSettings.pixHeight) + 3) & 0xFFFFFFFC;
#endif
        if (screenWidth < videoSettings.pixWidth)
            screenWidth = videoSettings.pixWidth;

#if !RETRO_USE_ORIGINAL_CODE
        if (customSettings.maxPixWidth && screenWidth > customSettings.maxPixWidth)
            screenWidth = customSettings.maxPixWidth;
#else
        if (screenWidth > DEFAULT_PIXWIDTH)
            screenWidth = DEFAULT_PIXWIDTH;
#endif

        memset(&screens[s].frameBuffer, 0, sizeof(screens[s].frameBuffer));
        SetScreenSize(s, screenWidth, screens[s].size.y);
    }

    pixelSize.x = screens[0].size.x;
    pixelSize.y = screens[0].size.y;

    // SDL_RenderSetLogicalSize(renderer, videoSettings.pixWidth, SCREEN_YSIZE);
    // SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

#if !RETRO_USE_ORIGINAL_CODE
    if (screenWidth <= 512 && maxPixHeight <= 256) {
#else
    if (maxPixHeight <= 256) {
#endif
        textureSize.x = 512.0;
        textureSize.y = 256.0;
    }
    else {
        textureSize.x = 1024.0;
        textureSize.y = 512.0;
    }
    // SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    // ok so, the PS2 has 4MiB of VRAM. if textureSize is 1024x512 each screen (thats four) uses 1MiB... idk what the code above is supposed to do but hopefully that doesn't happpen


    for (int32 s = 0; s < SCREEN_COUNT; ++s) {
        printf("allocating texture %i with %fx%f in size\n", s, textureSize.x, textureSize.y);
        printf("current pointer: %#08x\n", gsGlobal->CurrentPointer);
        screenTexture[s].Width  = textureSize.x;
        screenTexture[s].Height = textureSize.y;
        screenTexture[s].PSM    = GS_PSM_CT16;
        screenTexture[s].Filter = GS_FILTER_NEAREST;
        screenTexture[s].Mem    = (uint32*)memalign(128,
                                           gsKit_texture_size_ee(textureSize.x, textureSize.y, screenTexture[s].PSM));

        screenTexture[s].Vram   = gsKit_vram_alloc(gsGlobal,
                                                   gsKit_texture_size_ee(textureSize.x, textureSize.y, screenTexture[s].PSM),
                                                   GSKIT_ALLOC_USERBUFFER);

        if(screenTexture[s].Vram == GSKIT_ALLOC_ERROR)
        {
            PrintLog(PRINT_NORMAL, "ERROR: VRAM Allocation Failed.", SDL_GetError());
            return false;
        }
    }

    // TODO: video
    // SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    // imageTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RETRO_VIDEO_TEXTURE_W, RETRO_VIDEO_TEXTURE_H);
    // if (!imageTexture)
    //     return false;
    // SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    lastShaderID = -1;
    InitVertexBuffer();
    engine.inFocus          = 1;
    videoSettings.viewportX = 0;
    videoSettings.viewportY = 0;
    videoSettings.viewportW = 1.0 / viewSize.x;
    videoSettings.viewportH = 1.0 / viewSize.y;

    return true;
}

void RenderDevice::LoadShader(const char *fileName, bool32 linear) { PrintLog(PRINT_NORMAL, "This render device does not support shaders!"); }

bool RenderDevice::InitShaders()
{
    int32 maxShaders = 0;
#if RETRO_USE_MOD_LOADER
    // who knows maybe SDL3 will have shaders
    shaderCount = 0;
#endif

    if (videoSettings.shaderSupport) {
        LoadShader("None", false);
        LoadShader("Clean", true);
        LoadShader("CRT-Yeetron", true);
        LoadShader("CRT-Yee64", true);

#if RETRO_USE_MOD_LOADER
        // a place for mods to load custom shaders
        RunModCallbacks(MODCB_ONSHADERLOAD, NULL);
        userShaderCount = shaderCount;
#endif

        LoadShader("YUV-420", true);
        LoadShader("YUV-422", true);
        LoadShader("YUV-444", true);
        LoadShader("RGB-Image", true);
        maxShaders = shaderCount;
    }
    else {
        for (int32 s = 0; s < SHADER_COUNT; ++s) shaderList[s].linear = true;

        shaderList[0].linear = videoSettings.windowed ? false : shaderList[0].linear;
        maxShaders           = 1;
        shaderCount          = 1;
    }

    videoSettings.shaderID = videoSettings.shaderID >= maxShaders ? 0 : videoSettings.shaderID;

    return true;
}

bool RenderDevice::SetupRendering()
{
    GetDisplays();

    if (!InitGraphicsAPI() || !InitShaders())
        return false;

    int32 size = videoSettings.pixWidth >= SCREEN_YSIZE ? videoSettings.pixWidth : SCREEN_YSIZE;
    scanlines  = (ScanlineInfo *)malloc(size * sizeof(ScanlineInfo));
    memset(scanlines, 0, size * sizeof(ScanlineInfo));

    videoSettings.windowState = WINDOWSTATE_ACTIVE;
    videoSettings.dimMax      = 1.0;
    videoSettings.dimPercent  = 1.0;

    return true;
}

void RenderDevice::GetDisplays()
{
    displayWidth[0] = gsGlobal->Width;
    displayHeight[0] = gsGlobal->Height;
    displayCount = 1;

    displayInfo.displays                 = (decltype(displayInfo.displays))malloc(sizeof(displayInfo.displays->internal));
    displayInfo.displays[0].width        = displayWidth[0];
    displayInfo.displays[0].height       = displayHeight[0];
    displayInfo.displays[0].refresh_rate = videoSettings.refreshRate;
}

void RenderDevice::GetWindowSize(int32 *width, int32 *height)
{
    if (width)
        *width = gsGlobal->Width;

    if (height)
        *height = gsGlobal->Height;
}

#if 0
void RenderDevice::ProcessEvent(SDL_Event event)
{
    switch (event.type) {
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
                case SDL_WINDOWEVENT_MAXIMIZED: {
                    SDL_RestoreWindow(window);
                    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    SDL_ShowCursor(SDL_FALSE);
                    videoSettings.windowed = false;
                    break;
                }

                case SDL_WINDOWEVENT_CLOSE: isRunning = false; break;

                case SDL_WINDOWEVENT_FOCUS_GAINED:
#if RETRO_REV02
                    SKU::userCore->focusState = 0;
#endif
                    break;

                case SDL_WINDOWEVENT_FOCUS_LOST:
#if RETRO_REV02
                    SKU::userCore->focusState = 1;
#endif
                    break;
            }
            break;

        case SDL_CONTROLLERDEVICEADDED: {
            SDL_GameController *game_controller = SDL_GameControllerOpen(event.cdevice.which);

            if (game_controller != NULL) {
                uint32 id;
                char idBuffer[0x20];
                sprintf_s(idBuffer, sizeof(idBuffer), "SDLDevice%d", SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(game_controller)));
                GenerateHashCRC(&id, idBuffer);

                if (SKU::InitSDL2InputDevice(id, game_controller) == NULL)
                    SDL_GameControllerClose(game_controller);
            }

            break;
        }

        case SDL_CONTROLLERDEVICEREMOVED: {
            uint32 id;
            char idBuffer[0x20];
            sprintf_s(idBuffer, sizeof(idBuffer), "SDLDevice%d", event.cdevice.which);
            GenerateHashCRC(&id, idBuffer);

            RemoveInputDevice(InputDeviceFromID(id));
            break;
        }

        case SDL_APP_WILLENTERFOREGROUND:
#if RETRO_REV02
            SKU::userCore->focusState = 0;
#endif
            break;

        case SDL_APP_WILLENTERBACKGROUND:
#if RETRO_REV02
            SKU::userCore->focusState = 1;
#endif
            break;

        case SDL_APP_TERMINATING: isRunning = false; break;

        case SDL_MOUSEBUTTONDOWN:
            switch (event.button.button) {
                case SDL_BUTTON_LEFT: touchInfo.down[0] = true; touchInfo.count = 1;
#if !RETRO_REV02
                    RSDK::SKU::buttonDownCount++;
#endif
                    break;

                case SDL_BUTTON_RIGHT:
#if !RETRO_REV02 && RETRO_INPUTDEVICE_KEYBOARD
                    RSDK::SKU::specialKeyStates[3] = true;
                    RSDK::SKU::buttonDownCount++;
#endif
                    break;
            }
            break;

        case SDL_MOUSEBUTTONUP:
            switch (event.button.button) {
                case SDL_BUTTON_LEFT: touchInfo.down[0] = false; touchInfo.count = 0;
#if !RETRO_REV02
                    RSDK::SKU::buttonDownCount--;
#endif
                    break;

                case SDL_BUTTON_RIGHT:
#if !RETRO_REV02 && RETRO_INPUTDEVICE_KEYBOARD
                    RSDK::SKU::specialKeyStates[3] = false;
                    RSDK::SKU::buttonDownCount--;
#endif
                    break;
            }
            break;

        case SDL_FINGERMOTION:
        case SDL_FINGERDOWN:
        case SDL_FINGERUP: {
            int32 count     = SDL_GetNumTouchFingers(event.tfinger.touchId);
            touchInfo.count = 0;
            for (int32 i = 0; i < count; i++) {
                SDL_Finger *finger = SDL_GetTouchFinger(event.tfinger.touchId, i);
                if (finger) {
                    touchInfo.down[touchInfo.count] = true;
                    touchInfo.x[touchInfo.count]    = finger->x;
                    touchInfo.y[touchInfo.count]    = finger->y;
                    touchInfo.count++;
                }
            }
            break;
        }

        case SDL_KEYDOWN:
#if !RETRO_REV02
            ++RSDK::SKU::buttonDownCount;
#endif
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_RETURN:
                    if (event.key.keysym.mod & KMOD_LALT) {
                        videoSettings.windowed ^= 1;
                        UpdateGameWindow();
                        changedVideoSettings = false;
                        break;
                    }

#if !RETRO_REV02 && RETRO_INPUTDEVICE_KEYBOARD
                    RSDK::SKU::specialKeyStates[1] = true;
#endif
                    // [fallthrough]

                default:
#if RETRO_INPUTDEVICE_KEYBOARD
                    SKU::UpdateKeyState(event.key.keysym.scancode);
#endif
                    break;

                case SDL_SCANCODE_ESCAPE:
                    if (engine.devMenu) {
#if RETRO_REV0U
                        if (sceneInfo.state == ENGINESTATE_DEVMENU || RSDK::Legacy::gameMode == RSDK::Legacy::ENGINE_DEVMENU)
#else
                        if (sceneInfo.state == ENGINESTATE_DEVMENU)
#endif
                            CloseDevMenu();
                        else
                            OpenDevMenu();
                    }
                    else {
#if RETRO_INPUTDEVICE_KEYBOARD
                        SKU::UpdateKeyState(event.key.keysym.scancode);
#endif
                    }

#if !RETRO_REV02 && RETRO_INPUTDEVICE_KEYBOARD
                    RSDK::SKU::specialKeyStates[0] = true;
#endif
                    break;

#if !RETRO_USE_ORIGINAL_CODE
                case SDL_SCANCODE_F1:
                    if (engine.devMenu) {
                        sceneInfo.listPos--;
                        if (sceneInfo.listPos < sceneInfo.listCategory[sceneInfo.activeCategory].sceneOffsetStart
                            || sceneInfo.listPos >= sceneInfo.listCategory[sceneInfo.activeCategory].sceneOffsetEnd) {
                            sceneInfo.activeCategory--;
                            if (sceneInfo.activeCategory >= sceneInfo.categoryCount) {
                                sceneInfo.activeCategory = sceneInfo.categoryCount - 1;
                            }
                            sceneInfo.listPos = sceneInfo.listCategory[sceneInfo.activeCategory].sceneOffsetEnd - 1;
                        }

#if RETRO_REV0U
                        switch (engine.version) {
                            default: break;
                            case 5: LoadScene(); break;
                            case 4:
                            case 3: RSDK::Legacy::stageMode = RSDK::Legacy::STAGEMODE_LOAD; break;
                        }
#else
                        LoadScene();
#endif
                    }
                    break;

                case SDL_SCANCODE_F2:
                    if (engine.devMenu) {
                        sceneInfo.listPos++;
                        if (sceneInfo.listPos >= sceneInfo.listCategory[sceneInfo.activeCategory].sceneOffsetEnd || sceneInfo.listPos == 0) {
                            sceneInfo.activeCategory++;
                            if (sceneInfo.activeCategory >= sceneInfo.categoryCount) {
                                sceneInfo.activeCategory = 0;
                            }
                            sceneInfo.listPos = sceneInfo.listCategory[sceneInfo.activeCategory].sceneOffsetStart;
                        }

#if RETRO_REV0U
                        switch (engine.version) {
                            default: break;
                            case 5: LoadScene(); break;
                            case 4:
                            case 3: RSDK::Legacy::stageMode = RSDK::Legacy::STAGEMODE_LOAD; break;
                        }
#else
                        LoadScene();
#endif
                    }
                    break;
#endif

                case SDL_SCANCODE_F3:
                    if (userShaderCount)
                        videoSettings.shaderID = (videoSettings.shaderID + 1) % userShaderCount;
                    break;

#if !RETRO_USE_ORIGINAL_CODE
                case SDL_SCANCODE_F4:
                    if (engine.devMenu)
                        engine.showEntityInfo ^= 1;
                    break;

                case SDL_SCANCODE_F5:
                    if (engine.devMenu) {
                        // Quick-Reload
#if RETRO_USE_MOD_LOADER
                        if (event.key.keysym.mod & KMOD_LCTRL)
                            RefreshModFolders();
#endif

#if RETRO_REV0U
                        switch (engine.version) {
                            default: break;
                            case 5: LoadScene(); break;
                            case 4:
                            case 3: RSDK::Legacy::stageMode = RSDK::Legacy::STAGEMODE_LOAD; break;
                        }
#else
                        LoadScene();
#endif
                    }
                    break;

                case SDL_SCANCODE_F6:
                    if (engine.devMenu && videoSettings.screenCount > 1)
                        videoSettings.screenCount--;
                    break;

                case SDL_SCANCODE_F7:
                    if (engine.devMenu && videoSettings.screenCount < SCREEN_COUNT)
                        videoSettings.screenCount++;
                    break;

                case SDL_SCANCODE_F8:
                    if (engine.devMenu)
                        engine.showUpdateRanges ^= 1;
                    break;

                case SDL_SCANCODE_F9:
                    if (engine.devMenu)
                        showHitboxes ^= 1;
                    break;

                case SDL_SCANCODE_F10:
                    if (engine.devMenu)
                        engine.showPaletteOverlay ^= 1;
                    break;
#endif
                case SDL_SCANCODE_BACKSPACE:
                    if (engine.devMenu)
                        engine.gameSpeed = engine.fastForwardSpeed;
                    break;

                case SDL_SCANCODE_F11:
                case SDL_SCANCODE_INSERT:
                    if (engine.devMenu)
                        engine.frameStep = true;
                    break;

                case SDL_SCANCODE_F12:
                case SDL_SCANCODE_PAUSE:
                    if (engine.devMenu) {
#if RETRO_REV0U
                        switch (engine.version) {
                            default: break;
                            case 5:
                                if (sceneInfo.state != ENGINESTATE_NONE)
                                    sceneInfo.state ^= ENGINESTATE_STEPOVER;
                                break;
                            case 4:
                            case 3:
                                if (RSDK::Legacy::stageMode != ENGINESTATE_NONE)
                                    RSDK::Legacy::stageMode ^= RSDK::Legacy::STAGEMODE_STEPOVER;
                                break;
                        }
#else
                        if (sceneInfo.state != ENGINESTATE_NONE)
                            sceneInfo.state ^= ENGINESTATE_STEPOVER;
#endif
                    }
                    break;
            }
            break;

        case SDL_KEYUP:
#if !RETRO_REV02
            --RSDK::SKU::buttonDownCount;
#endif
            switch (event.key.keysym.scancode) {
                default:
#if RETRO_INPUTDEVICE_KEYBOARD
                    SKU::ClearKeyState(event.key.keysym.scancode);
#endif
                    break;

#if !RETRO_REV02 && RETRO_INPUTDEVICE_KEYBOARD
                case SDL_SCANCODE_ESCAPE:
                    RSDK::SKU::specialKeyStates[0] = false;
                    SKU::ClearKeyState(event.key.keysym.scancode);
                    break;

                case SDL_SCANCODE_RETURN:
                    RSDK::SKU::specialKeyStates[1] = false;
                    SKU::ClearKeyState(event.key.keysym.scancode);
                    break;
#endif
                case SDL_SCANCODE_BACKSPACE: engine.gameSpeed = 1; break;
            }
            break;

        case SDL_QUIT: isRunning = false; break;
    }
}
#endif

bool RenderDevice::ProcessEvents()
{
    // SDL_Event sdlEvent;
    //
    // while (SDL_PollEvent(&sdlEvent)) {
    //     ProcessEvent(sdlEvent);
    //
    //     if (!isRunning)
    //         return false;
    // }
    //
    // return isRunning;
    return true;
}

void RenderDevice::SetupImageTexture(int32 width, int32 height, uint8 *imagePixels)
{
#if 0
    if (lastTextureFormat != SHADER_RGB_IMAGE) {
        if (imageTexture)
            SDL_DestroyTexture(imageTexture);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

        imageTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

        lastTextureFormat = SHADER_RGB_IMAGE;
    }

    int32 texPitch = 0;
    uint32 *pixels = NULL;
    SDL_LockTexture(imageTexture, NULL, (void **)&pixels, &texPitch);

    int32 pitch           = (texPitch >> 2) - width;
    uint32 *imagePixels32 = (uint32 *)imagePixels;
    for (int32 y = 0; y < height; ++y) {
        for (int32 x = 0; x < width; ++x) {
            *pixels++ = *imagePixels32++;
        }

        pixels += pitch;
    }

    SDL_UnlockTexture(imageTexture);
#endif
}

void RenderDevice::SetupVideoTexture_YUV420(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                            int32 strideV)
{
#if 0

    if (lastTextureFormat != SHADER_YUV_420) {
        if (imageTexture)
            SDL_DestroyTexture(imageTexture);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

        imageTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, width, height);

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        lastTextureFormat = SHADER_YUV_420;
    }

    SDL_UpdateYUVTexture(imageTexture, NULL, yPlane, strideY, uPlane, strideU, vPlane, strideV);
#endif
}
void RenderDevice::SetupVideoTexture_YUV422(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                            int32 strideV)
{
#if 0
    if (lastTextureFormat != SHADER_YUV_422) {
        if (imageTexture)
            SDL_DestroyTexture(imageTexture);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

        imageTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, width, height);

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        lastTextureFormat = SHADER_YUV_422;
    }

    SDL_UpdateYUVTexture(imageTexture, NULL, yPlane, strideY, uPlane, strideU, vPlane, strideV);
#endif
}
void RenderDevice::SetupVideoTexture_YUV444(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                            int32 strideV)
{
#if 0
    if (lastTextureFormat != SHADER_YUV_444) {
        if (imageTexture)
            SDL_DestroyTexture(imageTexture);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

        imageTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, width, height);

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        lastTextureFormat = SHADER_YUV_444;
    }

    SDL_UpdateYUVTexture(imageTexture, NULL, yPlane, strideY, uPlane, strideU, vPlane, strideV);
#endif
}
