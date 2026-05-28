////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////       _______..___________.     ___      .______      .___________..______    __    __  .___________..___________.  ______   .__   __. .______     ______        _______. __  .___________. __    ______   .__   __.   ////
////      /       ||           |    /   \     |   _  \     |           ||   _  \  |  |  |  | |           ||           | /  __  \  |  \ |  | |   _  \   /  __  \      /       ||  | |           ||  |  /  __  \  |  \ |  |   ////
////     |   (----``---|  |----`   /  ^  \    |  |_)  |    `---|  |----`|  |_)  | |  |  |  | `---|  |----``---|  |----`|  |  |  | |   \|  | |  |_)  | |  |  |  |    |   (----`|  | `---|  |----`|  | |  |  |  | |   \|  |   ////
////      \   \        |  |       /  /_\  \   |      /         |  |     |   _  <  |  |  |  |     |  |         |  |     |  |  |  | |  . `  | |   ___/  |  |  |  |     \   \    |  |     |  |     |  | |  |  |  | |  . `  |   ////
////  .----)   |       |  |      /  _____  \  |  |\  \----.    |  |     |  |_)  | |  `--'  |     |  |         |  |     |  `--'  | |  |\   | |  |      |  `--'  | .----)   |   |  |     |  |     |  | |  `--'  | |  |\   |   ////
////  |_______/        |__|     /__/     \__\ | _| `._____|    |__|     |______/   \______/      |__|         |__|      \______/  |__| \__| | _|       \______/  |_______/    |__|     |__|     |__|  \______/  |__| \__|   ////
////                                                                                                                                                                                                                        ////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ApplyStyle(FrameworkElement const& element, std::wstring monitorName);
bool InitializeDebounce();
DispatcherTimer debounceTimer{nullptr};
#include <windhawk_utils.h>
#include <atomic>
#include <functional>
#include <string>
#include <dwmapi.h>
#include <roapi.h>
#include <winstring.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/base.h>
using namespace winrt::Windows::UI::Xaml;
struct {
    bool startMenuOnTheLeft;
;bool MoveFlyoutNotificationCenter=true;} g_settings_startbuttonposition;
enum class Target {
    Explorer,
    StartMenuExperienceHost,
};
Target g_target;
std::atomic<bool> g_taskbarViewDllLoadedStartButtonPosition;
thread_local bool g_TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride;
HWND g_searchMenuWnd;
int g_searchMenuOriginalX;
STDAPI GetDpiForMonitor(HMONITOR hmonitor,
                        MONITOR_DPI_TYPE dpiType,
                        UINT* dpiX,
                        UINT* dpiY);
void* CTaskBand_ITaskListWndSite_vftable;
void* CSecondaryTaskBand_ITaskListWndSite_vftable;
using CTaskBand_GetTaskbarHost_t = void*(WINAPI*)(void* pThis, void** result);
CTaskBand_GetTaskbarHost_t CTaskBand_GetTaskbarHost_Original;
void* TaskbarHost_FrameHeight_Original;
using CSecondaryTaskBand_GetTaskbarHost_t = void*(WINAPI*)(void* pThis,
                                                           void** result);
CSecondaryTaskBand_GetTaskbarHost_t CSecondaryTaskBand_GetTaskbarHost_Original;
using std__Ref_count_base__Decref_t = void(WINAPI*)(void* pThis);
std__Ref_count_base__Decref_t std__Ref_count_base__Decref_Original;
XamlRoot XamlRootFromTaskbarHostSharedPtr(void* taskbarHostSharedPtr[2]) {
    if (!taskbarHostSharedPtr[0] && !taskbarHostSharedPtr[1]) {
        return nullptr;
    }
    size_t taskbarElementIUnknownOffset = 0x48;
#if defined(_M_X64)
    {
        const BYTE* b = (const BYTE*)TaskbarHost_FrameHeight_Original;
        if (b[0] == 0x48 && b[1] == 0x83 && b[2] == 0xEC && b[4] == 0x48 &&
            b[5] == 0x83 && b[6] == 0xC1 && b[7] <= 0x7F) {
            taskbarElementIUnknownOffset = b[7];
        } else {
            Wh_Log(L"Unsupported TaskbarHost::FrameHeight");
        }
    }
#elif defined(_M_ARM64)
#else
#error "Unsupported architecture"
#endif
    auto* taskbarElementIUnknown =
        *(IUnknown**)((BYTE*)taskbarHostSharedPtr[0] +
                      taskbarElementIUnknownOffset);
    FrameworkElement taskbarElement = nullptr;
    taskbarElementIUnknown->QueryInterface(winrt::guid_of<FrameworkElement>(),
                                           winrt::put_abi(taskbarElement));
    auto result = taskbarElement ? taskbarElement.XamlRoot() : nullptr;
    std__Ref_count_base__Decref_Original(taskbarHostSharedPtr[1]);
    return result;
}
XamlRoot GetTaskbarXamlRoot(HWND hTaskbarWnd) {
    HWND hTaskSwWnd = (HWND)GetProp(hTaskbarWnd, L"TaskbandHWND");
    if (!hTaskSwWnd) {
        return nullptr;
    }
    void* taskBand = (void*)GetWindowLongPtr(hTaskSwWnd, 0);
    void* taskBandForTaskListWndSite = taskBand;
    for (int i = 0; *(void**)taskBandForTaskListWndSite !=
                    CTaskBand_ITaskListWndSite_vftable;
         i++) {
        if (i == 20) {
            return nullptr;
        }
        taskBandForTaskListWndSite = (void**)taskBandForTaskListWndSite + 1;
    }
    void* taskbarHostSharedPtr[2]{};
    CTaskBand_GetTaskbarHost_Original(taskBandForTaskListWndSite,
                                      taskbarHostSharedPtr);
    return XamlRootFromTaskbarHostSharedPtr(taskbarHostSharedPtr);
}
XamlRoot GetSecondaryTaskbarXamlRoot(HWND hSecondaryTaskbarWnd) {
    HWND hTaskSwWnd =
        (HWND)FindWindowEx(hSecondaryTaskbarWnd, nullptr, L"WorkerW", nullptr);
    if (!hTaskSwWnd) {
        return nullptr;
    }
    void* taskBand = (void*)GetWindowLongPtr(hTaskSwWnd, 0);
    void* taskBandForTaskListWndSite = taskBand;
    for (int i = 0; *(void**)taskBandForTaskListWndSite !=
                    CSecondaryTaskBand_ITaskListWndSite_vftable;
         i++) {
        if (i == 20) {
            return nullptr;
        }
        taskBandForTaskListWndSite = (void**)taskBandForTaskListWndSite + 1;
    }
    void* taskbarHostSharedPtr[2]{};
    CSecondaryTaskBand_GetTaskbarHost_Original(taskBandForTaskListWndSite,
                                               taskbarHostSharedPtr);
    return XamlRootFromTaskbarHostSharedPtr(taskbarHostSharedPtr);
}
using RunFromWindowThreadProc_t = void(WINAPI*)(void* parameter);
bool RunFromWindowThread(HWND hWnd,
                         RunFromWindowThreadProc_t proc,
                         void* procParam) {
    static const UINT runFromWindowThreadRegisteredMsg =
        RegisterWindowMessage(L"Windhawk_RunFromWindowThread_" WH_MOD_ID);
    struct RUN_FROM_WINDOW_THREAD_PARAM {
        RunFromWindowThreadProc_t proc;
        void* procParam;
    };
    DWORD dwThreadId = GetWindowThreadProcessId(hWnd, nullptr);
    if (dwThreadId == 0) {
        return false;
    }
    if (dwThreadId == GetCurrentThreadId()) {
        proc(procParam);
        return true;
    }
    HHOOK hook = SetWindowsHookEx(
        WH_CALLWNDPROC,
        [](int nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
            if (nCode == HC_ACTION) {
                const CWPSTRUCT* cwp = (const CWPSTRUCT*)lParam;
                if (cwp->message == runFromWindowThreadRegisteredMsg) {
                    RUN_FROM_WINDOW_THREAD_PARAM* param =
                        (RUN_FROM_WINDOW_THREAD_PARAM*)cwp->lParam;
                    param->proc(param->procParam);
                }
            }
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        },
        nullptr, dwThreadId);
    if (!hook) {
        return false;
    }
    RUN_FROM_WINDOW_THREAD_PARAM param;
    param.proc = proc;
    param.procParam = procParam;
    SendMessage(hWnd, runFromWindowThreadRegisteredMsg, 0, (LPARAM)&param);
    UnhookWindowsHookEx(hook);
    return true;
}
void ApplySettingsFromTaskbarThread() {
    Wh_Log(L"Applying settings");
    EnumThreadWindows(
        GetCurrentThreadId(),
        [](HWND hWnd, LPARAM lParam) -> BOOL {
            WCHAR szClassName[32];
            if (GetClassName(hWnd, szClassName, ARRAYSIZE(szClassName)) == 0) {
                return TRUE;
            }
            XamlRoot xamlRoot = nullptr;
            if (_wcsicmp(szClassName, L"Shell_TrayWnd") == 0) {
                xamlRoot = GetTaskbarXamlRoot(hWnd);
            } else if (_wcsicmp(szClassName, L"Shell_SecondaryTrayWnd") == 0) {
                xamlRoot = GetSecondaryTaskbarXamlRoot(hWnd);
            } else {
                return TRUE;
            }
            if (!xamlRoot) {g_already_requested_debounce_initializing=false;
                Wh_Log(L"Getting XamlRoot failed");
                return TRUE;
            }
const auto xamlRootContent = xamlRoot.Content().try_as<FrameworkElement>();
if (!xamlRootContent) {
g_already_requested_debounce_initializing=false;
return TRUE;
}
if (!debounceTimer) {
     xamlRootContent.Dispatcher().TryRunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::High, [xamlRootContent]() {
     InitializeDebounce();
  });
  return TRUE;
}
if (xamlRootContent && xamlRootContent.Dispatcher()) {
std::wstring monitorName = GetMonitorName(hWnd);
  xamlRootContent.Dispatcher().TryRunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::High, [xamlRootContent,monitorName]() {
    if (!ApplyStyle(xamlRootContent,monitorName)) {
      Wh_Log(L"ApplyStyles failed");
    }
  });
  return TRUE;
}
            return TRUE;
        },
        0);
}
void ApplySettingsStartButtonPosition(HWND hTaskbarWnd) {
    RunFromWindowThread(
        hTaskbarWnd, [](void* pParam) { ApplySettingsFromTaskbarThread(); }, 0);
}
using IUIElement_Arrange_t =
    HRESULT(WINAPI*)(void* pThis, winrt::Windows::Foundation::Rect rect);
IUIElement_Arrange_t IUIElement_Arrange_Original;
HRESULT WINAPI IUIElement_Arrange_Hook_StartButtonPosition(void* pThis,
                                       winrt::Windows::Foundation::Rect rect) {
    auto original = [=] { return IUIElement_Arrange_Original(pThis, rect); };
    if (!g_TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride || g_unloading) {
        return original();
    }
    FrameworkElement element = nullptr;
    ((IUnknown*)pThis)
        ->QueryInterface(winrt::guid_of<FrameworkElement>(),
                         winrt::put_abi(element));
    if (!element) {
        return original();
    }
    auto className = winrt::get_class_name(element);
    if (className != L"Taskbar.ExperienceToggleButton") {
        return original();
    }
    auto automationId =
        Automation::AutomationProperties::GetAutomationId(element);
    if (automationId != L"StartButton") {
        return original();
    }
    auto taskbarFrameRepeater =
        Media::VisualTreeHelper::GetParent(element).as<FrameworkElement>();
    auto widgetElement =
        EnumChildElements(taskbarFrameRepeater, [](FrameworkElement child) {
            auto childClassName = winrt::get_class_name(child);
            if (childClassName != L"Taskbar.AugmentedEntryPointButton") {
                return false;
            }
            if (child.Name() != L"AugmentedEntryPointButton") {
                return false;
            }
            auto margin = child.Margin();
            auto offset = child.ActualOffset();
            if (offset.x != margin.Left || offset.y != 0) {
                return false;
            }
            return true;
        });
    if (!widgetElement) {
        element.Dispatcher().TryRunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::High,[element]() {
                double width = element.ActualWidth();
                double minX = std::numeric_limits<double>::infinity();
                auto taskbarFrameRepeater =
                    Media::VisualTreeHelper::GetParent(element)
                        .as<FrameworkElement>();
                EnumChildElements(taskbarFrameRepeater,
                                  [&element, &minX](FrameworkElement child) {
                                      if (child == element) {
                                          return false;
                                      }
                                      auto offset = child.ActualOffset();
                                      if (offset.x >= 0 && offset.x < minX) {
                                          minX = offset.x;
                                      }
                                      return false;
                                  });
                if (minX < width) {
                    Thickness margin = element.Margin();
                    element.Margin(margin);
                } else if (minX > width * 2) {
                    Thickness margin = element.Margin();
                    element.Margin(margin);
                }
            });
    }
    winrt::Windows::Foundation::Rect newRect = rect;
    newRect.X = 0;
    return original();
}
using TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride_t =
    HRESULT(WINAPI*)(void* pThis,
                     void* context,
                     winrt::Windows::Foundation::Size size,
                     winrt::Windows::Foundation::Size* resultSize);
TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride_t
    TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride_Original;
HRESULT WINAPI TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride_Hook(
    void* pThis,
    void* context,
    winrt::Windows::Foundation::Size size,
    winrt::Windows::Foundation::Size* resultSize) {
    [[maybe_unused]] static bool hooked = [] {
        Shapes::Rectangle rectangle;
        IUIElement element = rectangle;
        void** vtable = *(void***)winrt::get_abi(element);
        auto arrange = (IUIElement_Arrange_t)vtable[92];
        WindhawkUtils::SetFunctionHook(arrange, IUIElement_Arrange_Hook_StartButtonPosition,
                                       &IUIElement_Arrange_Original);
        Wh_ApplyHookOperations();
        return true;
    }();
    g_TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride = true;
    HRESULT ret = TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride_Original(
        pThis, context, size, resultSize);
    g_TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride = false;
    return ret;
}
using ExperienceToggleButton_UpdateButtonPadding_t = void(WINAPI*)(void* pThis);
void WINAPI ExperienceToggleButton_UpdateButtonPadding_Hook_StartButtonPosition(void* pThis) {
    ExperienceToggleButton_UpdateButtonPadding_Original(pThis);
    return;  // fork: skip upstream's start-button panel mutations
    if (g_unloading) {
        return;
    }
    FrameworkElement toggleButtonElement = nullptr;
    ((IUnknown**)pThis)[1]->QueryInterface(winrt::guid_of<FrameworkElement>(),
                                           winrt::put_abi(toggleButtonElement));
    if (!toggleButtonElement) {
        return;
    }
    auto panelElement =
        FindChildByName(toggleButtonElement, L"ExperienceToggleButtonRootPanel")
            .try_as<Controls::Grid>();
    if (!panelElement) {
        return;
    }
    auto className = winrt::get_class_name(toggleButtonElement);
    if (className == L"Taskbar.ExperienceToggleButton") {
        auto automationId = Automation::AutomationProperties::GetAutomationId(
            toggleButtonElement);
        if (automationId == L"StartButton") {
            if (panelElement.Width() == 45) {
                panelElement.Width(55);
            }
            if (panelElement.Padding() == Thickness{2, 4, 2, 4}) {
                panelElement.Padding(Thickness{12, 4, 2, 4});
            }
        }
    }
}
using AugmentedEntryPointButton_UpdateButtonPadding_t =
    void(WINAPI*)(void* pThis);
void WINAPI AugmentedEntryPointButton_UpdateButtonPadding_Hook_StartButtonPosition(void* pThis) {
    AugmentedEntryPointButton_UpdateButtonPadding_Original(pThis);
    if (g_unloading) {
        return;
    }
    FrameworkElement button = nullptr;
    ((IUnknown**)pThis)[1]->QueryInterface(winrt::guid_of<FrameworkElement>(),
                                           winrt::put_abi(button));
    if (!button) {
        return;
    }
    button.Dispatcher().TryRunAsync(
        winrt::Windows::UI::Core::CoreDispatcherPriority::High, [button]() {
            auto offset = button.ActualOffset();
            if (offset.x != 0 || offset.y != 0) {
                return;
            }
            auto margin = button.Margin();
            margin.Left = 44;
            button.Margin(margin);
        });
}
using TrayUI__Hide_t = void(WINAPI*)(void* pThis);
TrayUI__Hide_t TrayUI__Hide_Original;
void WINAPI TrayUI__Hide_Hook(void* pThis) {
  TrayUI__Hide_Original(pThis);
  ApplySettingsFromTaskbarThreadIfRequired();
}
using CSecondaryTray__AutoHide_t = void(WINAPI*)(void* pThis, bool param1);
CSecondaryTray__AutoHide_t CSecondaryTray__AutoHide_Original;
void WINAPI CSecondaryTray__AutoHide_Hook(void* pThis, bool param1) {
  CSecondaryTray__AutoHide_Original(pThis, param1);
  ApplySettingsFromTaskbarThreadIfRequired();
}
using TrayUI_WndProc_t = LRESULT(WINAPI*)(void* pThis, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, bool* flag);
TrayUI_WndProc_t TrayUI_WndProc_Original;
LRESULT WINAPI TrayUI_WndProc_Hook(void* pThis, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, bool* flag) {
  ApplySettingsFromTaskbarThreadIfRequired();
  return TrayUI_WndProc_Original(pThis, hWnd, Msg, wParam, lParam, flag);
}
using CSecondaryTray_v_WndProc_t = LRESULT(WINAPI*)(void* pThis, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
CSecondaryTray_v_WndProc_t CSecondaryTray_v_WndProc_Original;
LRESULT WINAPI CSecondaryTray_v_WndProc_Hook(void* pThis, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
  ApplySettingsFromTaskbarThreadIfRequired();
  return CSecondaryTray_v_WndProc_Original(pThis, hWnd, Msg, wParam, lParam);
}
using CTaskBand__ProcessWindowDestroyed_t = void(WINAPI*)(void* pThis, void* pHwnd);
CTaskBand__ProcessWindowDestroyed_t CTaskBand__ProcessWindowDestroyed_Original;
void WINAPI CTaskBand__ProcessWindowDestroyed_Hook(void* pThis, void* pHwnd) {
  Wh_Log(L"CTaskBand::CTaskBand__ProcessWindowDestroyed_Hook Hook");
  CTaskBand__ProcessWindowDestroyed_Original(pThis, pHwnd);
  ApplySettingsFromTaskbarThreadIfRequired();
}
using CTaskBand__InsertItem_t = long(WINAPI*)(void* pThis, void* pHwnd, void** ppTaskItem, void* pHwnd1, void* pHwnd2);
CTaskBand__InsertItem_t CTaskBand__InsertItem_Original;
long WINAPI CTaskBand__InsertItem_Hook(void* pThis, void* pHwnd, void** ppTaskItem, void* pHwnd1, void* pHwnd2) {
  Wh_Log(L"CTaskBand::_InsertItem Hook");
  auto original_call = CTaskBand__InsertItem_Original(pThis, pHwnd, ppTaskItem, pHwnd1, pHwnd2);
  ApplySettingsFromTaskbarThreadIfRequired();
  return original_call;
}
using CTaskBand__UpdateAllIcons_t = void(WINAPI*)(void* pThis);
CTaskBand__UpdateAllIcons_t CTaskBand__UpdateAllIcons_Original;
void WINAPI CTaskBand__UpdateAllIcons_Hook(void* pThis) {
  Wh_Log(L"CTaskBand::_UpdateAllIcons Hook");
  CTaskBand__UpdateAllIcons_Original(pThis);
  ApplySettingsFromTaskbarThreadIfRequired();
}
using CTaskBand__TaskOrderChanged_t = void(WINAPI*)(void* pThis, void* pTaskGroup, int param);
CTaskBand__TaskOrderChanged_t CTaskBand__TaskOrderChanged_Original;
void WINAPI CTaskBand__TaskOrderChanged_Hook(void* pThis, void* pTaskGroup, int param) {
  Wh_Log(L"CTaskBand::TaskOrderChanged Hook");
  CTaskBand__TaskOrderChanged_Original(pThis, pTaskGroup, param);
  ApplySettingsFromTaskbarThreadIfRequired();
}
using CImpWndProc__WndProc_t = __int64(WINAPI*)(void* pThis, void* pHwnd, unsigned int msg, unsigned __int64 wParam, __int64 lParam);
CImpWndProc__WndProc_t CImpWndProc__WndProc_Original;
__int64 WINAPI CImpWndProc__WndProc_Hook(void* pThis, void* pHwnd, unsigned int msg, unsigned __int64 wParam, __int64 lParam) {
  ApplySettingsFromTaskbarThreadIfRequired();
  return CImpWndProc__WndProc_Original(pThis, pHwnd, msg, wParam, lParam);
}
using CTaskBand__WndProc_t = __int64(WINAPI*)(void* pThis, void* pHwnd, unsigned int msg, unsigned __int64 wParam, __int64 lParam);
CTaskBand__WndProc_t CTaskBand__WndProc_Original;
__int64 WINAPI CTaskBand__WndProc_Hook(void* pThis, void* pHwnd, unsigned int msg, unsigned __int64 wParam, __int64 lParam) {
  ApplySettingsFromTaskbarThreadIfRequired();
  return CTaskBand__WndProc_Original(pThis, pHwnd, msg, wParam, lParam);
}
using CTaskListWnd__WndProc_t = __int64(WINAPI*)(void* pThis, void* pHwnd, unsigned int msg, unsigned __int64 wParam, __int64 lParam);
CTaskListWnd__WndProc_t CTaskListWnd__WndProc_Original;
__int64 WINAPI CTaskListWnd__WndProc_Hook(void* pThis, void* pHwnd, unsigned int msg, unsigned __int64 wParam, __int64 lParam) {
  ApplySettingsFromTaskbarThreadIfRequired();
  return CTaskListWnd__WndProc_Original(pThis, pHwnd, msg, wParam, lParam);
}
using CSecondaryTaskBand__WndProc_t = __int64(WINAPI*)(void* pThis, void* pHwnd, unsigned int msg, unsigned __int64 wParam, __int64 lParam);
CSecondaryTaskBand__WndProc_t CSecondaryTaskBand__WndProc_Original;
__int64 WINAPI CSecondaryTaskBand__WndProc_Hook(void* pThis, void* pHwnd, unsigned int msg, unsigned __int64 wParam, __int64 lParam) {
  ApplySettingsFromTaskbarThreadIfRequired();
  return CSecondaryTaskBand__WndProc_Original(pThis, pHwnd, msg, wParam, lParam);
}
using CTraySearchControl__WndProc_t = __int64(WINAPI*)(void* pThis, void* pHwnd, unsigned int msg, unsigned __int64 wParam, __int64 lParam);
CTraySearchControl__WndProc_t CTraySearchControl__WndProc_Original;
__int64 WINAPI CTraySearchControl__WndProc_Hook(void* pThis, void* pHwnd, unsigned int msg, unsigned __int64 wParam, __int64 lParam) {
  ApplySettingsFromTaskbarThreadIfRequired();
  return CTraySearchControl__WndProc_Original(pThis, pHwnd, msg, wParam, lParam);
}
interface ITaskGroup;
interface ITaskItem;
using CTaskBand__UpdateItemIcon_WithArgs_t = void(WINAPI*)(void* pThis, ITaskGroup* param1, ITaskItem* param2);
CTaskBand__UpdateItemIcon_WithArgs_t CTaskBand__UpdateItemIcon_WithArgs_Original;
void WINAPI CTaskBand__UpdateItemIcon_WithArgs_Hook(void* pThis, ITaskGroup* param1, ITaskItem* param2) {
  Wh_Log(L"Method called: CTaskBand__UpdateItemIcon");
  CTaskBand__UpdateItemIcon_WithArgs_Original(pThis, param1, param2);
  ApplySettingsFromTaskbarThreadIfRequired();
}
using CTaskBand_RemoveIcon_WithArgs_t = void(WINAPI*)(void* pThis, ITaskItem* param1);
CTaskBand_RemoveIcon_WithArgs_t CTaskBand_RemoveIcon_WithArgs_Original;
void WINAPI CTaskBand_RemoveIcon_WithArgs_Hook(void* pThis, ITaskItem* param1) {
  Wh_Log(L"Method called: CTaskBand_RemoveIcon");
  CTaskBand_RemoveIcon_WithArgs_Original(pThis, param1);
  ApplySettingsFromTaskbarThreadIfRequired();
}
using ITaskbarSettings_get_Alignment_t = HRESULT(WINAPI*)(void* pThis, int* alignment);
ITaskbarSettings_get_Alignment_t ITaskbarSettings_get_Alignment_Original;
HRESULT WINAPI ITaskbarSettings_get_Alignment_Hook(void* pThis, int* alignment) {
  HRESULT ret = ITaskbarSettings_get_Alignment_Original(pThis, alignment);
  Wh_Log(L"Method called: ITaskbarSettings_get_Alignment_Hook alignment: %d", *alignment);
  if (SUCCEEDED(ret)) {
    *alignment = 1;
  }
  return ret;
}
#include <windowsx.h>
using CTaskListWnd_ComputeJumpViewPosition_t = HRESULT(WINAPI*)(void* pThis, void* taskBtnGroup, int param2, winrt::Windows::Foundation::Point* point, HorizontalAlignment* horizontalAlignment, VerticalAlignment* verticalAlignment);
CTaskListWnd_ComputeJumpViewPosition_t CTaskListWnd_ComputeJumpViewPosition_Original;
HRESULT WINAPI CTaskListWnd_ComputeJumpViewPosition_Hook(void* pThis, void* taskBtnGroup, int param2, winrt::Windows::Foundation::Point* point, HorizontalAlignment* horizontalAlignment, VerticalAlignment* verticalAlignment) {
  HRESULT ret = CTaskListWnd_ComputeJumpViewPosition_Original(pThis, taskBtnGroup, param2, point, horizontalAlignment, verticalAlignment);
  DWORD messagePos = GetMessagePos();
  POINT pt{
      GET_X_LPARAM(messagePos),
      GET_Y_LPARAM(messagePos),
  };
  point->X = pt.x;
  return ret;
}
using TrayUI__OnDPIChanged_WithoutArgs_t = void(WINAPI*)(void* pThis);
TrayUI__OnDPIChanged_WithoutArgs_t TrayUI__OnDPIChanged_WithoutArgs_Original;
void WINAPI TrayUI__OnDPIChanged_WithoutArgs_Hook(void* pThis) {
               TrayUI__OnDPIChanged_WithoutArgs_Original(pThis);
               g_invalidateDimensions = true;
            }
bool HookTaskbarDllSymbolsStartButtonPosition() {
    HMODULE module =
        LoadLibraryEx(L"taskbar.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) {
        Wh_Log(L"Failed to load taskbar.dll");
        return false;
    }
    WindhawkUtils::SYMBOL_HOOK taskbarDllHooks[] = {{{LR"(public: virtual void __cdecl CTaskBand::RemoveIcon(struct ITaskItem *))"}, &CTaskBand_RemoveIcon_WithArgs_Original, CTaskBand_RemoveIcon_WithArgs_Hook},
    {{LR"(protected: void __cdecl CTaskBand::_UpdateItemIcon(struct ITaskGroup *,struct ITaskItem *))"}, &CTaskBand__UpdateItemIcon_WithArgs_Original, CTaskBand__UpdateItemIcon_WithArgs_Hook},
    {
        {LR"(protected: static __int64 __cdecl CImpWndProc::s_WndProc(struct HWND__ *,unsigned int,unsigned __int64,__int64))"},
        &CImpWndProc__WndProc_Original,
        CImpWndProc__WndProc_Hook,
    },
    {
        {LR"(protected: static __int64 __cdecl CTraySearchControl::s_WndProc(struct HWND__ *,unsigned int,unsigned __int64,__int64))"},
        &CTraySearchControl__WndProc_Original,
        CTraySearchControl__WndProc_Hook,
    },
    {
        {LR"(private: virtual __int64 __cdecl CSecondaryTaskBand::v_WndProc(struct HWND__ *,unsigned int,unsigned __int64,__int64))"},
        &CSecondaryTaskBand__WndProc_Original,
        CSecondaryTaskBand__WndProc_Hook,
    },
    {
        {LR"(protected: virtual __int64 __cdecl CTaskBand::v_WndProc(struct HWND__ *,unsigned int,unsigned __int64,__int64))"},
        &CTaskBand__WndProc_Original,
        CTaskBand__WndProc_Hook,
    },
    {
        {LR"(protected: virtual __int64 __cdecl CTaskListWnd::v_WndProc(struct HWND__ *,unsigned int,unsigned __int64,__int64))"},
        &CTaskListWnd__WndProc_Original,
        CTaskListWnd__WndProc_Hook,
    },
    /////////////////////////////////////
    {
        {LR"(protected: long __cdecl CTaskBand::_InsertItem(struct HWND__ *,struct ITaskItem * *,struct HWND__ *,struct HWND__ *))"},
        &CTaskBand__InsertItem_Original,
        CTaskBand__InsertItem_Hook,
    },
    {
        {LR"(protected: void __cdecl CTaskBand::_UpdateAllIcons(void))"},
        &CTaskBand__UpdateAllIcons_Original,
        CTaskBand__UpdateAllIcons_Hook,
    },
    {
        {LR"(public: virtual void __cdecl CTaskBand::TaskOrderChanged(struct ITaskGroup *,int))"},
        &CTaskBand__TaskOrderChanged_Original,
        CTaskBand__TaskOrderChanged_Hook,
    },
    {
        {LR"(protected: void __cdecl CTaskBand::_ProcessWindowDestroyed(struct HWND__ *))"},
        &CTaskBand__ProcessWindowDestroyed_Original,
        CTaskBand__ProcessWindowDestroyed_Hook,
    },
    {
        {LR"(public: void __cdecl TrayUI::_Hide(void))"},
        &TrayUI__Hide_Original,
        TrayUI__Hide_Hook,
    },
    {
        {LR"(private: void __cdecl CSecondaryTray::_AutoHide(bool))"},
        &CSecondaryTray__AutoHide_Original,
        CSecondaryTray__AutoHide_Hook,
    },
    {
        {LR"(public: virtual __int64 __cdecl TrayUI::WndProc(struct HWND__ *,unsigned int,unsigned __int64,__int64,bool *))"},
        &TrayUI_WndProc_Original,
        TrayUI_WndProc_Hook,
    },
    {
        {LR"(private: virtual __int64 __cdecl CSecondaryTray::v_WndProc(struct HWND__ *,unsigned int,unsigned __int64,__int64))"},
        &CSecondaryTray_v_WndProc_Original,
        CSecondaryTray_v_WndProc_Hook,
    },
    {
        {LR"(public: virtual int __cdecl winrt::impl::produce<struct winrt::WindowsUdk::UI::Shell::implementation::TaskbarSettings,struct winrt::WindowsUdk::UI::Shell::ITaskbarSettings>::get_Alignment(int *))"},
        &ITaskbarSettings_get_Alignment_Original,
        ITaskbarSettings_get_Alignment_Hook,
    },
    {
        {LR"(protected: long __cdecl CTaskListWnd::_ComputeJumpViewPosition(struct ITaskBtnGroup *,int,struct Windows::Foundation::Point &,enum Windows::UI::Xaml::HorizontalAlignment &,enum Windows::UI::Xaml::VerticalAlignment &)const )"},
        &CTaskListWnd_ComputeJumpViewPosition_Original,
        CTaskListWnd_ComputeJumpViewPosition_Hook,
    },
    {{LR"(public: void __cdecl TrayUI::_OnDPIChanged(void))"}, &TrayUI__OnDPIChanged_WithoutArgs_Original, TrayUI__OnDPIChanged_WithoutArgs_Hook},
        {
            {LR"(const CTaskBand::`vftable'{for `ITaskListWndSite'})"},
            &CTaskBand_ITaskListWndSite_vftable,
        },
        {
            {LR"(const CSecondaryTaskBand::`vftable'{for `ITaskListWndSite'})"},
            &CSecondaryTaskBand_ITaskListWndSite_vftable,
        },
        {
            {LR"(public: virtual class std::shared_ptr<class TaskbarHost> __cdecl CTaskBand::GetTaskbarHost(void)const )"},
            &CTaskBand_GetTaskbarHost_Original,
        },
        {
            {LR"(public: int __cdecl TaskbarHost::FrameHeight(void)const )"},
            &TaskbarHost_FrameHeight_Original,
        },
        {
            {LR"(public: virtual class std::shared_ptr<class TaskbarHost> __cdecl CSecondaryTaskBand::GetTaskbarHost(void)const )"},
            &CSecondaryTaskBand_GetTaskbarHost_Original,
        },
        {
            {LR"(public: void __cdecl std::_Ref_count_base::_Decref(void))"},
            &std__Ref_count_base__Decref_Original,
        },
    };
    return HookSymbols(module, taskbarDllHooks, ARRAYSIZE(taskbarDllHooks));
}
using TaskbarTelemetry_StartItemEntranceAnimation_t = void(WINAPI*)(const bool&);
static TaskbarTelemetry_StartItemEntranceAnimation_t orig_StartItemEntranceAnimation = nullptr;
using TaskbarTelemetry_StartItemPlateEntranceAnimation_t = void(WINAPI*)(const bool&);
static TaskbarTelemetry_StartItemPlateEntranceAnimation_t orig_StartItemPlateEntranceAnimation = nullptr;
void WINAPI Hook_StartItemEntranceAnimation_call(const bool& b) {
  Wh_Log(L"[Hook] TaskbarTelemetry::StartItemEntranceAnimation(%d)", b);
  orig_StartItemEntranceAnimation(b);
  ApplySettingsDebounced(50);
}
void WINAPI Hook_StartItemPlateEntranceAnimation_call(const bool& b) {
  Wh_Log(L"[Hook] TaskbarTelemetry::StartItemPlateEntranceAnimation(%d)", b);
  orig_StartItemPlateEntranceAnimation(b);
  ApplySettingsDebounced(50);
}
using TaskbarTelemetry_StartEntranceAnimationCompleted_WithoutArgs_t = void(WINAPI*)(void* pThis);
TaskbarTelemetry_StartEntranceAnimationCompleted_WithoutArgs_t TaskbarTelemetry_StartEntranceAnimationCompleted_WithoutArgs_Original;
static void WINAPI TaskbarTelemetry_StartEntranceAnimationCompleted_WithoutArgs_Hook(void* pThis) {
  Wh_Log(L"Method called: TaskbarTelemetry_StartEntranceAnimationCompleted");
  TaskbarTelemetry_StartEntranceAnimationCompleted_WithoutArgs_Original(pThis);
  ApplySettingsDebounced(300);
  return;
}
using TaskbarTelemetry_StartHideAnimationCompleted_WithoutArgs_t = void(WINAPI*)(void* pThis);
TaskbarTelemetry_StartHideAnimationCompleted_WithoutArgs_t TaskbarTelemetry_StartHideAnimationCompleted_WithoutArgs_Original;
static void WINAPI TaskbarTelemetry_StartHideAnimationCompleted_WithoutArgs_Hook(void* pThis) {
  TaskbarTelemetry_StartHideAnimationCompleted_WithoutArgs_Original(pThis);
  Wh_Log(L"Method called: TaskbarTelemetry_StartHideAnimationCompleted");
  ApplySettingsDebounced(300);
  return;
}
bool HookTaskbarViewDllSymbolsStartButtonPosition(HMODULE module) {
    WindhawkUtils::SYMBOL_HOOK symbolHooks[] = {{{LR"(public: static void __cdecl TaskbarTelemetry::StartItemEntranceAnimation<bool const &>(bool const &))"}, &orig_StartItemEntranceAnimation, Hook_StartItemEntranceAnimation_call},
    {{LR"(public: static void __cdecl TaskbarTelemetry::StartItemPlateEntranceAnimation<bool const &>(bool const &))"}, &orig_StartItemPlateEntranceAnimation, Hook_StartItemPlateEntranceAnimation_call},
    {{LR"(public: static void __cdecl TaskbarTelemetry::StartHideAnimationCompleted(void))"}, &TaskbarTelemetry_StartHideAnimationCompleted_WithoutArgs_Original, TaskbarTelemetry_StartHideAnimationCompleted_WithoutArgs_Hook},
    {{LR"(public: static void __cdecl TaskbarTelemetry::StartEntranceAnimationCompleted(void))"}, &TaskbarTelemetry_StartEntranceAnimationCompleted_WithoutArgs_Original, TaskbarTelemetry_StartEntranceAnimationCompleted_WithoutArgs_Hook},
        {
            {LR"(public: virtual int __cdecl winrt::impl::produce<struct winrt::Taskbar::implementation::TaskbarCollapsibleLayout,struct winrt::Microsoft::UI::Xaml::Controls::IVirtualizingLayoutOverrides>::ArrangeOverride(void *,struct winrt::Windows::Foundation::Size,struct winrt::Windows::Foundation::Size *))"},
            &TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride_Original,
            TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride_Hook,
        },
        {
            {LR"(protected: virtual void __cdecl winrt::Taskbar::implementation::ExperienceToggleButton::UpdateButtonPadding(void))"},
            &ExperienceToggleButton_UpdateButtonPadding_Original,
            ExperienceToggleButton_UpdateButtonPadding_Hook_StartButtonPosition,
        },
        {
            {LR"(protected: virtual void __cdecl winrt::Taskbar::implementation::AugmentedEntryPointButton::UpdateButtonPadding(void))"},
            &AugmentedEntryPointButton_UpdateButtonPadding_Original,
            AugmentedEntryPointButton_UpdateButtonPadding_Hook_StartButtonPosition,
        },
    };
    return HookSymbols(module, symbolHooks, ARRAYSIZE(symbolHooks));
}
void HandleLoadedModuleIfTaskbarView(HMODULE module, LPCWSTR lpLibFileName) {
    if (!g_taskbarViewDllLoadedStartButtonPosition && GetTaskbarViewModuleHandle() == module &&
        !g_taskbarViewDllLoadedStartButtonPosition.exchange(true)) {
        Wh_Log(L"Loaded %s", lpLibFileName);
        if (HookTaskbarViewDllSymbolsStartButtonPosition(module)) {
            Wh_ApplyHookOperations();
        }
    }
}
using LoadLibraryExW_t = decltype(&LoadLibraryExW);
HMODULE WINAPI LoadLibraryExW_Hook_StartButtonPosition(LPCWSTR lpLibFileName,
                                   HANDLE hFile,
                                   DWORD dwFlags) {
    HMODULE module = LoadLibraryExW_Original(lpLibFileName, hFile, dwFlags);
    if (module) {
        HandleLoadedModuleIfTaskbarView(module, lpLibFileName);
    }
    return module;
}
std::wstring GetProcessFileName(DWORD dwProcessId) {
    HANDLE hProcess =
        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwProcessId);
    if (!hProcess) {
        return std::wstring{};
    }
    WCHAR processPath[MAX_PATH];
    DWORD dwSize = ARRAYSIZE(processPath);
    if (!QueryFullProcessImageName(hProcess, 0, processPath, &dwSize)) {
        CloseHandle(hProcess);
        return std::wstring{};
    }
    CloseHandle(hProcess);
    PCWSTR processFileNameUpper = wcsrchr(processPath, L'\\');
    if (!processFileNameUpper) {
        return std::wstring{};
    }
    processFileNameUpper++;
    return processFileNameUpper;
}
//
// Replaces the upstream taskbar-start-button-position.wh.cpp hook with a
// unified per-monitor popup-placement design that handles Start Menu,
// Search, and Notification Center across multi-monitor + mixed-DPI setups.
// See README of the fork for context.
//
// This file is injected by dependencies/main.py into the regenerated
// b_taskbar-start-button-position.wh.cpp, replacing upstream's smaller
// hook. Keeping the customization here (not in modified-dependencies/)
// means a clean `python assemble-mod.py` run pulls in latest upstream
// and re-injects our logic on top.
using DwmSetWindowAttribute_t = decltype(&DwmSetWindowAttribute);
DwmSetWindowAttribute_t DwmSetWindowAttribute_Original;
HRESULT WINAPI DwmSetWindowAttribute_Hook(HWND hwnd,
                                          DWORD dwAttribute,
                                          LPCVOID pvAttribute,
                                          DWORD cbAttribute) {
    auto original = [=]() {
        return DwmSetWindowAttribute_Original(hwnd, dwAttribute, pvAttribute,
                                              cbAttribute);
    };
    if (dwAttribute != DWMWA_CLOAK || cbAttribute != sizeof(BOOL)) {
        return original();
    }
    BOOL cloak = *(BOOL*)pvAttribute;
    if (cloak) {
        return original();
    }
    Wh_Log(L"> %08X", (DWORD)(DWORD_PTR)hwnd);
    DWORD processId = 0;
    if (!hwnd || !GetWindowThreadProcessId(hwnd, &processId)) {
        return original();
    }
    TCHAR className[256];GetClassName(hwnd, className, 256);std::wstring windowClassName(className);
std::wstring processFileName = GetProcessFileName(processId);
Wh_Log(L"process: %s, windowClassName: %s",processFileName.c_str(),windowClassName.c_str());
    {
        POINT diagCursor0 = {-1, -1};
        GetCursorPos(&diagCursor0);
        RECT diagRect0 = {};
        GetWindowRect(hwnd, &diagRect0);
        FILE* f = nullptr;
        WCHAR logPath[MAX_PATH];
        if (GetEnvironmentVariableW(L"TEMP", logPath, MAX_PATH)) {
            wcscat_s(logPath, MAX_PATH, L"\\windhawk_popup_log.txt");
            _wfopen_s(&f, logPath, L"a, ccs=UTF-8");
            if (f) {
                SYSTEMTIME st;
                GetLocalTime(&st);
                fwprintf(f,
                         L"%02d:%02d:%02d.%03d HOOK_ENTRY proc=%s class=%s "
                         L"hwnd=%p rect=(%ld,%ld,cx=%ld,cy=%ld) cursor=(%ld,%ld)\n",
                         st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                         processFileName.c_str(),
                         windowClassName.c_str(),
                         hwnd,
                         diagRect0.left, diagRect0.top,
                         diagRect0.right - diagRect0.left,
                         diagRect0.bottom - diagRect0.top,
                         diagCursor0.x, diagCursor0.y);
                fclose(f);
            }
        }
    }
    enum class Target {
        StartMenu,
        SearchHost,ShellExperienceHost,
    };
    Target target;
    if (_wcsicmp(processFileName.c_str(), L"StartMenuExperienceHost.exe") ==
        0) {
        target = Target::StartMenu;
    } else if (_wcsicmp(processFileName.c_str(), L"SearchHost.exe") == 0) {
        target = Target::SearchHost;
    }else if (_wcsicmp(processFileName.c_str(), L"ShellExperienceHost.exe") == 0) {
        target = Target::ShellExperienceHost;
    }  else {
        {
            POINT diagCursor = {-1, -1};
            GetCursorPos(&diagCursor);
            RECT diagRect = {};
            GetWindowRect(hwnd, &diagRect);
            FILE* f = nullptr;
            WCHAR logPath[MAX_PATH];
            if (GetEnvironmentVariableW(L"TEMP", logPath, MAX_PATH)) {
                wcscat_s(logPath, MAX_PATH, L"\\windhawk_popup_log.txt");
                _wfopen_s(&f, logPath, L"a, ccs=UTF-8");
                if (f) {
                    SYSTEMTIME st;
                    GetLocalTime(&st);
                    fwprintf(f,
                             L"%02d:%02d:%02d.%03d UNHANDLED proc=%s class=%s "
                             L"hwnd=%p rect=(%ld,%ld,cx=%ld,cy=%ld) cursor=(%ld,%ld)\n",
                             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                             processFileName.c_str(),
                             windowClassName.c_str(),
                             hwnd,
                             diagRect.left, diagRect.top,
                             diagRect.right - diagRect.left,
                             diagRect.bottom - diagRect.top,
                             diagCursor.x, diagCursor.y);
                    fclose(f);
                }
            }
        }
        return original();
    }
    HMONITOR monitor = nullptr;
    POINT cursorPos;
    if (GetCursorPos(&cursorPos)) {
        monitor = MonitorFromPoint(cursorPos, MONITOR_DEFAULTTONEAREST);
    }
    if (!monitor) {
        monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    }
    UINT monitorDpiX = 96;
    UINT monitorDpiY = 96;
    GetDpiForMonitor(monitor, MDT_DEFAULT, &monitorDpiX, &monitorDpiY);
    MONITORINFO monitorInfo{
        .cbSize = sizeof(MONITORINFO),
    };
    GetMonitorInfo(monitor, &monitorInfo);
    auto monitorName = GetMonitorName(monitor);
    auto iterationTbStates = g_taskbarStates.find(monitorName);
    if (iterationTbStates == g_taskbarStates.end()) {
      return original();
    }
    TaskbarState& taskbarState = iterationTbStates->second;
    RECT targetRect;
    if (!GetWindowRect(hwnd, &targetRect)) {
        Wh_Log(L"[POPUP-DBG] GetWindowRect failed for hwnd=%p", hwnd);
        return original();
    }
    int x = targetRect.left;
    int y = targetRect.top;
    int cx = targetRect.right - targetRect.left;
    int cy = targetRect.bottom - targetRect.top;
    HMONITOR primaryMonitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    bool moveStartSetting = g_settings_startbuttonposition.startMenuOnTheLeft;
    bool moveNCSetting    = g_settings_startbuttonposition.MoveFlyoutNotificationCenter;
    bool onPrimary         = (monitor == primaryMonitor);
    bool ncPrimaryOnlyMode = g_settings.userDefinedNotificationCenterPrimaryOnly;
    bool ncWantPlace       = moveNCSetting && (!ncPrimaryOnlyMode || onPrimary);
    bool wantPlace =
        !g_unloading &&
        ((target == Target::StartMenu          && moveStartSetting) ||
         (target == Target::SearchHost         && moveStartSetting) ||
         (target == Target::ShellExperienceHost && ncWantPlace));
    if (wantPlace) {
        static int g_naturalSearchCxDIPs = 0;
        static int g_naturalSearchCyDIPs = 0;
        {
            const int kDefaultSearchCxDIPs = 858;  // Win11 24H2 fallback
            const int kDefaultSearchCyDIPs = 890;
            const int kMinNaturalDIPs      = 700;
            const int kMaxNaturalDIPs      = 950;
            HMONITOR popupMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            if (target == Target::SearchHost &&
                popupMon == primaryMonitor &&
                monitor == primaryMonitor) {
                UINT primaryDpiX = 96, primaryDpiY = 96;
                GetDpiForMonitor(primaryMonitor, MDT_DEFAULT, &primaryDpiX, &primaryDpiY);
                if (primaryDpiX > 0) {
                    int observedCxDIPs = MulDiv(cx, 96, primaryDpiX);
                    if (observedCxDIPs >= kMinNaturalDIPs && observedCxDIPs <= kMaxNaturalDIPs) {
                        g_naturalSearchCxDIPs = observedCxDIPs;
                        g_naturalSearchCyDIPs = MulDiv(cy, 96, primaryDpiY);
                    }
                }
            }
            if (g_naturalSearchCxDIPs == 0) {
                g_naturalSearchCxDIPs = kDefaultSearchCxDIPs;
                g_naturalSearchCyDIPs = kDefaultSearchCyDIPs;
            }
        }
        int tbarLeftDIPs   = (int)taskbarState.lastStartButtonXCalculated;
        int tbarWidthDIPs  = (int)taskbarState.lastTargetWidth;
        int tbarCenterDIPs = tbarLeftDIPs + tbarWidthDIPs / 2;
        int tbarRightDIPs  = tbarLeftDIPs + tbarWidthDIPs;
        int tbarVisualHDIPs =
            (int)g_settings.userDefinedTaskbarHeight +
            std::abs((int)g_settings.userDefinedTaskbarOffsetY);
        int monW = monitorInfo.rcMonitor.right  - monitorInfo.rcMonitor.left;
        int monH = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
        if (target == Target::StartMenu) {
            x = monitorInfo.rcMonitor.left;
            y = monitorInfo.rcMonitor.top;
        } else if (target == Target::SearchHost) {
            cx = MulDiv(g_naturalSearchCxDIPs, monitorDpiX, 96);
            cy = MulDiv(g_naturalSearchCyDIPs, monitorDpiY, 96);
            int xLocal = MulDiv(tbarCenterDIPs, monitorDpiX, 96) - cx / 2;
            int tbarVisualHPx = MulDiv(tbarVisualHDIPs, monitorDpiY, 96);
            int marginPx      = MulDiv(6, monitorDpiY, 96);
            int yLocal = monH - tbarVisualHPx - marginPx - cy;
            if (xLocal < 10) xLocal = 10;
            if (xLocal + cx > monW - 10) xLocal = monW - cx - 10;
            if (yLocal < 10) yLocal = 10;
            x = monitorInfo.rcMonitor.left + xLocal;
            y = monitorInfo.rcMonitor.top  + yLocal;
            g_searchMenuWnd = hwnd;
            g_searchMenuOriginalX = targetRect.left;
        } else /* Target::ShellExperienceHost */ {
            if (cy < monH / 2) {
                return original();  // toast — leave Windows' positioning alone
            }
            int xLocal = MulDiv(tbarRightDIPs, monitorDpiX, 96) - cx;
            if (xLocal < 10) xLocal = 10;
            if (xLocal + cx > monW - 10) xLocal = monW - cx - 10;
            x = monitorInfo.rcMonitor.left + xLocal;
            y = monitorInfo.rcMonitor.top;  // top-anchor; cy already monitor-sized
        }
    }
    {
        int safeMinX = monitorInfo.rcMonitor.left   - cx + 100;
        int safeMaxX = monitorInfo.rcMonitor.right  - 100;
        int safeMinY = monitorInfo.rcMonitor.top    - cy + 100;
        int safeMaxY = monitorInfo.rcMonitor.bottom - 100;
        if (x < safeMinX) x = safeMinX;
        if (x > safeMaxX) x = safeMaxX;
        if (y < safeMinY) y = safeMinY;
        if (y > safeMaxY) y = safeMaxY;
    }
    Wh_Log(L"[POPUP-DBG] target=%d proc=%s monitor=%s mDPI=%u rcMon=(%ld,%ld)-(%ld,%ld) rcWork=(%ld,%ld)-(%ld,%ld) cursor=(%ld,%ld) winRect.in=(%ld,%ld,cx=%d,cy=%d) finalSetPos=(x=%d,y=%d,cx=%d,cy=%d) tbState{startBtnX=%.2f rootW=%.2f targetW=%.2f}",
           (int)target,
           processFileName.c_str(),
           monitorName.c_str(),
           monitorDpiX,
           monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
           monitorInfo.rcMonitor.right, monitorInfo.rcMonitor.bottom,
           monitorInfo.rcWork.left, monitorInfo.rcWork.top,
           monitorInfo.rcWork.right, monitorInfo.rcWork.bottom,
           cursorPos.x, cursorPos.y,
           targetRect.left, targetRect.top,
           targetRect.right - targetRect.left, targetRect.bottom - targetRect.top,
           x, y, cx, cy,
           taskbarState.lastStartButtonXCalculated,
           taskbarState.lastRootWidth,
           taskbarState.lastTargetWidth);
    {
      FILE* f = nullptr;
      WCHAR logPath[MAX_PATH];
      if (GetEnvironmentVariableW(L"TEMP", logPath, MAX_PATH)) {
        wcscat_s(logPath, MAX_PATH, L"\\windhawk_popup_log.txt");
        _wfopen_s(&f, logPath, L"a, ccs=UTF-8");
        if (f) {
          SYSTEMTIME st;
          GetLocalTime(&st);
          fwprintf(f, L"%02d:%02d:%02d.%03d target=%d proc=%s monitor=%s mDPI=%u rcMon=(%ld,%ld)-(%ld,%ld) rcWork=(%ld,%ld)-(%ld,%ld) cursor=(%ld,%ld) winRect.in=(%ld,%ld,cx=%d,cy=%d) finalSetPos=(x=%d,y=%d,cx=%d,cy=%d) tbState{startBtnX=%.2f rootW=%.2f targetW=%.2f} popupMon%s\n",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            (int)target,
            processFileName.c_str(),
            monitorName.c_str(),
            monitorDpiX,
            monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right, monitorInfo.rcMonitor.bottom,
            monitorInfo.rcWork.left, monitorInfo.rcWork.top,
            monitorInfo.rcWork.right, monitorInfo.rcWork.bottom,
            cursorPos.x, cursorPos.y,
            targetRect.left, targetRect.top,
            targetRect.right - targetRect.left, targetRect.bottom - targetRect.top,
            x, y, cx, cy,
            taskbarState.lastStartButtonXCalculated,
            taskbarState.lastRootWidth,
            taskbarState.lastTargetWidth,
            (MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST) == monitor) ? L"=cursor" : L"!=cursor");
          fclose(f);
        }
      }
    }
    SetWindowPos(hwnd, nullptr, x, y, cx, cy, SWP_NOZORDER | SWP_NOACTIVATE);
    return original();
}
namespace StartMenuUI {
bool g_applyStylePending;
bool g_inApplyStyle;
std::optional<double> g_previousCanvasLeft;
winrt::weak_ref<DependencyObject> g_startSizingFrameWeakRef;
int64_t g_canvasTopPropertyChangedToken;
int64_t g_canvasLeftPropertyChangedToken;
std::optional<HorizontalAlignment> g_previousHorizontalAlignment;
winrt::event_token g_layoutUpdatedToken;
winrt::event_token g_visibilityChangedToken;
HWND GetCoreWnd() {
    struct ENUM_WINDOWS_PARAM {
        HWND* hWnd;
    };
    HWND hWnd = nullptr;
    ENUM_WINDOWS_PARAM param = {&hWnd};
    EnumWindows(
        [](HWND hWnd, LPARAM lParam) -> BOOL {
            ENUM_WINDOWS_PARAM& param = *(ENUM_WINDOWS_PARAM*)lParam;
            DWORD dwProcessId = 0;
            if (!GetWindowThreadProcessId(hWnd, &dwProcessId) ||
                dwProcessId != GetCurrentProcessId()) {
                return TRUE;
            }
            WCHAR szClassName[32];
            if (GetClassName(hWnd, szClassName, ARRAYSIZE(szClassName)) == 0) {
                return TRUE;
            }
            if (_wcsicmp(szClassName, L"Windows.UI.Core.CoreWindow") == 0) {
                *param.hWnd = hWnd;
                return FALSE;
            }
            return TRUE;
        },
        (LPARAM)&param);
    return hWnd;
}
void ApplyStyle();
void ApplyStyleClassicStartMenu(FrameworkElement content, HMONITOR monitor) {
    FrameworkElement startSizingFrame =
        FindChildByClassName(content, L"StartDocked.StartSizingFrame");
    if (!startSizingFrame) {
        Wh_Log(L"Failed to find StartDocked.StartSizingFrame");
        return;
    }
    if (g_unloading) {
        if (g_previousCanvasLeft.has_value()) {
            Wh_Log(L"Restoring Canvas.Left to %f",
                   g_previousCanvasLeft.value());
            Controls::Canvas::SetLeft(startSizingFrame,
                                      g_previousCanvasLeft.value());
        }
    } else {
        if (!g_previousCanvasLeft.has_value()) {
            double canvasLeft = Controls::Canvas::GetLeft(startSizingFrame);
            if (canvasLeft) {
                g_previousCanvasLeft = canvasLeft;
            }
        }
        constexpr int kStartMenuMargin = 12;
        double newLeft = kStartMenuMargin;
        Wh_Log(L"Setting Canvas.Left to %f", newLeft);
        Controls::Canvas::SetLeft(startSizingFrame, newLeft);
        if (!g_startSizingFrameWeakRef.get()) {
            auto startSizingFrameDo = startSizingFrame.as<DependencyObject>();
            g_startSizingFrameWeakRef = startSizingFrameDo;
            g_canvasTopPropertyChangedToken =
                startSizingFrameDo.RegisterPropertyChangedCallback(
                    Controls::Canvas::TopProperty(),
                    [](DependencyObject sender, DependencyProperty property) {
                        double top = Controls::Canvas::GetTop(
                            sender.as<FrameworkElement>());
                        Wh_Log(L"Canvas.Top changed to %f", top);
                        if (!g_inApplyStyle) {
                            ApplyStyle();
                        }
                    });
            g_canvasLeftPropertyChangedToken =
                startSizingFrameDo.RegisterPropertyChangedCallback(
                    Controls::Canvas::LeftProperty(),
                    [](DependencyObject sender, DependencyProperty property) {
                        double left = Controls::Canvas::GetLeft(
                            sender.as<FrameworkElement>());
                        Wh_Log(L"Canvas.Left changed to %f", left);
                        if (!g_inApplyStyle) {
                            ApplyStyle();
                        }
                    });
        }
    }
}
void ApplyStyleRedesignedStartMenu(FrameworkElement content) {
    FrameworkElement frameRoot = FindChildByName(content, L"FrameRoot");
    if (!frameRoot) {
        Wh_Log(L"Failed to find Start menu frame root");
        return;
    }
    if (g_unloading) {
        frameRoot.HorizontalAlignment(g_previousHorizontalAlignment.value_or(
            HorizontalAlignment::Center));
    } else {
        if (!g_previousHorizontalAlignment) {
            g_previousHorizontalAlignment = frameRoot.HorizontalAlignment();
        }
        frameRoot.HorizontalAlignment(HorizontalAlignment::Left);
    }
}
void ApplyStyle() {
    g_inApplyStyle = true;
    HWND coreWnd = GetCoreWnd();
    HMONITOR monitor = MonitorFromWindow(coreWnd, MONITOR_DEFAULTTONEAREST);
    Wh_Log(L"Applying Start menu style for monitor %p", monitor);
    auto window = Window::Current();
    FrameworkElement content = window.Content().as<FrameworkElement>();
    winrt::hstring contentClassName = winrt::get_class_name(content);
    Wh_Log(L"Start menu content class name: %s", contentClassName.c_str());
    if (contentClassName == L"Windows.UI.Xaml.Controls.Canvas") {
        ApplyStyleClassicStartMenu(content, monitor);
    } else if (contentClassName == L"StartMenu.StartBlendedFlexFrame") {
        ApplyStyleRedesignedStartMenu(content);
    } else {
        Wh_Log(L"Error: Unsupported Start menu content class name");
    }
    g_inApplyStyle = false;
}
void Init() {
    if (g_layoutUpdatedToken) {
        return;
    }
    auto window = Window::Current();
    if (!window) {
        return;
    }
    if (!g_visibilityChangedToken) {
        g_visibilityChangedToken = window.VisibilityChanged(
            [](winrt::Windows::Foundation::IInspectable const& sender,
               winrt::Windows::UI::Core::VisibilityChangedEventArgs const&
                   args) {
                Wh_Log(L"Window visibility changed: %d", args.Visible());
                if (args.Visible()) {
                    g_applyStylePending = true;
                }
            });
    }
    auto contentUI = window.Content();
    if (!contentUI) {
        return;
    }
    auto content = contentUI.as<FrameworkElement>();
    g_layoutUpdatedToken = content.LayoutUpdated(
        [](winrt::Windows::Foundation::IInspectable const&,
           winrt::Windows::Foundation::IInspectable const&) {
            if (g_applyStylePending) {
                g_applyStylePending = false;
                ApplyStyle();
            }
        });
    ApplyStyle();
}
void Uninit() {
    if (!g_layoutUpdatedToken) {
        return;
    }
    auto window = Window::Current();
    if (!window) {
        return;
    }
    if (g_visibilityChangedToken) {
        window.VisibilityChanged(g_visibilityChangedToken);
        g_visibilityChangedToken = {};
    }
    auto contentUI = window.Content();
    if (!contentUI) {
        return;
    }
    auto content = contentUI.as<FrameworkElement>();
    content.LayoutUpdated(g_layoutUpdatedToken);
    g_layoutUpdatedToken = {};
    auto startSizingFrameDo = g_startSizingFrameWeakRef.get();
    if (startSizingFrameDo) {
        if (g_canvasTopPropertyChangedToken) {
            startSizingFrameDo.UnregisterPropertyChangedCallback(
                Controls::Canvas::TopProperty(),
                g_canvasTopPropertyChangedToken);
            g_canvasTopPropertyChangedToken = 0;
        }
        if (g_canvasLeftPropertyChangedToken) {
            startSizingFrameDo.UnregisterPropertyChangedCallback(
                Controls::Canvas::LeftProperty(),
                g_canvasLeftPropertyChangedToken);
            g_canvasLeftPropertyChangedToken = 0;
        }
    }
    g_startSizingFrameWeakRef = nullptr;
    ApplyStyle();
}
void SettingsChanged() {
    ApplyStyle();
}
using RoGetActivationFactory_t = decltype(&RoGetActivationFactory);
RoGetActivationFactory_t RoGetActivationFactory_Original;
HRESULT WINAPI RoGetActivationFactory_Hook(HSTRING activatableClassId,
                                           REFIID iid,
                                           void** factory) {
    thread_local static bool isInHook;
    if (isInHook) {
        return RoGetActivationFactory_Original(activatableClassId, iid,
                                               factory);
    }
    isInHook = true;
    if (wcscmp(WindowsGetStringRawBuffer(activatableClassId, nullptr),
               L"Windows.UI.Xaml.Hosting.XamlIsland") == 0) {
        try {
            Init();
        } catch (...) {
            HRESULT hr = winrt::to_hresult();
            Wh_Log(L"Error %08X", hr);
        }
    }
    HRESULT ret =
        RoGetActivationFactory_Original(activatableClassId, iid, factory);
    isInHook = false;
    return ret;
}
}  // namespace StartMenuUI
void RestoreMenuPositions() {
    if (g_searchMenuWnd && g_searchMenuOriginalX) {
        RECT rect;
        if (GetWindowRect(g_searchMenuWnd, &rect)) {
            int x = rect.left;
            int y = rect.top;
            int cx = rect.right - rect.left;
            int cy = rect.bottom - rect.top;
            if (g_searchMenuOriginalX != x) {
                x = g_searchMenuOriginalX;
                SetWindowPos(g_searchMenuWnd, nullptr, x, y, cx, cy,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        g_searchMenuWnd = nullptr;
        g_searchMenuOriginalX = 0;
    }
}
void LoadSettingsStartButtonPosition() {
    g_settings_startbuttonposition.startMenuOnTheLeft = Wh_GetIntSetting(L"MoveFlyoutStartMenu");
g_settings_startbuttonposition.MoveFlyoutNotificationCenter = Wh_GetIntSetting(L"MoveFlyoutNotificationCenter");
}
BOOL Wh_ModInitStartButtonPosition() {
    LoadSettingsStartButtonPosition();
    g_target = Target::Explorer;
    WCHAR moduleFilePath[MAX_PATH];
    switch (
        GetModuleFileName(nullptr, moduleFilePath, ARRAYSIZE(moduleFilePath))) {
        case 0:
        case ARRAYSIZE(moduleFilePath):
            Wh_Log(L"GetModuleFileName failed");
            break;
        default:
            if (PCWSTR moduleFileName = wcsrchr(moduleFilePath, L'\\')) {
                moduleFileName++;
                if (_wcsicmp(moduleFileName, L"StartMenuExperienceHost.exe") ==
                    0) {
                    g_target = Target::StartMenuExperienceHost;
                }
            } else {
                Wh_Log(L"GetModuleFileName returned an unsupported path");
            }
            break;
    }
    if (g_target == Target::StartMenuExperienceHost) {
        if (!g_settings_startbuttonposition.startMenuOnTheLeft) {
            return FALSE;
        }
        HMODULE winrtModule =
            GetModuleHandle(L"api-ms-win-core-winrt-l1-1-0.dll");
        auto pRoGetActivationFactory =
            (decltype(&RoGetActivationFactory))GetProcAddress(
                winrtModule, "RoGetActivationFactory");
        WindhawkUtils::SetFunctionHook(
            pRoGetActivationFactory, StartMenuUI::RoGetActivationFactory_Hook,
            &StartMenuUI::RoGetActivationFactory_Original);
        return TRUE;
    }
    if (!HookTaskbarDllSymbolsStartButtonPosition()) {
        return FALSE;
    }
    if (HMODULE taskbarViewModule = GetTaskbarViewModuleHandle()) {
        g_taskbarViewDllLoadedStartButtonPosition = true;
        if (!HookTaskbarViewDllSymbolsStartButtonPosition(taskbarViewModule)) {
            return FALSE;
        }
    } else {
        Wh_Log(L"Taskbar view module not loaded yet");
        HMODULE kernelBaseModule = GetModuleHandle(L"kernelbase.dll");
        auto pKernelBaseLoadLibraryExW =
            (decltype(&LoadLibraryExW))GetProcAddress(kernelBaseModule,
                                                      "LoadLibraryExW");
        WindhawkUtils::SetFunctionHook(pKernelBaseLoadLibraryExW,
                                       LoadLibraryExW_Hook_StartButtonPosition,
                                       &LoadLibraryExW_Original);
    }
    HMODULE dwmapiModule =
        LoadLibraryEx(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (dwmapiModule) {
        auto pDwmSetWindowAttribute =
            (decltype(&DwmSetWindowAttribute))GetProcAddress(
                dwmapiModule, "DwmSetWindowAttribute");
        if (pDwmSetWindowAttribute) {
            WindhawkUtils::SetFunctionHook(pDwmSetWindowAttribute,
                                           DwmSetWindowAttribute_Hook,
                                           &DwmSetWindowAttribute_Original);
        }
    }
    return TRUE;
}
void Wh_ModAfterInitStartButtonPosition() {
    if (g_target == Target::Explorer) {
        if (!g_taskbarViewDllLoadedStartButtonPosition) {
            if (HMODULE taskbarViewModule = GetTaskbarViewModuleHandle()) {
                if (!g_taskbarViewDllLoadedStartButtonPosition.exchange(true)) {
                    Wh_Log(L"Got Taskbar.View.dll");
                    if (HookTaskbarViewDllSymbolsStartButtonPosition(taskbarViewModule)) {
                        Wh_ApplyHookOperations();
                    }
                }
            }
        }
        HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
        if (hTaskbarWnd) {
            ApplySettingsStartButtonPosition(hTaskbarWnd);
        }
    } else if (g_target == Target::StartMenuExperienceHost) {
        HWND hCoreWnd = StartMenuUI::GetCoreWnd();
        if (hCoreWnd) {
            Wh_Log(L"Initializing - Found core window");
            RunFromWindowThread(
                hCoreWnd, [](PVOID) { StartMenuUI::Init(); }, nullptr);
        }
    }
}
void Wh_ModBeforeUninitStartButtonPosition() {
    g_unloading = true;
    if (g_target == Target::Explorer) {
        HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
        if (hTaskbarWnd) {
            ApplySettingsStartButtonPosition(hTaskbarWnd);
        }
    } else if (g_target == Target::StartMenuExperienceHost) {
        HWND hCoreWnd = StartMenuUI::GetCoreWnd();
        if (hCoreWnd) {
            Wh_Log(L"Uninitializing - Found core window");
            RunFromWindowThread(
                hCoreWnd, [](PVOID) { StartMenuUI::Uninit(); }, nullptr);
        }
    }
}
void Wh_ModUninitStartButtonPosition() {if(true)return;
    if (g_target == Target::Explorer) {
        RestoreMenuPositions();
    }
}
BOOL Wh_ModSettingsChangedStartButtonPosition() {
    if (g_target == Target::Explorer) {
        RestoreMenuPositions();
    }
    LoadSettingsStartButtonPosition();
    if (g_target == Target::StartMenuExperienceHost) {
        if (!g_settings_startbuttonposition.startMenuOnTheLeft) {
            return FALSE;
        }
        HWND hCoreWnd = StartMenuUI::GetCoreWnd();
        if (hCoreWnd) {
            Wh_Log(L"Applying settings - Found core window");
            RunFromWindowThread(
                hCoreWnd, [](PVOID) { StartMenuUI::SettingsChanged(); },
                nullptr);
        }
    }
    return TRUE;
}