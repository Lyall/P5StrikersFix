#include "stdafx.h"
#include "helper.hpp"
#include <inipp/inipp.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <safetyhook.hpp>

// Ini parser setup
inipp::Ini<char> ini;
std::string sFixName = "P5StrikersFix";
std::string sFixVer = "1.0.1";
std::string sLogFile = "P5StrikersFix.log";
std::string sConfigFile = "P5StrikersFix.ini";
std::string sExeName;
std::filesystem::path sExePath;
std::pair DesktopDimensions = { 0,0 };

// Ini variables
int iInjectionDelay;
bool bCustomRes;
int iCustomResX;
int iCustomResY;
bool bRTScaling;
bool bRTUseRenderScale;
bool bFixUI;
bool bFixFOV;
float fAdditionalFOV;
bool bDisableLetterboxing;
int iShadowQuality;
bool bFixAnalog;
bool bIntroSkip;

// Aspect ratio + HUD stuff
float fPi = (float)3.141592653;
float fAspectRatio;
float fNativeAspect = (float)16 / 9;
float fAspectMultiplier;
float fDefaultHUDWidth = (float)1920;
float fDefaultHUDHeight = (float)1080;
float fHUDWidth;
float fHUDHeight;
float fHUDWidthOffset;
float fHUDHeightOffset;

// Variables
float fRenderScale = 1.0f;
DWORD64 RenderScaleAddress;
bool bIsMoviePlaying = false;
DWORD64 MoviePlaybackAddress;
HMODULE baseModule = GetModuleHandle(NULL);

void Logging()
{
    // spdlog initialisation
    {
        try
        {
            auto logger = spdlog::basic_logger_mt(sFixName.c_str(), sLogFile, true);
            spdlog::set_default_logger(logger);

        }
        catch (const spdlog::spdlog_ex& ex)
        {
            AllocConsole();
            FILE* dummy;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            std::cout << "Log initialisation failed: " << ex.what() << std::endl;
        }
    }

    spdlog::flush_on(spdlog::level::debug);
    spdlog::info("{} v{} loaded.", sFixName.c_str(), sFixVer.c_str());
    spdlog::info("----------");

    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(baseModule, exePath, MAX_PATH);
    sExePath = exePath;
    sExeName = sExePath.filename().string();

    // Log module details
    spdlog::info("Module Name: {0:s}", sExeName.c_str());
    spdlog::info("Module Path: {0:s}", sExePath.string().c_str());
    spdlog::info("Module Address: 0x{0:x}", (uintptr_t)baseModule);
    spdlog::info("Module Timestamp: {0:d}", Memory::ModuleTimestamp(baseModule));
    spdlog::info("----------");
}

void ReadConfig()
{
    // Initialise config
    std::ifstream iniFile(sConfigFile);
    if (!iniFile)
    {
        spdlog::critical("Failed to load config file.");
        spdlog::critical("Make sure {} is present in the game folder.", sConfigFile);
    }
    else
    {
        ini.parse(iniFile);
    }

    // Read ini file
    inipp::get_value(ini.sections["Injection Delay"], "InjectionDelay", iInjectionDelay);
    inipp::get_value(ini.sections["Custom Resolution"], "Enabled", bCustomRes);
    inipp::get_value(ini.sections["Custom Resolution"], "Width", iCustomResX);
    inipp::get_value(ini.sections["Custom Resolution"], "Height", iCustomResY);
    inipp::get_value(ini.sections["Render Target Scaling"], "Enabled", bRTScaling);
    inipp::get_value(ini.sections["Render Target Scaling"], "UseRenderScale", bRTUseRenderScale);
    inipp::get_value(ini.sections["Fix UI"], "Enabled", bFixUI);
    inipp::get_value(ini.sections["Fix FOV"], "Enabled", bFixFOV);
    inipp::get_value(ini.sections["Fix FOV"], "AdditionalFOV", fAdditionalFOV);
    inipp::get_value(ini.sections["Disable Cutscene Letterboxing"], "Enabled", bDisableLetterboxing);
    inipp::get_value(ini.sections["Shadow Quality"], "Resolution", iShadowQuality);
    inipp::get_value(ini.sections["Fix Analog Movement"], "Enabled", bFixAnalog);
    inipp::get_value(ini.sections["Intro Skip"], "Enabled", bIntroSkip);

    // Log config parse
    spdlog::info("Config Parse: iInjectionDelay: {}ms", iInjectionDelay);
    spdlog::info("Config Parse: bCustomRes: {}", bCustomRes);
    spdlog::info("Config Parse: iCustomResX: {}", iCustomResX);
    spdlog::info("Config Parse: iCustomResY: {}", iCustomResY);
    spdlog::info("Config Parse: bRTScaling: {}", bRTScaling);
    spdlog::info("Config Parse: bRTUseRenderScale: {}", bRTUseRenderScale);
    spdlog::info("Config Parse: bFixUI: {}", bFixUI);
    spdlog::info("Config Parse: bFixFOV: {}", bFixFOV);
    spdlog::info("Config Parse: fAdditionalFOV: {}", fAdditionalFOV);
    spdlog::info("Config Parse: bDisableLetterboxing: {}", bDisableLetterboxing);
    spdlog::info("Config Parse: iShadowQuality: {}", iShadowQuality);
    spdlog::info("Config Parse: bFixAnalog: {}", bFixAnalog);
    spdlog::info("Config Parse: bIntroSkip: {}", bIntroSkip);
    spdlog::info("----------");

    // Calculate aspect ratio / use desktop res instead
    DesktopDimensions = Util::GetPhysicalDesktopDimensions();

    if (iCustomResX > 0 && iCustomResY > 0)
    {
        fAspectRatio = (float)iCustomResX / (float)iCustomResY;
    }
    else
    {
        iCustomResX = (int)DesktopDimensions.first;
        iCustomResY = (int)DesktopDimensions.second;
        fAspectRatio = (float)DesktopDimensions.first / (float)DesktopDimensions.second;
        spdlog::info("Custom Resolution: iCustomResX: Desktop Width: {}", iCustomResX);
        spdlog::info("Custom Resolution: iCustomResY: Desktop Height: {}", iCustomResY);
    }
    fAspectMultiplier = fAspectRatio / fNativeAspect;

    // HUD variables
    fHUDWidth = iCustomResY * fNativeAspect;
    fHUDHeight = (float)iCustomResY;
    fHUDWidthOffset = (float)(iCustomResX - fHUDWidth) / 2;
    fHUDHeightOffset = 0;
    if (fAspectRatio < fNativeAspect)
    {
        fHUDWidth = (float)iCustomResX;
        fHUDHeight = (float)iCustomResX / fNativeAspect;
        fHUDWidthOffset = 0;
        fHUDHeightOffset = (float)(iCustomResY - fHUDHeight) / 2;
    }

    // Log aspect ratio stuff
    spdlog::info("Custom Resolution: fAspectRatio: {}", fAspectRatio);
    spdlog::info("Custom Resolution: fAspectMultiplier: {}", fAspectMultiplier);
    spdlog::info("Custom Resolution: fHUDWidth: {}", fHUDWidth);
    spdlog::info("Custom Resolution: fHUDHeight: {}", fHUDHeight);
    spdlog::info("Custom Resolution: fHUDWidthOffset: {}", fHUDWidthOffset);
    spdlog::info("Custom Resolution: fHUDHeightOffset: {}", fHUDHeightOffset);
    spdlog::info("----------");
}

void EarlyPatch()
{
    if (bFixUI)
    {
        // Set UI aspect ratio to 16:9
        uint8_t* UIAspectScanResult = Memory::PatternScan(baseModule, "41 ?? 80 07 00 00 44 ?? ?? ?? ?? 41 ?? 38 04 00 00");
        if (UIAspectScanResult)
        {
            spdlog::info("UI Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)UIAspectScanResult - (uintptr_t)baseModule);
            if (fAspectRatio > fNativeAspect)
            {
                Memory::Write((uintptr_t)UIAspectScanResult + 0x2, (int)(1080 * fAspectRatio));
            }
            else if (fAspectRatio < fNativeAspect)
            {
                Memory::Write((uintptr_t)UIAspectScanResult + 0xD, (int)(1920 / fAspectRatio));
            }
        }
        else if (!UIAspectScanResult)
        {
            spdlog::error("UI Aspect Ratio: Pattern scan failed.");
        }
    }
}

void ResolutionFix()
{
    if (bCustomRes)
    {
        // Apply custom resolution
        uint8_t* ResolutionScanResult = Memory::PatternScan(baseModule, "89 ?? ?? ?? 00 00 48 ?? ?? ?? ?? ?? ?? 83 ?? ?? 77 ?? 8B ?? ?? ?? ?? ?? ?? EB ?? B9 D0 02 00 00");
        if (ResolutionScanResult)
        {
            spdlog::info("Custom Resolution: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResolutionScanResult - (uintptr_t)baseModule);

            static SafetyHookMid ResolutionWidthHook{};
            ResolutionWidthHook = safetyhook::create_mid(ResolutionScanResult,
                [](SafetyHookContext& ctx)
                {
                    ctx.rcx = iCustomResX;
                });

            static SafetyHookMid ResolutionHeightHook{};
            ResolutionHeightHook = safetyhook::create_mid(ResolutionScanResult + 0x27,
                [](SafetyHookContext& ctx)
                {
                    ctx.rcx = iCustomResY;
                });

            spdlog::info("Custom Resolution: Applied custom resolution of {}x{}", iCustomResX, iCustomResY);
        }
        else if (!ResolutionScanResult)
        {
            spdlog::error("Custom Resolution: Pattern scan failed.");
        }

        // Stop fullscreen mode from being scaled to 16:9
        uint8_t* FullscreenModeScanResult = Memory::PatternScan(baseModule, "80 ?? ?? ?? ?? ?? 00 0F ?? ?? ?? ?? ?? 8B ?? ?? ?? ?? ?? 0F ?? ?? F3 0F ?? ?? ?? ?? ?? ?? 0F ?? ??");
        if (FullscreenModeScanResult)
        {
            spdlog::info("Custom Resolution: Fullscreen: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)FullscreenModeScanResult - (uintptr_t)baseModule);

            Memory::PatchBytes((uintptr_t)FullscreenModeScanResult + 0x6, "\x05", 1);
        }
        else if (!FullscreenModeScanResult)
        {
            spdlog::error("Custom Resolution: Fullscreen: Pattern scan failed.");
        }

        // Stop borderless mode from being scaled to 16:9
        uint8_t* BorderlessModeScanResult = Memory::PatternScan(baseModule, "80 ?? ?? ?? ?? ?? 00 74 ?? 80 ?? ?? ?? ?? ?? 00 74 ?? 4C ?? ?? ?? ?? ?? ?? 48 ?? ?? ?? ?? ?? ??");
        if (BorderlessModeScanResult)
        {
            spdlog::info("Custom Resolution: Borderless: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)BorderlessModeScanResult - (uintptr_t)baseModule);

            Memory::PatchBytes((uintptr_t)BorderlessModeScanResult + 0x7, "\xEB", 1);
        }
        else if (!BorderlessModeScanResult)
        {
            spdlog::error("Custom Resolution: Borderless: Pattern scan failed.");
        }
    }

    if (bRTScaling)
    {
        // Get render scale address
        uint8_t* RenderScaleScanResult = Memory::PatternScan(baseModule, "00 00 00 00 66 0F ?? ?? ?? ?? ?? ?? 0F ?? ?? F3 0F ?? ?? ?? ?? ?? ?? C3");
        if (RenderScaleScanResult)
        {
            spdlog::info("Render Scale: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)RenderScaleScanResult - (uintptr_t)baseModule);
            RenderScaleAddress = Memory::GetAbsolute((uintptr_t)RenderScaleScanResult + 0x8);
            spdlog::info("Render Scale: Value address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)RenderScaleAddress - (uintptr_t)baseModule);
        }
        else if (!RenderScaleScanResult)
        {
            spdlog::error("Render Scale: Pattern scan failed.");
        }

        // Set render target resolution
        uint8_t* RenderTargetResolutionScanResult = Memory::PatternScan(baseModule, "41 ?? ?? 89 ?? ?? ?? 89 ?? ?? ?? 48 ?? ?? E8 ?? ?? ?? ??");
        uint8_t* RenderTargetResolution2ScanResult = Memory::PatternScan(baseModule, "41 ?? ?? 49 ?? ?? ?? 44 ?? ?? 89 ?? ?? ?? 89 ?? ?? ?? 44 ?? ?? ?? ??");
        if (RenderTargetResolutionScanResult && RenderTargetResolution2ScanResult)
        {
            spdlog::info("Render Target Resolution: Address 1 is {:s}+{:x}", sExeName.c_str(), (uintptr_t)RenderTargetResolutionScanResult - (uintptr_t)baseModule);
            static SafetyHookMid RenderTargetResolutionHook{};
            RenderTargetResolutionHook = safetyhook::create_mid(RenderTargetResolutionScanResult,
                [](SafetyHookContext& ctx)
                {
                    if (RenderScaleAddress && bRTUseRenderScale)
                    {
                        fRenderScale = (float)*reinterpret_cast<int*>(RenderScaleAddress) / 10;
                    }
                    ctx.r10 = static_cast<int>(iCustomResX * fRenderScale);
                    ctx.r8 = static_cast<int>(iCustomResY * fRenderScale);
                });

            spdlog::info("Render Target Resolution: Address 2 is {:s}+{:x}", sExeName.c_str(), (uintptr_t)RenderTargetResolution2ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid RenderTargetResolution2Hook{};
            RenderTargetResolution2Hook = safetyhook::create_mid(RenderTargetResolution2ScanResult,
                [](SafetyHookContext& ctx)
                {
                    if (RenderScaleAddress && bRTUseRenderScale)
                    {
                        fRenderScale = (float)*reinterpret_cast<int*>(RenderScaleAddress) / 10;
                    }
                    ctx.r13 = static_cast<int>(iCustomResX * fRenderScale);
                    ctx.rdi = static_cast<int>(iCustomResY * fRenderScale);
                });
        }
        else if (!RenderTargetResolutionScanResult || !RenderTargetResolution2ScanResult)
        {
            spdlog::error("Render Target Resolution: Pattern scan failed.");
        }
    }
}

void UIFix()
{
    if (bFixUI)
    {
        // Fix offset cursor position when UI is scaled to 16:9
        uint8_t* UICursorPos1ScanResult = Memory::PatternScan(baseModule, "0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? 0F ?? ?? 76 ?? 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? 0F ?? ??");
        uint8_t* UICursorPos2ScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? 66 0F ?? ?? F3 0F ?? ?? 66 0F ?? ?? ?? ?? 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? 29 ?? ?? ??");
        if (UICursorPos1ScanResult && UICursorPos2ScanResult)
        {
            spdlog::info("UI Cursor Position: Address 1 is {:s}+{:x}", sExeName.c_str(), (uintptr_t)UICursorPos1ScanResult - (uintptr_t)baseModule);

            static SafetyHookMid UICursorPos1MidHook{};
            UICursorPos1MidHook = safetyhook::create_mid(UICursorPos1ScanResult,
                [](SafetyHookContext& ctx)
                {
                    if (fAspectRatio > fNativeAspect)
                    {
                        ctx.xmm3.f32[0] = fHUDWidth;
                    }
                    else if (fAspectRatio < fNativeAspect)
                    {
                        ctx.xmm1.f32[0] = fHUDHeight;
                    }
                });

            spdlog::info("UI Cursor Position: Address 2 is {:s}+{:x}", sExeName.c_str(), (uintptr_t)UICursorPos2ScanResult - (uintptr_t)baseModule);

            static SafetyHookMid UICursorPos2MidHook{};
            UICursorPos2MidHook = safetyhook::create_mid(UICursorPos2ScanResult,
                [](SafetyHookContext& ctx)
                {
                    if (fAspectRatio > fNativeAspect)
                    {
                        ctx.xmm0.f32[0] = fHUDWidth;
                    }
                    else if (fAspectRatio < fNativeAspect)
                    {
                        ctx.rbx = (int)fHUDHeight;
                    }
                });
        }
        else if (!UICursorPos1ScanResult || !UICursorPos2ScanResult)
        {
            spdlog::error("UI Cursor Position: Pattern scan failed.");
        }

        // Fix floating markers being offset 
        uint8_t* MarkersScanResult = Memory::PatternScan(baseModule, "C7 ?? ?? 38 04 00 00 41 ?? 80 07 00 00 4C ?? ?? ??");
        uint8_t* Markers2ScanResult = Memory::PatternScan(baseModule, "41 ?? 80 07 00 00 C7 ?? ?? ?? 38 04 00 00 33 ??");
        if (MarkersScanResult && Markers2ScanResult)
        {
            spdlog::info("Markers: Address 1 is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MarkersScanResult - (uintptr_t)baseModule);
            spdlog::info("Markers: Address 2 is {:s}+{:x}", sExeName.c_str(), (uintptr_t)Markers2ScanResult - (uintptr_t)baseModule);

            if (fAspectRatio > fNativeAspect)
            {
                Memory::Write((uintptr_t)MarkersScanResult + 0x9, (int)(1080 * fAspectRatio));
                Memory::Write((uintptr_t)Markers2ScanResult + 0x2, (int)(1080 * fAspectRatio));
            }
            else if (fAspectRatio < fNativeAspect)
            {
                Memory::Write((uintptr_t)MarkersScanResult + 0x3, (int)(1920 / fAspectRatio));
                Memory::Write((uintptr_t)Markers2ScanResult + 0xA, (int)(1920 / fAspectRatio));
            }
        }
        else if (!MarkersScanResult || !Markers2ScanResult)
        {
            spdlog::error("Markers: Pattern scan failed.");
        }

        // Movie playback status
        uint8_t* MoviePlaybackScanResult = Memory::PatternScan(baseModule, "83 ?? ?? ?? ?? ?? FF 75 ?? 48 ?? ?? ?? ?? ?? ?? 48 ?? ?? FF ?? ?? ?? ?? ??");
        if (MoviePlaybackScanResult)
        {
            spdlog::info("Movie Playback: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MoviePlaybackScanResult - (uintptr_t)baseModule);
            MoviePlaybackAddress = Memory::GetAbsolute((uintptr_t)MoviePlaybackScanResult + 0xC) + 0x6C;
            spdlog::info("Movie Playback: Value address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MoviePlaybackAddress - (uintptr_t)baseModule);

        }
        else if (!MoviePlaybackScanResult)
        {
            spdlog::error("Movie Playback: Pattern scan failed.");
        }
    }

    if (bFixUI || bDisableLetterboxing)
    {
        // UI Width
        uint8_t* UIWidthScanResult = Memory::PatternScan(baseModule, "8B ?? ?? ?? ?? 00 89 ?? ?? 49 ?? ?? ?? 48 ?? ?? FF ?? ?? ?? ?? 00");
        if (UIWidthScanResult)
        {
            spdlog::info("UI Width: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)UIWidthScanResult - (uintptr_t)baseModule);

            static SafetyHookMid UIWidth2MidHook{};
            UIWidth2MidHook = safetyhook::create_mid(UIWidthScanResult,
                [](SafetyHookContext& ctx)
                {
                    if (ctx.rax)
                    {
                        // Get starting values
                        std::string sObjectName = (std::string)(char*)(ctx.rax + 0x280);
                        short iWidth = *reinterpret_cast<short*>(ctx.rax + 0xF0);
                        short iHeight = *reinterpret_cast<short*>(ctx.rax + 0xF2);
                        int iMarker = *reinterpret_cast<int*>(ctx.rax + 0x4C);
                        if (MoviePlaybackAddress)
                        {
                            bIsMoviePlaying = *reinterpret_cast<int*>(MoviePlaybackAddress);
                        }

                        // Find movie playback layer and add marker so it remains unmodified.
                        if (iWidth == (short)1920 && iHeight == (short)1080 && sObjectName.contains("parts_blank") && iMarker == 0 && bIsMoviePlaying)
                        {
                            // Add marker
                            iMarker = 420;
                            spdlog::info("UI Width: Fixed FMV playback.");
                        }

                        // Check for marker so we don't edit the same thing twice.
                        if (iMarker != 420)
                        {
                            // Resize all UI elements that are 1920-2048x1080-1200
                            if ((iWidth >= (short)1920 && iWidth <= (short)2048) && (iHeight >= (short)1080 && iHeight <= (short)1200))
                            {
                                if (fAspectRatio > fNativeAspect)
                                {
                                    iWidth = static_cast<short>(iHeight * fAspectRatio);
                                }
                                else if (fAspectRatio < fNativeAspect)
                                {
                                    iHeight = static_cast<short>(iWidth / fAspectRatio);
                                }
                                // Add marker
                                iMarker = 420;
                            }

                            // Cutscene letterboxing
                            if (iWidth == (short)1920 && iHeight == (short)256)
                            {
                                if (fAspectRatio > fNativeAspect)
                                {
                                    iWidth = static_cast<short>(1920 * fAspectMultiplier);
                                }
                                else if (fAspectRatio < fNativeAspect)
                                {
                                    // This is dumb, just flipping the texture. Maybe just disable letterboxing at <16:9?
                                    iHeight = static_cast<short>(-256 - fHUDHeightOffset);
                                }

                                if (bDisableLetterboxing)
                                {
                                    iWidth = (short)0;
                                    iHeight = (short)0;
                                }
                                // Add marker
                                iMarker = 420;
                            }
                        }

                        // Write modified values
                        *reinterpret_cast<short*>(ctx.rax + 0xF0) = iWidth;
                        *reinterpret_cast<short*>(ctx.rax + 0xF2) = iHeight;
                        *reinterpret_cast<short*>(ctx.rax + 0x4C) = iMarker;
                    }
                });
        }
        else if (!UIWidthScanResult)
        {
            spdlog::error("UI Width: Pattern scan failed.");
        }
    }
}

void FOVFix()
{   
    if (bFixFOV)
    {
        // Fix FOV during cutscenes
        uint8_t* CutsceneFOVScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? F3 0F ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? F3 0F ?? ?? ?? ?? 48 8D ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ??");
        if (CutsceneFOVScanResult)
        {
            spdlog::info("Cutscene FOV: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)CutsceneFOVScanResult - (uintptr_t)baseModule);

            static SafetyHookMid CutsceneFOVMidHook{};
            CutsceneFOVMidHook = safetyhook::create_mid(CutsceneFOVScanResult + 0xC,
                [](SafetyHookContext& ctx)
                {
                    if (fAspectRatio > fNativeAspect)
                    {
                        ctx.xmm0.f32[0] = fNativeAspect * fNativeAspect;
                    }
                    else if (fAspectRatio < fNativeAspect)
                    {
                        ctx.xmm0.f32[0] *= fAspectMultiplier;
                    }
                });
        }
        else if (!CutsceneFOVScanResult)
        {
            spdlog::error("Cutscene FOV: Pattern scan failed.");
        }

        // Fix FOV during gameplay
        uint8_t* GameplayFOVScanResult = Memory::PatternScan(baseModule, "E8 ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? 00 41 ?? 01 F3 0F ?? ?? ?? ?? ?? ?? 41 ?? ?? ?? 48 ?? ??");
        if (GameplayFOVScanResult)
        {
            spdlog::info("Gameplay FOV: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)GameplayFOVScanResult - (uintptr_t)baseModule);

            static SafetyHookMid GameplayFOVMidHook{};
            GameplayFOVMidHook = safetyhook::create_mid(GameplayFOVScanResult + 0xD,
                [](SafetyHookContext& ctx)
                {
                    if (fAspectRatio > fNativeAspect)
                    {
                        ctx.xmm0.f32[0] += fAdditionalFOV;
                    }
                    else if (fAspectRatio < fNativeAspect)
                    {
                        ctx.xmm0.f32[0] = atanf(tanf(ctx.xmm0.f32[0] * (fPi / 360)) / fAspectRatio * fNativeAspect) * (360 / fPi);
                        ctx.xmm0.f32[0] += fAdditionalFOV;
                    }
                });
        }
        else if (!GameplayFOVScanResult)
        {
            spdlog::error("Gameplay FOV: Pattern scan failed.");
        }
    }
}

void Misc()
{
    if (bIntroSkip) 
    {
        // Intro skip
        uint8_t* IntroSkipScanResult = Memory::PatternScan(baseModule, "C7 ?? ?? 04 00 00 00 48 ?? ?? ?? ?? 48 ?? ?? ?? 5F E9 ?? ?? ?? ?? 41 ?? 01");
        if (IntroSkipScanResult)
        {
            spdlog::info("Intro Skip: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)IntroSkipScanResult - (uintptr_t)baseModule);
            Memory::Write((uintptr_t)IntroSkipScanResult + 0x3, 10);
            spdlog::info("Intro Skip: Patched instruction.");
        }
        else if (!IntroSkipScanResult)
        {
            spdlog::error("Intro Skip: Pattern scan failed.");
        }
    }

    if (iShadowQuality != 0)
    {
        // Shadow Quality
        // Changes "high" quality shadow resolution
        uint8_t* ShadowQuality1ScanResult = Memory::PatternScan(baseModule, "00 10 00 00 00 10 00 00 4E 00 00 00 00 04 00 00");
        uint8_t* ShadowQuality2ScanResult = Memory::PatternScan(baseModule, "BA 00 10 00 00 44 ?? ?? EB ?? BA 00 08 00 00");
        if (ShadowQuality1ScanResult && ShadowQuality2ScanResult)
        {
            spdlog::info("Shadow Quality: Address 1 is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ShadowQuality1ScanResult - (uintptr_t)baseModule);
            spdlog::info("Shadow Quality: Address 2 is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ShadowQuality2ScanResult - (uintptr_t)baseModule);

            Memory::Write((uintptr_t)ShadowQuality1ScanResult, (int)iShadowQuality * 2);
            Memory::Write((uintptr_t)ShadowQuality1ScanResult + 0x4, (int)iShadowQuality * 2);
            Memory::Write((uintptr_t)ShadowQuality2ScanResult + 0x1, (int)iShadowQuality * 2);
        }
        else if (!ShadowQuality1ScanResult || !ShadowQuality2ScanResult)
        {
            spdlog::error("Shadow Quality: Pattern scan(s) failed.");
        }
    }

    if (bFixAnalog) 
    {
        // Fix 8-way analog movement
        uint8_t* XInputGetStateScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? 8D ?? ?? 83 ?? 07 77 ?? 48 ?? ?? ?? ?? 8B ?? E8 ?? ?? ?? ??");
        if (XInputGetStateScanResult)
        {
            spdlog::info("Analog Movement Fix: XInputGetState: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)XInputGetStateScanResult - (uintptr_t)baseModule);
            Memory::PatchBytes((uintptr_t)XInputGetStateScanResult, "\x0F\x57\xFF\x90\x90\x90\x90\x90", 8);
            spdlog::info("Analog Movement Fix: XInputGetState: Patched instruction.");
        }
        else if (!XInputGetStateScanResult)
        {
            spdlog::error("Analog Movement Fix: XInputGetState: Pattern scan failed.");
        }
    }
}

DWORD __stdcall Main(void*)
{
    Logging();
    ReadConfig();
    EarlyPatch();
    Sleep(iInjectionDelay);
    ResolutionFix();
    UIFix();
    FOVFix();
    Misc();
    return true; //end thread
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);

        if (mainHandle)
        {
            CloseHandle(mainHandle);
        }
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

