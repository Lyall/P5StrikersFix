#include "stdafx.h"
#include "helper.hpp"
#include <inipp/inipp.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <safetyhook.hpp>

// Ini parser setup
inipp::Ini<char> ini;
std::string sFixName = "P5StrikersFix";
std::string sFixVer = "0.8.1";
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
bool bFixUI;
bool bFixFOV;
float fAdditionalFOV;
bool bDisableLetterboxing;

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
    inipp::get_value(ini.sections["Fix UI"], "Enabled", bFixUI);
    inipp::get_value(ini.sections["Fix FOV"], "Enabled", bFixFOV);
    inipp::get_value(ini.sections["Fix FOV"], "AdditionalFOV", fAdditionalFOV);
    inipp::get_value(ini.sections["Disable Cutscene Letterboxing"], "Enabled", bDisableLetterboxing);

    // Log config parse
    spdlog::info("Config Parse: iInjectionDelay: {}ms", iInjectionDelay);
    spdlog::info("Config Parse: bCustomRes: {}", bCustomRes);
    spdlog::info("Config Parse: iCustomResX: {}", iCustomResX);
    spdlog::info("Config Parse: iCustomResY: {}", iCustomResY);
    spdlog::info("Config Parse: bFixUI: {}", bFixUI);
    spdlog::info("Config Parse: bFixFOV: {}", bFixFOV);
    spdlog::info("Config Parse: fAdditionalFOV: {}", fAdditionalFOV);
    spdlog::info("Config Parse: bDisableLetterboxing: {}", bDisableLetterboxing);
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

void ResolutionFix()
{
    if (bCustomRes)
    {
        // Apply custom resolution
        uint8_t* ResolutionScanResult = Memory::PatternScan(baseModule, "89 ?? ?? ?? 00 00 48 ?? ?? ?? ?? ?? ?? 83 ?? ?? 77 ?? 8B ?? ?? ?? ?? ?? ?? EB ?? B9 D0 02 00 00");
        if (ResolutionScanResult)
        {
            spdlog::info("Custom Resolution: Address is {0:s}+{1:x}", sExeName.c_str(), (uintptr_t)ResolutionScanResult - (uintptr_t)baseModule);

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
            spdlog::info("Custom Resolution: Fullscreen: Address is {0:s}+{1:x}", sExeName.c_str(), (uintptr_t)FullscreenModeScanResult - (uintptr_t)baseModule);

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
            spdlog::info("Custom Resolution: Borderless: Address is {0:s}+{1:x}", sExeName.c_str(), (uintptr_t)BorderlessModeScanResult - (uintptr_t)baseModule);

            Memory::PatchBytes((uintptr_t)BorderlessModeScanResult + 0x7, "\xEB", 1);
        }
        else if (!BorderlessModeScanResult)
        {
            spdlog::error("Custom Resolution: Borderless: Pattern scan failed.");
        }
    }
}

void UIFix()
{
    if (bFixUI || bDisableLetterboxing)
    {
        // Force 16:9 UI
        uint8_t* UIAspectScanResult = Memory::PatternScan(baseModule, "49 ?? ?? 01 75 ?? 0F ?? ?? 41 ?? 01 0F ?? ??");
        if (UIAspectScanResult)
        {
            spdlog::info("UI Aspect Ratio: Address is {0:s}+{1:x}", sExeName.c_str(), (uintptr_t)UIAspectScanResult - (uintptr_t)baseModule);

            static SafetyHookMid UIAspectMidHook{};
            UIAspectMidHook = safetyhook::create_mid(UIAspectScanResult + 0x6,
                [](SafetyHookContext& ctx)
                {
                    if (fAspectRatio > fNativeAspect)
                    {
                        *reinterpret_cast<float*>(ctx.rax + 0x3C) = fAspectRatio;
                    }
                    else if (fAspectRatio < fNativeAspect)
                    {
                        *reinterpret_cast<float*>(ctx.rax + 0x38) = (atanf(tanf(45.0f * (fPi / 360)) / fAspectRatio * fNativeAspect) * (360 / fPi)) * fPi / 180;
                        *reinterpret_cast<float*>(ctx.rax + 0x3C) = fAspectRatio;
                    }
                });
        }
        else if (!UIAspectScanResult)
        {
            spdlog::error("UI Aspect Ratio: Pattern scan failed.");
        }

        // UI Width
        uint8_t* UIWidthScanResult = Memory::PatternScan(baseModule, "8B ?? ?? ?? ?? 00 89 ?? ?? 49 ?? ?? ?? 48 ?? ?? FF ?? ?? ?? ?? 00");
        if (UIWidthScanResult)
        {
            spdlog::info("UI Width: Address is {0:s}+{1:x}", sExeName.c_str(), (uintptr_t)UIWidthScanResult - (uintptr_t)baseModule);
            
            static SafetyHookMid UIWidth2MidHook{};
            UIWidth2MidHook = safetyhook::create_mid(UIWidthScanResult,
                [](SafetyHookContext& ctx)
                {
                    if (ctx.rax + 0xF0 && ctx.rax + 0xF2)
                    {
                        // Useful for finding UI object names
                        //std::string objectName = (std::string)(char*)(ctx.rax + 0x280);
                        //spdlog::info("UI Width: Object name = {}: {}x{}", objectName, *reinterpret_cast<short*>(ctx.rax + 0xF0), *reinterpret_cast<short*>(ctx.rax + 0xF2));
                         
                        if (*reinterpret_cast<short*>(ctx.rax + 0xF0) == (short)1922 && *reinterpret_cast<short*>(ctx.rax + 0xF2) == (short)1082)
                        {
                            if (fAspectRatio > fNativeAspect)
                            {
                                *reinterpret_cast<short*>(ctx.rax + 0xF0) = 1082 * fAspectRatio;
                            }
                            else if (fAspectRatio < fNativeAspect)
                            {
                                *reinterpret_cast<short*>(ctx.rax + 0xF2) = 1922 / fAspectRatio;
                            }
                        }

                        if (*reinterpret_cast<short*>(ctx.rax + 0xF0) == (short)1924 && *reinterpret_cast<short*>(ctx.rax + 0xF2) == (short)1084)
                        {
                            if (fAspectRatio > fNativeAspect)
                            {
                                *reinterpret_cast<short*>(ctx.rax + 0xF0) = 1084 * fAspectRatio;
                            }
                            else if (fAspectRatio < fNativeAspect)
                            {
                                *reinterpret_cast<short*>(ctx.rax + 0xF2) = 1924 / fAspectRatio;
                            }
                        }
                                                
                        if (*reinterpret_cast<short*>(ctx.rax + 0xF0) == (short)2048 && *reinterpret_cast<short*>(ctx.rax + 0xF2) == (short)1200)
                        {
                            if (fAspectRatio > fNativeAspect)
                            {
                                *reinterpret_cast<short*>(ctx.rax + 0xF0) = 1200 * fAspectRatio;
                            }
                            else if (fAspectRatio < fNativeAspect)
                            {
                                *reinterpret_cast<short*>(ctx.rax + 0xF2) = 2048 / fAspectRatio;
                            }
                        }

                        if (*reinterpret_cast<short*>(ctx.rax + 0xF0) == (short)2016 && *reinterpret_cast<short*>(ctx.rax + 0xF2) == (short)1134)
                        {
                            if (fAspectRatio > fNativeAspect)
                            {
                                *reinterpret_cast<short*>(ctx.rax + 0xF0) = 1134 * fAspectRatio;
                            }
                            else if (fAspectRatio < fNativeAspect)
                            {
                                *reinterpret_cast<short*>(ctx.rax + 0xF2) = 2016 / fAspectRatio;
                            }
                        }

                        if (*reinterpret_cast<short*>(ctx.rax + 0xF0) > (short)1920)
                        {
                            //spdlog::info("UI Width: Over 1920px: Object name = {}: {}x{}", objectName, *reinterpret_cast<short*>(ctx.rax + 0xF0), *reinterpret_cast<short*>(ctx.rax + 0xF2));
                            if (fAspectRatio > fNativeAspect)
                            {
                                //*reinterpret_cast<short*>(ctx.rax + 0xF0) = (short)2048 * fAspectMultiplier;
                            }
                        }

                        // Cutscene letterboxing
                        if (*reinterpret_cast<short*>(ctx.rax + 0xF0) == (short)1920 && *reinterpret_cast<short*>(ctx.rax + 0xF2) == (short)256)
                        {
                            if (fAspectRatio > fNativeAspect)
                            {
                                *reinterpret_cast<short*>(ctx.rax + 0xF0) = (short)1920 * fAspectMultiplier;
                            }

                            if (bDisableLetterboxing)
                            {
                                *reinterpret_cast<short*>(ctx.rax + 0xF0) = (short)0;
                                *reinterpret_cast<short*>(ctx.rax + 0xF0) = (short)0;
                            }
                        }
                      
                        // 1080p UI elements
                        if (*reinterpret_cast<short*>(ctx.rax + 0xF0) == (short)1920 && *reinterpret_cast<short*>(ctx.rax + 0xF2) == (short)1080)
                        {
                            if (fAspectRatio > fNativeAspect)
                            {
                                *reinterpret_cast<short*>(ctx.rax + 0xF0) = (short)1080 * fAspectRatio;
                            }
                            else if (fAspectRatio < fNativeAspect)
                            {
                                *reinterpret_cast<short*>(ctx.rax + 0xF2) = (short)1920 / fAspectRatio;
                            }
                        }
                    }
                });      
        }
        else if (!UIWidthScanResult)
        {
            spdlog::error("UI Width: Pattern scan failed.");
        }

        /*
        // Fix offset cursor position when UI is scaled to 16:9
        uint8_t* UICursorPosScanResult = Memory::PatternScan(baseModule, "0F ?? ?? 66 ?? ?? ?? 0F ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? 66 ?? ?? ?? 0F ?? ?? ?? 0F ?? ?? 0F ?? ??");
        if (UICursorPosScanResult)
        {
            spdlog::info("UI Cursor Position: Address is {0:s}+{1:x}", sExeName.c_str(), (uintptr_t)UICursorPosScanResult - (uintptr_t)baseModule);

            static SafetyHookMid UICursorPosMidHook{};
            UICursorPosMidHook = safetyhook::create_mid(UICursorPosScanResult + 0x3,
                [](SafetyHookContext& ctx)
                {
                    ctx.xmm5.f32[0] *= fAspectMultiplier;
                });
        }
        else if (!UICursorPosScanResult)
        {
            spdlog::error("UI Cursor Position: Pattern scan failed.");
        }
        */
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
            spdlog::info("Cutscene FOV: Address is {0:s}+{1:x}", sExeName.c_str(), (uintptr_t)CutsceneFOVScanResult - (uintptr_t)baseModule);

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
            spdlog::info("Gameplay FOV: Address is {0:s}+{1:x}", sExeName.c_str(), (uintptr_t)GameplayFOVScanResult - (uintptr_t)baseModule);

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

DWORD __stdcall Main(void*)
{
    Logging();
    ReadConfig();
    Sleep(iInjectionDelay);
    ResolutionFix();
    UIFix();
    FOVFix();
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

