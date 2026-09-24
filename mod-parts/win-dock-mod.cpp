
//////////////////////////////////////////////////////
//////////////////////////////////////////////////////
//////////////////////////////////////////////////////
/////      .___________.     ___       __        /////
/////      |           |    /   \     |  |       /////
/////      `---|  |----`   /  ^  \    |  |       /////
/////          |  |       /  /_\  \   |  |       /////
/////          |  |      /  _____  \  |  |       /////
/////          |__|     /__/     \__\ |__|       /////
/////                                            /////
//////////////////////////////////////////////////////
//////////////////////////////////////////////////////
//////////////////////////////////////////////////////
#include <dwmapi.h>
#include <chrono>
#include <string>
#include <regex>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <limits>
#include <utility>
#include <windhawk_api.h>
#include <windhawk_utils.h>
#include <functional>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Data.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Media.Animation.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/base.h>
#include <commctrl.h>
#include <roapi.h>
#include <winstring.h>
#include <string_view>
#include <vector>
#include <atomic>
#include <cmath>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.Storage.Search.h>
#include <thread>
#include <windows.h>
#include <psapi.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <mutex>
#include <condition_variable>
#include <memory>
using namespace winrt::Windows::UI::Xaml;

#ifndef HSHELL_GETMINRECT
#define HSHELL_GETMINRECT 5
#endif

struct SHELLHOOKINFO_TAI {
  HWND hwnd;
  RECT rc;
};

struct MinimizeAnimationMeasuredButtonTai {
  RECT visibleRectPx{};
  std::wstring matchName;
};

struct MinimizeAnimationCorrectionTai {
  double layoutOffsetXDip{0.0};
  double visualScale{1.0};
  double scaleCenterXDip{0.0};
  double rasterizationScale{1.0};
  RECT monitorRect{};
  RECT clampXRect{};
  bool hasClampXRect{false};
  std::vector<MinimizeAnimationMeasuredButtonTai> measuredButtons;
};

static std::mutex g_minimizeAnimationCorrectionMutexTai;
static std::unordered_map<
    std::wstring,
    std::shared_ptr<const MinimizeAnimationCorrectionTai>>
    g_minimizeAnimationCorrectionByMonitorNameTai;
static std::atomic_bool g_minimizeAnimationCorrectionReadyTai{false};
static std::atomic_bool g_minimizeAnimationCorrectionUninitializingTai{false};

static thread_local LPARAM g_minimizeAnimationLastCorrectedLParamTai = 0;
static thread_local HWND g_minimizeAnimationLastCorrectedHwndTai = nullptr;
static thread_local RECT g_minimizeAnimationLastCorrectedRawTai{};
static thread_local DWORD g_minimizeAnimationLastCorrectedTickTai = 0;

using SendMessageW_t = LRESULT(WINAPI*)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
static SendMessageW_t SendMessageW_OriginalTai = nullptr;


static bool ReadShellHookInfoTai(LPARAM lParam, SHELLHOOKINFO_TAI* shellHookInfo) {
  if (!lParam || !shellHookInfo) {
    return false;
  }

#if defined(_MSC_VER)
  __try {
    *shellHookInfo = *reinterpret_cast<const SHELLHOOKINFO_TAI*>(lParam);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
#else
  *shellHookInfo = *reinterpret_cast<const SHELLHOOKINFO_TAI*>(lParam);
#endif

  return true;
}

static bool WriteShellHookRectTai(LPARAM lParam, const RECT& rect) {
  if (!lParam) {
    return false;
  }

#if defined(_MSC_VER)
  __try {
    reinterpret_cast<SHELLHOOKINFO_TAI*>(lParam)->rc = rect;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
#else
  reinterpret_cast<SHELLHOOKINFO_TAI*>(lParam)->rc = rect;
#endif

  return true;
}

static bool GetMonitorRectByNameTai(const std::wstring& monitorName, RECT* rect) {
  if (!rect || monitorName.empty()) {
    return false;
  }

  struct EnumContext {
    const std::wstring* monitorName;
    RECT* rect;
    bool found;
  } context{&monitorName, rect, false};

  EnumDisplayMonitors(
      nullptr,
      nullptr,
      [](HMONITOR monitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
        auto* context = reinterpret_cast<EnumContext*>(lParam);
        if (!context || !context->monitorName || !context->rect) {
          return TRUE;
        }

        MONITORINFOEXW monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (GetMonitorInfoW(monitor, &monitorInfo) &&
            _wcsicmp(monitorInfo.szDevice, context->monitorName->c_str()) == 0) {
          *context->rect = monitorInfo.rcMonitor;
          context->found = true;
          return FALSE;
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&context));

  return context.found;
}

static int RectWidthTai(const RECT& rect) {
  return rect.right - rect.left;
}

static int ClampIntTai(int value, int minValue, int maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

static void ClampRectCenterXToBoundsTai(RECT* rect, const RECT& bounds) {
  if (!rect || IsRectEmpty(rect) || IsRectEmpty(&bounds)) {
    return;
  }

  const int width = std::max(1, RectWidthTai(*rect));
  const int currentCenterX = rect->left + width / 2;

  int minCenterX = bounds.left + width / 2;
  int maxCenterX = bounds.right - width / 2;
  if (minCenterX > maxCenterX) {
    minCenterX = maxCenterX = (bounds.left + bounds.right) / 2;
  }

  const int newCenterX = ClampIntTai(currentCenterX, minCenterX, maxCenterX);
  OffsetRect(rect, newCenterX - currentCenterX, 0);
}

static std::wstring TrimTai(const std::wstring& value) {
  const auto first = value.find_first_not_of(L" \t\r\n");
  if (first == std::wstring::npos) {
    return L"";
  }
  const auto last = value.find_last_not_of(L" \t\r\n");
  return value.substr(first, last - first + 1);
}

static std::wstring GetWindowTextSafeTai(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd)) {
    return L"";
  }

  int length = GetWindowTextLengthW(hwnd);
  if (length <= 0 || length > 32767) {
    return L"";
  }

  std::wstring text(static_cast<size_t>(length) + 1, L'\0');
  int copied = GetWindowTextW(hwnd, text.data(), length + 1);
  if (copied <= 0) {
    return L"";
  }
  text.resize(static_cast<size_t>(copied));
  return TrimTai(text);
}

static std::wstring StripExeExtensionTai(std::wstring value) {
  value = TrimTai(value);
  if (value.size() > 4 && _wcsicmp(value.c_str() + value.size() - 4, L".exe") == 0) {
    value.resize(value.size() - 4);
  }
  return value;
}

static bool TryGetTargetWindowProcessBaseNameTai(HWND hwnd, std::wstring* processBaseName) {
  if (!hwnd || !processBaseName || !IsWindow(hwnd)) {
    return false;
  }

  DWORD processId = 0;
  GetWindowThreadProcessId(hwnd, &processId);
  if (!processId) {
    return false;
  }

  std::wstring processName = StripExeExtensionTai(GetProcessFileName(processId));
  if (processName.empty()) {
    return false;
  }

  *processBaseName = processName;
  return true;
}

static bool IsUsableMeasuredButtonRectTai(const RECT& rect) {
  return rect.right > rect.left && rect.bottom > rect.top;
}

static bool TextEqualsOrdinalIgnoreCaseTai(
    std::wstring_view left,
    std::wstring_view right) {
  if (left.size() != right.size() ||
      left.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return false;
  }

  return CompareStringOrdinal(
             left.data(),
             static_cast<int>(left.size()),
             right.data(),
             static_cast<int>(right.size()),
             TRUE) == CSTR_EQUAL;
}

static bool TextContainsOrdinalIgnoreCaseTai(
    std::wstring_view haystack,
    std::wstring_view needle) {
  if (haystack.empty() || needle.empty() || needle.size() > haystack.size() ||
      needle.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return false;
  }

  const int needleLength = static_cast<int>(needle.size());
  const size_t lastStart = haystack.size() - needle.size();
  for (size_t start = 0; start <= lastStart; ++start) {
    if (CompareStringOrdinal(
            haystack.data() + start,
            needleLength,
            needle.data(),
            needleLength,
            TRUE) == CSTR_EQUAL) {
      return true;
    }
  }

  return false;
}

static const MinimizeAnimationMeasuredButtonTai* FindMeasuredButtonForTargetWindowTai(
    HWND targetWindow,
    const RECT& currentRect,
    const MinimizeAnimationCorrectionTai& correction,
    int* matchTier) {
  if (!targetWindow || !IsWindow(targetWindow) || correction.measuredButtons.empty()) {
    return nullptr;
  }

  if (matchTier) {
    *matchTier = 0;
  }

  const std::wstring title = GetWindowTextSafeTai(targetWindow);
  const int currentCenterX = currentRect.left + std::max(1, RectWidthTai(currentRect)) / 2;

  const MinimizeAnimationMeasuredButtonTai* best = nullptr;
  int bestTier = 0;
  LONG bestDistance = LONG_MAX;

  auto considerButton = [&](const MinimizeAnimationMeasuredButtonTai& button,
                            int tier) {
    if (tier <= 0) {
      return;
    }

    const int buttonCenterX = button.visibleRectPx.left + RectWidthTai(button.visibleRectPx) / 2;
    const LONG distance = buttonCenterX >= currentCenterX ? buttonCenterX - currentCenterX : currentCenterX - buttonCenterX;
    if (!best || tier > bestTier || (tier == bestTier && distance < bestDistance)) {
      best = &button;
      bestTier = tier;
      bestDistance = distance;
    }
  };

  if (!title.empty()) {
    for (const auto& button : correction.measuredButtons) {
      if (!IsUsableMeasuredButtonRectTai(button.visibleRectPx) ||
          button.matchName.empty()) {
        continue;
      }

      if (TextEqualsOrdinalIgnoreCaseTai(button.matchName, title)) {
        considerButton(button, 3);
      } else if (
          TextContainsOrdinalIgnoreCaseTai(button.matchName, title) ||
          TextContainsOrdinalIgnoreCaseTai(title, button.matchName)) {
        considerButton(button, 2);
      }
    }
  }

  if (best) {
    if (matchTier) {
      *matchTier = bestTier;
    }
    return best;
  }

  std::wstring processBaseName;
  if (!TryGetTargetWindowProcessBaseNameTai(
          targetWindow,
          &processBaseName)) {
    return nullptr;
  }

  for (const auto& button : correction.measuredButtons) {
    if (!IsUsableMeasuredButtonRectTai(button.visibleRectPx) ||
        button.matchName.empty()) {
      continue;
    }

    if (TextContainsOrdinalIgnoreCaseTai(
            button.matchName,
            processBaseName)) {
      considerButton(button, 1);
    }
  }

  if (matchTier) {
    *matchTier = bestTier;
  }
  return best;
}

static bool TryUseMeasuredTaskbarButtonRectTai(
    HWND targetWindow,
    RECT* rect,
    const MinimizeAnimationCorrectionTai& correction,
    int* matchTier) {
  if (!rect || correction.measuredButtons.empty()) {
    return false;
  }

  const auto* button = FindMeasuredButtonForTargetWindowTai(targetWindow, *rect, correction, matchTier);
  if (!button || !IsUsableMeasuredButtonRectTai(button->visibleRectPx)) {
    return false;
  }

  rect->left = button->visibleRectPx.left;
  rect->right = button->visibleRectPx.right;
  return true;
}

static LONG TransformMinimizeRectXTai(LONG xPx, const MinimizeAnimationCorrectionTai& correction) {
  const double dpiScale = std::max(0.01, correction.rasterizationScale);
  double xDip = (static_cast<double>(xPx) - static_cast<double>(correction.monitorRect.left)) / dpiScale;

  // First apply the RootGrid translation that visually brings the virtual
  // taskbar surface back to the real screen, then apply the same island scale.
  xDip += correction.layoutOffsetXDip;
  xDip = correction.scaleCenterXDip + ((xDip - correction.scaleCenterXDip) * correction.visualScale);

  return static_cast<LONG>(std::lround(static_cast<double>(correction.monitorRect.left) + xDip * dpiScale));
}

static std::shared_ptr<const MinimizeAnimationCorrectionTai>
GetMinimizeAnimationCorrectionForWindowTai(HWND targetWindow) {
  if (!targetWindow ||
      !g_minimizeAnimationCorrectionReadyTai.load(std::memory_order_acquire) ||
      g_minimizeAnimationCorrectionUninitializingTai.load(std::memory_order_acquire)) {
    return nullptr;
  }

  if (!IsWindow(targetWindow)) {
    return nullptr;
  }

  HMONITOR monitor = MonitorFromWindow(targetWindow, MONITOR_DEFAULTTONEAREST);
  if (!monitor) {
    return nullptr;
  }

  const std::wstring monitorName = GetMonitorName(monitor);
  if (monitorName.empty()) {
    return nullptr;
  }

  std::lock_guard<std::mutex> lock(g_minimizeAnimationCorrectionMutexTai);
  auto it = g_minimizeAnimationCorrectionByMonitorNameTai.find(monitorName);
  if (it == g_minimizeAnimationCorrectionByMonitorNameTai.end()) {
    return nullptr;
  }

  return it->second;
}

static bool WasMinimizeAnimationPayloadAlreadyCorrectedTai(
    LPARAM lParam,
    const SHELLHOOKINFO_TAI& shellHookInfo) {
  if (!lParam) {
    return false;
  }

  if (g_minimizeAnimationLastCorrectedLParamTai != lParam ||
      g_minimizeAnimationLastCorrectedHwndTai != shellHookInfo.hwnd ||
      !EqualRect(&g_minimizeAnimationLastCorrectedRawTai, &shellHookInfo.rc)) {
    return false;
  }

  return GetTickCount() - g_minimizeAnimationLastCorrectedTickTai < 1000;
}

static void RememberMinimizeAnimationCorrectedPayloadTai(
    LPARAM lParam,
    const SHELLHOOKINFO_TAI& shellHookInfo) {
  if (!lParam) {
    return;
  }

  g_minimizeAnimationLastCorrectedLParamTai = lParam;
  g_minimizeAnimationLastCorrectedHwndTai = shellHookInfo.hwnd;
  g_minimizeAnimationLastCorrectedRawTai = shellHookInfo.rc;
  g_minimizeAnimationLastCorrectedTickTai = GetTickCount();
}

static SHORT SignedLowWordTai(LONG value) {
  return static_cast<SHORT>(static_cast<WORD>(value & 0xFFFF));
}

static SHORT SignedHighWordTai(LONG value) {
  return static_cast<SHORT>(static_cast<WORD>((static_cast<DWORD>(value) >> 16) & 0xFFFF));
}

static LONG PackSignedPointTai(LONG x, LONG y) {
  return static_cast<LONG>((static_cast<DWORD>(static_cast<WORD>(y)) << 16) |
                           static_cast<DWORD>(static_cast<WORD>(x)));
}

static bool TryDecodePackedMinimizeRectTai(const SHELLHOOKINFO_TAI& shellHookInfo, RECT* decodedRect) {
  if (!decodedRect) {
    return false;
  }

  // On recent Windows 11 builds, the SHELLHOOKINFO rc for HSHELL_GETMINRECT can
  // arrive as two packed POINTS rather than a normal RECT:
  //   rc.left = MAKELONG(left, top)
  //   rc.top  = MAKELONG(right, bottom)
  //   rc.right/rc.bottom = 0
  // Example raw log: rc=(87304471,93399320,0,0)
  // Decoded: (10519,1332,10520,1425)
  if (shellHookInfo.rc.right != 0 || shellHookInfo.rc.bottom != 0) {
    return false;
  }

  RECT candidate{};
  candidate.left = SignedLowWordTai(shellHookInfo.rc.left);
  candidate.top = SignedHighWordTai(shellHookInfo.rc.left);
  candidate.right = SignedLowWordTai(shellHookInfo.rc.top);
  candidate.bottom = SignedHighWordTai(shellHookInfo.rc.top);

  if (candidate.right < candidate.left) {
    std::swap(candidate.left, candidate.right);
  }
  if (candidate.bottom < candidate.top) {
    std::swap(candidate.top, candidate.bottom);
  }

  if (candidate.left == 0 && candidate.top == 0 &&
      candidate.right == 0 && candidate.bottom == 0) {
    return false;
  }

  // The packed format is often point-like or very narrow, so do not reject
  // small widths. Only reject obviously unusable vertical geometry.
  if (candidate.bottom <= candidate.top) {
    return false;
  }

  *decodedRect = candidate;
  return true;
}

static RECT EncodePackedMinimizeRectTai(const RECT& rect) {
  RECT encoded{};
  encoded.left = PackSignedPointTai(rect.left, rect.top);
  encoded.top = PackSignedPointTai(rect.right, rect.bottom);
  encoded.right = 0;
  encoded.bottom = 0;
  return encoded;
}

static void CorrectMinimizeAnimationRectTai(HWND targetWindow, RECT* rect) {
  if (!targetWindow || !rect || IsRectEmpty(rect)) {
    return;
  }

  auto correction =
      GetMinimizeAnimationCorrectionForWindowTai(targetWindow);
  if (!correction) {
    return;
  }

  RECT before = *rect;

  LONG transformedLeft = TransformMinimizeRectXTai(rect->left, *correction);
  LONG transformedRight = TransformMinimizeRectXTai(rect->right, *correction);
  if (transformedRight < transformedLeft) {
    std::swap(transformedLeft, transformedRight);
  }

  rect->left = transformedLeft;
  rect->right = transformedRight;

  int measuredMatchTier = 0;
  const bool usedMeasuredButtonRect =
      TryUseMeasuredTaskbarButtonRectTai(
          targetWindow,
          rect,
          *correction,
          &measuredMatchTier);

  if (correction->hasClampXRect) {
    ClampRectCenterXToBoundsTai(rect, correction->clampXRect);
  }

  if (!EqualRect(&before, rect)) {
    Wh_Log(L"[MinRectFix] hwnd=%p before=(%ld,%ld,%ld,%ld) after=(%ld,%ld,%ld,%ld) mode=%s matchTier=%d offsetDip=%.2f scale=%.4f centerDip=%.2f dpiScale=%.3f measuredButtons=%zu",
           targetWindow,
           before.left,
           before.top,
           before.right,
           before.bottom,
           rect->left,
           rect->top,
           rect->right,
           rect->bottom,
           usedMeasuredButtonRect ? L"measured" : L"transform",
           measuredMatchTier,
           correction->layoutOffsetXDip,
           correction->visualScale,
           correction->scaleCenterXDip,
           correction->rasterizationScale,
           correction->measuredButtons.size());
  }
}

static bool TryCorrectShellHookMinRectMessageTai(UINT Msg, WPARAM wParam, LPARAM lParam) {
  if (!g_shellHookMessageTai || Msg != g_shellHookMessageTai ||
      wParam != HSHELL_GETMINRECT || !lParam ||
      g_minimizeAnimationCorrectionUninitializingTai.load(std::memory_order_acquire)) {
    return false;
  }

  SHELLHOOKINFO_TAI shellHookInfo{};
  if (!ReadShellHookInfoTai(lParam, &shellHookInfo)) {
    Wh_Log(L"[MinRectFix] skipped unreadable HSHELL_GETMINRECT payload");
    return true;
  }

  if (!shellHookInfo.hwnd || !IsWindow(shellHookInfo.hwnd)) {
    return true;
  }

  if (WasMinimizeAnimationPayloadAlreadyCorrectedTai(lParam, shellHookInfo)) {
    return true;
  }

  Wh_Log(L"[MinRectFix] HSHELL_GETMINRECT received target=%p raw=(%ld,%ld,%ld,%ld)",
         shellHookInfo.hwnd,
         shellHookInfo.rc.left,
         shellHookInfo.rc.top,
         shellHookInfo.rc.right,
         shellHookInfo.rc.bottom);

  if (!IsRectEmpty(&shellHookInfo.rc)) {
    RECT corrected = shellHookInfo.rc;
    CorrectMinimizeAnimationRectTai(shellHookInfo.hwnd, &corrected);
    if (!EqualRect(&shellHookInfo.rc, &corrected) && WriteShellHookRectTai(lParam, corrected)) {
      SHELLHOOKINFO_TAI remembered = shellHookInfo;
      remembered.rc = corrected;
      RememberMinimizeAnimationCorrectedPayloadTai(lParam, remembered);
    }
    return true;
  }

  RECT decodedPackedRect{};
  if (TryDecodePackedMinimizeRectTai(shellHookInfo, &decodedPackedRect)) {
    RECT before = decodedPackedRect;
    CorrectMinimizeAnimationRectTai(shellHookInfo.hwnd, &decodedPackedRect);

    if (!EqualRect(&before, &decodedPackedRect)) {
      RECT encoded = EncodePackedMinimizeRectTai(decodedPackedRect);
      if (WriteShellHookRectTai(lParam, encoded)) {
        SHELLHOOKINFO_TAI remembered = shellHookInfo;
        remembered.rc = encoded;
        RememberMinimizeAnimationCorrectedPayloadTai(lParam, remembered);
        Wh_Log(L"[MinRectFix] packed decoded before=(%ld,%ld,%ld,%ld) after=(%ld,%ld,%ld,%ld) encoded=(%ld,%ld,%ld,%ld)",
               before.left,
               before.top,
               before.right,
               before.bottom,
               decodedPackedRect.left,
               decodedPackedRect.top,
               decodedPackedRect.right,
               decodedPackedRect.bottom,
               encoded.left,
               encoded.top,
               encoded.right,
               encoded.bottom);
      }
    } else {
      Wh_Log(L"[MinRectFix] packed decoded no-op rect=(%ld,%ld,%ld,%ld)",
             decodedPackedRect.left,
             decodedPackedRect.top,
             decodedPackedRect.right,
             decodedPackedRect.bottom);
    }

    return true;
  }

  Wh_Log(L"[MinRectFix] skipped empty/unrecognized min rect payload");
  return true;
}

static void ClearMinimizeAnimationCorrectionForMonitorTai(const std::wstring& monitorName) {
  if (monitorName.empty()) {
    return;
  }

  std::shared_ptr<const MinimizeAnimationCorrectionTai> removedCorrection;
  {
    std::lock_guard<std::mutex> lock(g_minimizeAnimationCorrectionMutexTai);
    auto it = g_minimizeAnimationCorrectionByMonitorNameTai.find(monitorName);
    if (it != g_minimizeAnimationCorrectionByMonitorNameTai.end()) {
      removedCorrection = std::move(it->second);
      g_minimizeAnimationCorrectionByMonitorNameTai.erase(it);
    }
  }
}

static void SetMinimizeAnimationCorrectionForMonitorTai(
    const std::wstring& monitorName,
    const RECT& monitorRect,
    double layoutOffsetXDip,
    double visualScale,
    double scaleCenterXDip,
    double rasterizationScale,
    const RECT* clampXRect,
    std::vector<MinimizeAnimationMeasuredButtonTai> measuredButtons) {
  if (monitorName.empty() ||
      IsRectEmpty(&monitorRect) ||
      g_minimizeAnimationCorrectionUninitializingTai.load(std::memory_order_acquire)) {
    return;
  }

  if (!std::isfinite(layoutOffsetXDip) ||
      !std::isfinite(visualScale) ||
      !std::isfinite(scaleCenterXDip) ||
      !std::isfinite(rasterizationScale)) {
    return;
  }

  auto correction = std::make_shared<MinimizeAnimationCorrectionTai>();
  correction->layoutOffsetXDip = layoutOffsetXDip;
  correction->visualScale = std::clamp(visualScale, 0.01, 4.0);
  correction->scaleCenterXDip = scaleCenterXDip;
  correction->rasterizationScale = std::clamp(rasterizationScale, 0.25, 8.0);
  correction->monitorRect = monitorRect;

  if (clampXRect && !IsRectEmpty(clampXRect) &&
      clampXRect->right > clampXRect->left) {
    correction->clampXRect = *clampXRect;
    correction->hasClampXRect = true;
  }

  correction->measuredButtons = std::move(measuredButtons);

  std::shared_ptr<const MinimizeAnimationCorrectionTai> previousCorrection;
  {
    std::lock_guard<std::mutex> lock(g_minimizeAnimationCorrectionMutexTai);
    auto it = g_minimizeAnimationCorrectionByMonitorNameTai.find(monitorName);
    if (it != g_minimizeAnimationCorrectionByMonitorNameTai.end() &&
        it->second) {
      previousCorrection = it->second;
    }
    g_minimizeAnimationCorrectionByMonitorNameTai[monitorName] = correction;
  }

  bool shouldLog = true;
  if (previousCorrection) {
    const auto& previous = *previousCorrection;
    shouldLog =
        std::abs(previous.layoutOffsetXDip - correction->layoutOffsetXDip) > 0.01 ||
        std::abs(previous.visualScale - correction->visualScale) > 0.0001 ||
        std::abs(previous.scaleCenterXDip - correction->scaleCenterXDip) > 0.01 ||
        std::abs(previous.rasterizationScale - correction->rasterizationScale) > 0.0001 ||
        previous.hasClampXRect != correction->hasClampXRect ||
        previous.measuredButtons.size() != correction->measuredButtons.size() ||
        !EqualRect(&previous.monitorRect, &correction->monitorRect) ||
        (correction->hasClampXRect &&
         !EqualRect(&previous.clampXRect, &correction->clampXRect));
  }

  if (shouldLog) {
    Wh_Log(L"[MinRectFix] correction monitor=%s offsetDip=%.2f scale=%.4f centerDip=%.2f dpiScale=%.3f clamp=%d [%ld..%ld] measuredButtons=%zu",
           monitorName.c_str(),
           correction->layoutOffsetXDip,
           correction->visualScale,
           correction->scaleCenterXDip,
           correction->rasterizationScale,
           correction->hasClampXRect ? 1 : 0,
           correction->hasClampXRect ? correction->clampXRect.left : 0,
           correction->hasClampXRect ? correction->clampXRect.right : 0,
           correction->measuredButtons.size());
  }
}

static LRESULT WINAPI SendMessageW_HookTai(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
  LRESULT result = SendMessageW_OriginalTai
      ? SendMessageW_OriginalTai(hWnd, Msg, wParam, lParam)
      : 0;

  TryCorrectShellHookMinRectMessageTai(Msg, wParam, lParam);
  return result;
}

static void InitMinimizeAnimationCorrectionTai() {
  g_minimizeAnimationCorrectionUninitializingTai.store(false, std::memory_order_release);
  g_minimizeAnimationCorrectionReadyTai.store(false, std::memory_order_release);
  g_shellHookMessageTai = RegisterWindowMessageW(L"SHELLHOOK");

  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  if (user32) {
    auto sendMessageW = reinterpret_cast<SendMessageW_t>(GetProcAddress(user32, "SendMessageW"));
    if (sendMessageW) {
      if (WindhawkUtils::Wh_SetFunctionHookT(sendMessageW,
                                             SendMessageW_HookTai,
                                             &SendMessageW_OriginalTai)) {
        Wh_Log(L"[MinRectFix] Successfully hooked SendMessageW");
      } else {
        Wh_Log(L"[MinRectFix] Failed to hook SendMessageW");
      }
    }
  }

  g_minimizeAnimationCorrectionReadyTai.store(g_shellHookMessageTai != 0, std::memory_order_release);
  Wh_Log(L"[MinRectFix] initialized without persistent window subclassing");
}

static void UninitMinimizeAnimationCorrectionTai() {
  g_minimizeAnimationCorrectionUninitializingTai.store(true, std::memory_order_release);
  g_minimizeAnimationCorrectionReadyTai.store(false, std::memory_order_release);

  decltype(g_minimizeAnimationCorrectionByMonitorNameTai) correctionsToDestroy;
  {
    std::lock_guard<std::mutex> lock(g_minimizeAnimationCorrectionMutexTai);
    correctionsToDestroy.swap(
        g_minimizeAnimationCorrectionByMonitorNameTai);
  }

  g_shellHookMessageTai = 0;
}

std::wstring EscapeXmlAttribute(std::wstring_view data) {
  std::wstring buffer;
  buffer.reserve(data.size());
  for (wchar_t c : data) buffer.append((c == L'&') ? L"&amp;" : (c == L'\"') ? L"&quot;" : (c == L'<') ? L"&lt;" : (c == L'>') ? L"&gt;" : std::wstring(1, c));
  return buffer;
}

Style GetStyleFromXamlSetters(const std::wstring_view type, const std::wstring_view xamlStyleSetters, std::wstring& outXaml) {
  std::wstring xaml =
      LR"(<ResourceDictionary
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    xmlns:d="http://schemas.microsoft.com/expression/blend/2008"
    xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006"
    xmlns:muxc="using:Microsoft.UI.Xaml.Controls")";
  if (auto pos = type.rfind('.'); pos != type.npos) {
    auto typeNamespace = std::wstring_view(type).substr(0, pos);
    auto typeName = std::wstring_view(type).substr(pos + 1);
    xaml += L"\n    xmlns:windhawkstyler=\"using:";
    xaml += EscapeXmlAttribute(typeNamespace);
    xaml += L"\">\n    <Style TargetType=\"windhawkstyler:";
    xaml += EscapeXmlAttribute(typeName);
    xaml += L"\">\n";
  } else {
    xaml += L">\n    <Style TargetType=\"";
    xaml += EscapeXmlAttribute(type);
    xaml += L"\">\n";
  }
  xaml += xamlStyleSetters;
  xaml +=
      L"    </Style>\n"
      L"</ResourceDictionary>";
  outXaml = xaml;
  auto resourceDictionary = Markup::XamlReader::Load(xaml).as<ResourceDictionary>();
  auto [styleKey, styleInspectable] = resourceDictionary.First().Current();
  return styleInspectable.as<Style>();
}

void SetElementPropertyFromString(FrameworkElement obj, const std::wstring& type, const std::wstring& propertyName, const std::wstring& propertyValue, bool isXamlValue) {
  if(!obj) return;
  std::wstring outXamlResult;
  try {
    std::wstring xamlSetter = L"<Setter Property=\"";
    xamlSetter += EscapeXmlAttribute(propertyName);
    xamlSetter += L"\"";
    if (isXamlValue) {
      xamlSetter +=
          L">\n"
          L"    <Setter.Value>\n";
      xamlSetter += propertyValue;
      xamlSetter += L"\n    </Setter.Value>\n";
      xamlSetter += L"</Setter>";
    } else {
      xamlSetter += L" Value=\"";
      xamlSetter += EscapeXmlAttribute(propertyValue);
      xamlSetter += L"\"/>";
    }
    auto style = GetStyleFromXamlSetters(type, xamlSetter, outXamlResult);
    for (uint32_t i = 0; i < style.Setters().Size(); ++i) {
      auto setter = style.Setters().GetAt(i).as<Setter>();
      obj.SetValue(setter.Property(), setter.Value());
    }
  } catch (const std::exception& ex) {
    if (!outXamlResult.empty()) {
      Wh_Log(L"Error: %S. Xaml Result: %s", ex.what(), outXamlResult.c_str());
    } else {
      Wh_Log(L"Error: %S", ex.what());
    }
  } catch (const winrt::hresult_error& ex) {
    if (!outXamlResult.empty()) {
      Wh_Log(L"Error %08X: %s. Xaml Result: %s", ex.code(), ex.message().c_str(), outXamlResult.c_str());
    } else {
      Wh_Log(L"Error %08X: %s", ex.code(), ex.message().c_str());
    }
  } catch (...) {
    if (!outXamlResult.empty()) {
      Wh_Log(L"Unknown error occurred while setting property. Xaml Result: %s", outXamlResult.c_str());
    } else {
      Wh_Log(L"Unknown error occurred while setting property.");
    }
  }
}

void SetElementPropertyFromString(FrameworkElement obj, const std::wstring& type, const std::wstring& propertyName, const std::wstring& propertyValue) { return SetElementPropertyFromString(obj, type, propertyName, propertyValue, false); }

std::vector<std::wstring> SplitAndTrim(PCWSTR input) {
  std::vector<std::wstring> result;

  if (!input || *input == L'\0') {
    return result;
  }
  std::wstringstream ss(input);
  std::wstring item;
  while (std::getline(ss, item, L';')) {
    size_t start = item.find_first_not_of(L" \t");
    size_t end = item.find_last_not_of(L" \t");

    if (start != std::wstring::npos && end != std::wstring::npos) {
      std::wstring trimmed = item.substr(start, end - start + 1);
      if (!trimmed.empty()) {
        result.push_back(trimmed);
      }
    }
  }
  return result;
}

void CompileDividedAppPatternsTai(
    std::vector<std::wstring> const& patterns) {
  g_settings.compiledDividedAppPatterns.clear();
  g_settings.compiledDividedAppPatterns.reserve(patterns.size());
  for (const auto& pattern : patterns) {
    try {
      g_settings.compiledDividedAppPatterns.emplace_back(
          pattern,
          std::regex_constants::icase |
              std::regex_constants::optimize);
    } catch (const std::regex_error&) {
      Wh_Log(L"Invalid DividedAppNames regex ignored: %s", pattern.c_str());
    }
  }
}

bool MatchesDividedAppPatternTai(
    std::wstring const& value,
    std::wregex const& pattern) {
  try {
    return std::regex_search(value, pattern);
  } catch (const std::regex_error&) {
    return false;
  }
}


std::atomic<bool> g_scheduled_low_priority_update = false;
std::atomic<bool> g_delayed_apply_worker_running = false;
std::atomic<int64_t> g_delayed_apply_due_ms = 0;
std::atomic<unsigned long long> g_delayed_apply_generation = 0;
std::atomic<bool> g_initial_style_apply_completed = false;
std::atomic<bool> g_initial_taskbar_size_apply_done = false;
std::atomic<int64_t> g_initial_style_apply_not_before_ms = 0;
std::atomic<int> g_force_style_apply_passes = 0;
std::atomic<int> g_high_priority_dispatch_passes = 0;
std::atomic<int> g_reset_animation_target_passes = 0;
std::atomic<int64_t> g_last_geometry_critical_apply_ms = 0;
std::atomic<bool> g_animation_followup_worker_running = false;
std::atomic<int64_t> g_suppress_low_priority_apply_until_ms = 0;
std::atomic<bool> g_worker_threads_stopping = false;
std::mutex g_delayed_apply_worker_thread_mutex;
std::thread g_delayed_apply_worker_thread;
std::mutex g_delayed_apply_worker_wait_mutex;
std::condition_variable g_delayed_apply_worker_wake;
std::mutex g_animation_followup_worker_thread_mutex;
std::thread g_animation_followup_worker_thread;
constexpr int kDefaultStyleDebounceDelayMs = 150;
constexpr int kTaskbarIslandAnimationDurationMs = 250;
constexpr int kStartButtonAnchorStablePassesRequired = 2;
constexpr double kTaskbarVirtualSurfaceMaxPhysicalWidth =
    static_cast<double>(std::numeric_limits<SHORT>::max()) / 2.0;
constexpr int kLowPriorityStyleDelayMs =
    kDefaultStyleDebounceDelayMs + (kTaskbarIslandAnimationDurationMs * 3);
constexpr int kExplorerStartupSettleAnimationWindows = 6;
constexpr int kInitialExplorerStyleDelayMs =
    kLowPriorityStyleDelayMs + kDefaultStyleDebounceDelayMs +
    (kTaskbarIslandAnimationDurationMs * kExplorerStartupSettleAnimationWindows);
constexpr int kGeometryCriticalApplyMinIntervalMs = kDefaultStyleDebounceDelayMs / 2;
constexpr int kInitialExplorerStyleRetryDelayMs = kTaskbarIslandAnimationDurationMs * 2;
constexpr int kButtonSizeLowPrioritySuppressionMs = kTaskbarIslandAnimationDurationMs;
constexpr int kGeometryCriticalLowPrioritySuppressionMs =
    (kTaskbarIslandAnimationDurationMs * 2) + kDefaultStyleDebounceDelayMs;
constexpr int kScheduledLowPriorityFlagTtlMs =
    kLowPriorityStyleDelayMs + kDefaultStyleDebounceDelayMs + kTaskbarIslandAnimationDurationMs;
constexpr double kMinimumTrustedRefreshHz = 24.0;
constexpr double kMaximumTrustedRefreshHz = 1000.0;
constexpr int kDefaultFrameIntervalMs = 16;
constexpr int kMinimumFrameIntervalMs = 1;
constexpr int kMaximumFrameIntervalMs = 42;
constexpr int kAnimationFollowupGraceFrames = 2;
constexpr float kDisplayGeometryChangeToleranceDip = 0.5f;
constexpr float kDisplayRasterizationChangeTolerance = 0.001f;
template <typename TAnimation>
void ConfigureTaskbarIslandAnimation(TAnimation const& animation) {
  animation.Duration(winrt::Windows::Foundation::TimeSpan(std::chrono::milliseconds(kTaskbarIslandAnimationDurationMs)));
  animation.DelayTime(winrt::Windows::Foundation::TimeSpan(std::chrono::milliseconds(0)));
}

winrt::Windows::UI::Composition::CompositionEasingFunction CreateTaskbarIslandEasingFunction(
    winrt::Windows::UI::Composition::Compositor const& compositor) {
  if (!compositor) {
    return nullptr;
  }

  // Keep every moving piece on the same curve. The explicit ease-out avoids
  // one visual catching up linearly while another decelerates, which reads as
  // rubber-banding when the taskbar island is resized, translated, and scaled
  // at the same time.
  return compositor.CreateCubicBezierEasingFunction(
      winrt::Windows::Foundation::Numerics::float2{0.16f, 1.0f},
      winrt::Windows::Foundation::Numerics::float2{0.30f, 1.0f});
}

template <typename TAnimation, typename TValue>
void InsertTaskbarIslandKeyFrame(TAnimation const& animation,
                                 float progress,
                                 TValue const& value) {
  if (auto compositor = animation.Compositor()) {
    if (auto easing = CreateTaskbarIslandEasingFunction(compositor)) {
      animation.InsertKeyFrame(progress, value, easing);
      return;
    }
  }

  animation.InsertKeyFrame(progress, value);
}

float ApplyTaskbarIslandEasingForEstimate(float progress) {
  progress = std::clamp(progress, 0.0f, 1.0f);
  // Approximation of the compositor ease-out used above. This is only for
  // calculating a safe start value when an in-flight background shape animation
  // is retargeted; all real animations use the compositor easing function.
  const float inv = 1.0f - progress;
  return 1.0f - (inv * inv * inv);
}

float LerpFloat(float from, float to, float progress) {
  return from + ((to - from) * progress);
}
float EstimateAnimationValue(float from, float to, int64_t startedMs, int64_t nowMs) {
  if (startedMs <= 0 || nowMs <= startedMs) {
    return from;
  }
  float progress = static_cast<float>(nowMs - startedMs) / static_cast<float>(kTaskbarIslandAnimationDurationMs);
  if (progress <= 0.0f) {
    return from;
  }
  if (progress >= 1.0f) {
    return to;
  }
   return LerpFloat(from, to, ApplyTaskbarIslandEasingForEstimate(progress));
}

static float SnapToPhysicalPixel(float value, float rasterizationScale);

float SnapScaleForPhysicalPixels(float scale,
                                 float unscaledWidth,
                                 float rasterizationScale) {
  if (unscaledWidth <= 0.0f || !std::isfinite(unscaledWidth) ||
      !std::isfinite(scale)) {
    return 1.0f;
  }

  const float snappedWidth =
      SnapToPhysicalPixel(unscaledWidth * scale, rasterizationScale);
  if (snappedWidth <= 0.0f || !std::isfinite(snappedWidth)) {
    return scale;
  }

  return snappedWidth / unscaledWidth;
}

float CalculateTaskbarIslandScale(float screenLeft,
                                  float screenRight,
                                  float screenWidth,
                                  float scaleCenterX,
                                  float rasterizationScale) {
  if (screenWidth <= 0.0f || screenRight <= screenLeft ||
      !std::isfinite(screenLeft) || !std::isfinite(screenRight) ||
      !std::isfinite(screenWidth) || !std::isfinite(scaleCenterX)) {
    return 1.0f;
  }

  const float unscaledWidth = screenRight - screenLeft;
  float targetScale = 1.0f;

  // Scale only as much as needed to keep the island inside the current screen.
  // This is intentionally not limited by a user setting: once the task area is
  // wider than the monitor, shrinking to any size is safer than allowing
  // Explorer's native overflow layout to appear and destabilize the taskbar.
  if (screenLeft < 0.0f && scaleCenterX > screenLeft) {
    targetScale = std::min(targetScale,
                           scaleCenterX / (scaleCenterX - screenLeft));
  }
  if (screenRight > screenWidth && screenRight > scaleCenterX) {
    targetScale = std::min(targetScale,
                           (screenWidth - scaleCenterX) /
                               (screenRight - scaleCenterX));
  }

  if (!std::isfinite(targetScale) || targetScale <= 0.0f) {
    targetScale = screenWidth / unscaledWidth;
  }
  if (!std::isfinite(targetScale) || targetScale <= 0.0f) {
    return 1.0f;
  }

  targetScale = std::min(targetScale, 1.0f);
  const float snappedScale = SnapScaleForPhysicalPixels(targetScale,
                                                        unscaledWidth,
                                                        rasterizationScale);
  if (std::isfinite(snappedScale) && snappedScale > 0.0f) {
    // Pixel snapping can round the scaled width up by a fraction of a pixel.
    // Never let snapping pick a larger scale than the geometric fit.
    targetScale = std::min(targetScale, snappedScale);
  }
  return std::min(targetScale, 1.0f);
}
float ApplyScaleToScreenX(float screenX, float scaleCenterX, float scale) {
  return scaleCenterX + ((screenX - scaleCenterX) * scale);
}

void SetVisualScaleCenterAndAnimate(
    winrt::Windows::UI::Composition::Visual const& visual,
    float targetScale,
    float localCenterX,
    float localCenterY,
    float visualOffsetTolerance,
    bool animate) {
  if (!visual) {
    return;
  }

  if (!std::isfinite(targetScale) || targetScale <= 0.0f) {
    targetScale = 1.0f;
  }
  if (!std::isfinite(localCenterX)) {
    localCenterX = 0.0f;
  }
  if (!std::isfinite(localCenterY)) {
    localCenterY = 0.0f;
  }

  visual.CenterPoint({localCenterX, localCenterY, visual.CenterPoint().z});

  const auto currentScale = visual.Scale();
  if (std::abs(currentScale.x - targetScale) <= visualOffsetTolerance &&
      std::abs(currentScale.y - targetScale) <= visualOffsetTolerance) {
    return;
  }

  if (animate) {
    if (auto compositor = visual.Compositor()) {
      auto scaleAnimation = compositor.CreateVector3KeyFrameAnimation();
      ConfigureTaskbarIslandAnimation(scaleAnimation);
      InsertTaskbarIslandKeyFrame(
          scaleAnimation,
          1.0f,
          winrt::Windows::Foundation::Numerics::float3{
              targetScale, targetScale, currentScale.z});
      visual.StartAnimation(L"Scale", scaleAnimation);
      return;
    }
  }

  visual.StopAnimation(L"Scale");
  visual.Scale({targetScale, targetScale, currentScale.z});
}
void ResetBackgroundVisualTargetCache(TaskbarState& state) {
  state.lastBackgroundShapeTargetWidth = 0.0f;
  state.lastBackgroundShapeTargetHeight = 0.0f;
  state.lastBackgroundShapeTargetOffsetX = 0.0f;
  state.lastBackgroundShapeTargetOffsetY = 0.0f;
  state.backgroundAnimationFromWidth = 0.0f;
  state.backgroundAnimationToWidth = 0.0f;
  state.backgroundAnimationFromOffsetX = 0.0f;
  state.backgroundAnimationToOffsetX = 0.0f;
  state.backgroundAnimationFromOffsetY = 0.0f;
  state.backgroundAnimationToOffsetY = 0.0f;
  state.backgroundAnimationStartMs = 0;
}

void ResetBackgroundVisualCache(TaskbarState& state) {
  ResetBackgroundVisualTargetCache(state);
  state.backgroundFillIdentity = 0;
  state.lastBackgroundStyleGeneration = 0;
}

struct TaskbarBackgroundCompositionResourcesTai {
  winrt::Windows::UI::Composition::CompositionRoundedRectangleGeometry clipGeometry{nullptr};
  winrt::Windows::UI::Composition::CompositionGeometricClip clip{nullptr};
  winrt::Windows::UI::Composition::ShapeVisual borderVisual{nullptr};
  winrt::Windows::UI::Composition::CompositionRoundedRectangleGeometry borderGeometry{nullptr};
  winrt::Windows::UI::Composition::CompositionSpriteShape borderShape{nullptr};
  winrt::Windows::UI::Composition::CompositionColorBrush borderBrush{nullptr};
};

bool TryGetTaskbarBackgroundCompositionResourcesTai(
    FrameworkElement const& backgroundFillChild,
    winrt::Windows::UI::Composition::Visual const& backgroundFillVisual,
    TaskbarBackgroundCompositionResourcesTai* resources) {
  if (!backgroundFillChild || !backgroundFillVisual || !resources) {
    return false;
  }

  try {
    auto clip = backgroundFillVisual.Clip().try_as<
        winrt::Windows::UI::Composition::CompositionGeometricClip>();
    auto clipGeometry = clip
        ? clip.Geometry().try_as<
              winrt::Windows::UI::Composition::CompositionRoundedRectangleGeometry>()
        : nullptr;
    auto borderVisual =
        winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::
            GetElementChildVisual(backgroundFillChild)
                .try_as<winrt::Windows::UI::Composition::ShapeVisual>();
    if (!clipGeometry || !borderVisual || borderVisual.Shapes().Size() != 1) {
      return false;
    }

    auto borderShape = borderVisual.Shapes()
        .GetAt(0)
        .try_as<winrt::Windows::UI::Composition::CompositionSpriteShape>();
    auto borderGeometry = borderShape
        ? borderShape.Geometry().try_as<
              winrt::Windows::UI::Composition::CompositionRoundedRectangleGeometry>()
        : nullptr;
    auto borderBrush = borderShape
        ? borderShape.StrokeBrush().try_as<
              winrt::Windows::UI::Composition::CompositionColorBrush>()
        : nullptr;
    if (!borderShape || !borderGeometry || !borderBrush) {
      return false;
    }

    resources->clipGeometry = clipGeometry;
    resources->clip = clip;
    resources->borderVisual = borderVisual;
    resources->borderGeometry = borderGeometry;
    resources->borderShape = borderShape;
    resources->borderBrush = borderBrush;
    return true;
  } catch (...) {
    return false;
  }
}

bool CreateTaskbarBackgroundCompositionResourcesTai(
    FrameworkElement const& backgroundFillChild,
    winrt::Windows::UI::Composition::Visual const& backgroundFillVisual,
    winrt::Windows::UI::Composition::Compositor const& compositor,
    TaskbarBackgroundCompositionResourcesTai* resources) {
  if (!backgroundFillChild || !backgroundFillVisual || !compositor ||
      !resources) {
    return false;
  }

  try {
    resources->clipGeometry = compositor.CreateRoundedRectangleGeometry();
    resources->clip = compositor.CreateGeometricClip(resources->clipGeometry);
    resources->borderVisual = compositor.CreateShapeVisual();
    resources->borderGeometry = compositor.CreateRoundedRectangleGeometry();
    resources->borderShape =
        compositor.CreateSpriteShape(resources->borderGeometry);
    resources->borderBrush = compositor.CreateColorBrush();
    resources->borderShape.StrokeBrush(resources->borderBrush);
    resources->borderShape.FillBrush(nullptr);
    resources->borderVisual.Shapes().Append(resources->borderShape);
    backgroundFillVisual.Clip(resources->clip);
    winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::
        SetElementChildVisual(backgroundFillChild, resources->borderVisual);
    return true;
  } catch (...) {
    *resources = {};
    return false;
  }
}

void ResetAnimationTargetCache(TaskbarState& state) {
  state.hasLastTargetTaskFrameOffsetX = false;
  state.hasLastTargetTaskbarIslandScale = false;
  state.lastTargetTaskbarIslandScale = 1.0f;
  state.lastTaskbarIslandScaleCenterX = 0.0f;
  state.hasLastTargetTrayOffsetX = false;
  state.hasLastTargetWidgetOffset = false;
  state.hasLastStartButtonAnchorRect = false;
  state.hasStableStartButtonAnchorRect = false;
  state.startButtonAnchorStablePasses = 0;
  ResetBackgroundVisualTargetCache(state);
}
bool CheckAndUpdateDisplayGeometrySignature(TaskbarState& state,
                                            FrameworkElement const& xamlRootContent,
                                            FrameworkElement const& taskFrame,
                                            float rasterizationScale) {
  const float rootWidth = static_cast<float>(xamlRootContent.ActualWidth());
  const float rootHeight = static_cast<float>(xamlRootContent.ActualHeight());
  const float taskFrameWidth = taskFrame ? static_cast<float>(taskFrame.ActualWidth()) : 0.0f;
  const float taskFrameHeight = taskFrame ? static_cast<float>(taskFrame.ActualHeight()) : 0.0f;

  const bool validSignature =
      std::isfinite(rootWidth) && rootWidth > 0.0f &&
      std::isfinite(rootHeight) && rootHeight > 0.0f &&
      std::isfinite(rasterizationScale) && rasterizationScale > 0.0f;
  if (!validSignature) {
    return false;
  }

  const bool changed =
      !state.hasLastDisplayGeometrySignature ||
      std::abs(state.lastObservedRootWidth - rootWidth) > kDisplayGeometryChangeToleranceDip ||
      std::abs(state.lastObservedRootHeight - rootHeight) > kDisplayGeometryChangeToleranceDip ||
      std::abs(state.lastObservedRasterizationScale - rasterizationScale) > kDisplayRasterizationChangeTolerance ||
      std::abs(state.lastObservedTaskFrameWidth - taskFrameWidth) > kDisplayGeometryChangeToleranceDip ||
      std::abs(state.lastObservedTaskFrameHeight - taskFrameHeight) > kDisplayGeometryChangeToleranceDip;

  state.lastObservedRootWidth = rootWidth;
  state.lastObservedRootHeight = rootHeight;
  state.lastObservedRasterizationScale = rasterizationScale;
  state.lastObservedTaskFrameWidth = taskFrameWidth;
  state.lastObservedTaskFrameHeight = taskFrameHeight;
  state.hasLastDisplayGeometrySignature = true;

  return changed;
}
void ApplySettings(HWND hTaskbarWnd);
int64_t DelayedApplyNowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
int GetCompositionFrameIntervalMs(HWND hwnd) {
  DWM_TIMING_INFO timing{};
  timing.cbSize = sizeof(timing);
  if (SUCCEEDED(DwmGetCompositionTimingInfo(hwnd, &timing)) &&
      timing.rateRefresh.uiNumerator > 0 &&
      timing.rateRefresh.uiDenominator > 0) {
    const double detectedRefreshHz =
        static_cast<double>(timing.rateRefresh.uiNumerator) /
        static_cast<double>(timing.rateRefresh.uiDenominator);
    const double clampedRefreshHz = std::clamp(detectedRefreshHz,
                                               kMinimumTrustedRefreshHz,
                                               kMaximumTrustedRefreshHz);
    return ClampInt(static_cast<int>((1000.0 / clampedRefreshHz) + 0.5),
                    kMinimumFrameIntervalMs,
                    kMaximumFrameIntervalMs);
  }
  return kDefaultFrameIntervalMs;
}
int GetTaskbarIslandFollowupPassCount(HWND hwnd) {
  const int frameIntervalMs = GetCompositionFrameIntervalMs(hwnd);
  const int followupWindowMs =
      kTaskbarIslandAnimationDurationMs +
      (frameIntervalMs * kAnimationFollowupGraceFrames);
  return std::max(1, (followupWindowMs + frameIntervalMs - 1) / frameIntervalMs);
}
void ArmStyleApplyPasses(int passCount, bool resetAnimationTargets = false) {
  passCount = std::max(1, passCount);
  g_force_style_apply_passes.store(passCount);
  g_high_priority_dispatch_passes.store(passCount);
  if (resetAnimationTargets) {
    g_reset_animation_target_passes.store(passCount);
  }
}
void ArmStyleFollowupPasses(HWND hwnd, bool resetAnimationTargets = false) {
  ArmStyleApplyPasses(GetTaskbarIslandFollowupPassCount(hwnd), resetAnimationTargets);
}
void ArmSingleStyleApplyPass(bool resetAnimationTargets = false) {
  ArmStyleApplyPasses(1, resetAnimationTargets);
}
bool WaitForConditionWithTimeout(std::function<bool()> condition,
                                 int timeoutMs,
                                 int pollIntervalMs) {
  const int64_t startMs = DelayedApplyNowMs();
  while (!condition()) {
    if (g_unloading && timeoutMs > 0) {
      // During unload we still wait briefly for workers/hooks to exit, but never
      // spin forever inside code that may be unloaded by recompilation.
    }
    const int64_t elapsedMs = DelayedApplyNowMs() - startMs;
    if (elapsedMs >= timeoutMs) {
      return false;
    }
    Sleep(static_cast<DWORD>(std::max(1, pollIntervalMs)));
  }
  return true;
}
void QueueTaskbarAnimationFollowup(HWND hTaskbarWnd) {
  if (g_unloading || g_worker_threads_stopping.load() ||
      !hTaskbarWnd || !IsWindow(hTaskbarWnd)) {
    return;
  }
  bool expected = false;
  if (!g_animation_followup_worker_running.compare_exchange_strong(expected, true)) {
    return;
  }

  std::lock_guard<std::mutex> lock(g_animation_followup_worker_thread_mutex);
  if (g_unloading || g_worker_threads_stopping.load()) {
    g_animation_followup_worker_running = false;
    return;
  }
  if (g_animation_followup_worker_thread.joinable()) {
    g_animation_followup_worker_thread.join();
  }

  try {
    g_animation_followup_worker_thread = std::thread([hTaskbarWnd]() {
      struct FollowupWorkerGuard {
        ~FollowupWorkerGuard() { g_animation_followup_worker_running = false; }
      } followupWorkerGuard;
      try {
        const int frameIntervalMs = GetCompositionFrameIntervalMs(hTaskbarWnd);
        const int followupWindowMs =
            kTaskbarIslandAnimationDurationMs +
            (frameIntervalMs * kAnimationFollowupGraceFrames);
        int elapsedMs = 0;
        while (elapsedMs < followupWindowMs) {
          Sleep(static_cast<DWORD>(frameIntervalMs));
          elapsedMs += frameIntervalMs;
          if (g_unloading || g_worker_threads_stopping.load() ||
              !hTaskbarWnd || !IsWindow(hTaskbarWnd)) {
            break;
          }
          ArmSingleStyleApplyPass();
          ApplySettings(hTaskbarWnd);
        }
      } catch (winrt::hresult_error const& ex) {
        Wh_Log(L"Animation follow-up worker failed %08X: %s", ex.code(), ex.message().c_str());
      } catch (...) {
        Wh_Log(L"Animation follow-up worker failed: %08X", winrt::to_hresult());
      }
    });
  } catch (std::exception const& ex) {
    g_animation_followup_worker_running = false;
    Wh_Log(L"Failed to create animation follow-up worker: %S", ex.what());
  } catch (...) {
    g_animation_followup_worker_running = false;
    Wh_Log(L"Failed to create animation follow-up worker");
  }
}

void DelayedApplyWorker();
void EnsureDelayedApplyWorker() {
  if (g_unloading || g_worker_threads_stopping.load()) {
    return;
  }

  bool expected = false;
  if (g_delayed_apply_worker_running.compare_exchange_strong(expected, true)) {
    std::lock_guard<std::mutex> lock(g_delayed_apply_worker_thread_mutex);
    if (g_unloading || g_worker_threads_stopping.load()) {
      g_delayed_apply_worker_running = false;
      return;
    }
    if (g_delayed_apply_worker_thread.joinable()) {
      g_delayed_apply_worker_thread.join();
    }
    try {
      g_delayed_apply_worker_thread = std::thread(DelayedApplyWorker);
    } catch (std::exception const& ex) {
      g_delayed_apply_worker_running = false;
      Wh_Log(L"Failed to create delayed apply worker: %S", ex.what());
      return;
    } catch (...) {
      g_delayed_apply_worker_running = false;
      Wh_Log(L"Failed to create delayed apply worker");
      return;
    }
  }

  g_delayed_apply_worker_wake.notify_one();
}

void RequestTaskbarButtonSizeRelayout() {
  if (g_unloading) {
    return;
  }

  // Taskbar buttons are virtualized/recycled. Instead of forcing an arbitrary
  // number of relayout passes, mark button widths as customized and kick one
  // immediate style pass. Every realized/recycled button is validated in the
  // normal taskbar child loop and fixed only if its width is wrong.
  g_taskbarButtonWidthCustomized = true;
  ArmSingleStyleApplyPass(true);
  g_scheduled_low_priority_update = false;
  g_suppress_low_priority_apply_until_ms =
      DelayedApplyNowMs() + kButtonSizeLowPrioritySuppressionMs;

  HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
  if (hTaskbarWnd && IsWindow(hTaskbarWnd)) {
    ApplySettings(hTaskbarWnd);
  }
}

void ArmInitialExplorerStyleApplyDelay() {
  g_initial_style_apply_completed = false;
  g_initial_taskbar_size_apply_done = false;
  g_initial_style_apply_not_before_ms =
      DelayedApplyNowMs() + kInitialExplorerStyleDelayMs;
  Wh_Log(L"Initial Explorer style apply armed");
}
bool InitializeDebounce() {
  // Kept as a compatibility shim for older call sites. The old DispatcherTimer
  // debounce was removed because timer creation/stop could race Explorer/XAML
  // initialization and crash. Scheduling is now handled by DelayedApplyWorker.
  g_worker_threads_stopping = false;
  g_already_requested_debounce_initializing = false;
  return true;
}

void CleanupDebounce() {
  g_worker_threads_stopping = true;
  g_already_requested_debounce_initializing = false;
  g_scheduled_low_priority_update = false;
  g_delayed_apply_due_ms = 0;
  g_delayed_apply_generation.fetch_add(1);
  g_delayed_apply_worker_wake.notify_all();

  std::thread animationFollowupWorker;
  {
    std::lock_guard<std::mutex> lock(g_animation_followup_worker_thread_mutex);
    animationFollowupWorker = std::move(g_animation_followup_worker_thread);
  }

  std::thread delayedApplyWorker;
  {
    std::lock_guard<std::mutex> lock(g_delayed_apply_worker_thread_mutex);
    delayedApplyWorker = std::move(g_delayed_apply_worker_thread);
  }

  if (animationFollowupWorker.joinable()) {
    animationFollowupWorker.join();
  }
  if (delayedApplyWorker.joinable()) {
    delayedApplyWorker.join();
  }

  g_animation_followup_worker_running = false;
  g_delayed_apply_worker_running = false;
}
void DelayedApplyWorker() {
  struct DelayedWorkerGuard {
    ~DelayedWorkerGuard() {
      g_delayed_apply_worker_running = false;
    }
  } delayedWorkerGuard;

  std::unique_lock<std::mutex> waitLock(g_delayed_apply_worker_wait_mutex);
  try {
    for (;;) {
      if (g_unloading || g_worker_threads_stopping.load()) {
        break;
      }

      int64_t dueMs = g_delayed_apply_due_ms.load();
      if (dueMs <= 0) {
        g_delayed_apply_worker_wake.wait(waitLock, [] {
          return g_unloading || g_worker_threads_stopping.load() ||
                 g_delayed_apply_due_ms.load() > 0;
        });
        continue;
      }

      int64_t nowMs = DelayedApplyNowMs();
      if (nowMs < dueMs) {
        g_delayed_apply_worker_wake.wait_for(
            waitLock,
            std::chrono::milliseconds(std::max<int64_t>(1, dueMs - nowMs)),
            [dueMs] {
              return g_unloading || g_worker_threads_stopping.load() ||
                     g_delayed_apply_due_ms.load() != dueMs;
            });
        continue;
      }

      unsigned long long generation = g_delayed_apply_generation.load();
      waitLock.unlock();

      HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
      if (!hTaskbarWnd || !IsWindow(hTaskbarWnd)) {
        Wh_Log(L"Delayed apply postponed: taskbar window is not ready");
        g_delayed_apply_generation.fetch_add(1);
        g_delayed_apply_due_ms = DelayedApplyNowMs() + kInitialExplorerStyleRetryDelayMs;
        waitLock.lock();
        continue;
      }

      g_scheduled_low_priority_update = false;
      Wh_Log(L"Delayed apply triggered");
      const bool initialStyleApply = !g_initial_style_apply_completed.load();
      if (initialStyleApply) {
        ArmStyleFollowupPasses(hTaskbarWnd, true);
      }
      if (!g_initial_style_apply_completed.load() &&
          !g_initial_taskbar_size_apply_done.exchange(true)) {
        ApplySettingsTBIconSize(g_settings_tbiconsize.taskbarHeight);
      }
      ApplySettings(hTaskbarWnd);
      if (!g_initial_style_apply_completed.load() && !g_unloading) {
        Wh_Log(L"Initial ApplyStyle did not complete; retrying delayed apply");
        g_delayed_apply_generation.fetch_add(1);
        g_delayed_apply_due_ms = DelayedApplyNowMs() + kInitialExplorerStyleRetryDelayMs;
        waitLock.lock();
        continue;
      }

      if (g_delayed_apply_generation.load() == generation) {
        int64_t expectedDueMs = dueMs;
        g_delayed_apply_due_ms.compare_exchange_strong(expectedDueMs, 0);
      }
      waitLock.lock();
    }
  } catch (winrt::hresult_error const& ex) {
    Wh_Log(L"Delayed apply worker failed %08X: %s", ex.code(), ex.message().c_str());
  } catch (...) {
    Wh_Log(L"Delayed apply worker failed: %08X", winrt::to_hresult());
  }
}
void ApplySettingsDebounced(int delayMs) {
  if (g_unloading) {
    return;
  }
  if (delayMs <= 0) {
    delayMs = kLowPriorityStyleDelayMs;
  }
  if (delayMs < 1) {
    delayMs = kDefaultStyleDebounceDelayMs;
  }
  int64_t nowMs = DelayedApplyNowMs();
  int64_t dueMs = nowMs + delayMs;
  int64_t initialNotBeforeMs = g_initial_style_apply_not_before_ms.load();
  if (!g_initial_style_apply_completed.load() && initialNotBeforeMs > dueMs) {
    dueMs = initialNotBeforeMs;
  }
  g_delayed_apply_generation.fetch_add(1);
  g_delayed_apply_due_ms = dueMs;
  Wh_Log(L"Scheduled delayed apply in %lld ms", static_cast<long long>(std::max<int64_t>(0, dueMs - nowMs)));
  EnsureDelayedApplyWorker();
}
void ApplySettingsDebounced() {
  ApplySettingsDebounced(kDefaultStyleDebounceDelayMs);
}
void ApplySettingsFromTaskbarThreadImmediately() {
  if (g_unloading) {
    return;
  }
  int64_t initialNotBeforeMs = g_initial_style_apply_not_before_ms.load();
  if (!g_initial_style_apply_completed.load() && initialNotBeforeMs > DelayedApplyNowMs()) {
    ApplySettingsDebounced(1);
    return;
  }
  g_scheduled_low_priority_update = false;
  g_delayed_apply_due_ms = 0;
  g_delayed_apply_generation.fetch_add(1);
  HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
  ArmStyleFollowupPasses(hTaskbarWnd);
  g_suppress_low_priority_apply_until_ms =
      DelayedApplyNowMs() + kGeometryCriticalLowPrioritySuppressionMs;
  if (!hTaskbarWnd || !IsWindow(hTaskbarWnd)) {
    Wh_Log(L"Immediate apply skipped: taskbar window is not ready");
    return;
  }
  Wh_Log(L"Immediate taskbar animation apply");
  ApplySettings(hTaskbarWnd);
  QueueTaskbarAnimationFollowup(hTaskbarWnd);
}
void ApplySettingsFromTaskbarThreadGeometryChanged() {
  if (g_unloading) {
    return;
  }
  int64_t nowMs = DelayedApplyNowMs();
  int64_t initialNotBeforeMs = g_initial_style_apply_not_before_ms.load();
  if (!g_initial_style_apply_completed.load() && initialNotBeforeMs > nowMs) {
    ApplySettingsDebounced(1);
    return;
  }

  HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
  int64_t lastMs = g_last_geometry_critical_apply_ms.load();
  while (nowMs - lastMs >= kGeometryCriticalApplyMinIntervalMs) {
    if (g_last_geometry_critical_apply_ms.compare_exchange_weak(lastMs, nowMs)) {
      g_scheduled_low_priority_update = false;
      ArmStyleFollowupPasses(hTaskbarWnd);
      g_suppress_low_priority_apply_until_ms =
          nowMs + kGeometryCriticalLowPrioritySuppressionMs;
      ApplySettingsDebounced(1);
      if (hTaskbarWnd && IsWindow(hTaskbarWnd)) {
        QueueTaskbarAnimationFollowup(hTaskbarWnd);
      }
      return;
    }
  }

  ArmStyleFollowupPasses(hTaskbarWnd);
}
void ScheduleInitialExplorerStyleApply() {
  ApplySettingsDebounced(kInitialExplorerStyleDelayMs);
}

bool IsWeirdFrameworkElement(winrt::Windows::UI::Xaml::FrameworkElement const& element) {
  if (!element) return true;
  try {
    auto transform = element.TransformToVisual(nullptr);
    winrt::Windows::Foundation::Rect rect = transform.TransformBounds(
        winrt::Windows::Foundation::Rect(0, 0, element.ActualWidth(), element.ActualHeight()));
    return rect.Width <= 0.0 || rect.Height <= 0.0 || rect.X < -kLayoutToleranceDip || rect.Y < -kLayoutToleranceDip;
  } catch (...) {
    // Overflow/recycled taskbar elements can briefly be disconnected from the
    // visual tree while Explorer is rebuilding the task list. Treat them as
    // invalid instead of letting a transient XAML exception take down Explorer.
    return true;
  }
}
bool IsTaskbarWidgetsEnabled() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"TaskbarDa", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return value == 1;
        }
        RegCloseKey(hKey);
    }
    return false;
}
static float SnapToPhysicalPixel(float value, float rasterizationScale = 1.0f) {
  if (rasterizationScale <= 0.0f) {
    rasterizationScale = 1.0f;
  }

  float scaledValue = value * rasterizationScale;
  float snappedScaledValue =
      (scaledValue >= 0.0f)
          ? static_cast<float>(static_cast<long>(scaledValue + 0.5f))
          : -static_cast<float>(static_cast<long>(-scaledValue + 0.5f));

  return snappedScaledValue / rasterizationScale;
}

static float GetRasterizationScale(FrameworkElement const& element) {
  if (element) {
    auto xamlRoot = element.XamlRoot();
    if (xamlRoot) {
      float rasterizationScale = static_cast<float>(xamlRoot.RasterizationScale());
      if (rasterizationScale > 0.0f) {
        return rasterizationScale;
      }
    }
  }

  return 1.0f;
}

using StartDocked__StartSizingFrame_UpdateWindowRegion_WithArgs_t = void(WINAPI*)(void* pThis, winrt::Windows::Foundation::Size param1);
StartDocked__StartSizingFrame_UpdateWindowRegion_WithArgs_t StartDocked__StartSizingFrame_UpdateWindowRegion_WithArgs_Original;
void WINAPI StartDocked__StartSizingFrame_UpdateWindowRegion_WithArgs_Hook(void* pThis, winrt::Windows::Foundation::Size param1) {
 if (StartDocked__StartSizingFrame_UpdateWindowRegion_WithArgs_Original) {
    StartDocked__StartSizingFrame_UpdateWindowRegion_WithArgs_Original(pThis, param1);
  }
    Wh_Log(L"Method called: StartDocked__StartSizingFrame_UpdateWindowRegion (Width: %.2f, Height: %.2f)", param1.Width, param1.Height);
  const int measuredStartMenuWidth = static_cast<int>(param1.Width + 0.5f);
  if (measuredStartMenuWidth > 0 && g_lastRecordedStartMenuWidth != measuredStartMenuWidth) {
    g_lastRecordedStartMenuWidth = measuredStartMenuWidth;
    Wh_SetIntValue(L"lastRecordedStartMenuWidth", g_lastRecordedStartMenuWidth);
  }
}

std::atomic<int64_t> g_update_flag_set_time_ms = 0;
int64_t NowMs() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
void ResetFlagAfterDelay() {
  std::this_thread::sleep_for(std::chrono::milliseconds(kScheduledLowPriorityFlagTtlMs));
  int64_t now = NowMs();
  int64_t set_time = g_update_flag_set_time_ms.load();
  if (g_scheduled_low_priority_update && (now - set_time >= kScheduledLowPriorityFlagTtlMs)) {
    g_scheduled_low_priority_update = false;
  }
}

void ApplySettingsFromTaskbarThreadIfRequired() {
  if (!g_unloading && DelayedApplyNowMs() < g_suppress_low_priority_apply_until_ms.load()) {
    return;
  }
  if (!g_scheduled_low_priority_update.exchange(true)) {
    g_update_flag_set_time_ms = NowMs();
    Wh_Log(L"Scheduled low priority update");
    ApplySettingsDebounced(-1);
  }
}

void SetDividerForElement(FrameworkElement const& element, float const& panelHeight, bool dividerVisible, bool dividerShouldBeOnLeft = g_settings.userDefinedDividerLeftAligned) {
  if (!element) return;

  if (panelHeight <= 0.0f) return;

  auto visual = winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(element);
  if (!visual) return;
  auto compositor = visual.Compositor();
  if (!compositor) return;

  auto shapeVisual = compositor.CreateShapeVisual();
  if (!shapeVisual) return;
  dividerVisible = dividerVisible && !g_unloading;
  if (dividerVisible) {
    auto lineGeometry = compositor.CreateLineGeometry();
    if (!lineGeometry) return;
    auto lineShape = compositor.CreateSpriteShape(lineGeometry);
    if (!lineShape) return;

    float borderThicknessFloat = static_cast<float>(g_settings.userDefinedAppsDividerThickness) * 2.0f;
    float scaledHeight = panelHeight * g_settings.userDefinedAppsDividerVerticalScale;
    float yOffset = (panelHeight - scaledHeight) * 0.5f;
    auto size = visual.Size();
    float xOffset = (dividerShouldBeOnLeft) ? 0.0f : (size.x - borderThicknessFloat / 2.0f);
    shapeVisual.Size({borderThicknessFloat, scaledHeight});
    shapeVisual.Offset({xOffset, yOffset, 0.0f});

    lineGeometry.Start({0.0f, 0.0f});
    lineGeometry.End({0.0f, scaledHeight});

    winrt::Windows::UI::Color borderColor = {g_settings.userDefinedTaskbarBorderOpacity, static_cast<BYTE>(g_settings.borderColorR), static_cast<BYTE>(g_settings.borderColorG), static_cast<BYTE>(g_settings.borderColorB)};
    auto strokeBrush = compositor.CreateColorBrush(borderColor);
    if (!strokeBrush) return;

    lineShape.StrokeBrush(strokeBrush);
    lineShape.StrokeThickness(borderThicknessFloat);
    shapeVisual.Shapes().Append(lineShape);
  }
  winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::SetElementChildVisual(element, shapeVisual);
}
void ChangeControlCenterIconSize(FrameworkElement const& systemTrayFrameGrid) {
  if (!g_settings.userDefinedStyleTrayArea) return;

  if (auto ControlCenterButton = FindChildByName(systemTrayFrameGrid, L"ControlCenterButton")) {
    if (auto innerGrid = FindChildByClassName(ControlCenterButton, L"Windows.UI.Xaml.Controls.Grid")) {
      if (auto ContentPresenter = FindChildByName(innerGrid, L"ContentPresenter")) {
        if (auto innerItemPresenter = FindChildByClassName(ContentPresenter, L"Windows.UI.Xaml.Controls.ItemsPresenter")) {
          if (auto innerStackPanel = FindChildByClassName(innerItemPresenter, L"Windows.UI.Xaml.Controls.StackPanel")) {
            auto userDefinedTrayIconSizeStr = std::to_wstring(g_settings.userDefinedTrayIconSize);

            int childCount = Media::VisualTreeHelper::GetChildrenCount(innerStackPanel);
            for (int i = 0; i < childCount; ++i) {
              auto child = Media::VisualTreeHelper::GetChild(innerStackPanel, i).try_as<FrameworkElement>();
              if (!child) continue;
              auto SystemTrayIcon = FindChildByName(child, L"SystemTrayIcon");
              if (!SystemTrayIcon) continue;
              auto ContainerGrid = FindChildByName(SystemTrayIcon, L"ContainerGrid");
              if (!ContainerGrid) continue;
              auto ContentGrid = FindChildByName(ContainerGrid, L"ContentGrid");
              if (!ContentGrid) continue;
              auto TextIconContent = FindChildByClassName(ContentGrid, L"SystemTray.TextIconContent");
              if (!TextIconContent) continue;
              auto ContainerGridInner = FindChildByName(TextIconContent, L"ContainerGrid");
              if (!ContainerGridInner) continue;

              if (auto Layer = FindChildByName(ContainerGridInner, L"Underlay")) {
                if (auto InnerTextBlock = FindChildByName(Layer, L"InnerTextBlock")) {
                  SetElementPropertyFromString(InnerTextBlock, L"Windows.UI.Xaml.Controls.TextBlock", L"FontSize", userDefinedTrayIconSizeStr);
                }
              }

              if (auto Layer = FindChildByName(ContainerGridInner, L"Base")) {
                if (auto InnerTextBlock = FindChildByName(Layer, L"InnerTextBlock")) {
                  SetElementPropertyFromString(InnerTextBlock, L"Windows.UI.Xaml.Controls.TextBlock", L"FontSize", userDefinedTrayIconSizeStr);
                }
              }

              if (auto Layer = FindChildByName(ContainerGridInner, L"AccentOverlay")) {
                if (auto InnerTextBlock = FindChildByName(Layer, L"InnerTextBlock")) {
                  SetElementPropertyFromString(InnerTextBlock, L"Windows.UI.Xaml.Controls.TextBlock", L"FontSize", userDefinedTrayIconSizeStr);
                }
              }
            }
          }
        }
      }
    }
  }
}
void ProcessStackPanelChildren(FrameworkElement const& stackPanel, float const& panelHeight) {
  if (!g_settings.userDefinedStyleTrayArea) return;

  auto userDefinedTaskButtonCornerRadius = std::to_wstring(g_settings.userDefinedTaskButtonCornerRadius);
  int childCount = Media::VisualTreeHelper::GetChildrenCount(stackPanel);
  for (int i = 0; i < childCount; ++i) {
    auto child = Media::VisualTreeHelper::GetChild(stackPanel, i).try_as<FrameworkElement>();
    if (!child) continue;

    auto notifyItemIcon = FindChildByName(child, L"NotifyItemIcon");
    if (!notifyItemIcon) continue;

    auto containerGrid = FindChildByName(notifyItemIcon, L"ContainerGrid");
    if (!containerGrid) continue;

    auto innerContentPresenter = FindChildByName(containerGrid, L"ContentPresenter");
    if (!innerContentPresenter) continue;

    auto contentGrid = FindChildByName(innerContentPresenter, L"ContentGrid");
    if (!contentGrid) continue;

    auto imageIconContent = FindChildByClassName(contentGrid, L"SystemTray.ImageIconContent");
    if (!imageIconContent) continue;

    auto innerContainerGrid = FindChildByName(imageIconContent, L"ContainerGrid");
    if (!innerContainerGrid) continue;

    auto image = FindChildByClassName(innerContainerGrid, L"Windows.UI.Xaml.Controls.Image");

    if (!image) continue;

    auto imageCtrl = image.try_as<winrt::Windows::UI::Xaml::Controls::Image>();

    if (!imageCtrl) continue;

    if (g_settings.userDefinedStyleTrayArea) {
      child.Width(g_settings.userDefinedTrayButtonSize);
      child.Height(g_settings.userDefinedTaskbarHeight);

      SetElementPropertyFromString(containerGrid, L"Windows.UI.Xaml.Controls.Grid", L"CornerRadius", userDefinedTaskButtonCornerRadius);

      image.Width(g_settings.userDefinedTrayIconSize);
      image.Height(g_settings.userDefinedTrayIconSize);
    }
  }
}
void StyleNativeDividerElement(winrt::Windows::UI::Xaml::FrameworkElement const& element) {
  if (!element) return;
  using namespace winrt::Windows::UI::Xaml::Hosting;
  using namespace winrt::Windows::Foundation::Numerics;

  element.Opacity(g_unloading ? 1.0f : std::min(1.0f, static_cast<float>(g_settings.userDefinedTaskbarBorderOpacity / 255.0f)));
  element.Width(std::max(0.0, g_settings.userDefinedAppsDividerThickness * 0.99));

  if (auto visual = ElementCompositionPreview::GetElementVisual(element)) {
    if (auto compositor = visual.Compositor()) {
      visual.CenterPoint({0.0f, static_cast<float>(element.ActualHeight()) / 2.0f, 0.0f});
      visual.Scale({1.0f, g_unloading ? 1.0f : g_settings.userDefinedAppsDividerVerticalScale, 1.0f});
    }
  }

  PCWSTR originalHex = Wh_GetStringSetting(L"TaskbarBorderColorHex");
  PCWSTR hex = originalHex;
  if (!hex || *hex == L'\0') {
    hex = L"#ffffff";
  }

  if (*hex == L'#') ++hex;

  std::wstring fillBrush = L"<SolidColorBrush Color=\"#" + std::wstring(hex) + L"\"/>";
  SetElementPropertyFromString(element, L"Windows.UI.Xaml.Shapes.Rectangle", L"Fill", fillBrush.c_str(), true);

  if (originalHex) {
    Wh_FreeStringSetting(originalHex);
  }
}
void InvalidateTaskbarButtonLayoutElement(FrameworkElement const& element) {
  if (!element) {
    return;
  }
  try {
    element.InvalidateMeasure();
    element.InvalidateArrange();
  } catch (...) {
  }
}

double GetEffectiveTaskbarButtonTargetWidth() {
  const double defaultWidth = g_smallIconSize
                                  ? kSystemSmallTaskbarButtonSize
                                  : kSystemMediumTaskbarButtonSize;
  return g_unloading
             ? defaultWidth
             : static_cast<double>(g_smallIconSize
                                       ? g_settings_tbiconsize.taskbarButtonWidthSmall
                                       : g_settings_tbiconsize.taskbarButtonWidth);
}

bool IsExpectedTaskbarButtonDimension(double value, double expected) {
  if (!std::isfinite(value) || value <= 0.0) {
    return false;
  }
  return std::abs(value - expected) <= kLayoutToleranceDip;
}

bool EnsureElementTaskbarButtonWidth(FrameworkElement const& element,
                                     double targetWidth,
                                     bool allowHardWidth) {
  if (!element) {
    return false;
  }

  bool changed = false;
  try {
    if (!IsExpectedTaskbarButtonDimension(element.MinWidth(), targetWidth)) {
      element.MinWidth(targetWidth);
      changed = true;
    }

    if (allowHardWidth) {
      if (!IsExpectedTaskbarButtonDimension(element.MaxWidth(), targetWidth)) {
        element.MaxWidth(targetWidth);
        changed = true;
      }

      const double currentWidth = element.Width();
      const double actualWidth = element.ActualWidth();
      const bool explicitWidthWrong =
          !std::isfinite(currentWidth) ||
          !IsExpectedTaskbarButtonDimension(currentWidth, targetWidth);
      const bool actualWidthWrong =
          actualWidth > 0.0 &&
          !IsExpectedTaskbarButtonDimension(actualWidth, targetWidth);

      if (explicitWidthWrong || actualWidthWrong) {
        element.Width(targetWidth);
        changed = true;
      }
    }

    if (changed) {
      InvalidateTaskbarButtonLayoutElement(element);
    }
  } catch (...) {
  }

  return changed;
}
bool IsStartButtonElement(FrameworkElement const& child,
                          winrt::hstring const& className) {
  if (!child || className != L"Taskbar.ExperienceToggleButton") {
    return false;
  }
  try {
    return Automation::AutomationProperties::GetAutomationId(child) == L"StartButton";
  } catch (...) {
    return false;
  }
}

bool ApplyTaskbarButtonSizeToElement(FrameworkElement const& child,
                                     winrt::hstring const& className) {
  if (!child) {
    return false;
  }

  const bool isTaskbarButton =
      className == L"Taskbar.TaskListButton" ||
      className == L"Taskbar.ExperienceToggleButton" ||
      className == L"Taskbar.OverflowToggleButton" ||
      className == L"Taskbar.SearchBoxButton";
  if (!isTaskbarButton) {
    return false;
  }

  const double targetWidth = static_cast<double>(g_settings.userDefinedTaskbarButtonSize);
  const bool allowHardWidth =
      className != L"Taskbar.SearchBoxButton" ||
      !FindChildByName(child, L"SearchBoxTextBlock");
  bool changed = EnsureElementTaskbarButtonWidth(child, targetWidth, allowHardWidth);

  FrameworkElement innerElementChild = nullptr;
  if (className == L"Taskbar.SearchBoxButton") {
    innerElementChild = FindChildByClassName(child, L"Taskbar.TaskListButtonPanel");
    if (!innerElementChild) {
      innerElementChild = FindChildByName(child, L"SearchBoxButtonRootPanel");
    }
  } else if (className == L"Taskbar.TaskListButton") {
    innerElementChild = FindChildByClassName(child, L"Taskbar.TaskListLabeledButtonPanel");
    if (!innerElementChild) {
      innerElementChild = FindChildByClassName(child, L"Taskbar.TaskListButtonPanel");
    }
  } else if (className == L"Taskbar.ExperienceToggleButton") {
    innerElementChild = FindChildByName(child, L"ExperienceToggleButtonRootPanel");
  } else if (className == L"Taskbar.OverflowToggleButton") {
    innerElementChild = FindChildByName(child, L"OverflowToggleButtonRootPanel");
  } else {
    innerElementChild = FindChildByClassName(child, L"Taskbar.TaskListButtonPanel");
  }

  if (innerElementChild) {
    changed = EnsureElementTaskbarButtonWidth(innerElementChild, targetWidth, allowHardWidth) || changed;
  }

  return changed;
}

struct ChildLayoutObservationTai {
  FrameworkElement element{nullptr};
  winrt::hstring className;
  winrt::Windows::Foundation::Rect rect{};
  winrt::Windows::Foundation::Rect rootRect{};
  std::wstring automationName;
  bool hasValidRect{false};
  bool hasValidRootRect{false};
  bool isStartButton{false};
};

struct ChildrenLayoutMeasurementTai {
  std::vector<ChildLayoutObservationTai> children;
  double capturedContentWidth{0.0};
  double totalWidth{0.0};
  double leftMostEdge{0.0};
  double rightMostEdge{0.0};
  int validChildrenCount{0};
  int visualChildCount{0};
};

ChildrenLayoutMeasurementTai CaptureChildrenLayoutTai(
    FrameworkElement const& element) {
  ChildrenLayoutMeasurementTai measurement;
  if (!element) {
    return measurement;
  }

  int childrenCount = Media::VisualTreeHelper::GetChildrenCount(element);
  measurement.visualChildCount = childrenCount;
  measurement.children.reserve(static_cast<size_t>(std::max(0, childrenCount)));

  for (int i = 0; i < childrenCount; i++) {
    ChildLayoutObservationTai observation;
    observation.element =
        Media::VisualTreeHelper::GetChild(element, i).try_as<FrameworkElement>();
    if (!observation.element) {
      Wh_Log(L"Failed to get child %d of %d", i + 1, childrenCount);
      continue;
    }

    observation.className = winrt::get_class_name(observation.element);
    observation.isStartButton =
        IsStartButtonElement(observation.element, observation.className);
    if (observation.className != L"Taskbar.AugmentedEntryPointButton") {
      const double actualWidth = observation.element.ActualWidth();
      if (std::isfinite(actualWidth) && actualWidth > 0.0) {
        measurement.capturedContentWidth += actualWidth;
      }
    }
    if (observation.className == L"Taskbar.TaskListButton") {
      try {
        auto value = observation.element.GetValue(
            Automation::AutomationProperties::NameProperty());
        observation.automationName =
            winrt::unbox_value_or<winrt::hstring>(value, L"").c_str();
      } catch (...) {
        observation.automationName.clear();
      }
    }

    measurement.children.push_back(std::move(observation));
  }

  return measurement;
}

bool IsValidChildLayoutRectTai(
    winrt::Windows::Foundation::Rect const& rect) {
  return rect.Width > 0.0 &&
         rect.Height > 0.0 &&
         std::isfinite(rect.X) &&
         std::isfinite(rect.Y) &&
         std::isfinite(rect.Width) &&
         std::isfinite(rect.Height) &&
         rect.X >= -kLayoutToleranceDip &&
         rect.Y >= -kLayoutToleranceDip;
}

void RefreshChildrenLayoutMeasurementTai(
    FrameworkElement const& boundsRelativeTo,
    FrameworkElement const& rootBoundsRelativeTo,
    ChildrenLayoutMeasurementTai* measurement) {
  if (!boundsRelativeTo || !measurement) {
    return;
  }

  measurement->totalWidth = 0.0;
  measurement->leftMostEdge = 0.0;
  measurement->rightMostEdge = 0.0;
  measurement->validChildrenCount = 0;

  double minEdge = std::numeric_limits<double>::infinity();
  double maxEdge = -std::numeric_limits<double>::infinity();
  for (auto& observation : measurement->children) {
    observation.hasValidRect = false;
    observation.hasValidRootRect = false;

    try {
      auto transform =
          observation.element.TransformToVisual(boundsRelativeTo);
      observation.rect = transform.TransformBounds(
          winrt::Windows::Foundation::Rect(
              0,
              0,
              observation.element.ActualWidth(),
              observation.element.ActualHeight()));
      observation.hasValidRect = IsValidChildLayoutRectTai(observation.rect);
    } catch (...) {
      observation.hasValidRect = false;
    }

    if (rootBoundsRelativeTo) {
      try {
        auto transform =
            observation.element.TransformToVisual(rootBoundsRelativeTo);
        observation.rootRect = transform.TransformBounds(
            winrt::Windows::Foundation::Rect(
                0,
                0,
                observation.element.ActualWidth(),
                observation.element.ActualHeight()));
        observation.hasValidRootRect =
            IsValidChildLayoutRectTai(observation.rootRect);
      } catch (...) {
        observation.hasValidRootRect = false;
      }
    }

    if (observation.hasValidRect &&
        observation.className != L"Taskbar.AugmentedEntryPointButton") {
      measurement->totalWidth += observation.rect.Width;
      minEdge = std::min(minEdge, static_cast<double>(observation.rect.X));
      maxEdge = std::max(
          maxEdge,
          static_cast<double>(observation.rect.X + observation.rect.Width));
      measurement->validChildrenCount++;
    }
  }

  if (measurement->validChildrenCount > 0 &&
      minEdge != std::numeric_limits<double>::infinity()) {
    measurement->leftMostEdge = minEdge;
    measurement->rightMostEdge = maxEdge;
  }
}

ChildrenLayoutMeasurementTai MeasureValidChildren(
    FrameworkElement const& element,
    FrameworkElement const& boundsRelativeTo = nullptr) {
  auto measurement = CaptureChildrenLayoutTai(element);
  RefreshChildrenLayoutMeasurementTai(
      boundsRelativeTo ? boundsRelativeTo : element,
      nullptr,
      &measurement);
  return measurement;
}

FrameworkElement FindCapturedChildByClassNameTai(
    ChildrenLayoutMeasurementTai const& measurement,
    PCWSTR className) {
  for (const auto& observation : measurement.children) {
    if (observation.className == className) {
      return observation.element;
    }
  }
  return nullptr;
}

const ChildLayoutObservationTai* FindCapturedStartButtonTai(
    ChildrenLayoutMeasurementTai const& measurement) {
  for (const auto& observation : measurement.children) {
    if (observation.isStartButton) {
      return &observation;
    }
  }
  return nullptr;
}

uint64_t AppendChildStyleSignatureTai(
    uint64_t signature,
    std::wstring_view value) {
  constexpr uint64_t kFnvPrime = 1099511628211ULL;
  for (wchar_t ch : value) {
    signature ^= static_cast<uint64_t>(ch);
    signature *= kFnvPrime;
  }
  return signature;
}

uint64_t GetChildStyleSignatureTai(
    ChildrenLayoutMeasurementTai const& measurement) {
  constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
  constexpr uint64_t kFnvPrime = 1099511628211ULL;
  uint64_t signature = kFnvOffsetBasis;
  for (const auto& child : measurement.children) {
    signature ^= static_cast<uint64_t>(
        reinterpret_cast<uintptr_t>(winrt::get_abi(child.element)));
    signature *= kFnvPrime;
    signature = AppendChildStyleSignatureTai(
        signature,
        std::wstring_view(child.className.c_str(), child.className.size()));
    signature = AppendChildStyleSignatureTai(signature, child.automationName);
    signature ^= child.hasValidRect ? 1ULL : 0ULL;
    signature *= kFnvPrime;
  }
  signature ^= static_cast<uint64_t>(measurement.children.size());
  signature *= kFnvPrime;
  return signature;
}

bool ApplyTaskbarButtonSizing(
    ChildrenLayoutMeasurementTai const& measurement) {
  bool layoutChanged = false;
  for (const auto& child : measurement.children) {
    layoutChanged =
        ApplyTaskbarButtonSizeToElement(child.element, child.className) ||
        layoutChanged;
  }
  return layoutChanged;
}

void ApplyMeasuredChildStyles(
    ChildrenLayoutMeasurementTai const& measurement) {
  const float tbHeightFloat = static_cast<float>(g_settings.userDefinedTaskbarHeight);
  auto userDefinedTaskButtonCornerRadius = std::to_wstring(g_settings.userDefinedTaskButtonCornerRadius);
  for (const auto& observation : measurement.children) {
    if (!observation.hasValidRect) {
      continue;
    }

    auto const& child = observation.element;
    auto const& className = observation.className;
    SetElementPropertyFromString(
        child,
        className.c_str(),
        L"CornerRadius",
        userDefinedTaskButtonCornerRadius);
    if (className == L"Taskbar.SearchBoxButton") {
      // Search only needs the common sizing helper above.
    } else if (className == L"Taskbar.TaskListButton") {
      auto innerElementChild = FindChildByClassName(child, L"Taskbar.TaskListLabeledButtonPanel");
      if (innerElementChild) {
        auto iconElementChild = FindChildByName(innerElementChild, L"Icon");
        if (iconElementChild) {
          iconElementChild.Width(g_settings.userDefinedTaskbarIconSize);
          iconElementChild.Height(g_settings.userDefinedTaskbarIconSize);
          SetDividerForElement(child, tbHeightFloat, false);
          if (!observation.automationName.empty()) {
            for (const auto& pattern :
                 g_settings.compiledDividedAppPatterns) {
              if (MatchesDividedAppPatternTai(
                      observation.automationName,
                      pattern)) {
                SetDividerForElement(child, tbHeightFloat, true);
                break;
              }
            }
          }
        }
      }
    } else if (className == L"Taskbar.AugmentedEntryPointButton") {  // widget element
      child.Margin(Thickness{0, 0, 0, 0});
      auto ExperienceToggleButtonRootPanelElement = FindChildByName(child, L"ExperienceToggleButtonRootPanel");
      if (ExperienceToggleButtonRootPanelElement) {
        ExperienceToggleButtonRootPanelElement.Margin(Thickness{0, 0, 0, 0});
      }
      continue;
    } else if (className == L"Taskbar.OverflowToggleButton") {  // overflow button
      if (auto OverflowToggleButtonRootPanel = FindChildByName(child, L"OverflowToggleButtonRootPanel")) {
        if (auto RightOverflowButtonDivider = FindChildByName(OverflowToggleButtonRootPanel, L"RightOverflowButtonDivider")) {
          if (g_settings.userDefinedTrayAreaDivider) {
            RightOverflowButtonDivider.Opacity(0);
          } else {
            StyleNativeDividerElement(RightOverflowButtonDivider);
          }
        }
      }
    }
    if (auto iconPanelElement = FindChildByName(child, L"IconPanel")) {
      if (auto mostRecentlyUsedDivider = FindChildByName(iconPanelElement, L"MostRecentlyUsedDivider")) {
        StyleNativeDividerElement(mostRecentlyUsedDivider);
      }
      if (auto progressIndicator = FindChildByName(iconPanelElement, L"ProgressIndicator")) {
        if (auto layoutRoot = FindChildByName(progressIndicator, L"LayoutRoot")) {
          if (auto progressBarRoot = FindChildByName(layoutRoot, L"ProgressBarRoot")) {
            if (auto border = FindChildByClassName(progressBarRoot, L"Windows.UI.Xaml.Controls.Border")) {
              if (auto grid = FindChildByClassName(border, L"Windows.UI.Xaml.Controls.Grid")) {
                grid.Height(3.8);
                if (auto progressBarTrack = FindChildByName(grid, L"ProgressBarTrack")) {
                  progressBarTrack.Opacity(0.5);
                }
              }
            }
          }
        }
      } else if (auto runningIndicator = FindChildByName(iconPanelElement, L"RunningIndicator")) {
        runningIndicator.Height(3.5);
        runningIndicator.Opacity(1);
      }
    }
  }
}

void ApplyChildStylesIfRequired(
    FrameworkElement const& container,
    FrameworkElement const& rootBoundsRelativeTo,
    ChildrenLayoutMeasurementTai* measurement,
    TaskbarChildStyleCache* cache,
    uint64_t styleGeneration) {
  if (!container || !measurement || !cache) {
    return;
  }

  uint64_t signature = GetChildStyleSignatureTai(*measurement);
  if (cache->valid &&
      cache->generation == styleGeneration &&
      cache->signature == signature) {
    return;
  }

  if (ApplyTaskbarButtonSizing(*measurement)) {
    container.UpdateLayout();
    *measurement = CaptureChildrenLayoutTai(container);
    RefreshChildrenLayoutMeasurementTai(
        container,
        rootBoundsRelativeTo,
        measurement);
    signature = GetChildStyleSignatureTai(*measurement);
  }

  ApplyMeasuredChildStyles(*measurement);
  cache->generation = styleGeneration;
  cache->signature = signature;
  cache->valid = true;
}


void DisableElementClip(FrameworkElement const& element) {
  if (!element) {
    return;
  }
  try {
    element.Clip(nullptr);
  } catch (...) {
  }
}

void DisableClipForAncestorChain(FrameworkElement element, int maxDepth = 8) {
  for (int i = 0; element && i < maxDepth; ++i) {
    DisableElementClip(element);
    try {
      element = Media::VisualTreeHelper::GetParent(element).try_as<FrameworkElement>();
    } catch (...) {
      break;
    }
  }
}

bool SetVirtualLayoutWidth(FrameworkElement const& element, double width) {
  if (!element || !std::isfinite(width) || width <= 0.0) {
    return false;
  }

  bool changed = false;
  try {
    if (!IsExpectedTaskbarButtonDimension(element.Width(), width)) {
      element.Width(width);
      changed = true;
    }
    if (!IsExpectedTaskbarButtonDimension(element.MinWidth(), width)) {
      element.MinWidth(width);
      changed = true;
    }
    if (!IsExpectedTaskbarButtonDimension(element.MaxWidth(), width)) {
      element.MaxWidth(width);
      changed = true;
    }
    element.HorizontalAlignment(HorizontalAlignment::Left);
    DisableElementClip(element);
    if (changed) {
      element.InvalidateMeasure();
      element.InvalidateArrange();
    }
  } catch (...) {
  }
  return changed;
}
double CalculateDynamicTaskbarVirtualSurfaceWidth(int visualChildCount,
                                                  double capturedContentWidth,
                                                  double rootWidth,
                                                  double rasterizationScale) {
  if (g_unloading ||
      !std::isfinite(rootWidth) ||
      rootWidth <= 0.0 ||
      !std::isfinite(rasterizationScale) ||
      rasterizationScale <= 0.0) {
    return rootWidth;
  }

  const double buttonSize = std::max<double>(
      1.0, static_cast<double>(g_settings.userDefinedTaskbarButtonSize));
  const double childCountEstimate =
      static_cast<double>(std::max(visualChildCount, 1)) * buttonSize;
  const double measuredContentWidth =
      std::isfinite(capturedContentWidth) && capturedContentWidth > 0.0
          ? capturedContentWidth
          : 0.0;
  const double requiredContentWidth =
      std::max(measuredContentWidth, childCountEstimate);

  // Keep one monitor width free so ItemsRepeater can realize newly added or
  // previously overflowed buttons on subsequent layout passes.
  const double requestedVirtualWidth = requiredContentWidth + rootWidth;

  // Packed HSHELL_GETMINRECT coordinates use signed 16-bit components. Limit
  // the virtual surface to half that physical span so centering and scaling
  // retain coordinate headroom on either side of the island.
  const double coordinateSafeWidth =
      kTaskbarVirtualSurfaceMaxPhysicalWidth / rasterizationScale;
  const double maximumVirtualWidth =
      std::max(rootWidth, coordinateSafeWidth);
  const double virtualWidth =
      std::clamp(requestedVirtualWidth, rootWidth, maximumVirtualWidth);
  return std::isfinite(virtualWidth) && virtualWidth > rootWidth
      ? virtualWidth
      : rootWidth;
}
void ApplyVirtualTaskbarLayoutSurface(FrameworkElement const& xamlRootContent,
                                      FrameworkElement const& taskFrame,
                                      FrameworkElement const& rootGridTaskBar,
                                      FrameworkElement const& taskbarFrameRepeater,
                                      FrameworkElement const& taskbarBackground,
                                      FrameworkElement const& backgroundFillParent,
                                      FrameworkElement const& backgroundFillChild,
                                      double virtualWidth) {
  if (!std::isfinite(virtualWidth) || virtualWidth <= 0.0) {
    return;
  }

  // Keep the physical taskbar frame at the monitor width. Only the taskbar
  // content surface is virtualized. This gives ItemsRepeater more measure space
  // without making the whole taskbar window logically wider.
  DisableElementClip(xamlRootContent);
  DisableElementClip(taskFrame);
  DisableClipForAncestorChain(rootGridTaskBar);
  SetVirtualLayoutWidth(rootGridTaskBar, virtualWidth);
  SetVirtualLayoutWidth(taskbarFrameRepeater, virtualWidth);
  SetVirtualLayoutWidth(taskbarBackground, virtualWidth);
  SetVirtualLayoutWidth(backgroundFillParent, virtualWidth);
  SetVirtualLayoutWidth(backgroundFillChild, virtualWidth);
}

void SetTaskbarOverflowButtonSuppressed(FrameworkElement const& overflowButton,
                                        bool suppress,
                                        TaskbarState* state) {
  if (!state) {
    return;
  }

  if (!overflowButton) {
    state->lastOverflowButtonIdentity = 0;
    state->overflowButtonSuppressionKnown = false;
    return;
  }

  try {
    const uintptr_t overflowButtonIdentity =
        reinterpret_cast<uintptr_t>(winrt::get_abi(overflowButton));
    const bool suppressionTransition =
        !state->overflowButtonSuppressionKnown ||
        state->lastOverflowButtonIdentity != overflowButtonIdentity ||
        state->overflowButtonSuppressed != suppress;

    if (suppress) {
      overflowButton.Opacity(0.0);
      overflowButton.IsHitTestVisible(false);
      overflowButton.MinWidth(0.0);
      overflowButton.MaxWidth(0.0);
      overflowButton.Width(0.0);
      overflowButton.Clip(nullptr);
      if (suppressionTransition) {
        overflowButton.InvalidateMeasure();
        overflowButton.InvalidateArrange();
        overflowButton.UpdateLayout();
      }
    } else {
      overflowButton.Opacity(1.0);
      overflowButton.IsHitTestVisible(true);
      overflowButton.ClearValue(FrameworkElement::WidthProperty());
      overflowButton.ClearValue(FrameworkElement::MinWidthProperty());
      overflowButton.ClearValue(FrameworkElement::MaxWidthProperty());
    }

    state->lastOverflowButtonIdentity = overflowButtonIdentity;
    state->overflowButtonSuppressionKnown = true;
    state->overflowButtonSuppressed = suppress;
  } catch (...) {
  }
}


// ---- Temporary taskbar geometry diagnostics -------------------------------
// Set to false after collecting logs. The throttle keeps ApplyStyle from
// flooding Windhawk logs during insertion/removal animations.
constexpr bool kDebugTaskbarGeometry = true;
constexpr int64_t kDebugTaskbarGeometryMinIntervalMs = 250;
std::atomic<int64_t> g_lastTaskbarGeometryDebugLogMs = 0;

bool ShouldLogTaskbarGeometry(bool interesting) {
  if constexpr (!kDebugTaskbarGeometry) {
    return false;
  }
  if (!interesting) {
    return false;
  }
  const int64_t nowMs = DelayedApplyNowMs();
  int64_t lastMs = g_lastTaskbarGeometryDebugLogMs.load();
  while (nowMs - lastMs >= kDebugTaskbarGeometryMinIntervalMs) {
    if (g_lastTaskbarGeometryDebugLogMs.compare_exchange_weak(lastMs, nowMs)) {
      return true;
    }
  }
  return false;
}

bool TryGetDebugBoundsRelativeTo(FrameworkElement const& element,
                                 winrt::Windows::UI::Xaml::UIElement const& relativeTo,
                                 winrt::Windows::Foundation::Rect& rect) {
  if (!element) {
    return false;
  }
  try {
    auto transform = element.TransformToVisual(relativeTo);
    rect = transform.TransformBounds(
        winrt::Windows::Foundation::Rect(0, 0, element.ActualWidth(), element.ActualHeight()));
    return std::isfinite(rect.X) &&
           std::isfinite(rect.Y) &&
           std::isfinite(rect.Width) &&
           std::isfinite(rect.Height);
  } catch (...) {
    return false;
  }
}
static bool BuildMeasuredMinimizeAnimationButtonsTai(
    ChildrenLayoutMeasurementTai const& taskbarChildrenMeasurement,
    const RECT& monitorRect,
    double targetTaskRootOffsetXDip,
    double targetTaskbarIslandScale,
    double targetScaleCenterScreenXDip,
    double rasterizationScale,
    std::vector<MinimizeAnimationMeasuredButtonTai>* measuredButtons) {
  if (!measuredButtons ||
      IsRectEmpty(&monitorRect) ||
      !std::isfinite(targetTaskRootOffsetXDip) ||
      !std::isfinite(targetTaskbarIslandScale) ||
      !std::isfinite(targetScaleCenterScreenXDip) ||
      !std::isfinite(rasterizationScale) ||
      rasterizationScale <= 0.0) {
    return false;
  }

  measuredButtons->clear();
  measuredButtons->reserve(taskbarChildrenMeasurement.children.size());

  for (const auto& observation : taskbarChildrenMeasurement.children) {
    if (observation.className != L"Taskbar.TaskListButton" ||
        !observation.hasValidRootRect) {
      continue;
    }

    auto const& layoutRect = observation.rootRect;
    const double unscaledLeftDip = layoutRect.X + targetTaskRootOffsetXDip;
    const double unscaledRightDip = layoutRect.X + layoutRect.Width + targetTaskRootOffsetXDip;
    const double visibleLeftDip = ApplyScaleToScreenX(
        static_cast<float>(unscaledLeftDip),
        static_cast<float>(targetScaleCenterScreenXDip),
        static_cast<float>(targetTaskbarIslandScale));
    const double visibleRightDip = ApplyScaleToScreenX(
        static_cast<float>(unscaledRightDip),
        static_cast<float>(targetScaleCenterScreenXDip),
        static_cast<float>(targetTaskbarIslandScale));

    if (!std::isfinite(visibleLeftDip) || !std::isfinite(visibleRightDip)) {
      continue;
    }

    RECT visibleRectPx{};
    visibleRectPx.left = monitorRect.left + static_cast<LONG>(std::lround(std::min(visibleLeftDip, visibleRightDip) * rasterizationScale));
    visibleRectPx.right = monitorRect.left + static_cast<LONG>(std::lround(std::max(visibleLeftDip, visibleRightDip) * rasterizationScale));
    // Preserve only the measured X interval. The shell min-rect already carries
    // the correct bottom-taskbar Y coordinates; root-relative XAML Y values are
    // not in the same coordinate space on every monitor/taskbar edge.
    visibleRectPx.top = 1;
    visibleRectPx.bottom = 2;

    if (!IsUsableMeasuredButtonRectTai(visibleRectPx)) {
      continue;
    }

    measuredButtons->push_back(MinimizeAnimationMeasuredButtonTai{
        visibleRectPx,
        TrimTai(observation.automationName)});
  }

  return !measuredButtons->empty();
}

static void UpdateMinimizeAnimationCorrectionForMonitorTai(
    const std::wstring& monitorName,
    ChildrenLayoutMeasurementTai const& taskbarChildrenMeasurement,
    bool useVirtualTaskbarSurface,
    double targetTaskRootOffsetXDip,
    double targetTaskbarIslandScale,
    double targetScaleCenterScreenXDip,
    double rasterizationScale,
    double scaledBackgroundLeftScreenDip,
    double scaledBackgroundRightScreenDip) {
  if (g_unloading ||
      (!useVirtualTaskbarSurface &&
       targetTaskbarIslandScale >= static_cast<double>(0.999f))) {
    ClearMinimizeAnimationCorrectionForMonitorTai(monitorName);
    return;
  }

  RECT monitorRect{};
  RECT taskbarClampRect{};
  const bool haveMonitorRect = GetMonitorRectByNameTai(monitorName, &monitorRect);
  if (haveMonitorRect) {
    taskbarClampRect.left = monitorRect.left +
        static_cast<LONG>(std::lround(scaledBackgroundLeftScreenDip * rasterizationScale));
    taskbarClampRect.right = monitorRect.left +
        static_cast<LONG>(std::lround(scaledBackgroundRightScreenDip * rasterizationScale));
    taskbarClampRect.top = monitorRect.top;
    taskbarClampRect.bottom = monitorRect.bottom;
    if (taskbarClampRect.right <= taskbarClampRect.left) {
      taskbarClampRect = monitorRect;
    }
  }

  std::vector<MinimizeAnimationMeasuredButtonTai> measuredButtons;
  if (haveMonitorRect) {
    BuildMeasuredMinimizeAnimationButtonsTai(
        taskbarChildrenMeasurement,
        monitorRect,
        targetTaskRootOffsetXDip,
        targetTaskbarIslandScale,
        targetScaleCenterScreenXDip,
        rasterizationScale,
        &measuredButtons);
  }

  // The transform remains the fallback for shell payloads that can't be
  // matched to a freshly measured taskbar button.
  SetMinimizeAnimationCorrectionForMonitorTai(
      monitorName,
      monitorRect,
      targetTaskRootOffsetXDip,
      targetTaskbarIslandScale,
      targetScaleCenterScreenXDip,
      rasterizationScale,
      haveMonitorRect ? &taskbarClampRect : nullptr,
      std::move(measuredButtons));
}

void LogDebugRect(PCWSTR label, bool ok, winrt::Windows::Foundation::Rect const& rect) {
  if (!ok) {
    Wh_Log(L"    %-18s unavailable", label);
    return;
  }
  Wh_Log(L"    %-18s X=%8.2f Y=%8.2f W=%8.2f H=%8.2f R=%8.2f B=%8.2f",
         label,
         rect.X,
         rect.Y,
         rect.Width,
         rect.Height,
         rect.X + rect.Width,
         rect.Y + rect.Height);
}

void LogElementGeometry(PCWSTR tag,
                        FrameworkElement const& element,
                        FrameworkElement const& rootGridTaskBar,
                        FrameworkElement const& taskbarFrameRepeater,
                        FrameworkElement const& taskFrame) {
  if (!element) {
    Wh_Log(L"[TBGEOM] %s: <null>", tag);
    return;
  }

  std::wstring className;
  std::wstring name;
  std::wstring automationId;
  try { className = std::wstring(winrt::get_class_name(element).c_str()); } catch (...) { className = L"<class?>"; }
  try { name = std::wstring(element.Name().c_str()); } catch (...) { name = L""; }
  try { automationId = std::wstring(Automation::AutomationProperties::GetAutomationId(element).c_str()); } catch (...) { automationId = L""; }

  auto parent = Media::VisualTreeHelper::GetParent(element).try_as<FrameworkElement>();
  std::wstring parentClass;
  std::wstring parentName;
  if (parent) {
    try { parentClass = std::wstring(winrt::get_class_name(parent).c_str()); } catch (...) { parentClass = L"<class?>"; }
    try { parentName = std::wstring(parent.Name().c_str()); } catch (...) { parentName = L""; }
  } else {
    parentClass = L"<null>";
  }

  Thickness margin{};
  winrt::Windows::Foundation::Numerics::float3 actualOffset{};
  try { margin = element.Margin(); } catch (...) {}
  try { actualOffset = element.ActualOffset(); } catch (...) {}

  winrt::Windows::Foundation::Numerics::float3 visualOffset{};
  winrt::Windows::Foundation::Numerics::float2 visualSize{};
  if (auto visual = winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(element)) {
    visualOffset = visual.Offset();
    visualSize = visual.Size();
  }

  Wh_Log(L"[TBGEOM] %s class=%s name=%s aid=%s parent=%s/%s",
         tag,
         className.c_str(),
         name.c_str(),
         automationId.c_str(),
         parentClass.c_str(),
         parentName.c_str());
  // VA/HA use the Windows.UI.Xaml enum ordering: Top/Left=0, Center=1,
  // Bottom/Right=2, Stretch=3.
  int verticalAlignmentValue = -1;
  int horizontalAlignmentValue = -1;
  try { verticalAlignmentValue = static_cast<int>(element.VerticalAlignment()); } catch (...) {}
  try { horizontalAlignmentValue = static_cast<int>(element.HorizontalAlignment()); } catch (...) {}
  Wh_Log(L"    size Actual=%8.2fx%-8.2f W=%8.2f H=%8.2f MinW=%8.2f MaxW=%8.2f MinH=%8.2f MaxH=%8.2f VA=%d HA=%d Margin=(%.2f,%.2f,%.2f,%.2f)",
         element.ActualWidth(),
         element.ActualHeight(),
         element.Width(),
         element.Height(),
         element.MinWidth(),
         element.MaxWidth(),
         element.MinHeight(),
         element.MaxHeight(),
         verticalAlignmentValue,
         horizontalAlignmentValue,
         margin.Left,
         margin.Top,
         margin.Right,
         margin.Bottom);
  Wh_Log(L"    ActualOffset=(%.2f,%.2f,%.2f) VisualOffset=(%.2f,%.2f,%.2f) VisualSize=(%.2f,%.2f)",
         actualOffset.x,
         actualOffset.y,
         actualOffset.z,
         visualOffset.x,
         visualOffset.y,
         visualOffset.z,
         visualSize.x,
         visualSize.y);

  winrt::Windows::Foundation::Rect rect{};
  LogDebugRect(L"to-parent", parent ? TryGetDebugBoundsRelativeTo(element, parent, rect) : false, rect);
  LogDebugRect(L"to-repeater", taskbarFrameRepeater ? TryGetDebugBoundsRelativeTo(element, taskbarFrameRepeater, rect) : false, rect);
  LogDebugRect(L"to-rootGrid", rootGridTaskBar ? TryGetDebugBoundsRelativeTo(element, rootGridTaskBar, rect) : false, rect);
  LogDebugRect(L"to-taskFrame", taskFrame ? TryGetDebugBoundsRelativeTo(element, taskFrame, rect) : false, rect);
  LogDebugRect(L"to-xamlRoot", TryGetDebugBoundsRelativeTo(element, winrt::Windows::UI::Xaml::UIElement{nullptr}, rect), rect);
}

void LogAncestorGeometryChain(PCWSTR tag,
                              FrameworkElement element,
                              FrameworkElement const& rootGridTaskBar,
                              int maxDepth = 8) {
  Wh_Log(L"[TBGEOM] ancestor-chain for %s", tag);
  for (int depth = 0; element && depth < maxDepth; ++depth) {
    std::wstring className;
    std::wstring name;
    try { className = std::wstring(winrt::get_class_name(element).c_str()); } catch (...) { className = L"<class?>"; }
    try { name = std::wstring(element.Name().c_str()); } catch (...) { name = L""; }

    Thickness margin{};
    winrt::Windows::Foundation::Numerics::float3 actualOffset{};
    winrt::Windows::Foundation::Numerics::float3 visualOffset{};
    try { margin = element.Margin(); } catch (...) {}
    try { actualOffset = element.ActualOffset(); } catch (...) {}
    if (auto visual = winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(element)) {
      visualOffset = visual.Offset();
    }

    winrt::Windows::Foundation::Rect rect{};
    const bool okRoot = rootGridTaskBar && TryGetDebugBoundsRelativeTo(element, rootGridTaskBar, rect);
    int verticalAlignmentValue = -1;
    try { verticalAlignmentValue = static_cast<int>(element.VerticalAlignment()); } catch (...) {}

    Wh_Log(L"    #%d class=%s name=%s VA=%d Actual=%6.2fx%-6.2f ActualOffset=(%.2f,%.2f,%.2f) VisualOffset=(%.2f,%.2f,%.2f) Margin=(%.2f,%.2f,%.2f,%.2f) rootRect=%s X=%.2f Y=%.2f W=%.2f H=%.2f",
           depth,
           className.c_str(),
           name.c_str(),
           verticalAlignmentValue,
           element.ActualWidth(),
           element.ActualHeight(),
           actualOffset.x,
           actualOffset.y,
           actualOffset.z,
           visualOffset.x,
           visualOffset.y,
           visualOffset.z,
           margin.Left,
           margin.Top,
           margin.Right,
           margin.Bottom,
           okRoot ? L"ok" : L"--",
           okRoot ? rect.X : 0.0,
           okRoot ? rect.Y : 0.0,
           okRoot ? rect.Width : 0.0,
           okRoot ? rect.Height : 0.0);

    element = Media::VisualTreeHelper::GetParent(element).try_as<FrameworkElement>();
  }
}

void LogTaskbarGeometryProbe(PCWSTR reason,
                             std::wstring const& monitorName,
                             FrameworkElement const& xamlRootContent,
                             FrameworkElement const& taskFrame,
                             FrameworkElement const& rootGridTaskBar,
                             FrameworkElement const& taskbarFrameRepeater,
                             FrameworkElement const& trayFrame,
                             FrameworkElement const& systemTrayFrameGrid,
                             FrameworkElement const& taskbarBackground,
                             FrameworkElement const& backgroundFillChild,
                             FrameworkElement const& startButton,
                             double rootWidth,
                             double childrenWidthTaskbarDbl,
                             double taskbarLeftEdge,
                             double taskbarRightEdge,
                             double startButtonLeft,
                             double startButtonTop,
                             double startButtonWidth,
                             double startButtonHeight,
                             double predictedCenteredTaskbarLeft,
                             bool isOverflowing,
                             bool taskbarLayoutIsEdgeClamped,
                             bool useStableStartButtonAnchor,
                             float targetContentLeft,
                             float targetContentWidth,
                             float targetTaskRootOffsetX,
                             float targetOffsetXTray,
                             float targetBackgroundLeftScreen,
                             float targetBackgroundRightScreen,
                             TaskbarState const& state) {
  Wh_Log(L"[TBGEOM] ===== %s monitor=%s overflow=%d edgeClamp=%d useStableStart=%d =====",
         reason,
         monitorName.c_str(),
         isOverflowing ? 1 : 0,
         taskbarLayoutIsEdgeClamped ? 1 : 0,
         useStableStartButtonAnchor ? 1 : 0);
  Wh_Log(L"[TBGEOM] scalars rootWidth=%.2f taskChildren=%.2f measuredEdges=[%.2f..%.2f] predictedLeft=%.2f",
         rootWidth,
         childrenWidthTaskbarDbl,
         taskbarLeftEdge,
         taskbarRightEdge,
         predictedCenteredTaskbarLeft);
  Wh_Log(L"[TBGEOM] startSample X=%.2f Y=%.2f W=%.2f H=%.2f stable=%d stableRect X=%.2f Y=%.2f W=%.2f H=%.2f stablePasses=%d",
         startButtonLeft,
         startButtonTop,
         startButtonWidth,
         startButtonHeight,
         state.hasStableStartButtonAnchorRect ? 1 : 0,
         state.stableStartButtonAnchorLeft,
         state.stableStartButtonAnchorTop,
         state.stableStartButtonAnchorWidth,
         state.stableStartButtonAnchorHeight,
         state.startButtonAnchorStablePasses);
  Wh_Log(L"[TBGEOM] targets contentLeft=%.2f contentWidth=%.2f rootOffsetX=%.2f trayOffsetX=%.2f bgScreen=[%.2f..%.2f]",
         targetContentLeft,
         targetContentWidth,
         targetTaskRootOffsetX,
         targetOffsetXTray,
         targetBackgroundLeftScreen,
         targetBackgroundRightScreen);

  LogElementGeometry(L"xamlRootContent", xamlRootContent, rootGridTaskBar, taskbarFrameRepeater, taskFrame);
  LogElementGeometry(L"taskFrame", taskFrame, rootGridTaskBar, taskbarFrameRepeater, taskFrame);
  LogElementGeometry(L"rootGridTaskBar", rootGridTaskBar, rootGridTaskBar, taskbarFrameRepeater, taskFrame);
  LogElementGeometry(L"repeater", taskbarFrameRepeater, rootGridTaskBar, taskbarFrameRepeater, taskFrame);
  LogElementGeometry(L"startButton", startButton, rootGridTaskBar, taskbarFrameRepeater, taskFrame);
  LogElementGeometry(L"trayFrame", trayFrame, rootGridTaskBar, taskbarFrameRepeater, taskFrame);
  LogElementGeometry(L"trayGrid", systemTrayFrameGrid, rootGridTaskBar, taskbarFrameRepeater, taskFrame);
  LogElementGeometry(L"taskbarBackground", taskbarBackground, rootGridTaskBar, taskbarFrameRepeater, taskFrame);
  LogElementGeometry(L"backgroundFill", backgroundFillChild, rootGridTaskBar, taskbarFrameRepeater, taskFrame);
  LogAncestorGeometryChain(L"startButton", startButton, rootGridTaskBar);
  LogAncestorGeometryChain(L"repeater", taskbarFrameRepeater, rootGridTaskBar);
  // Fork addition: SystemTrayFrameGrid -> SystemTrayFrame -> XAML root is the
  // chain that decides the tray's vertical placement, so log it alongside the
  // task-area chains.
  LogAncestorGeometryChain(L"trayGrid", systemTrayFrameGrid, rootGridTaskBar);
  Wh_Log(L"[TBGEOM] ===== end =====");
}
void UpdateGlobalSettings() {
  std::lock_guard<std::recursive_mutex> settingsLock(g_settingsMutex);
  auto getInt = [&](PCWSTR key) { return Wh_GetIntSetting(key); };
  auto clamp = [](int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; };
  // Booleans
  g_settings.userDefinedFlatTaskbarBottomCorners = (getInt(L"FlatTaskbarBottomCorners") != 0);
  g_settings.userDefinedFullWidthTaskbarBackground = (getInt(L"FullWidthTaskbarBackground") != 0) || g_unloading;
  if (g_settings.userDefinedFullWidthTaskbarBackground) g_settings.userDefinedFlatTaskbarBottomCorners = true;
  g_settings.userDefinedIgnoreShowDesktopButton = (getInt(L"IgnoreShowDesktopButton") != 0);
  g_settings.userDefinedTrayAreaDivider = (getInt(L"TrayAreaDivider") != 0) && !g_unloading;
  g_settings.userDefinedStyleTrayArea = (getInt(L"StyleTrayArea") != 0);
  g_settings.userDefinedAlignFlyoutInner = (getInt(L"AlignFlyoutInner") != 0);
  g_settings.userDefinedNotificationCenterPrimaryOnly = (getInt(L"NotificationCenterPrimaryOnly") != 0);
  g_settings.userDefinedCustomizeTaskbarBackground = (getInt(L"CustomizeTaskbarBackground") != 0);
  g_settings.userDefinedDisableCustomBlurBackground = (getInt(L"DisableCustomBlurBackground") != 0);
  PCWSTR appsDividerAlignment = Wh_GetStringSetting(L"AppsDividerAlignment");
  g_settings.userDefinedDividerLeftAligned =
      appsDividerAlignment && _wcsicmp(appsDividerAlignment, L"left") == 0;
  if (appsDividerAlignment) {
    Wh_FreeStringSetting(appsDividerAlignment);
  }
  // Gaps & Padding (non-negative)
  g_settings.userDefinedTrayTaskGap = g_unloading ? 0 : std::max(0, getInt(L"TrayTaskGap"));
  g_settings.userDefinedTaskbarBackgroundHorizontalPadding = g_unloading ? 0 : std::max(0, getInt(L"TaskbarBackgroundHorizontalPadding"));
  // Offset Y (negative up; non-positive settings sit on the screen edge)
  const int offsetY = std::max(-2, getInt(L"TaskbarOffsetY"));
  g_settings.userDefinedTaskbarOffsetY =
      (g_unloading || g_settings.userDefinedFlatTaskbarBottomCorners)
          ? 0
          : -offsetY;
  // Height & Sizes
  int h = ClampInt(abs(ReadPositiveIntSettingOrDefault(L"TaskbarHeight", kDefaultTaskbarHeight)),
                   kMinTaskbarHeight,
                   kMaxTaskbarHeight);
  g_settings.userDefinedTaskbarHeight = g_unloading ? kSystemMediumTaskbarButtonSize : h;
  int taskbarButtonSize = ClampInt(abs(ReadPositiveIntSettingOrDefault(L"TaskbarButtonSize", kDefaultTaskbarButtonSize)),
                                   kMinTaskbarButtonSize,
                                   kMaxTaskbarButtonSize);
  g_settings.userDefinedTaskbarButtonSize = g_unloading ? kSystemMediumTaskbarButtonSize : taskbarButtonSize;
  const int maxTaskbarIconSize =
      GetMaxTaskbarIconSizeForLayout(static_cast<int>(g_settings.userDefinedTaskbarHeight),
                                     static_cast<int>(g_settings.userDefinedTaskbarButtonSize));
  int taskbarIconSize = ClampInt(abs(ReadPositiveIntSettingOrDefault(L"TaskbarIconSize", kDefaultTaskbarIconSize)),
                                 kMinTaskbarIconSize,
                                 maxTaskbarIconSize);
  g_settings.userDefinedTaskbarIconSize = g_unloading ? kSystemMediumTaskbarIconSize : taskbarIconSize;
  g_settings.userDefinedTrayIconSize = std::max(kMinTrayIconSize, getInt(L"TrayIconSize"));
  g_settings.userDefinedTrayButtonSize = std::max(kMinTrayButtonSize, getInt(L"TrayButtonSize"));
  // Corner radii
  float tcr = float(fmax(0.0f, getInt(L"TaskbarCornerRadius")));
  tcr = fmin(tcr, g_settings.userDefinedTaskbarHeight / 2.0f);
  g_settings.userDefinedTaskbarCornerRadius = g_unloading ? 0.0f : tcr;
  int btnCr = clamp(abs(getInt(L"TaskButtonCornerRadius")), 0, g_settings.userDefinedTaskbarHeight / 2);
  g_settings.userDefinedTaskButtonCornerRadius = g_unloading ? 0 : btnCr;
  // Opacities & tints (0–100)
  int bgOp = clamp(abs(getInt(L"TaskbarBackgroundOpacity")), 0, 100);
  g_settings.userDefinedTaskbarBackgroundOpacity = bgOp;
  g_settings.userDefinedTaskbarBackgroundTint = clamp(abs(getInt(L"TaskbarBackgroundTint")), 0, 100);
  g_settings.userDefinedTaskbarBackgroundLuminosity = clamp(abs(getInt(L"TaskbarBackgroundLuminosity")), 0, 100);
  g_settings.userDefinedTaskbarBackgroundBlurAmount = clamp(abs(getInt(L"TaskbarBackgroundBlurAmount")), 0, 100);
  g_settings.userDefinedTaskbarBackgroundTintSaturation = clamp(abs(getInt(L"TaskbarBackgroundTintSaturation")), 0, 500);
  g_settings.userDefinedTaskbarBackgroundInversion = clamp(abs(getInt(L"TaskbarBackgroundInversion")), 0, 100);
  PCWSTR bgTintColor = Wh_GetStringSetting(L"TaskbarBackgroundTintColor");
  g_settings.userDefinedTaskbarBackgroundTintColor = (bgTintColor && *bgTintColor) ? bgTintColor : L"{ThemeResource CardStrokeColorDefaultSolid}";
  if (bgTintColor) {
    Wh_FreeStringSetting(bgTintColor);
  }
  PCWSTR bgFallbackColor = Wh_GetStringSetting(L"TaskbarBackgroundFallbackColor");
  g_settings.userDefinedTaskbarBackgroundFallbackColor = (bgFallbackColor && *bgFallbackColor) ? bgFallbackColor : L"{ThemeResource CardStrokeColorDefaultSolid}";
  if (bgFallbackColor) {
    Wh_FreeStringSetting(bgFallbackColor);
  }
  // Border opacity: 0–255
  int bOp = clamp(abs(getInt(L"TaskbarBorderOpacity")), 0, 100);
  g_settings.userDefinedTaskbarBorderOpacity = uint8_t(round(bOp * 2.55f));
  // Border thickness: 0.0–10.0 (10% of [0–100])
  g_settings.userDefinedTaskbarBorderThickness = g_unloading ? 0.0f : (clamp(abs(getInt(L"TaskbarBorderThickness")), 0, 100) * 0.1f);
  g_settings.userDefinedAppsDividerThickness = g_unloading ? 0.0f : (clamp(abs(getInt(L"AppsDividerThickness")), 0, 100) * 0.1f);
  g_settings.userDefinedAppsDividerVerticalScale = g_unloading ? 0.0f : (clamp(abs(getInt(L"AppsDividerVerticalScale")), 0, 100) / 100.0f);
  // Border color
  PCWSTR originalHex = Wh_GetStringSetting(L"TaskbarBorderColorHex");
  PCWSTR hex = originalHex;
  if (!hex || *hex == L'\0') {
    hex = L"#ffffff";
  }
  if (*hex == L'#') ++hex;
  unsigned int r = 255, g = 255, b = 255;
  if (swscanf_s(hex, L"%02x%02x%02x", &r, &g, &b) != 3) {
    r = g = b = 255;
  }
  g_settings.borderColorR = r;
  g_settings.borderColorG = g;
  g_settings.borderColorB = b;
  if (originalHex) {
    Wh_FreeStringSetting(originalHex);
  }
  // String list
  PCWSTR dividerAppNames = Wh_GetStringSetting(L"DividedAppNames");
  CompileDividedAppPatternsTai(SplitAndTrim(dividerAppNames));
  if (dividerAppNames) {
    Wh_FreeStringSetting(dividerAppNames);
  }
}
bool HasInvalidSettings() {
  std::lock_guard<std::recursive_mutex> settingsLock(g_settingsMutex);
  if (g_settings.userDefinedTrayTaskGap < 0) return true;
  if (g_settings.userDefinedTaskbarBackgroundHorizontalPadding < 0) return true;
  if ((int)g_settings.userDefinedTaskbarOffsetY < -2 && !g_settings.userDefinedFlatTaskbarBottomCorners) return true;
  if (g_settings.userDefinedTaskbarHeight < kMinTaskbarHeight || g_settings.userDefinedTaskbarHeight > kMaxTaskbarHeight) return true;
  if (g_settings.userDefinedTaskbarIconSize <= 0) return true;
  if (g_settings.userDefinedTrayIconSize <= 0) return true;
  if (g_settings.userDefinedTaskbarButtonSize <= 0) return true;
  if (g_settings.userDefinedTrayButtonSize <= 0) return true;
  if (g_settings.userDefinedTaskbarCornerRadius < 0.0f || g_settings.userDefinedTaskbarCornerRadius > (g_settings.userDefinedTaskbarHeight / 2.0f)) return true;
  if (g_settings.userDefinedTaskButtonCornerRadius < 0 || g_settings.userDefinedTaskButtonCornerRadius > (g_settings.userDefinedTaskbarHeight / 2)) return true;
  if (g_settings.userDefinedTaskbarBackgroundOpacity > 100) return true;
  if (g_settings.userDefinedTaskbarBackgroundTint > 100) return true;
  if (g_settings.userDefinedTaskbarBackgroundLuminosity > 100) return true;
  if (g_settings.userDefinedTaskbarBackgroundBlurAmount > 100) return true;
  if (g_settings.userDefinedTaskbarBackgroundTintSaturation > 500) return true;
  if (g_settings.userDefinedTaskbarBackgroundInversion > 100) return true;
  if (g_settings.userDefinedTaskbarBorderOpacity > 255) return true;
  if (g_settings.userDefinedTaskbarBorderThickness < 0.0 || g_settings.userDefinedTaskbarBorderThickness > 10.0) return true;
  return false;
}
void LogAllSettings() {
  std::lock_guard<std::recursive_mutex> settingsLock(g_settingsMutex);
  Wh_Log(L"setting %d %s", g_settings.userDefinedTrayTaskGap, L"userDefinedTrayTaskGap");
  Wh_Log(L"setting %d %s", g_settings.userDefinedTaskbarBackgroundHorizontalPadding, L"userDefinedTaskbarBackgroundHorizontalPadding");
  Wh_Log(L"setting %d %s", g_settings.userDefinedTaskbarOffsetY, L"userDefinedTaskbarOffsetY");
  Wh_Log(L"setting %d %s", g_settings.userDefinedTaskbarHeight, L"userDefinedTaskbarHeight");
  Wh_Log(L"setting %d %s", g_settings.userDefinedTaskbarIconSize, L"userDefinedTaskbarIconSize");
  Wh_Log(L"setting %d %s", g_settings.userDefinedTrayIconSize, L"userDefinedTrayIconSize");
  Wh_Log(L"setting %d %s", g_settings.userDefinedTaskbarButtonSize, L"userDefinedTaskbarButtonSize");
  Wh_Log(L"setting %d %s", g_settings.userDefinedTrayButtonSize, L"userDefinedTrayButtonSize");
  Wh_Log(L"setting %d %s", (int)g_settings.userDefinedTaskbarCornerRadius, L"userDefinedTaskbarCornerRadius");
  Wh_Log(L"setting %d %s", g_settings.userDefinedTaskButtonCornerRadius, L"userDefinedTaskButtonCornerRadius");
  Wh_Log(L"setting %d %s", g_settings.userDefinedFlatTaskbarBottomCorners ? 1 : 0, L"userDefinedFlatTaskbarBottomCorners");
  Wh_Log(L"setting %d %s", g_settings.userDefinedTaskbarBackgroundOpacity, L"userDefinedTaskbarBackgroundOpacity");
  Wh_Log(L"setting %d %s", g_settings.userDefinedTaskbarBackgroundTint, L"userDefinedTaskbarBackgroundTint");
  Wh_Log(L"setting %d %s", g_settings.userDefinedTaskbarBackgroundLuminosity, L"userDefinedTaskbarBackgroundLuminosity");
  Wh_Log(L"setting %d %s", g_settings.userDefinedTaskbarBackgroundBlurAmount, L"userDefinedTaskbarBackgroundBlurAmount");
  Wh_Log(L"setting %s %s", g_settings.userDefinedTaskbarBackgroundTintColor.c_str(), L"userDefinedTaskbarBackgroundTintColor");
  Wh_Log(L"setting %d %s", g_settings.userDefinedTaskbarBackgroundTintSaturation, L"userDefinedTaskbarBackgroundTintSaturation");
  Wh_Log(L"setting %d %s", g_settings.userDefinedTaskbarBackgroundInversion, L"userDefinedTaskbarBackgroundInversion");
  Wh_Log(L"setting %s %s", g_settings.userDefinedTaskbarBackgroundFallbackColor.c_str(), L"userDefinedTaskbarBackgroundFallbackColor");
  Wh_Log(L"setting %d %s", g_settings.userDefinedTaskbarBorderOpacity, L"userDefinedTaskbarBorderOpacity");
  Wh_Log(L"setting %d %s", (int)(g_settings.userDefinedTaskbarBorderThickness * 100.0 / 10.0), L"userDefinedTaskbarBorderThickness (scaled)");
  Wh_Log(L"setting %d %s", g_settings.userDefinedFullWidthTaskbarBackground ? 1 : 0, L"userDefinedFullWidthTaskbarBackground");
  Wh_Log(L"setting %d %s", g_settings.userDefinedIgnoreShowDesktopButton ? 1 : 0, L"userDefinedIgnoreShowDesktopButton");
  Wh_Log(L"setting %d %s", g_settings.userDefinedStyleTrayArea ? 1 : 0, L"userDefinedStyleTrayArea");
  Wh_Log(L"setting %d %s", g_settings.userDefinedTrayAreaDivider ? 1 : 0, L"userDefinedTrayAreaDivider");
  Wh_Log(L"setting %d %s", g_settings.borderColorR, L"borderColorR");
  Wh_Log(L"setting %d %s", g_settings.borderColorG, L"borderColorG");
  Wh_Log(L"setting %d %s", g_settings.borderColorB, L"borderColorB");
  Wh_Log(L"setting %d %s", g_settings.userDefinedCustomizeTaskbarBackground ? 1 : 0, L"userDefinedCustomizeTaskbarBackground");
  Wh_Log(L"setting %d %s", g_settings.userDefinedDisableCustomBlurBackground ? 1 : 0, L"userDefinedDisableCustomBlurBackground");
}
// ---------------------------------------------------------------------------
// Fork addition: clip the taskbar window's clickable region to the visible
// island.
//
// The mod condenses the taskbar visually, but Shell_TrayWnd itself stays full
// monitor width, so right-clicks (and the "Task Manager / Taskbar settings"
// menu) land on the empty strips either side of the island. SetWindowRgn makes
// the OS route mouse input only to pixels inside the island; SetWindowRgn(NULL)
// restores the full-width clickable area.
//
// Inputs are monitor-relative DIPs and must already be POST island-scale: the
// island is shrunk with a composition Visual.Scale when it would overflow, so
// feeding pre-scale geometry here would clip a region wider than what is
// actually drawn. Only the X axis is clipped -- the full window height stays
// clickable so the auto-hide reveal zone is unaffected.
//
// Windows sets and clears this region too. It clips an auto-hidden taskbar to
// the sliver still on its monitor, so the rest cannot show on a neighbouring
// monitor, and clears the region again when the taskbar comes back. So the
// region is left to Windows while the taskbar is not entirely on its monitor,
// and otherwise checked against the window on every pass rather than set only
// when the island moves: that missed Windows clearing it, and never undid a
// region written to the wrong taskbar, which clipped the island mid-way on a
// laptop below an external monitor (upstream issue #32).
static bool IsTaskbarWindowOnItsMonitorTai(HWND taskbarWindow,
                                           RECT const& windowRect) {
  MONITORINFO monitorInfo{.cbSize = sizeof(MONITORINFO)};
  if (!GetMonitorInfoW(GetTaskbarMonitorTai(taskbarWindow), &monitorInfo)) {
    return false;
  }
  RECT const& monitor = monitorInfo.rcMonitor;
  return windowRect.left >= monitor.left && windowRect.top >= monitor.top &&
         windowRect.right <= monitor.right &&
         windowRect.bottom <= monitor.bottom;
}

// Fork addition: the taskbar window on a monitor, for callers that have only
// its name. Explorer keeps one taskbar per monitor.
static HWND FindTaskbarWindowForMonitorTai(std::wstring const& monitorName) {
  struct Context {
    const std::wstring* monitorName;
    HWND window;
  } context{&monitorName, nullptr};
  EnumWindows(
      [](HWND hWnd, LPARAM lParam) -> BOOL {
        auto* context = reinterpret_cast<Context*>(lParam);
        DWORD pid = 0;
        if (!GetWindowThreadProcessId(hWnd, &pid) ||
            pid != GetCurrentProcessId() || !IsTaskbarWindowClassTai(hWnd)) {
          return TRUE;
        }
        if (GetMonitorName(GetTaskbarMonitorTai(hWnd)) !=
            *context->monitorName) {
          return TRUE;
        }
        context->window = hWnd;
        return FALSE;
      },
      reinterpret_cast<LPARAM>(&context));
  return context.window;
}

// regionBox holds the bounds of the region this function last set, which is
// how a region of Windows' is told apart from the mod's. force re-applies a
// region the window already has the bounds of, for when only its corner
// radius changed. Returns false when the window is not on its monitor.
bool UpdateTaskbarWindowRegion(HWND taskbarWindow,
                               float visibleXDip,
                               float visibleWidthDip,
                               float cornerRadiusDip,
                               float rasterizationScale,
                               bool force,
                               RECT* regionBox) {
  RECT wnd{};
  if (!taskbarWindow || !GetWindowRect(taskbarWindow, &wnd)) {
    return false;
  }
  RECT current{};
  const int currentType = GetWindowRgnBox(taskbarWindow, &current);
  const bool hasRegion =
      currentType == SIMPLEREGION || currentType == COMPLEXREGION;
  const bool hasOwnRegion = hasRegion && EqualRect(&current, regionBox);
  if (!IsTaskbarWindowOnItsMonitorTai(taskbarWindow, wnd)) {
    // Auto-hidden, or sliding in or out. The clip goes, so that anywhere
    // along the screen edge brings the taskbar back, as without the mod.
    if (hasOwnRegion) {
      SetWindowRgn(taskbarWindow, nullptr, TRUE);
    }
    return false;
  }
  const int wndW = wnd.right - wnd.left;
  const int wndH = wnd.bottom - wnd.top;
  if (wndW <= 0 || wndH <= 0) {
    return false;
  }
  const float scale = rasterizationScale > 0.0f ? rasterizationScale : 1.0f;
  int x1 = static_cast<int>(std::lround(visibleXDip * scale));
  int x2 = static_cast<int>(
      std::lround((visibleXDip + visibleWidthDip) * scale));
  if (x1 < 0) x1 = 0;
  if (x2 > wndW) x2 = wndW;
  if (x2 - x1 < 10) {
    // Too small to be useful; fall back to the full window rather than
    // risk making the taskbar unclickable.
    if (hasRegion) {
      SetWindowRgn(taskbarWindow, nullptr, TRUE);
    }
    return true;
  }
  if (!force && hasOwnRegion && std::abs(current.left - x1) <= 1 &&
      std::abs(current.right - x2) <= 1 &&
      std::abs(current.bottom - wndH) <= 1) {
    return true;
  }
  int cr = static_cast<int>(std::lround(cornerRadiusDip * scale));
  if (cr < 0) cr = 0;
  if (cr * 2 > x2 - x1) cr = (x2 - x1) / 2;
  if (cr * 2 > wndH) cr = wndH / 2;
  HRGN hRgn = (cr > 0) ? CreateRoundRectRgn(x1, 0, x2 + 1, wndH + 1,
                                            cr * 2, cr * 2)
                       : CreateRectRgn(x1, 0, x2, wndH);
  // SetWindowRgn takes ownership of hRgn; do not DeleteObject after.
  SetWindowRgn(taskbarWindow, hRgn, TRUE);
  if (GetWindowRgnBox(taskbarWindow, regionBox) == ERROR) {
    SetRectEmpty(regionBox);
  }
  return true;
}

// Fork addition: a secondary taskbar could sit on screen with its XAML island
// window hidden, drawing nothing, and moving the mouse to the screen edge did
// not bring it back (the laptop taskbar that seemed to render "outside the
// visible area").
//
// Windows hides a taskbar's island while the taskbar is auto-hidden
// (TaskbarHost::SetAutoHide). A secondary taskbar being set up, when a monitor
// is connected or Explorer restarts, takes that from the primary taskbar's
// state instead of its own (TaskbarHost::Start_System calls
// TrayUI::GetAutoHideFlags), while the secondary taskbar itself starts out
// shown. So if the primary is auto-hidden at that moment, the new taskbar is
// shown but empty, and since it counts as shown, hovering does not unhide it.
// It recovers only once it has auto-hidden and come back, and "Taskbar
// auto-hide when maximized" puts that off until something is maximized there.
//
// Windows hides the island only after sliding the taskbar off its monitor, and
// shows it before sliding it back, so a hidden island on a taskbar that sits
// entirely on its monitor is always this state. It is shown the way
// TaskbarHost::SetAutoHide shows it: the island's IsVisible is nothing but the
// window's visibility, set with ShowWindow(SW_SHOWNA).
void RepairSecondaryTaskbarIslandTai(HWND taskbarWindow,
                                     std::wstring const& monitorName) {
  if (g_unloading || !taskbarWindow || !IsWindowVisible(taskbarWindow)) {
    return;
  }
  WCHAR className[64]{};
  if (!GetClassNameW(taskbarWindow, className, ARRAYSIZE(className)) ||
      _wcsicmp(className, L"Shell_SecondaryTrayWnd") != 0) {
    return;
  }
  BOOL cloaked = FALSE;
  if (SUCCEEDED(DwmGetWindowAttribute(taskbarWindow, DWMWA_CLOAKED, &cloaked,
                                      sizeof(cloaked))) &&
      cloaked) {
    return;
  }
  HWND island = FindWindowExW(
      taskbarWindow, nullptr,
      L"Windows.UI.Composition.DesktopWindowContentBridge", nullptr);
  RECT windowRect{};
  if (!island || IsWindowVisible(island) ||
      !GetWindowRect(taskbarWindow, &windowRect) ||
      !IsTaskbarWindowOnItsMonitorTai(taskbarWindow, windowRect)) {
    return;
  }
  ShowWindow(island, SW_SHOWNA);
  WCHAR detail[160];
  swprintf_s(detail, ARRAYSIZE(detail),
             L"shown monitor=%s window=(%ld,%ld)-(%ld,%ld)",
             monitorName.c_str(), windowRect.left, windowRect.top,
             windowRect.right, windowRect.bottom);
  Wh_Log(L"[SecondaryTaskbarIsland] %s", detail);
  LogTaskbarEventToFileTai(L"SecondaryTaskbarIsland", detail);
}

// Clear the region on every taskbar window in this process, regardless of
// monitor. Used on unload so a clipped taskbar is never left behind if the
// final ApplyStyle pass does not reach the region update.
void ClearAllTaskbarWindowRegionsTai() {
  EnumWindows(
      [](HWND hWnd, LPARAM) -> BOOL {
        DWORD pid = 0;
        if (!GetWindowThreadProcessId(hWnd, &pid) ||
            pid != GetCurrentProcessId()) {
          return TRUE;
        }
        if (IsTaskbarWindowClassTai(hWnd)) {
          SetWindowRgn(hWnd, nullptr, TRUE);
        }
        return TRUE;
      },
      0);
}

// Fork addition: open the keyboard layout flyout -- the input switcher, shown
// by clicking the language indicator or pressing Win+Space -- above the
// language indicator on the island instead of at the right edge of the screen.
//
// InputSwitch.dll hosts that flyout inside explorer.exe, as a top-level
// Shell_InputSwitchTopLevelWindow driven from its own UI thread. It centres the
// window on an anchor rectangle from the tray, and that anchor is the tray at
// its stock position: this mod moves the tray with a composition Offset on
// SystemTrayFrameGrid, which XAML layout never sees. The window is only ever
// moved by FlyoutWindowHost::SetWindowPosition, a SetWindowPos without
// SWP_NOMOVE, so SetWindowPos_Hook re-centres it there, before it is drawn.
// Windows' choice of monitor and its Y above the taskbar are kept.
//
// That hook runs on the input switcher's thread, where the taskbar's XAML must
// not be touched, so ApplyStyle records where each monitor's indicator is drawn
// while it has the tray's target geometry in hand. Values are monitor-relative
// DIPs, already post island-scale, keyed by monitor device name.
static std::mutex g_languageIndicatorCentersMutex;
static std::unordered_map<std::wstring, float> g_languageIndicatorCentersXDip;

static bool IsLanguageIndicatorContentTai(winrt::hstring const& className) {
  return className == L"SystemTray.LanguageTextIconContent" ||
         className == L"SystemTray.LanguageImageIconContent";
}

// Depth-first search for the indicator's content, which is only the glyph,
// then up to the SystemTray.IconView that owns it, which is the whole button.
// Collapsed subtrees are skipped, so a hidden indicator counts as absent.
static FrameworkElement FindLanguageIndicatorTai(
    FrameworkElement const& systemTrayFrameGrid) {
  constexpr int kMaxDepth = 16;
  std::vector<std::pair<FrameworkElement, int>> pending;
  pending.emplace_back(systemTrayFrameGrid, 0);
  while (!pending.empty()) {
    auto [element, depth] = std::move(pending.back());
    pending.pop_back();
    if (element.Visibility() !=
        winrt::Windows::UI::Xaml::Visibility::Visible) {
      continue;
    }
    if (depth > 0 &&
        IsLanguageIndicatorContentTai(winrt::get_class_name(element))) {
      for (auto parent = Media::VisualTreeHelper::GetParent(element)
                             .try_as<FrameworkElement>();
           parent && parent != systemTrayFrameGrid;
           parent = Media::VisualTreeHelper::GetParent(parent)
                        .try_as<FrameworkElement>()) {
        if (winrt::get_class_name(parent) == L"SystemTray.IconView") {
          return parent;
        }
      }
      return element;
    }
    if (depth >= kMaxDepth) {
      continue;
    }
    const int childrenCount =
        Media::VisualTreeHelper::GetChildrenCount(element);
    for (int i = childrenCount - 1; i >= 0; i--) {
      if (auto child = Media::VisualTreeHelper::GetChild(element, i)
                           .try_as<FrameworkElement>()) {
        pending.emplace_back(std::move(child), depth + 1);
      }
    }
  }
  return nullptr;
}

// Grid-local coordinates never include SystemTrayFrameGrid's own composition
// transform, so replay the one ApplyStyle gives it: the arranged position,
// plus the Offset the tray animation targets, then the island scale about the
// grid visual's CenterPoint. trayFrame sits directly in the XAML root and the
// grid directly in trayFrame, so their ActualOffsets add up to the grid's
// arranged position in the root.
static void RecordLanguageIndicatorCenterTai(
    std::wstring const& monitorName,
    FrameworkElement const& trayFrame,
    FrameworkElement const& systemTrayFrameGrid,
    float targetOffsetXTray,
    float trayScaleCenterLocalX,
    float targetTaskbarIslandScale) {
  std::optional<float> centerXDip;
  if (!g_unloading) {
    try {
      if (auto indicator = FindLanguageIndicatorTai(systemTrayFrameGrid)) {
        const float width = static_cast<float>(indicator.ActualWidth());
        if (width > 0.0f) {
          const float localCenterX =
              indicator.TransformToVisual(systemTrayFrameGrid)
                  .TransformPoint({width / 2.0f, 0.0f})
                  .X;
          const float gridVisualLeft = trayFrame.ActualOffset().x +
                                       systemTrayFrameGrid.ActualOffset().x +
                                       targetOffsetXTray;
          const float center = ApplyScaleToScreenX(
              gridVisualLeft + localCenterX,
              gridVisualLeft + trayScaleCenterLocalX,
              targetTaskbarIslandScale);
          if (std::isfinite(center)) {
            centerXDip = center;
          }
        }
      }
    } catch (...) {
      centerXDip.reset();
    }
  }
  std::lock_guard<std::mutex> lock(g_languageIndicatorCentersMutex);
  auto it = g_languageIndicatorCentersXDip.find(monitorName);
  if (!centerXDip) {
    if (it != g_languageIndicatorCentersXDip.end()) {
      g_languageIndicatorCentersXDip.erase(it);
      Wh_Log(L"[InputSwitch] %s: no language indicator", monitorName.c_str());
    }
    return;
  }
  if (it == g_languageIndicatorCentersXDip.end() ||
      std::abs(it->second - *centerXDip) > 0.5f) {
    g_languageIndicatorCentersXDip[monitorName] = *centerXDip;
    Wh_Log(L"[InputSwitch] %s: language indicator centre at %.2f DIP",
           monitorName.c_str(), *centerXDip);
  }
}

static bool TryGetLanguageIndicatorCenterXDipTai(
    std::wstring const& monitorName,
    float* centerXDip) {
  std::lock_guard<std::mutex> lock(g_languageIndicatorCentersMutex);
  auto it = g_languageIndicatorCentersXDip.find(monitorName);
  if (it == g_languageIndicatorCentersXDip.end()) {
    return false;
  }
  *centerXDip = it->second;
  return true;
}

static void ClearLanguageIndicatorCentersTai() {
  std::lock_guard<std::mutex> lock(g_languageIndicatorCentersMutex);
  g_languageIndicatorCentersXDip.clear();
}

// Returns the X that centres the flyout on the language indicator of the
// monitor Windows placed it on, clamped to that monitor's work area. Staying
// on Windows' monitor means this move never changes the flyout's DPI.
static int PlaceInputSwitchFlyoutXTai(HWND hWnd,
                                      int x,
                                      int y,
                                      int cx,
                                      int cy,
                                      UINT flags) {
  // FlyoutWindowHost::SetWindowPosition passes SWP_NOSIZE with a zero size;
  // FlyoutWindowHost::SetWindowSize has sized the window just before.
  if ((flags & SWP_NOSIZE) || cx <= 0 || cy <= 0) {
    RECT windowRect{};
    if (!GetWindowRect(hWnd, &windowRect)) {
      return x;
    }
    cx = windowRect.right - windowRect.left;
    cy = windowRect.bottom - windowRect.top;
  }
  if (cx <= 0 || cy <= 0) {
    return x;
  }
  const RECT proposedRect{x, y, x + cx, y + cy};
  HMONITOR monitor = MonitorFromRect(&proposedRect, MONITOR_DEFAULTTONEAREST);
  MONITORINFO monitorInfo{.cbSize = sizeof(MONITORINFO)};
  if (!monitor || !GetMonitorInfoW(monitor, &monitorInfo)) {
    return x;
  }
  const std::wstring monitorName = GetMonitorName(monitor);
  PCWSTR anchor = L"language";
  float anchorCenterXDip = 0.0f;
  if (!TryGetLanguageIndicatorCenterXDipTai(monitorName, &anchorCenterXDip)) {
    // Secondary taskbars have no language indicator. Windows centres the
    // flyout on the whole stock tray, so centre it on the island's tray.
    TaskbarFlyoutStateSnapshot taskbarState;
    if (!TryGetTaskbarFlyoutStateSnapshot(monitorName, &taskbarState) ||
        taskbarState.lastRightMostEdgeTray <= 0 ||
        taskbarState.lastLeftMostEdgeTray >=
            static_cast<float>(taskbarState.lastRightMostEdgeTray)) {
      Wh_Log(L"[InputSwitch] No anchor recorded for monitor %s",
             monitorName.c_str());
      return x;
    }
    anchorCenterXDip =
        (taskbarState.lastLeftMostEdgeTray +
         static_cast<float>(taskbarState.lastRightMostEdgeTray)) /
        2.0f;
    anchor = L"tray";
  }
  UINT monitorDpiX = 96;
  UINT monitorDpiY = 96;
  if (FAILED(GetDpiForMonitor(monitor, MDT_DEFAULT, &monitorDpiX,
                              &monitorDpiY)) ||
      monitorDpiX == 0) {
    monitorDpiX = 96;
  }
  const int anchorCenterX =
      monitorInfo.rcMonitor.left +
      static_cast<int>(std::lround(anchorCenterXDip * monitorDpiX / 96.0f));
  const RECT& workArea = monitorInfo.rcWork;
  const int newX = std::max(
      static_cast<int>(workArea.left),
      std::min(anchorCenterX - (cx / 2), static_cast<int>(workArea.right) - cx));
  Wh_Log(L"[InputSwitch] %s: anchor=%s centre=%d X %d -> %d (cx=%d)",
         monitorName.c_str(), anchor, anchorCenterX, x, newX, cx);
  LogInputSwitchPlacementToFileTai(monitorName.c_str(), anchor,
                                   anchorCenterXDip, monitorDpiX, x, newX, y,
                                   cx, cy);
  return newX;
}

// Fork addition: open the Notification Center -- the clock and calendar
// flyout -- beside the island's tray on every open, including the first one
// after Explorer starts.
//
// ShellExperienceHost draws that flyout, but Explorer places it. On every Show,
// CActionCenterExperienceManager in twinui.pcshell.dll calls PositionView,
// which takes the rect GetViewPosition computes -- right-anchored in the work
// area of the monitor the flyout was opened on -- and pushes it to the window,
// both directly and through ShellExperienceHost. The start button position
// dependency only corrects the window when Explorer uncloaks it, and only once
// its title reads "Notification Center". On the first open after Explorer
// starts, the window is uncloaked while still 1x1 and untitled, and only then
// moved to the screen edge, so that open stayed at the edge. Shifting the rect
// GetViewPosition returns sends every push to the spot the uncloak correction
// picks, so the two never disagree.
//
// twinui.pcshell.dll loads after this mod when Explorer starts, so the hook is
// then installed from a thread that waits for the module.
using CActionCenterExperienceManager_GetViewPosition_t =
    long(WINAPI*)(void* pThis, RECT* rect);
CActionCenterExperienceManager_GetViewPosition_t
    CActionCenterExperienceManager_GetViewPosition_Original;

// Mirrors the ShellExperienceHost branch of
// dependencies/patches/startbuttonposition_start_menu_position_code.cpp, which
// still runs at uncloak; keep the two in step. Only the horizontal position
// changes, as there, and the same cases are left alone: a flyout that does not
// hang from the top of its monitor (the y != 0 check there), and secondary
// monitors under NotificationCenterPrimaryOnly.
static void PlaceNotificationCenterViewRectTai(RECT* rect) {
  if (g_unloading ||
      !g_settings_startbuttonposition.MoveFlyoutNotificationCenter) {
    return;
  }
  // Until the flyout reports its size, the first open asks for a zero-width
  // rect at the monitor's edge. The next PositionView brings the width.
  const int cx = rect->right - rect->left;
  if (cx <= 0 || rect->bottom <= rect->top) {
    return;
  }
  HMONITOR monitor = MonitorFromRect(rect, MONITOR_DEFAULTTONEAREST);
  MONITORINFO monitorInfo{.cbSize = sizeof(MONITORINFO)};
  if (!monitor || !GetMonitorInfoW(monitor, &monitorInfo)) {
    return;
  }
  if (rect->top != monitorInfo.rcMonitor.top) {
    return;
  }
  if (GetUserDefinedNotificationCenterPrimaryOnly() &&
      !(monitorInfo.dwFlags & MONITORINFOF_PRIMARY)) {
    return;
  }
  const std::wstring monitorName = GetMonitorName(monitor);
  TaskbarFlyoutStateSnapshot taskbarState;
  if (!TryGetTaskbarFlyoutStateSnapshot(monitorName, &taskbarState) ||
      taskbarState.lastRightMostEdgeTray <= 0 ||
      taskbarState.lastRootWidth <= 0.0f) {
    Wh_Log(L"[NotificationCenter] No tray recorded for monitor %s",
           monitorName.c_str());
    return;
  }
  UINT monitorDpiX = 96;
  UINT monitorDpiY = 96;
  if (FAILED(GetDpiForMonitor(monitor, MDT_DEFAULT, &monitorDpiX,
                              &monitorDpiY)) ||
      monitorDpiX == 0) {
    monitorDpiX = 96;
  }
  const float dpiScale = monitorDpiX / 96.0f;
  const bool alignFlyoutInner = GetUserDefinedAlignFlyoutInner();
  const int flyoutInnerPaddingPx = GetFlyoutInnerPaddingPx(dpiScale);
  const float absRootWidth = taskbarState.lastRootWidth * dpiScale;
  int localX = static_cast<int>(
      taskbarState.lastRightMostEdgeTray * dpiScale -
      (alignFlyoutInner ? (cx - flyoutInnerPaddingPx) : (cx / 2.0f)));
  localX = std::max(0, std::min(localX, static_cast<int>(absRootWidth - cx)));
  const int x = monitorInfo.rcMonitor.left + localX;
  Wh_Log(L"[NotificationCenter] %s: X %ld -> %d (cx=%d)", monitorName.c_str(),
         rect->left, x, cx);
  LogNotificationCenterPlacementToFileTai(monitorName.c_str(), monitorDpiX,
                                          taskbarState.lastRightMostEdgeTray,
                                          *rect, x);
  OffsetRect(rect, x - rect->left, 0);
}

long WINAPI CActionCenterExperienceManager_GetViewPosition_Hook(void* pThis,
                                                                RECT* rect) {
  g_hookCallCounter++;
  const long result =
      CActionCenterExperienceManager_GetViewPosition_Original(pThis, rect);
  if (SUCCEEDED(result) && rect) {
    try {
      PlaceNotificationCenterViewRectTai(rect);
    } catch (...) {
      // Keep Windows' rect: an exception must not unwind into Explorer.
    }
  }
  g_hookCallCounter--;
  return result;
}

// Fork addition: open the Start menu on the monitor in use when it is opened
// other than with a taskbar's Start button (the Win key, Ctrl+Esc).
//
// XamlLauncher::ShowStartView calls AdjustMonitorConnectedIfNeeded for those,
// which moves the launcher to the primary monitor unless it is there already,
// so the Win key always opened Start on the main display. A Start button click
// skips that call; the launcher was moved to the clicked taskbar's monitor
// beforehand, with ConnectToMonitor(HWND, POINT). Doing the same with the
// monitor in use has Windows lay the Start menu, and the Search pane that
// opens with it, out on that monitor itself; moving the laid-out window
// afterwards draws it part-way across the wrong monitor.
//
// ConnectToMonitor returns false when the launcher is on that monitor already,
// so its result is not a failure.
using ImmersiveMonitorHelper_AdjustMonitorConnectedIfNeeded_t =
    HRESULT(WINAPI*)(void* pThis);
static ImmersiveMonitorHelper_AdjustMonitorConnectedIfNeeded_t
    ImmersiveMonitorHelper_AdjustMonitorConnectedIfNeeded_Original = nullptr;
using ImmersiveMonitorHelper_ConnectToMonitor_t =
    bool(WINAPI*)(void* pThis, HWND window, POINT point);
static ImmersiveMonitorHelper_ConnectToMonitor_t
    ImmersiveMonitorHelper_ConnectToMonitor_Original = nullptr;

// The monitor of the window being worked in; or, when that is the desktop or a
// taskbar, which belong to no one monitor, the monitor under the mouse.
static HMONITOR GetMonitorInUseTai() {
  if (HWND foreground = GetForegroundWindow()) {
    HWND root = GetAncestor(foreground, GA_ROOTOWNER);
    WCHAR className[64]{};
    if (!root || !GetClassNameW(root, className, ARRAYSIZE(className)) ||
        (_wcsicmp(className, L"Progman") != 0 &&
         _wcsicmp(className, L"WorkerW") != 0 &&
         !IsTaskbarWindowClassTai(root))) {
      if (HMONITOR monitor =
              MonitorFromWindow(foreground, MONITOR_DEFAULTTONULL)) {
        return monitor;
      }
    }
  }
  POINT cursor{};
  return GetCursorPos(&cursor)
             ? MonitorFromPoint(cursor, MONITOR_DEFAULTTONULL)
             : nullptr;
}

static bool MonitorHasTaskbarTai(HMONITOR monitor) {
  struct Context {
    HMONITOR monitor;
    bool found;
  } context{monitor, false};
  EnumWindows(
      [](HWND hWnd, LPARAM lParam) -> BOOL {
        auto* context = reinterpret_cast<Context*>(lParam);
        DWORD pid = 0;
        if (GetWindowThreadProcessId(hWnd, &pid) &&
            pid == GetCurrentProcessId() && IsTaskbarWindowClassTai(hWnd) &&
            GetTaskbarMonitorTai(hWnd) == context->monitor) {
          context->found = true;
          return FALSE;
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&context));
  return context.found;
}

HRESULT WINAPI ImmersiveMonitorHelper_AdjustMonitorConnectedIfNeeded_Hook(
    void* pThis) {
  if (!g_unloading && ImmersiveMonitorHelper_ConnectToMonitor_Original &&
      Wh_GetIntSetting(L"StartMenuOnActiveMonitor")) {
    HMONITOR monitor = GetMonitorInUseTai();
    MONITORINFOEXW monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (monitor && MonitorHasTaskbarTai(monitor) &&
        GetMonitorInfoW(monitor, &monitorInfo)) {
      const RECT& bounds = monitorInfo.rcMonitor;
      const POINT center{(bounds.left + bounds.right) / 2,
                         (bounds.top + bounds.bottom) / 2};
      ImmersiveMonitorHelper_ConnectToMonitor_Original(pThis, nullptr, center);
      Wh_Log(L"[StartMenu] opening on the monitor in use: %s",
             monitorInfo.szDevice);
      return S_OK;
    }
  }
  return ImmersiveMonitorHelper_AdjustMonitorConnectedIfNeeded_Original(pThis);
}

static bool HookStartMenuMonitorTai(HMODULE twinuiPcshellModule) {
  WindhawkUtils::SYMBOL_HOOK twinuiPcshellHooks[] = {
      {{LR"(public: bool __cdecl ImmersiveMonitorHelper::ConnectToMonitor(struct HWND__ *,struct tagPOINT))"},
       &ImmersiveMonitorHelper_ConnectToMonitor_Original},
      {{LR"(public: long __cdecl ImmersiveMonitorHelper::AdjustMonitorConnectedIfNeeded(void))"},
       &ImmersiveMonitorHelper_AdjustMonitorConnectedIfNeeded_Original,
       ImmersiveMonitorHelper_AdjustMonitorConnectedIfNeeded_Hook},
  };
  const bool hooked = WindhawkUtils::HookSymbols(
      twinuiPcshellModule, twinuiPcshellHooks, ARRAYSIZE(twinuiPcshellHooks));
  Wh_Log(L"[StartMenu] monitor hook %s", hooked ? L"set" : L"unavailable");
  return hooked;
}

static bool HookNotificationCenterViewPositionTai(HMODULE twinuiPcshellModule) {
  WindhawkUtils::SYMBOL_HOOK twinuiPcshellHooks[] = {
      {{LR"(private: long __cdecl CActionCenterExperienceManager::GetViewPosition(struct tagRECT &))"},
       &CActionCenterExperienceManager_GetViewPosition_Original,
       CActionCenterExperienceManager_GetViewPosition_Hook},
  };
  const bool hooked = WindhawkUtils::HookSymbols(
      twinuiPcshellModule, twinuiPcshellHooks, ARRAYSIZE(twinuiPcshellHooks));
  Wh_Log(L"[NotificationCenter] GetViewPosition hook %s",
         hooked ? L"set" : L"unavailable");
  // Fork addition: see ImmersiveMonitorHelper_AdjustMonitorConnectedIfNeeded_Hook.
  // Hooked separately so that either can be missing on a Windows build.
  const bool startMenuHooked = HookStartMenuMonitorTai(twinuiPcshellModule);
  return hooked || startMenuHooked;
}

// A Win32 thread and event rather than std::thread: a std::thread still
// joinable when this DLL's globals are destroyed at process exit would
// terminate Explorer.
static HANDLE g_notificationCenterHookWaitThread = nullptr;
static HANDLE g_notificationCenterHookWaitStopEvent = nullptr;
static bool g_notificationCenterHookPending = false;

static DWORD WINAPI NotificationCenterHookWaitThreadTai(LPVOID) {
  constexpr DWORD kPollIntervalMs = 250;
  constexpr ULONGLONG kGiveUpAfterMs = 2 * 60 * 1000;
  const ULONGLONG deadline = GetTickCount64() + kGiveUpAfterMs;
  do {
    if (HMODULE module = GetModuleHandleW(L"twinui.pcshell.dll")) {
      // The first time, the symbols can take a while to download.
      // StopNotificationCenterHookWaitTai waits for this thread, so the hook
      // is never applied during unload.
      if (HookNotificationCenterViewPositionTai(module) &&
          WaitForSingleObject(g_notificationCenterHookWaitStopEvent, 0) ==
              WAIT_TIMEOUT) {
        Wh_ApplyHookOperations();
      }
      return 0;
    }
    if (GetTickCount64() >= deadline) {
      Wh_Log(L"[NotificationCenter] twinui.pcshell.dll never loaded");
      return 0;
    }
  } while (WaitForSingleObject(g_notificationCenterHookWaitStopEvent,
                               kPollIntervalMs) == WAIT_TIMEOUT);
  return 0;
}

// Wh_ModInit: hooks at once when the mod loads into a running Explorer, as
// after compiling or enabling it; otherwise leaves the hook to the wait thread.
static void HookNotificationCenterViewPositionIfLoadedTai() {
  HMODULE module = GetModuleHandleW(L"twinui.pcshell.dll");
  g_notificationCenterHookPending = !module;
  if (module) {
    HookNotificationCenterViewPositionTai(module);
  }
}

// Wh_ModAfterInit.
static void StartNotificationCenterHookWaitTai() {
  if (!g_notificationCenterHookPending || g_notificationCenterHookWaitThread) {
    return;
  }
  g_notificationCenterHookWaitStopEvent =
      CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!g_notificationCenterHookWaitStopEvent) {
    return;
  }
  g_notificationCenterHookWaitThread = CreateThread(
      nullptr, 0, NotificationCenterHookWaitThreadTai, nullptr, 0, nullptr);
  if (!g_notificationCenterHookWaitThread) {
    Wh_Log(L"[NotificationCenter] Failed to start the hook wait thread");
    CloseHandle(g_notificationCenterHookWaitStopEvent);
    g_notificationCenterHookWaitStopEvent = nullptr;
  }
}

// Wh_ModBeforeUninit.
static void StopNotificationCenterHookWaitTai() {
  if (g_notificationCenterHookWaitStopEvent) {
    SetEvent(g_notificationCenterHookWaitStopEvent);
  }
  if (g_notificationCenterHookWaitThread) {
    WaitForSingleObject(g_notificationCenterHookWaitThread, INFINITE);
    CloseHandle(g_notificationCenterHookWaitThread);
    g_notificationCenterHookWaitThread = nullptr;
  }
  if (g_notificationCenterHookWaitStopEvent) {
    CloseHandle(g_notificationCenterHookWaitStopEvent);
    g_notificationCenterHookWaitStopEvent = nullptr;
  }
}

// Fork addition: open the taskbar's right-click menus at the island instead of
// at the edges of the screen -- the clock's and the system tray icons'
// (network, volume, battery, language and the rest) at its right end, the
// Start button's at its left end.
//
// Tray icons: SystemTray.dll shows their menus through
// ContextMenuPositionHelper. With the feature set current builds ship,
// ShowContextMenuFlyout opens the MenuFlyout at a point from GetShowPosition:
// ContextMenuMargin in from the taskbar root's top-right corner, whichever
// icon was clicked. XAML puts the menu's top-left corner on that point and
// flips it to the point's left or top on an axis where it does not fit
// (measured on this build, not documented). At the corner the menu flips above
// and to the left, next to where the tray ends on a stock taskbar -- except a
// one-item menu, which fits under the point and covers the taskbar. The
// GetShowPosition hook moves the point left by as much as the Notification
// Center and Quick Settings move, and once the menu has opened, the menu's
// Opened handler (PositionContextMenuPopup) puts its bottom-right corner on the
// point, where it would open to the right of it. Builds that centre the menu on
// its icon instead (AlignContextMenuPopup) never call GetShowPosition, and XAML
// already sees the icon where the island draws it, so those menus are left
// alone.
//
// Start button: with MoveFlyoutStartMenu on, the start button position
// dependency reports a left-aligned taskbar while this menu opens, so
// ShowStartButtonContextMenuAsync opens it TopEdgeAlignedLeft at
// GetLeadingStartFlyoutPosition, a margin in from the taskbar frame's top-left
// corner -- the left edge of the screen. The hook moves the point right by as
// much as the Start and Search menus move.
//
// Both keep Windows' place relative to the flyouts beside them, inside the
// island whatever AlignFlyoutInner says, as Windows keeps them inside the
// taskbar.

// The hooks only get XAML elements, so ApplyStyle records which monitor each
// taskbar's XamlRoot is on. Keyed by the XamlRoot's COM identity, which is
// compared and never dereferenced.
static std::mutex g_xamlRootMonitorNamesMutexTai;
static std::unordered_map<void*, std::wstring> g_xamlRootMonitorNamesTai;

static void* GetComIdentityTai(winrt::Windows::Foundation::IInspectable const& object) {
  return winrt::get_abi(object.as<winrt::Windows::Foundation::IUnknown>());
}

static void RecordXamlRootMonitorTai(FrameworkElement const& xamlRootContent,
                                     std::wstring const& monitorName) {
  void* identity = nullptr;
  try {
    if (auto xamlRoot = xamlRootContent.XamlRoot()) {
      identity = GetComIdentityTai(xamlRoot);
    }
  } catch (...) {
    // Never let the record fail an ApplyStyle pass.
  }
  if (!identity) {
    return;
  }
  std::lock_guard<std::mutex> lock(g_xamlRootMonitorNamesMutexTai);
  auto [it, inserted] = g_xamlRootMonitorNamesTai.try_emplace(identity, monitorName);
  if (!inserted && it->second != monitorName) {
    it->second = monitorName;
  }
}

static bool TryGetXamlRootMonitorTai(XamlRoot const& xamlRoot,
                                     std::wstring* monitorName) {
  void* identity = GetComIdentityTai(xamlRoot);
  std::lock_guard<std::mutex> lock(g_xamlRootMonitorNamesMutexTai);
  auto it = g_xamlRootMonitorNamesTai.find(identity);
  if (it == g_xamlRootMonitorNamesTai.end()) {
    return false;
  }
  *monitorName = it->second;
  return true;
}

// WindowsUdk.UI.Shell.TaskbarLocation.
constexpr int kTaskbarLocationTopTai = 1;
constexpr int kTaskbarLocationBottomTai = 3;

// Monitor, DPI-scaled flyout inner padding and island geometry for the taskbar
// that owns xamlRoot.
struct ContextMenuTaskbarTai {
  std::wstring monitorName;
  TaskbarFlyoutStateSnapshot state;
  float innerPaddingDip;
};

static bool TryGetContextMenuTaskbarTai(XamlRoot const& xamlRoot,
                                        ContextMenuTaskbarTai* taskbar) {
  if (!xamlRoot || !TryGetXamlRootMonitorTai(xamlRoot, &taskbar->monitorName) ||
      !TryGetTaskbarFlyoutStateSnapshot(taskbar->monitorName, &taskbar->state)) {
    return false;
  }
  float rasterizationScale = static_cast<float>(xamlRoot.RasterizationScale());
  if (!(rasterizationScale > 0.0f)) {
    rasterizationScale = 1.0f;
  }
  taskbar->innerPaddingDip =
      GetFlyoutInnerPaddingPx(rasterizationScale) / rasterizationScale;
  return true;
}

// Handed from the GetShowPosition hook to the PositionContextMenuPopup hook for
// the menu being opened on this thread. Values are root DIPs. Trivial members
// only, as it is thread_local.
struct TrayContextMenuPlacementTai {
  bool pending;
  void* grid;  // The icon's Grid, compared never dereferenced.
  ULONGLONG tick;
  float rootWidth;
  float margin;
  float anchorX;  // Where the menu's right edge goes.
  float anchorY;  // Where its bottom goes, or its top under a top taskbar.
  bool above;
  int trayRight;
  WCHAR monitorName[CCHDEVICENAME];
};
static thread_local TrayContextMenuPlacementTai g_trayContextMenuPlacementTai;

static void MoveTrayContextMenuAnchorTai(winrt::Windows::Foundation::Point& position,
                                         int taskbarLocation,
                                         double margin,
                                         Controls::Grid const& grid,
                                         double rootWidth) {
  auto& placement = g_trayContextMenuPlacementTai;
  placement.pending = false;
  if (g_unloading ||
      (taskbarLocation != kTaskbarLocationTopTai &&
       taskbarLocation != kTaskbarLocationBottomTai) ||
      Wh_GetIntSetting(L"MoveTrayContextMenus") == 0) {
    return;
  }
  ContextMenuTaskbarTai taskbar;
  if (!TryGetContextMenuTaskbarTai(grid.XamlRoot(), &taskbar)) {
    return;
  }
  if (taskbar.state.lastRightMostEdgeTray <= 0) {
    Wh_Log(L"[TrayContextMenu] No tray recorded for monitor %s",
           taskbar.monitorName.c_str());
    return;
  }
  // The point is relative to the grid. Move it only when it is the root's
  // corner described above; a point Windows derives some other way is kept.
  const auto gridOrigin =
      grid.TransformToVisual(nullptr).TransformPoint({0.0f, 0.0f});
  const float windowsAnchorX = gridOrigin.X + position.X;
  const float cornerX = static_cast<float>(rootWidth - margin);
  if (std::abs(windowsAnchorX - cornerX) > 1.0f) {
    Wh_Log(L"[TrayContextMenu] %s: anchor %.2f is not the corner %.2f",
           taskbar.monitorName.c_str(), windowsAnchorX, cornerX);
    return;
  }
  // As far as PlaceNotificationCenterViewRectTai moves the Notification Center
  // with AlignFlyoutInner on, and never past Windows' own corner.
  const float shift =
      std::min(0.0f, taskbar.state.lastRightMostEdgeTray +
                         taskbar.innerPaddingDip - static_cast<float>(rootWidth));
  position.X += shift;
  placement.pending = true;
  placement.grid = winrt::get_abi(grid);
  placement.tick = GetTickCount64();
  placement.rootWidth = static_cast<float>(rootWidth);
  placement.margin = static_cast<float>(margin);
  placement.anchorX = cornerX + shift;
  placement.anchorY = gridOrigin.Y + position.Y;
  placement.above = taskbarLocation == kTaskbarLocationBottomTai;
  placement.trayRight = taskbar.state.lastRightMostEdgeTray;
  wcsncpy_s(placement.monitorName, taskbar.monitorName.c_str(), _TRUNCATE);
  Wh_Log(L"[TrayContextMenu] %s: anchor %.2f -> %.2f",
         taskbar.monitorName.c_str(), cornerX, placement.anchorX);
  LogContextMenuPlacementToFileTai(
      L"TrayContextMenu", L"anchor", placement.monitorName, placement.trayRight,
      placement.rootWidth, placement.margin, 0.0f, 0.0f, cornerX,
      placement.anchorY, placement.anchorX, placement.anchorY);
}

// XAML put a corner of the menu on the point, so wherever it landed is within
// one menu size of the target. An offset further out is not the menu's
// position; it is logged and left alone.
static bool ShouldSettleContextMenuOffsetTai(float placed,
                                             float target,
                                             float menuSize) {
  const float error = std::abs(placed - target);
  return error > 0.5f && error <= menuSize + 1.0f;
}

static void SettleTrayContextMenuTai(Controls::Primitives::Popup const& popup,
                                     Controls::Grid const& grid) {
  auto& placement = g_trayContextMenuPlacementTai;
  if (!placement.pending) {
    return;
  }
  placement.pending = false;
  constexpr ULONGLONG kPlacementTtlMs = 10000;
  if (g_unloading || placement.grid != winrt::get_abi(grid) ||
      GetTickCount64() - placement.tick > kPlacementTtlMs) {
    return;
  }
  auto menu = popup.Child().try_as<FrameworkElement>();
  if (!menu) {
    return;
  }
  const float menuWidth = static_cast<float>(menu.ActualWidth());
  const float menuHeight = static_cast<float>(menu.ActualHeight());
  if (!(menuWidth > 0.0f) || !(menuHeight > 0.0f)) {
    return;
  }
  // The menu's right edge on the point, kept in the root with the margin
  // Windows' AlignContextMenuPopup keeps; its bottom on the point, or its top
  // under a top taskbar.
  const float left = std::max(
      placement.margin,
      std::min(placement.anchorX - menuWidth,
               placement.rootWidth - placement.margin - menuWidth));
  const float top =
      placement.above ? placement.anchorY - menuHeight : placement.anchorY;
  const float placedLeft = static_cast<float>(popup.HorizontalOffset());
  const float placedTop = static_cast<float>(popup.VerticalOffset());
  const bool moveX = ShouldSettleContextMenuOffsetTai(placedLeft, left, menuWidth);
  const bool moveY = ShouldSettleContextMenuOffsetTai(placedTop, top, menuHeight);
  if (moveX) {
    popup.HorizontalOffset(left);
  }
  if (moveY) {
    popup.VerticalOffset(top);
  }
  Wh_Log(L"[TrayContextMenu] %s: opened at (%.2f,%.2f), %.2fx%.2f, now (%.2f,%.2f)",
         placement.monitorName, placedLeft, placedTop, menuWidth, menuHeight,
         moveX ? left : placedLeft, moveY ? top : placedTop);
  LogContextMenuPlacementToFileTai(
      L"TrayContextMenu", (moveX || moveY) ? L"moved" : L"kept",
      placement.monitorName, placement.trayRight, placement.rootWidth,
      placement.margin, menuWidth, menuHeight, placedLeft, placedTop,
      moveX ? left : placedLeft, moveY ? top : placedTop);
}

// The Point is returned through a hidden pointer, as MSVC does for a class with
// constructors, so it is spelled out as the first argument.
using ContextMenuPositionHelper_GetShowPosition_t =
    winrt::Windows::Foundation::Point*(WINAPI*)(
        winrt::Windows::Foundation::Point* result,
        int taskbarLocation,
        double margin,
        void* grid,  // Grid const&
        double rootWidth,
        double rootHeight);
ContextMenuPositionHelper_GetShowPosition_t
    ContextMenuPositionHelper_GetShowPosition_Original;
winrt::Windows::Foundation::Point* WINAPI
ContextMenuPositionHelper_GetShowPosition_Hook(
    winrt::Windows::Foundation::Point* result,
    int taskbarLocation,
    double margin,
    void* grid,
    double rootWidth,
    double rootHeight) {
  g_hookCallCounter++;
  auto* position = ContextMenuPositionHelper_GetShowPosition_Original(
      result, taskbarLocation, margin, grid, rootWidth, rootHeight);
  if (position && grid) {
    try {
      MoveTrayContextMenuAnchorTai(
          *position, taskbarLocation, margin,
          *static_cast<Controls::Grid const*>(grid), rootWidth);
    } catch (...) {
      // Keep Windows' point: an exception must not unwind into Explorer.
      g_trayContextMenuPlacementTai.pending = false;
    }
  }
  g_hookCallCounter--;
  return position;
}

// The last argument is not used by current builds.
using ContextMenuPositionHelper_PositionContextMenuPopup_t =
    void(WINAPI*)(void* popup,  // Popup const&
                  void* grid,   // Grid const&
                  double margin,
                  int taskbarLocation,
                  double extra);
ContextMenuPositionHelper_PositionContextMenuPopup_t
    ContextMenuPositionHelper_PositionContextMenuPopup_Original;
void WINAPI ContextMenuPositionHelper_PositionContextMenuPopup_Hook(
    void* popup,
    void* grid,
    double margin,
    int taskbarLocation,
    double extra) {
  g_hookCallCounter++;
  ContextMenuPositionHelper_PositionContextMenuPopup_Original(
      popup, grid, margin, taskbarLocation, extra);
  if (popup && grid) {
    try {
      SettleTrayContextMenuTai(
          *static_cast<Controls::Primitives::Popup const*>(popup),
          *static_cast<Controls::Grid const*>(grid));
    } catch (...) {
      // Keep the menu where it is: an exception must not unwind into Explorer.
    }
  }
  g_hookCallCounter--;
}

// Called from the taskbar-icon-size dependency's HookSystemTraySymbols, which
// runs whenever SystemTray.dll is or becomes loaded.
bool HookTrayContextMenuPositionTai(HMODULE systemTrayModule) {
  WindhawkUtils::SYMBOL_HOOK systemTrayHooks[] = {
      {{LR"(struct winrt::Windows::Foundation::Point __cdecl ContextMenuPositionHelper::GetShowPosition(enum winrt::WindowsUdk::UI::Shell::TaskbarLocation,double,struct winrt::Windows::UI::Xaml::Controls::Grid const &,double,double))"},
       &ContextMenuPositionHelper_GetShowPosition_Original,
       ContextMenuPositionHelper_GetShowPosition_Hook},
      {{LR"(void __cdecl ContextMenuPositionHelper::PositionContextMenuPopup(struct winrt::Windows::UI::Xaml::Controls::Primitives::Popup const &,struct winrt::Windows::UI::Xaml::Controls::Grid const &,double,enum winrt::WindowsUdk::UI::Shell::TaskbarLocation,double))"},
       &ContextMenuPositionHelper_PositionContextMenuPopup_Original,
       ContextMenuPositionHelper_PositionContextMenuPopup_Hook},
  };
  const bool hooked = WindhawkUtils::HookSymbols(
      systemTrayModule, systemTrayHooks, ARRAYSIZE(systemTrayHooks));
  Wh_Log(L"[TrayContextMenu] ContextMenuPositionHelper hooks %s",
         hooked ? L"set" : L"unavailable");
  return hooked;
}

static void MoveStartButtonContextMenuAnchorTai(
    winrt::Windows::Foundation::Point& position,
    int taskbarLocation,
    FrameworkElement const& taskbarFrame,
    Thickness const& margin) {
  if (g_unloading || !g_settings_startbuttonposition.startMenuOnTheLeft ||
      (taskbarLocation != kTaskbarLocationTopTai &&
       taskbarLocation != kTaskbarLocationBottomTai)) {
    return;
  }
  // The point is relative to the frame. Move it only when it is the frame's
  // leading corner described above; a point Windows derives some other way is
  // kept.
  if (taskbarFrame.FlowDirection() != FlowDirection::LeftToRight ||
      std::abs(position.X - static_cast<float>(margin.Left)) > 0.5f) {
    return;
  }
  auto xamlRoot = taskbarFrame.XamlRoot();
  ContextMenuTaskbarTai taskbar;
  if (!TryGetContextMenuTaskbarTai(xamlRoot, &taskbar)) {
    return;
  }
  if (!(taskbar.state.lastStartButtonXCalculated > 0.0f)) {
    Wh_Log(L"[StartButtonContextMenu] No start button recorded for monitor %s",
           taskbar.monitorName.c_str());
    return;
  }
  // As far as the Start and Search menus move with AlignFlyoutInner on (see
  // startbuttonposition_start_menu_position_code.cpp), and never left of
  // Windows' own point.
  const auto frameOrigin =
      taskbarFrame.TransformToVisual(nullptr).TransformPoint({0.0f, 0.0f});
  const float shift =
      std::max(0.0f, taskbar.state.lastStartButtonXCalculated -
                         taskbar.innerPaddingDip - frameOrigin.X);
  const float windowsX = frameOrigin.X + position.X;
  const float windowsY = frameOrigin.Y + position.Y;
  position.X += shift;
  Wh_Log(L"[StartButtonContextMenu] %s: anchor %.2f -> %.2f",
         taskbar.monitorName.c_str(), windowsX, windowsX + shift);
  LogContextMenuPlacementToFileTai(
      L"StartButtonContextMenu", L"anchor", taskbar.monitorName.c_str(),
      static_cast<int>(taskbar.state.lastStartButtonXCalculated + 0.5f),
      static_cast<float>(xamlRoot.Size().Width),
      static_cast<float>(margin.Left), 0.0f, 0.0f, windowsX, windowsY,
      windowsX + shift, windowsY);
}

// The Point is returned through a hidden pointer, like GetShowPosition's. The
// TaskbarFrame is passed by value, as a pointer to the caller's copy, which the
// function releases.
using GetLeadingStartFlyoutPosition_t =
    winrt::Windows::Foundation::Point*(WINAPI*)(
        winrt::Windows::Foundation::Point* result,
        int taskbarLocation,
        void* taskbarFrame,
        Thickness const* margin);
GetLeadingStartFlyoutPosition_t GetLeadingStartFlyoutPosition_Original;
winrt::Windows::Foundation::Point* WINAPI GetLeadingStartFlyoutPosition_Hook(
    winrt::Windows::Foundation::Point* result,
    int taskbarLocation,
    void* taskbarFrame,
    Thickness const* margin) {
  g_hookCallCounter++;
  // Take a reference before the call releases the caller's.
  FrameworkElement frame{nullptr};
  try {
    if (void* frameAbi = taskbarFrame ? *static_cast<void**>(taskbarFrame)
                                      : nullptr) {
      winrt::Windows::Foundation::IUnknown frameUnknown;
      winrt::copy_from_abi(frameUnknown, frameAbi);
      frame = frameUnknown.try_as<FrameworkElement>();
    }
  } catch (...) {
    frame = nullptr;
  }
  auto* position = GetLeadingStartFlyoutPosition_Original(
      result, taskbarLocation, taskbarFrame, margin);
  if (position && frame && margin) {
    try {
      MoveStartButtonContextMenuAnchorTai(*position, taskbarLocation, frame,
                                          *margin);
    } catch (...) {
      // Keep Windows' point: an exception must not unwind into Explorer.
    }
  }
  g_hookCallCounter--;
  return position;
}

// Called from the start button position dependency's
// HookTaskbarViewDllSymbolsStartButtonPosition, which runs whenever
// Taskbar.View.dll is or becomes loaded.
bool HookStartButtonContextMenuPositionTai(HMODULE taskbarViewModule) {
  WindhawkUtils::SYMBOL_HOOK taskbarViewHooks[] = {
      {{LR"(struct winrt::Windows::Foundation::Point __cdecl GetLeadingStartFlyoutPosition(enum winrt::WindowsUdk::UI::Shell::TaskbarLocation,struct winrt::Taskbar::TaskbarFrame,struct winrt::Windows::UI::Xaml::Thickness const &))"},
       &GetLeadingStartFlyoutPosition_Original,
       GetLeadingStartFlyoutPosition_Hook},
  };
  const bool hooked = WindhawkUtils::HookSymbols(
      taskbarViewModule, taskbarViewHooks, ARRAYSIZE(taskbarViewHooks));
  Wh_Log(L"[StartButtonContextMenu] GetLeadingStartFlyoutPosition hook %s",
         hooked ? L"set" : L"unavailable");
  return hooked;
}

bool ApplyStyle(FrameworkElement const& xamlRootContent, std::wstring monitorName) {
  if (!xamlRootContent) {
    Wh_Log(L"xamlRootContent is null");
    return false;
  }
  try {
  std::lock_guard<std::recursive_mutex> settingsLock(g_settingsMutex);
  auto stateHandle = GetOrCreateTaskbarState(
      monitorName,
      reinterpret_cast<uintptr_t>(GetComIdentityTai(xamlRootContent)));
  if (!stateHandle) {
    Wh_Log(L"Failed to get taskbar state for monitor: %s", monitorName.c_str());
    return false;
  }
  std::lock_guard<std::recursive_mutex> stateLock(stateHandle->mutex);
  auto& state = *stateHandle;
  Wh_Log(L"ApplyStyle for monitor: %s", monitorName.c_str());
  // Fork addition: see g_xamlRootMonitorNamesTai.
  RecordXamlRootMonitorTai(xamlRootContent, monitorName);
  g_scheduled_low_priority_update = false;
  bool forceStyleApply = false;
  int forceStyleApplyPasses = g_force_style_apply_passes.load();
  while (forceStyleApplyPasses > 0) {
    if (g_force_style_apply_passes.compare_exchange_weak(forceStyleApplyPasses, forceStyleApplyPasses - 1)) {
      forceStyleApply = true;
      break;
    }
  }
  bool resetAnimationTargetsThisPass = false;
  int resetAnimationTargetPasses = g_reset_animation_target_passes.load();
  while (resetAnimationTargetPasses > 0) {
    if (g_reset_animation_target_passes.compare_exchange_weak(resetAnimationTargetPasses, resetAnimationTargetPasses - 1)) {
      resetAnimationTargetsThisPass = true;
      forceStyleApply = true;
      break;
    }
  }
  if (resetAnimationTargetsThisPass) {
    ResetAnimationTargetCache(state);
  }
  const float rasterizationScale = GetRasterizationScale(xamlRootContent);
  auto snapPx = [rasterizationScale](float value) -> float {
    return SnapToPhysicalPixel(value, rasterizationScale);
  };
  auto taskFrame = FindChildByClassName(xamlRootContent, L"Taskbar.TaskbarFrame");
  if (!taskFrame) {
    Wh_Log(L"Failed to find Taskbar.TaskbarFrame");
    return false;
  }
   const bool displayGeometryChanged = CheckAndUpdateDisplayGeometrySignature(
      state, xamlRootContent, taskFrame, rasterizationScale);
  if (displayGeometryChanged && !g_unloading) {
    Wh_Log(L"Taskbar display geometry changed; invalidating cached layout targets");
    ResetAnimationTargetCache(state);
    forceStyleApply = true;
    RequestTaskbarDimensionInvalidation();
    if (HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd()) {
      ArmStyleFollowupPasses(hTaskbarWnd, true);
    }
  }
  auto now = std::chrono::steady_clock::now();
  if (!forceStyleApply && !g_unloading && now - state.lastApplyStyleTime < std::chrono::milliseconds(200)) {
    return true;
  }
  state.lastApplyStyleTime = now;
  auto rootGridTaskBar = FindChildByName(taskFrame, L"RootGrid");
  if (!rootGridTaskBar) {
    Wh_Log(L"Failed to find RootGrid in taskFrame");
    return false;
  }
  auto taskbarFrameRepeater = FindChildByName(rootGridTaskBar, L"TaskbarFrameRepeater");
  if (!taskbarFrameRepeater) {
    Wh_Log(L"Failed to find TaskbarFrameRepeater in rootGridTaskBar");
    return false;
  }
  auto trayFrame = FindChildByClassName(xamlRootContent, L"SystemTray.SystemTrayFrame");
  if (!trayFrame) {
    Wh_Log(L"Failed to find SystemTray.SystemTrayFrame");
    return false;
  }
  auto systemTrayFrameGrid = FindChildByName(trayFrame, L"SystemTrayFrameGrid");
  if (!systemTrayFrameGrid) {
    Wh_Log(L"Failed to find SystemTrayFrameGrid in trayFrame");
    return false;
  }
  auto showDesktopButton = FindChildByName(systemTrayFrameGrid, L"ShowDesktopStack");
  if (!showDesktopButton) {
    Wh_Log(L"Failed to find ShowDesktopStack in systemTrayFrameGrid");
    return false;
  }
  auto taskbarBackground = FindChildByClassName(rootGridTaskBar, L"Taskbar.TaskbarBackground");
  if (!taskbarBackground) {
    Wh_Log(L"Failed to find Taskbar.TaskbarBackground in rootGridTaskBar");
    return false;
  }
  auto backgroundFillParent = FindChildByClassName(taskbarBackground, L"Windows.UI.Xaml.Controls.Grid");
  if (!backgroundFillParent) {
    Wh_Log(L"Failed to find backgroundFillParent in taskbarBackground");
    return false;
  }
  auto backgroundFillChild = FindChildByName(backgroundFillParent, L"BackgroundFill");
  if (!backgroundFillChild) {
    Wh_Log(L"Failed to find BackgroundFill in backgroundFillParent");
    return false;
  }
  auto notificationAreaIcons = FindChildByName(systemTrayFrameGrid, L"NotificationAreaIcons");
  if (!notificationAreaIcons) {
    Wh_Log(L"Failed to find NotificationAreaIcons in systemTrayFrameGrid");
    return false;
  }
  auto itemsPresenter = FindChildByClassName(notificationAreaIcons, L"Windows.UI.Xaml.Controls.ItemsPresenter");
  if (!itemsPresenter) {
    Wh_Log(L"Failed to find ItemsPresenter in notificationAreaIcons");
    return false;
  }
  auto stackPanel = FindChildByClassName(itemsPresenter, L"Windows.UI.Xaml.Controls.StackPanel");
  if (!stackPanel) {
    Wh_Log(L"Failed to find StackPanel in itemsPresenter");
    return false;
  }
  auto taskbarChildrenMeasurement =
      CaptureChildrenLayoutTai(taskbarFrameRepeater);
  bool widgetPresent = IsTaskbarWidgetsEnabled();
  auto widgetElement = widgetPresent
      ? FindCapturedChildByClassNameTai(
            taskbarChildrenMeasurement,
            L"Taskbar.AugmentedEntryPointButton")
      : nullptr;
  auto widgetMainView = widgetElement ? FindChildByName(widgetElement, L"ExperienceToggleButtonRootPanel") : widgetElement;
  widgetPresent = widgetPresent && widgetMainView != nullptr;
  auto widgetElementWidth = widgetPresent && widgetMainView ? widgetMainView.ActualWidth() : 0;
  if (widgetPresent && widgetElementWidth <= 0) {
    Wh_Log(L"Error: widgetPresent && widgetElementWidth<=0");
    return false;
  }
  auto widgetElementInnerChild = widgetPresent ? FindChildByClassName(widgetElement, L"Taskbar.TaskListButtonPanel") : nullptr;
  auto widgetElementVisibleWidth = widgetElementInnerChild ? widgetElementInnerChild.ActualWidth() : 0;
  auto widgetElementVisibleHeight = widgetElementInnerChild ? widgetElementInnerChild.ActualHeight() : 0;
  if (widgetElementInnerChild && widgetElementVisibleWidth <= 0) {
    Wh_Log(L"Error: widgetElementInnerChild && widgetElementVisibleWidth<=0");
    return false;
  }
  if (widgetElementInnerChild && widgetElementVisibleHeight <= 0) {
    Wh_Log(L"Error: widgetElementInnerChild && widgetElementVisibleHeight<=0");
    return false;
  }
  auto overflowButton = FindCapturedChildByClassNameTai(
      taskbarChildrenMeasurement,
      L"Taskbar.OverflowToggleButton");
  bool isOverflowing = overflowButton != nullptr && !IsWeirdFrameworkElement(overflowButton);
  double rootWidth = xamlRootContent.ActualWidth();
  state.lastRootWidth = static_cast<float>(rootWidth);
  const double minimumRootWidth =
      std::max<double>(1.0, static_cast<double>(g_settings.userDefinedTaskbarButtonSize));
  if (!g_unloading && rootWidth < minimumRootWidth) {
    Wh_Log(L"root width is too small");
    return false;
  }
   const double taskbarVirtualSurfaceWidth = g_unloading
      ? rootWidth
      : CalculateDynamicTaskbarVirtualSurfaceWidth(
            taskbarChildrenMeasurement.visualChildCount,
            taskbarChildrenMeasurement.capturedContentWidth,
            rootWidth,
            rasterizationScale);
  const bool useVirtualTaskbarSurface =
      !g_unloading &&
      std::isfinite(taskbarVirtualSurfaceWidth) &&
      taskbarVirtualSurfaceWidth > rootWidth + kLayoutToleranceDip;
  if (useVirtualTaskbarSurface) {
    ApplyVirtualTaskbarLayoutSurface(xamlRootContent,
                                     taskFrame,
                                     rootGridTaskBar,
                                     taskbarFrameRepeater,
                                     taskbarBackground,
                                     backgroundFillParent,
                                     backgroundFillChild,
                                     taskbarVirtualSurfaceWidth);
    SetTaskbarOverflowButtonSuppressed(overflowButton, true, &state);
    isOverflowing = false;
  } else {
    SetTaskbarOverflowButtonSuppressed(overflowButton, false, &state);
  }
  const double taskbarLayoutSurfaceWidth = useVirtualTaskbarSurface
      ? std::max(rootWidth, taskbarFrameRepeater.ActualWidth())
      : rootWidth;
  int childrenCountTaskbar = 0;
  double taskbarLeftEdge = 0.0;
  double taskbarRightEdge = 0.0;
  double startButtonLeft = 0.0;
  double startButtonTop = 0.0;
  double startButtonWidth = 0.0;
  double startButtonHeight = 0.0;
  const uint64_t childStyleGeneration =
      g_taskbarChildStyleGeneration.load(std::memory_order_acquire);
  RefreshChildrenLayoutMeasurementTai(
      taskbarFrameRepeater,
      rootGridTaskBar,
      &taskbarChildrenMeasurement);
  ApplyChildStylesIfRequired(
      taskbarFrameRepeater,
      rootGridTaskBar,
      &taskbarChildrenMeasurement,
      &state.taskbarChildStyleCache,
      childStyleGeneration);
  const double childrenWidthTaskbarDbl =
      taskbarChildrenMeasurement.totalWidth;
  childrenCountTaskbar = taskbarChildrenMeasurement.validChildrenCount;
  taskbarLeftEdge = taskbarChildrenMeasurement.leftMostEdge;
  taskbarRightEdge = taskbarChildrenMeasurement.rightMostEdge;
  winrt::Windows::Foundation::Rect startButtonAnchorRect{};
  auto startButtonObservation =
      FindCapturedStartButtonTai(taskbarChildrenMeasurement);
  auto startButtonElement = startButtonObservation
      ? startButtonObservation->element
      : nullptr;
  if (startButtonObservation && startButtonObservation->hasValidRootRect) {
    startButtonAnchorRect = startButtonObservation->rootRect;
    startButtonLeft = startButtonAnchorRect.X;
    startButtonTop = startButtonAnchorRect.Y;
    startButtonWidth = startButtonAnchorRect.Width;
    startButtonHeight = startButtonAnchorRect.Height;
    state.lastStartButtonXActual = static_cast<float>(startButtonAnchorRect.X);
  }
  if (!g_unloading && childrenWidthTaskbarDbl <= 0) {
    Wh_Log(L"Error: childrenWidthTaskbarDbl <= 0");
    return false;
  }
  double actualTaskbarBoundsWidthDbl = taskbarRightEdge - taskbarLeftEdge;
  if (actualTaskbarBoundsWidthDbl <= 0.0) {
    actualTaskbarBoundsWidthDbl = childrenWidthTaskbarDbl;
    taskbarLeftEdge = (rootWidth - actualTaskbarBoundsWidthDbl) / 2.0;
    taskbarRightEdge = taskbarLeftEdge + actualTaskbarBoundsWidthDbl;
  }
  // Use the sum of valid child widths as the logical task width. The measured
  // min/max bounds are still useful as an anchor near overflow, but during
  // insert/remove animations they can include temporary gaps from recycled
  // views and make the island width overshoot.
  unsigned int childrenWidthTaskbar = static_cast<unsigned int>(childrenWidthTaskbarDbl + 0.5);
  signed int rightMostEdgeTaskbar = static_cast<signed int>(taskbarRightEdge + 0.5);
  if (!g_unloading && childrenCountTaskbar < 1) {
    Wh_Log(L"Error: childrenCountTaskbar < 1");
    return false;
  }
  const unsigned int minimumTaskbarChildrenWidth =
      static_cast<unsigned int>(std::max<double>(1.0, g_settings.userDefinedTaskbarButtonSize * 0.25));
  if (!g_unloading && childrenWidthTaskbar <= minimumTaskbarChildrenWidth) {
    Wh_Log(L"Error: childrenWidthTaskbar is too small");
    return false;
  }
  if (!g_unloading && rightMostEdgeTaskbar < 0) {
    Wh_Log(L"Error: rightMostEdgeTaskbar < 0");
    return false;
  }
  bool rightMostEdgeChangedTaskbar = (state.lastTaskbarData.rightMostEdge != rightMostEdgeTaskbar);
  if (isOverflowing != state.wasOverflowing) {
    ResetAnimationTargetCache(state);
    forceStyleApply = true;
  }
  if (rightMostEdgeChangedTaskbar || state.lastTaskbarData.rightMostEdge == 0.0 || isOverflowing) {
    state.lastTaskbarData.childrenCount = childrenCountTaskbar;
    state.lastTaskbarData.rightMostEdge = rightMostEdgeTaskbar;
    state.lastTaskbarData.childrenWidth = childrenWidthTaskbar;
  }
  trayFrame.Clip(nullptr);
    DisableElementClip(systemTrayFrameGrid);
  DisableClipForAncestorChain(systemTrayFrameGrid);
  auto trayHorizontalAlignmentRef = trayFrame
      .GetValue(FrameworkElement::HorizontalAlignmentProperty())
      .try_as<winrt::Windows::Foundation::IReference<HorizontalAlignment>>();
  if (trayHorizontalAlignmentRef && trayHorizontalAlignmentRef.Value() == HorizontalAlignment::Center) {
    trayFrame.SetValue(FrameworkElement::HorizontalAlignmentProperty(), winrt::box_value(HorizontalAlignment::Right));
  }
  // Fork addition. TaskbarFrame and SystemTrayFrame are siblings in the same
  // XAML root grid, and TaskbarOffsetY makes that root 2*TaskbarOffsetY taller
  // than the island, which is drawn inset by TaskbarOffsetY within it. taskFrame
  // is given the full root height below, but trayFrame keeps the bare island
  // height, so where the tray lands is decided entirely by its vertical
  // alignment. Nothing ever set one: this relied on XAML implicitly centering an
  // explicitly sized child, and Windows 11 25H2 stopped resolving it that way,
  // leaving the clock, battery, volume and language indicator off by exactly
  // TaskbarOffsetY (upstream issues #26 and #30). Pin it here rather than beside
  // the height writes below, because those only run when dimensions are
  // invalidated while SystemTrayFrame_Height_Hook rewrites the tray frame on
  // every SystemTrayController::UpdateFrameSize; asserting it on every pass is
  // what stops the misalignment from returning after an Explorer restart.
  if (g_unloading) {
    trayFrame.ClearValue(FrameworkElement::VerticalAlignmentProperty());
  } else {
    auto trayVerticalAlignmentRef = trayFrame
        .GetValue(FrameworkElement::VerticalAlignmentProperty())
        .try_as<winrt::Windows::Foundation::IReference<VerticalAlignment>>();
    if (!trayVerticalAlignmentRef ||
        trayVerticalAlignmentRef.Value() != VerticalAlignment::Center) {
      trayFrame.SetValue(FrameworkElement::VerticalAlignmentProperty(), winrt::box_value(VerticalAlignment::Center));
    }
  }
  int childrenCountTray = 0;
  auto trayChildrenMeasurement =
      MeasureValidChildren(systemTrayFrameGrid);
  ApplyChildStylesIfRequired(
      systemTrayFrameGrid,
      nullptr,
      &trayChildrenMeasurement,
      &state.trayChildStyleCache,
      childStyleGeneration);
  double trayFrameWidthDbl = trayChildrenMeasurement.totalWidth;
  childrenCountTray = trayChildrenMeasurement.validChildrenCount;
  if (!g_unloading && trayFrameWidthDbl <= 0) {
    Wh_Log(L"Error: trayFrameWidthDbl <= 0");
    return false;
  }
  if (!g_unloading && childrenCountTray <= 0) {
    Wh_Log(L"Error: childrenCountTray <= 0");
    return false;
  }
  const double showDesktopButtonActualWidth = showDesktopButton ? showDesktopButton.ActualWidth() : 0.0;
  if (g_settings.userDefinedIgnoreShowDesktopButton && showDesktopButtonActualWidth > 0.0) {
    trayFrameWidthDbl = std::max(0.0, trayFrameWidthDbl - showDesktopButtonActualWidth);
  }
  int trayGapPlusExtras = g_settings.userDefinedTrayTaskGap + widgetElementVisibleWidth + (widgetPresent ? -6 + g_settings.userDefinedTrayTaskGap : 0);
  const double trayFrameWidthSafeDbl = std::max(0.0, trayFrameWidthDbl + static_cast<double>(trayGapPlusExtras));
  const unsigned int trayFrameWidth = static_cast<unsigned int>(trayFrameWidthSafeDbl + 0.5);
  if (!g_unloading && childrenCountTray == 0) {
    Wh_Log(L"Error: childrenCountTray == 0");
    return false;
  }
  if (!g_unloading && trayFrameWidth <= 1) {
    Wh_Log(L"Error: trayFrameWidth <= 1");
    return false;
  }
  const float targetContentWidth = static_cast<float>(childrenWidthTaskbar + trayFrameWidth);
  const float targetContentLeft = snapPx(static_cast<float>((rootWidth - targetContentWidth) / 2.0f));
  const float targetTrayLogicalLeft = snapPx(targetContentLeft + static_cast<float>(childrenWidthTaskbar));
  float targetOffsetXTray = snapPx(targetTrayLogicalLeft - static_cast<float>(rootWidth - trayFrameWidth));
  // tray animations
  auto systemTrayFrameGridVisual = winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(systemTrayFrameGrid);
  if (!systemTrayFrameGridVisual) {
    Wh_Log(L"Error: !SystemTrayFrameGridVisual");
    return false;
  }
  auto originalOffset = systemTrayFrameGridVisual.Offset();
  if (state.initOffsetX == -1) {
    state.initOffsetX = originalOffset.x;
  }
  auto rootGridTaskBarVisual = winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(rootGridTaskBar);
  if (!rootGridTaskBarVisual) {
    Wh_Log(L"Error: !rootGridTaskBarVisual");
    return false;
  }
  auto taskbarFrameRepeaterVisual = winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(taskbarFrameRepeater);
  if (!taskbarFrameRepeaterVisual) {
    Wh_Log(L"Error: !taskbarFrameRepeaterVisual");
    return false;
  }
  const float visualOffsetTolerance =
      std::max<float>(static_cast<float>(kLayoutToleranceDip),
                      2.0f / GetRasterizationScale(xamlRootContent));
  // Move the stable RootGrid instead of TaskbarFrameRepeater. The repeater is
  // virtualized and can expose transient/recycled child offsets during insert
  // and remove animations, which makes compositor Offset animations overshoot.
  // RootGrid remains the authoritative visual transform; the repeater itself is
  // kept horizontally neutral below.
  //
  // The Start button rect is sampled in RootGrid coordinates, but it is not used
  // as a live target. During task insertion/removal, Explorer can animate the
  // repeater's internal layout; chasing that transient Start rect causes a
  // feedback loop where RootGrid keeps catching up to Windows' own animation.
  // Only promote the sampled rect to an anchor after it has been stable for a
  // couple of ApplyStyle passes. Until then, keep using the last stable anchor
  // or the edge-clamp fallback from the root-grid version.
  const bool hasStartButtonAnchorSample =
      startButtonWidth > 0.0 &&
      startButtonHeight > 0.0 &&
      std::isfinite(startButtonLeft) &&
      std::isfinite(startButtonTop);
  const bool startButtonSampleChanged =
      hasStartButtonAnchorSample &&
      (!state.hasLastStartButtonAnchorRect ||
       std::abs(state.lastStartButtonAnchorLeft - static_cast<float>(startButtonLeft)) > visualOffsetTolerance ||
       std::abs(state.lastStartButtonAnchorTop - static_cast<float>(startButtonTop)) > visualOffsetTolerance ||
       std::abs(state.lastStartButtonAnchorWidth - static_cast<float>(startButtonWidth)) > visualOffsetTolerance ||
       std::abs(state.lastStartButtonAnchorHeight - static_cast<float>(startButtonHeight)) > visualOffsetTolerance);
  if (hasStartButtonAnchorSample) {
    if (startButtonSampleChanged) {
      state.startButtonAnchorStablePasses = 0;
    } else if (state.startButtonAnchorStablePasses < kStartButtonAnchorStablePassesRequired) {
      state.startButtonAnchorStablePasses++;
    }
    state.lastStartButtonAnchorLeft = static_cast<float>(startButtonLeft);
    state.lastStartButtonAnchorTop = static_cast<float>(startButtonTop);
    state.lastStartButtonAnchorWidth = static_cast<float>(startButtonWidth);
    state.lastStartButtonAnchorHeight = static_cast<float>(startButtonHeight);
    state.hasLastStartButtonAnchorRect = true;
    if (state.startButtonAnchorStablePasses >= kStartButtonAnchorStablePassesRequired) {
      state.stableStartButtonAnchorLeft = static_cast<float>(startButtonLeft);
      state.stableStartButtonAnchorTop = static_cast<float>(startButtonTop);
      state.stableStartButtonAnchorWidth = static_cast<float>(startButtonWidth);
      state.stableStartButtonAnchorHeight = static_cast<float>(startButtonHeight);
      state.hasStableStartButtonAnchorRect = true;
    }
  } else {
    state.hasLastStartButtonAnchorRect = false;
    state.hasStableStartButtonAnchorRect = false;
    state.startButtonAnchorStablePasses = 0;
  }
  const double predictedCenteredTaskbarLeft =
      (taskbarLayoutSurfaceWidth - static_cast<double>(childrenWidthTaskbar)) / 2.0;
  const double taskbarEdgeClampTolerance =
      std::max<double>(visualOffsetTolerance,
                       static_cast<double>(g_settings.userDefinedTaskbarButtonSize) * 0.25);
  const bool taskbarLayoutIsEdgeClamped =
      isOverflowing ||
      taskbarLeftEdge <= taskbarEdgeClampTolerance ||
      taskbarRightEdge >= taskbarLayoutSurfaceWidth - taskbarEdgeClampTolerance ||
      predictedCenteredTaskbarLeft <= taskbarEdgeClampTolerance;
  // Windows can shift the taskbar item group left before the official overflow
  // button appears, especially when the centered item group would get too close
  // to the tray/suffixed area. Do not turn that into a second live anchor state
  // machine: just broaden the original stable Start-button anchor path for this
  // specific tray-collision condition.
  const double trayFrameLeft = rootWidth - static_cast<double>(trayFrameWidth);
  const double predictedTaskbarRightOnScreen =
      targetContentLeft + static_cast<double>(childrenWidthTaskbar);
  const double predictedGapToTray =
      trayFrameLeft - predictedTaskbarRightOnScreen;
  const double minimumTaskbarToTrayGap =
      std::max<double>(static_cast<double>(visualOffsetTolerance) * 2.0,
                       static_cast<double>(g_settings.userDefinedTaskbarButtonSize) * 0.75);
  const bool taskbarLayoutIsTrayConstrained =
      !useVirtualTaskbarSurface &&
      trayFrameWidth > 0 &&
      taskbarRightEdge > taskbarLeftEdge &&
      predictedGapToTray < minimumTaskbarToTrayGap &&
      std::abs(taskbarLeftEdge - predictedCenteredTaskbarLeft) >
          visualOffsetTolerance;
  const bool stableStartAnchorMatchesCurrentSample =
      state.hasStableStartButtonAnchorRect &&
      hasStartButtonAnchorSample &&
      std::abs(state.stableStartButtonAnchorLeft -
               static_cast<float>(startButtonLeft)) <= visualOffsetTolerance &&
      std::abs(state.stableStartButtonAnchorWidth -
               static_cast<float>(startButtonWidth)) <= visualOffsetTolerance;
  const double fallbackTaskbarAnchorLeft =
      taskbarLayoutIsEdgeClamped ? taskbarLeftEdge : predictedCenteredTaskbarLeft;
  const bool useStableStartButtonAnchor =
      state.hasStableStartButtonAnchorRect &&
      (taskbarLayoutIsEdgeClamped ||
       isOverflowing ||
       (taskbarLayoutIsTrayConstrained && stableStartAnchorMatchesCurrentSample));
  const double taskbarAnchorLeft = useStableStartButtonAnchor
      ? static_cast<double>(state.stableStartButtonAnchorLeft)
      : fallbackTaskbarAnchorLeft;
  float targetTaskRootOffsetX = snapPx(targetContentLeft - static_cast<float>(taskbarAnchorLeft));
  float centeredTaskbarRightEdge = targetTrayLogicalLeft;
  const float taskbarBackgroundPadding =
      static_cast<float>(g_settings.userDefinedTaskbarBackgroundHorizontalPadding);
  float targetWidth = g_unloading
      ? static_cast<float>(rootWidth)
      : (static_cast<float>(childrenWidthTaskbar + trayFrameWidth) +
         (taskbarBackgroundPadding * 2.0f));
  if (targetWidth < 1) {
    Wh_Log(L"Error: targetWidth<1");
    return false;
  }
  const float targetBackgroundLeftScreen =
      snapPx(targetContentLeft - taskbarBackgroundPadding);
  const float targetBackgroundRightScreen =
      snapPx(targetBackgroundLeftScreen + targetWidth);
  const float targetScaleCenterScreenX =
      snapPx((targetBackgroundLeftScreen + targetBackgroundRightScreen) * 0.5f);
  const float targetTaskbarIslandScale = g_unloading
      ? 1.0f
      : CalculateTaskbarIslandScale(targetBackgroundLeftScreen,
                                    targetBackgroundRightScreen,
                                    static_cast<float>(rootWidth),
                                    targetScaleCenterScreenX,
                                    rasterizationScale);
  const float scaledBackgroundLeftScreen = snapPx(
      ApplyScaleToScreenX(targetBackgroundLeftScreen,
                          targetScaleCenterScreenX,
                          targetTaskbarIslandScale));
  const float scaledBackgroundRightScreen = snapPx(
      ApplyScaleToScreenX(targetBackgroundRightScreen,
                          targetScaleCenterScreenX,
                          targetTaskbarIslandScale));
  const bool taskbarIslandScaleTargetChanged =
      !state.hasLastTargetTaskbarIslandScale ||
      std::abs(state.lastTargetTaskbarIslandScale - targetTaskbarIslandScale) > 0.001f ||
      std::abs(state.lastTaskbarIslandScaleCenterX - targetScaleCenterScreenX) > visualOffsetTolerance;
  const uint64_t dimensionInvalidationGeneration =
      g_dimensionInvalidationGeneration.load(std::memory_order_acquire);
  const bool invalidateDimensionsThisPass =
      state.lastDimensionInvalidationGeneration != dimensionInvalidationGeneration;
  UpdateMinimizeAnimationCorrectionForMonitorTai(
      monitorName,
      taskbarChildrenMeasurement,
      useVirtualTaskbarSurface,
      static_cast<double>(targetTaskRootOffsetX),
      static_cast<double>(targetTaskbarIslandScale),
      static_cast<double>(targetScaleCenterScreenX),
      static_cast<double>(rasterizationScale),
      static_cast<double>(scaledBackgroundLeftScreen),
      static_cast<double>(scaledBackgroundRightScreen));
  // Fork addition: keep the taskbar's clickable region matched to the visible
  // island. Uses the POST-scale bounds computed above so the clip tracks the
  // island when it is shrunk on overflow, and the anchored (not centered) left
  // edge. Placed before the "nothing changed" early-out below so it still runs
  // on cheap passes; UpdateTaskbarWindowRegion is a no-op when the window
  // already has the right region.
  {
    HWND taskbarWindow = g_applyStyleTaskbarWindowTai
                             ? g_applyStyleTaskbarWindowTai
                             : FindTaskbarWindowForMonitorTai(monitorName);
    const bool clearRegion =
        g_unloading || g_settings.userDefinedFullWidthTaskbarBackground;
    if (clearRegion) {
      RECT current{};
      if (taskbarWindow &&
          GetWindowRgnBox(taskbarWindow, &current) >= SIMPLEREGION &&
          EqualRect(&current, &state.lastRegionBox)) {
        SetWindowRgn(taskbarWindow, nullptr, TRUE);
      }
      SetRectEmpty(&state.lastRegionBox);
    } else {
      const float regionCorner =
          g_settings.userDefinedTaskbarCornerRadius * targetTaskbarIslandScale;
      const bool regionCornerChanged =
          std::abs(state.lastRegionCorner - regionCorner) > 0.5f;
      if (UpdateTaskbarWindowRegion(
              taskbarWindow, scaledBackgroundLeftScreen,
              scaledBackgroundRightScreen - scaledBackgroundLeftScreen,
              regionCorner, rasterizationScale, regionCornerChanged,
              &state.lastRegionBox)) {
        state.lastRegionCorner = regionCorner;
      }
    }
  }
  if (!forceStyleApply && !invalidateDimensionsThisPass && !g_unloading &&
      std::abs(targetOffsetXTray - systemTrayFrameGridVisual.Offset().x) <= visualOffsetTolerance &&
      childrenWidthTaskbar == state.lastChildrenWidthTaskbar &&
      trayFrameWidth == state.lastTrayFrameWidth &&
      std::abs(targetTaskRootOffsetX - rootGridTaskBarVisual.Offset().x) <= visualOffsetTolerance &&
      std::abs(taskbarFrameRepeaterVisual.Offset().x) <= visualOffsetTolerance &&
      !taskbarIslandScaleTargetChanged) {
    Wh_Log(L"taskbar root/tray offsets/scale are within tolerance %f; widths didn't change: %d, %d", visualOffsetTolerance, childrenWidthTaskbar, state.lastTrayFrameWidth);
    return true;
  }
  if (childrenWidthTaskbar < 1) {
    state.lastChildrenWidthTaskbar = 1;
  } else {
    state.lastChildrenWidthTaskbar = static_cast<unsigned int>(childrenWidthTaskbar);
  }
  if (trayFrameWidth < 1) {
    state.lastTrayFrameWidth = 1;
  } else {
    state.lastTrayFrameWidth = static_cast<unsigned int>(trayFrameWidth);
  }
  signed int userDefinedTaskbarOffsetY = (g_settings.userDefinedFlatTaskbarBottomCorners || g_settings.userDefinedFullWidthTaskbarBackground) ? 0 : g_settings.userDefinedTaskbarOffsetY;
  if (ShouldLogTaskbarGeometry(isOverflowing || taskbarLayoutIsEdgeClamped || taskbarLayoutIsTrayConstrained || useStableStartButtonAnchor || targetTaskbarIslandScale < 0.999f || forceStyleApply || invalidateDimensionsThisPass)) {
    Wh_Log(L"[TBGEOM] virtualSurface=%d virtualWidth=%.2f actualRepeaterWidth=%.2f layoutSurfaceWidth=%.2f overflowSuppressed=%d",
           useVirtualTaskbarSurface ? 1 : 0,
           taskbarVirtualSurfaceWidth,
           taskbarFrameRepeater.ActualWidth(),
           taskbarLayoutSurfaceWidth,
           (overflowButton && useVirtualTaskbarSurface) ? 1 : 0);
    Wh_Log(L"[TBGEOM] scale target=%.4f centerX=%.2f rawBg=[%.2f..%.2f] scaledBg=[%.2f..%.2f]",
           targetTaskbarIslandScale,
           targetScaleCenterScreenX,
           targetBackgroundLeftScreen,
           targetBackgroundRightScreen,
           scaledBackgroundLeftScreen,
           scaledBackgroundRightScreen);
    Wh_Log(L"[TBGEOM] trayConstraint=%d predictedGapToTray=%.2f minGap=%.2f stableStartMatches=%d",
           taskbarLayoutIsTrayConstrained ? 1 : 0,
           predictedGapToTray,
           minimumTaskbarToTrayGap,
           stableStartAnchorMatchesCurrentSample ? 1 : 0);
    LogTaskbarGeometryProbe(
        L"ApplyStyle",
        monitorName,
        xamlRootContent,
        taskFrame,
        rootGridTaskBar,
        taskbarFrameRepeater,
        trayFrame,
        systemTrayFrameGrid,
        taskbarBackground,
        backgroundFillChild,
        startButtonElement,
        rootWidth,
        childrenWidthTaskbarDbl,
        taskbarLeftEdge,
        taskbarRightEdge,
        startButtonLeft,
        startButtonTop,
        startButtonWidth,
        startButtonHeight,
        predictedCenteredTaskbarLeft,
        isOverflowing,
        taskbarLayoutIsEdgeClamped,
        useStableStartButtonAnchor,
        targetContentLeft,
        targetContentWidth,
        targetTaskRootOffsetX,
        targetOffsetXTray,
        targetBackgroundLeftScreen,
        targetBackgroundRightScreen,
        state);
  }
  const float unscaledStartButtonScreenX = useStableStartButtonAnchor
      ? snapPx(state.stableStartButtonAnchorLeft + targetTaskRootOffsetX)
      : targetBackgroundLeftScreen;
  state.lastStartButtonXCalculated = snapPx(
      ApplyScaleToScreenX(unscaledStartButtonScreenX,
                          targetScaleCenterScreenX,
                          targetTaskbarIslandScale));
  auto heightValue = (g_settings.userDefinedTaskbarHeight + abs(userDefinedTaskbarOffsetY * 2));
  if (heightValue < g_settings.userDefinedTaskbarHeight / 2) {
    Wh_Log(L"Error: heightValue<g_settings.userDefinedTaskbarHeight/2");
    return false;
  }
  if (invalidateDimensionsThisPass) {
    if (g_settings.userDefinedTaskbarHeight <= 0) {
      Wh_Log(L"Invalid size detected! Panel Height");
      return false;
    }
    if (heightValue <= 0) {
      Wh_Log(L"Invalid size detected! Panel Height");
      return false;
    }
    trayFrame.Height(g_settings.userDefinedTaskbarHeight);
    trayFrame.MaxHeight(g_settings.userDefinedTaskbarHeight);
    taskFrame.Height(heightValue);
    taskFrame.MaxHeight(heightValue);
    taskbarFrameRepeater.Height(g_settings.userDefinedTaskbarHeight);
    taskbarFrameRepeater.MaxHeight(g_settings.userDefinedTaskbarHeight);
    ApplyVirtualTaskbarLayoutSurface(xamlRootContent,
                                     taskFrame,
                                     rootGridTaskBar,
                                     taskbarFrameRepeater,
                                     taskbarBackground,
                                     backgroundFillParent,
                                     backgroundFillChild,
                                     taskbarVirtualSurfaceWidth);
    state.lastDimensionInvalidationGeneration = dimensionInvalidationGeneration;
  }
  // Any previous version of the mod may have left a horizontal Offset
  // animation on the repeater. Clear only X so RootGrid owns task-area X
  // motion, but preserve the repeater's native Y offset. Windows/XAML can use
  // that Y component to vertically align the task buttons with the tray.
  if (std::abs(taskbarFrameRepeaterVisual.Offset().x) > visualOffsetTolerance) {
    auto repeaterOffset = taskbarFrameRepeaterVisual.Offset();
    taskbarFrameRepeaterVisual.StopAnimation(L"Offset");
    taskbarFrameRepeaterVisual.Offset({0.0f, repeaterOffset.y, repeaterOffset.z});
  }
  const float rootGridScaleCenterLocalX =
      snapPx(targetScaleCenterScreenX - targetTaskRootOffsetX);
  const float rootGridScaleCenterLocalY =
      snapPx(static_cast<float>(rootGridTaskBar.ActualHeight()) * 0.5f);
  const float trayScaleCenterLocalX =
      snapPx(targetScaleCenterScreenX - targetTrayLogicalLeft);
  const float trayScaleCenterLocalY =
      snapPx(static_cast<float>(systemTrayFrameGrid.ActualHeight()) * 0.5f);
  SetVisualScaleCenterAndAnimate(rootGridTaskBarVisual,
                                 targetTaskbarIslandScale,
                                 rootGridScaleCenterLocalX,
                                 rootGridScaleCenterLocalY,
                                 0.001f,
                                 !g_unloading);
  SetVisualScaleCenterAndAnimate(systemTrayFrameGridVisual,
                                 targetTaskbarIslandScale,
                                 trayScaleCenterLocalX,
                                 trayScaleCenterLocalY,
                                 0.001f,
                                 !g_unloading);
  state.lastTargetTaskbarIslandScale = targetTaskbarIslandScale;
  state.lastTaskbarIslandScaleCenterX = targetScaleCenterScreenX;
  state.hasLastTargetTaskbarIslandScale = true;
  if (auto taskRootVisualCompositor = rootGridTaskBarVisual.Compositor()) {
    if (!g_unloading) {
      const bool taskRootOffsetChanged =
          invalidateDimensionsThisPass ||
          !state.hasLastTargetTaskFrameOffsetX ||
          std::abs(state.lastTargetTaskFrameOffsetX - targetTaskRootOffsetX) > visualOffsetTolerance;
      if (taskRootOffsetChanged) {
        auto taskRootOffsetAnimation = taskRootVisualCompositor.CreateVector3KeyFrameAnimation();
        ConfigureTaskbarIslandAnimation(taskRootOffsetAnimation);
        InsertTaskbarIslandKeyFrame(taskRootOffsetAnimation, 1.0f, winrt::Windows::Foundation::Numerics::float3{targetTaskRootOffsetX, rootGridTaskBarVisual.Offset().y, rootGridTaskBarVisual.Offset().z});
        rootGridTaskBarVisual.StartAnimation(L"Offset", taskRootOffsetAnimation);
        state.lastTargetTaskFrameOffsetX = targetTaskRootOffsetX;
        state.hasLastTargetTaskFrameOffsetX = true;
      }
    } else {
      rootGridTaskBarVisual.StopAnimation(L"Offset");
      rootGridTaskBarVisual.Offset({0.0f, 0.0f, rootGridTaskBarVisual.Offset().z});
      taskbarFrameRepeaterVisual.StopAnimation(L"Offset");
      taskbarFrameRepeaterVisual.Offset({0.0f, 0.0f, taskbarFrameRepeaterVisual.Offset().z});
      state.hasLastTargetTaskFrameOffsetX = false;
    }
  }
  auto taskbarVisual = rootGridTaskBarVisual;
  auto trayVisualCompositor = systemTrayFrameGridVisual.Compositor();
    if (trayVisualCompositor) {
    if (!g_unloading) {
      const bool trayOffsetChanged =
          invalidateDimensionsThisPass ||
          !state.hasLastTargetTrayOffsetX ||
          std::abs(state.lastTargetTrayOffsetX - targetOffsetXTray) > visualOffsetTolerance;
      if (trayOffsetChanged) {
        auto trayAnimation = trayVisualCompositor.CreateVector3KeyFrameAnimation();
        ConfigureTaskbarIslandAnimation(trayAnimation);
        InsertTaskbarIslandKeyFrame(trayAnimation, 1.0f, winrt::Windows::Foundation::Numerics::float3{targetOffsetXTray, systemTrayFrameGridVisual.Offset().y, systemTrayFrameGridVisual.Offset().z});
        systemTrayFrameGridVisual.StartAnimation(L"Offset", trayAnimation);
        state.lastTargetTrayOffsetX = targetOffsetXTray;
        state.hasLastTargetTrayOffsetX = true;
      }
    } else {
      systemTrayFrameGridVisual.Offset({0.0f, 0.0f, 0.0f});
      state.hasLastTargetTrayOffsetX = false;
    }
  }
  if (widgetPresent && widgetMainView) {
    if (widgetElement) {
      auto widgetVisualParent = winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(widgetElement);
      if (widgetVisualParent && widgetVisualParent.Offset().x != 0.0f) {
        widgetVisualParent.Offset({0.0f, widgetVisualParent.Offset().y, widgetVisualParent.Offset().z});
      }
    }
    auto widgetVisual = winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(widgetMainView);
     if (widgetVisual) {
      if (!g_unloading) {
        auto compositorWidget = widgetVisual.Compositor();
        if (compositorWidget) {
          // Widget visual is inside the RootGrid-shifted subtree, so its local
          // target must exclude the RootGrid compositor offset. Its screen
          // position still lands after the centered task buttons.
          float targetOffsetXWidget = centeredTaskbarRightEdge - targetTaskRootOffsetX - 8.0f + g_settings.userDefinedTrayTaskGap;
          targetOffsetXWidget = snapPx(targetOffsetXWidget);
          float targetOffsetYWidget = snapPx(static_cast<float>(abs(g_settings.userDefinedTaskbarHeight - widgetElementVisibleHeight)));
          const bool widgetOffsetChanged =
              invalidateDimensionsThisPass ||
              !state.hasLastTargetWidgetOffset ||
              std::abs(state.lastTargetWidgetOffsetX - targetOffsetXWidget) > visualOffsetTolerance ||
              std::abs(state.lastTargetWidgetOffsetY - targetOffsetYWidget) > visualOffsetTolerance;
          if (widgetOffsetChanged) {
            auto widgetOffsetAnimation = compositorWidget.CreateVector3KeyFrameAnimation();
            ConfigureTaskbarIslandAnimation(widgetOffsetAnimation);
            InsertTaskbarIslandKeyFrame(widgetOffsetAnimation, 1.0f, winrt::Windows::Foundation::Numerics::float3{targetOffsetXWidget, targetOffsetYWidget, taskbarVisual.Offset().z});
            widgetVisual.StartAnimation(L"Offset", widgetOffsetAnimation);
            state.lastTargetWidgetOffsetX = targetOffsetXWidget;
            state.lastTargetWidgetOffsetY = targetOffsetYWidget;
            state.hasLastTargetWidgetOffset = true;
          }
        }
      } else {
        widgetVisual.Offset({0.0f, 0.0f, 0.0f});
        state.hasLastTargetWidgetOffset = false;
      }
    }
  }
  if (state.lastTargetWidth <= static_cast<float>(minimumTaskbarChildrenWidth)) {
    state.lastTargetWidth = static_cast<float>(rootWidth);
    if (!g_unloading && state.lastTargetWidth <= 0) {
      Wh_Log(L"Error: g_unloading && state.lastTargetWidth <= 0");
      return false;
    }
  }
  const float targetWidthRect = !g_settings.userDefinedFullWidthTaskbarBackground ? targetWidth : static_cast<float>(rootWidth);
  if (!g_unloading && targetWidthRect <= 0) {
    Wh_Log(L"Error: targetWidthRect<=0");
    return false;
  }
  int rightMostEdgeTray = static_cast<int>(scaledBackgroundRightScreen + 0.5f);
  if (state.lastRightMostEdgeTray != rightMostEdgeTray) {
    state.lastRightMostEdgeTray = rightMostEdgeTray;
    Wh_SetIntValue((L"lastRightMostEdgeTray_" + monitorName).c_str(), rightMostEdgeTray);
  }
  float leftMostEdgeTray = scaledBackgroundRightScreen - (static_cast<float>(trayFrameWidth) * targetTaskbarIslandScale);
  if (leftMostEdgeTray != state.lastLeftMostEdgeTray) {
    state.lastLeftMostEdgeTray = leftMostEdgeTray;
    Wh_SetIntValue((L"lastLeftMostEdgeTray_" + monitorName).c_str(), static_cast<int>(leftMostEdgeTray));
  }
  // Fork addition: see g_languageIndicatorCentersXDip.
  RecordLanguageIndicatorCenterTai(monitorName, trayFrame, systemTrayFrameGrid,
                                   targetOffsetXTray, trayScaleCenterLocalX,
                                   targetTaskbarIslandScale);
  const auto targetHeightPrelim = (!g_settings.userDefinedFullWidthTaskbarBackground ? g_settings.userDefinedTaskbarHeight : xamlRootContent.ActualHeight());
  if (!g_unloading && targetHeightPrelim <= 0) {
    Wh_Log(L"Error: targetHeightPrelim<=0");
    return false;
  }
  const auto clipHeight = static_cast<float>(targetHeightPrelim + ((g_settings.userDefinedFlatTaskbarBottomCorners) ? (targetHeightPrelim - g_settings.userDefinedTaskbarCornerRadius) : 0.0f));
  if (!g_unloading && clipHeight <= 0) {
    Wh_Log(L"Error: clipHeight<=0");
    return false;
  }
  ProcessStackPanelChildren(stackPanel, clipHeight);
  ChangeControlCenterIconSize(systemTrayFrameGrid);
  auto trayOverflowArrowNotifyIconStack = FindChildByName(systemTrayFrameGrid, L"NotifyIconStack");
  if (trayOverflowArrowNotifyIconStack) {
    SetDividerForElement(trayOverflowArrowNotifyIconStack, clipHeight, g_settings.userDefinedTrayAreaDivider, true);
  } else {
    SetDividerForElement(stackPanel, clipHeight, g_settings.userDefinedTrayAreaDivider, true);
  }
  //  if (widgetPresent && widgetElementInnerChild) {
  //    SetDividerForElement(widgetElementInnerChild, clipHeight, widgetPresent && g_settings.userDefinedTrayAreaDivider, true);
  //  }
  const bool shouldApplyCustomTaskbarBackground = !g_unloading && g_settings.userDefinedCustomizeTaskbarBackground;
  const bool shouldClearCustomTaskbarBackground = g_unloading || (!shouldApplyCustomTaskbarBackground && state.hasCustomTaskbarBackgroundVisuals);
  auto taskbarStroke = FindChildByName(backgroundFillParent, L"BackgroundStroke");
  auto screenEdgeStroke = FindChildByName(rootGridTaskBar, L"ScreenEdgeStroke");
  // you can also try SystemAccentColor
  auto backgroundFillVisual = winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(backgroundFillChild);
  auto compositorTaskBackground = backgroundFillVisual
      ? backgroundFillVisual.Compositor()
      : nullptr;
  const uintptr_t backgroundFillIdentity =
      reinterpret_cast<uintptr_t>(winrt::get_abi(backgroundFillChild));
  const bool backgroundFillChanged =
      state.backgroundFillIdentity != backgroundFillIdentity;
  const bool backgroundStyleChanged =
      backgroundFillChanged ||
      state.lastBackgroundStyleGeneration != childStyleGeneration;
  bool backgroundBrushReady = true;
  if (shouldApplyCustomTaskbarBackground) {
    if (taskbarStroke) {
      taskbarStroke.Opacity(0.0);
    }
    if (screenEdgeStroke) {
      screenEdgeStroke.Opacity(0.0);
    }
    if (!state.hasCustomTaskbarBackgroundVisuals || backgroundStyleChanged) {
      backgroundBrushReady =
          ApplyWindhawkBlurToBackgroundFill(backgroundFillChild);
    }
    state.backgroundFillIdentity = backgroundFillIdentity;
    state.hasCustomTaskbarBackgroundVisuals = true;
//    For custom brush
//    auto compositor = winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(backgroundFillChild).Compositor();
//    float blurAmount = float(g_settings.userDefinedTaskbarBackgroundLuminosity);
//    winrt::Windows::Foundation::Numerics::float4 tint = {0,0,0,0};
//    auto blurBrush = winrt::make<XamlBlurBrush>(compositor, blurAmount, tint);
//    auto rectangle = backgroundFillChild.try_as<winrt::Windows::UI::Xaml::Shapes::Rectangle>();
//    if (rectangle){
//    rectangle.Fill(blurBrush);
//    }
  } else if (shouldClearCustomTaskbarBackground) {
    if (taskbarStroke) {
      taskbarStroke.ClearValue(UIElement::OpacityProperty());
    }
    if (screenEdgeStroke) {
      screenEdgeStroke.ClearValue(UIElement::OpacityProperty());
    }
    ClearWindhawkBlurFromBackgroundFill(backgroundFillChild);
    if (backgroundFillVisual) {
      backgroundFillVisual.Clip(nullptr);
    }
    winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::
        SetElementChildVisual(
            backgroundFillChild,
            winrt::Windows::UI::Composition::Visual{nullptr});
    ResetBackgroundVisualCache(state);
    state.hasCustomTaskbarBackgroundVisuals = false;
  }
  // borders and corners
  if (shouldApplyCustomTaskbarBackground) {
    if (backgroundFillVisual) {
      if (compositorTaskBackground) {
        const float userDefinedTaskbarBorderThicknessFloat = static_cast<float>(g_settings.userDefinedTaskbarBorderThickness);
        // BackgroundFill belongs to RootGrid, and RootGrid is now translated.
        // Convert the desired screen-space island left into RootGrid-local
        // coordinates so the island background and task/tray content stay in
        // the same coordinate system.
        const float offsetXRect = snapPx(targetBackgroundLeftScreen - targetTaskRootOffsetX);
        const float newOffsetYRect = snapPx(static_cast<float>(abs(userDefinedTaskbarOffsetY)) );
        const bool backgroundShapeTargetChanged =
            std::abs(state.lastBackgroundShapeTargetWidth - targetWidthRect) > visualOffsetTolerance ||
            std::abs(state.lastBackgroundShapeTargetHeight - clipHeight) > visualOffsetTolerance ||
            std::abs(state.lastBackgroundShapeTargetOffsetX - offsetXRect) > visualOffsetTolerance ||
            std::abs(state.lastBackgroundShapeTargetOffsetY - newOffsetYRect) > visualOffsetTolerance;
        TaskbarBackgroundCompositionResourcesTai backgroundResources;
        bool createdBackgroundResources = false;
        if (backgroundFillChanged ||
            !TryGetTaskbarBackgroundCompositionResourcesTai(
                backgroundFillChild,
                backgroundFillVisual,
                &backgroundResources)) {
          createdBackgroundResources =
              CreateTaskbarBackgroundCompositionResourcesTai(
                  backgroundFillChild,
                  backgroundFillVisual,
                  compositorTaskBackground,
                  &backgroundResources);
        }

        if (backgroundResources.clipGeometry &&
            backgroundResources.borderVisual &&
            backgroundResources.borderGeometry &&
            backgroundResources.borderShape &&
            backgroundResources.borderBrush) {
          if (createdBackgroundResources || backgroundStyleChanged) {
            const float borderCornerRadius =
                g_settings.userDefinedTaskbarCornerRadius -
                userDefinedTaskbarBorderThicknessFloat / 2.0f;
            backgroundResources.clipGeometry.CornerRadius({
                g_settings.userDefinedTaskbarCornerRadius,
                g_settings.userDefinedTaskbarCornerRadius});
            backgroundResources.borderGeometry.CornerRadius({
                borderCornerRadius,
                borderCornerRadius});
            backgroundResources.borderGeometry.Offset({
                userDefinedTaskbarBorderThicknessFloat / 2.0f,
                userDefinedTaskbarBorderThicknessFloat / 2.0f});
            backgroundResources.borderShape.StrokeThickness(
                g_settings.userDefinedTaskbarBorderThickness);
            backgroundResources.borderShape.FillBrush(nullptr);
            backgroundResources.borderBrush.Color({
                g_settings.userDefinedTaskbarBorderOpacity,
                static_cast<BYTE>(g_settings.borderColorR),
                static_cast<BYTE>(g_settings.borderColorG),
                static_cast<BYTE>(g_settings.borderColorB)});
            backgroundResources.borderGeometry.Size({
                std::max(
                    0.0f,
                    targetWidthRect - userDefinedTaskbarBorderThicknessFloat),
                std::max(
                    0.0f,
                    clipHeight - userDefinedTaskbarBorderThicknessFloat)});
          }

          if (createdBackgroundResources ||
              backgroundShapeTargetChanged ||
              backgroundStyleChanged) {
            float animationStartWidth = targetWidthRect;
            float animationStartOffsetX = offsetXRect;
            float animationStartOffsetY = newOffsetYRect;
            const int64_t animationNowMs = DelayedApplyNowMs();
            const bool canContinueRunningBackgroundAnimation =
                !invalidateDimensionsThisPass &&
                state.backgroundAnimationStartMs > 0 &&
                animationNowMs - state.backgroundAnimationStartMs <
                    kTaskbarIslandAnimationDurationMs;
            if (!g_settings.userDefinedFullWidthTaskbarBackground) {
              if (canContinueRunningBackgroundAnimation) {
                animationStartWidth = EstimateAnimationValue(state.backgroundAnimationFromWidth, state.backgroundAnimationToWidth, state.backgroundAnimationStartMs, animationNowMs);
                animationStartOffsetX = EstimateAnimationValue(state.backgroundAnimationFromOffsetX, state.backgroundAnimationToOffsetX, state.backgroundAnimationStartMs, animationNowMs);
                animationStartOffsetY = EstimateAnimationValue(state.backgroundAnimationFromOffsetY, state.backgroundAnimationToOffsetY, state.backgroundAnimationStartMs, animationNowMs);
              } else if (state.lastTargetWidth > static_cast<float>(minimumTaskbarChildrenWidth) && std::abs(state.lastTargetWidth - rootWidth) > visualOffsetTolerance) {
                animationStartWidth = state.lastTargetWidth;
                animationStartOffsetX = state.lastTargetOffsetX == 0.0f ? offsetXRect : snapPx(state.lastTargetOffsetX);
                animationStartOffsetY = snapPx(state.lastTargetOffsetY);
              }
            }

            backgroundResources.clipGeometry.StopAnimation(L"Size");
            backgroundResources.clipGeometry.StopAnimation(L"Offset");
            backgroundResources.borderVisual.StopAnimation(L"Size");
            backgroundResources.borderVisual.StopAnimation(L"Offset");
            backgroundResources.borderGeometry.StopAnimation(L"Size");

            if (!g_settings.userDefinedFullWidthTaskbarBackground) {
              backgroundResources.clipGeometry.Size(
                  {animationStartWidth, clipHeight});
              backgroundResources.borderVisual.Size(
                  {animationStartWidth, clipHeight});
              backgroundResources.borderGeometry.Size({
                  std::max(
                      0.0f,
                      animationStartWidth -
                          userDefinedTaskbarBorderThicknessFloat),
                  std::max(
                      0.0f,
                      clipHeight - userDefinedTaskbarBorderThicknessFloat)});
              backgroundResources.clipGeometry.Offset(
                  {animationStartOffsetX, animationStartOffsetY});
              backgroundResources.borderVisual.Offset(
                  {animationStartOffsetX, animationStartOffsetY, 0.0f});

              auto sizeAnimationRect = compositorTaskBackground.CreateVector2KeyFrameAnimation();
              ConfigureTaskbarIslandAnimation(sizeAnimationRect);
              sizeAnimationRect.InsertKeyFrame(0.0f, {animationStartWidth, clipHeight});
              InsertTaskbarIslandKeyFrame(sizeAnimationRect, 1.0f, winrt::Windows::Foundation::Numerics::float2{targetWidthRect, clipHeight});
              auto sizeAnimationBorderGeometry = compositorTaskBackground.CreateVector2KeyFrameAnimation();
              ConfigureTaskbarIslandAnimation(sizeAnimationBorderGeometry);
              sizeAnimationBorderGeometry.InsertKeyFrame(0.0f, {animationStartWidth - userDefinedTaskbarBorderThicknessFloat, clipHeight - userDefinedTaskbarBorderThicknessFloat});
              InsertTaskbarIslandKeyFrame(sizeAnimationBorderGeometry, 1.0f, winrt::Windows::Foundation::Numerics::float2{targetWidthRect - userDefinedTaskbarBorderThicknessFloat, clipHeight - userDefinedTaskbarBorderThicknessFloat});
              backgroundResources.clipGeometry.StartAnimation(
                  L"Size",
                  sizeAnimationRect);
              backgroundResources.borderVisual.StartAnimation(
                  L"Size",
                  sizeAnimationRect);
              backgroundResources.borderGeometry.StartAnimation(
                  L"Size",
                  sizeAnimationBorderGeometry);
              auto offsetAnimationRect = compositorTaskBackground.CreateVector2KeyFrameAnimation();
              ConfigureTaskbarIslandAnimation(offsetAnimationRect);
              offsetAnimationRect.InsertKeyFrame(0.0f, {animationStartOffsetX, animationStartOffsetY});
              InsertTaskbarIslandKeyFrame(offsetAnimationRect, 1.0f, winrt::Windows::Foundation::Numerics::float2{offsetXRect, newOffsetYRect});
              auto offsetAnimationRect3V = compositorTaskBackground.CreateVector3KeyFrameAnimation();
              ConfigureTaskbarIslandAnimation(offsetAnimationRect3V);
              offsetAnimationRect3V.InsertKeyFrame(0.0f, {animationStartOffsetX, animationStartOffsetY, 0.0f});
              InsertTaskbarIslandKeyFrame(offsetAnimationRect3V, 1.0f, winrt::Windows::Foundation::Numerics::float3{offsetXRect, newOffsetYRect, 0.0f});
              backgroundResources.clipGeometry.StartAnimation(
                  L"Offset",
                  offsetAnimationRect);
              backgroundResources.borderVisual.StartAnimation(
                  L"Offset",
                  offsetAnimationRect3V);
              state.backgroundAnimationFromWidth = animationStartWidth;
              state.backgroundAnimationToWidth = targetWidthRect;
              state.backgroundAnimationFromOffsetX = animationStartOffsetX;
              state.backgroundAnimationToOffsetX = offsetXRect;
              state.backgroundAnimationFromOffsetY = animationStartOffsetY;
              state.backgroundAnimationToOffsetY = newOffsetYRect;
              state.backgroundAnimationStartMs = animationNowMs;
              state.lastTargetOffsetX = offsetXRect;
              state.lastTargetOffsetY = newOffsetYRect;
            } else {
              state.lastTargetOffsetX = 0;
              state.lastTargetOffsetY = 0;
              state.backgroundAnimationStartMs = 0;
              backgroundResources.clipGeometry.Size(
                  {targetWidthRect, clipHeight});
              backgroundResources.borderVisual.Size({targetWidthRect, clipHeight});
              if(rootGridTaskBarVisual){
              backgroundResources.clipGeometry.Offset({static_cast<float>(-rootGridTaskBarVisual.Offset().x), 0.0f});
              backgroundResources.borderVisual.Offset({static_cast<float>(-rootGridTaskBarVisual.Offset().x), 0.0f, 0.0f});
              }
              backgroundResources.borderGeometry.Size({
                  std::max(
                      0.0f,
                      targetWidthRect - userDefinedTaskbarBorderThicknessFloat),
                  std::max(
                      0.0f,
                      clipHeight - userDefinedTaskbarBorderThicknessFloat)});
            }
            state.lastBackgroundShapeTargetWidth = targetWidthRect;
            state.lastBackgroundShapeTargetHeight = clipHeight;
            state.lastBackgroundShapeTargetOffsetX = offsetXRect;
            state.lastBackgroundShapeTargetOffsetY = newOffsetYRect;
          }

          if (backgroundBrushReady) {
            state.lastBackgroundStyleGeneration = childStyleGeneration;
          }
        }
      }
    }
  }
  state.wasOverflowing = isOverflowing;
  state.lastTargetWidth = targetWidth;
  g_initial_style_apply_completed = true;
  g_initial_style_apply_not_before_ms = 0;
  return true;
  } catch (winrt::hresult_error const& ex) {
    Wh_Log(L"ApplyStyle failed %08X: %s", ex.code(), ex.message().c_str());
  } catch (std::exception const& ex) {
    Wh_Log(L"ApplyStyle failed: %S", ex.what());
  } catch (...) {
    Wh_Log(L"ApplyStyle failed: %08X", winrt::to_hresult());
  }
  return false;
}
void ApplySettings(HWND hTaskbarWnd) {
  if (hTaskbarWnd && IsWindow(hTaskbarWnd)) {
    RunFromWindowThread(hTaskbarWnd, [](void* pParam) { ApplySettingsFromTaskbarThread(); }, 0);
  }
}
void RefreshSettings() {
  unsigned int oldTaskbarButtonSize;
  {
    std::lock_guard<std::recursive_mutex> settingsLock(g_settingsMutex);
    oldTaskbarButtonSize = g_settings.userDefinedTaskbarButtonSize;
  }
  Wh_ModSettingsChangedTBIconSize();
  Wh_ModSettingsChangedStartButtonPosition();
  UpdateGlobalSettings();
  RequestTaskbarChildStyleRefresh();
  RequestTaskbarDimensionInvalidation();
  unsigned int newTaskbarButtonSize;
  {
    std::lock_guard<std::recursive_mutex> settingsLock(g_settingsMutex);
    newTaskbarButtonSize = g_settings.userDefinedTaskbarButtonSize;
  }
  if (!g_unloading && oldTaskbarButtonSize > 0 &&
      oldTaskbarButtonSize != newTaskbarButtonSize) {
    RequestTaskbarButtonSizeRelayout();
  }
}
void ResetGlobalVars() {
  for (const auto& stateHandle : GetTaskbarStatesSnapshot()) {
    std::lock_guard<std::recursive_mutex> stateLock(stateHandle->mutex);
    auto& state = *stateHandle;
    state.lastTaskbarData.childrenCount = 0;
    state.lastTaskbarData.rightMostEdge = 0;
    // state.lastTaskbarData.childrenWidth = 0;
    state.lastChildrenWidthTaskbar = 0;
    // state.lastTrayFrameWidth = 0;
    state.wasOverflowing = false;
  }
  RequestTaskbarDimensionInvalidation();
}
bool g_PartialMode = false;
void Wh_ModSettingsChanged() {
  if (g_PartialMode) {
    return;
  }
  Wh_Log(L"Settings Changed");
  ResetGlobalVars();
  RefreshSettings();
  ApplySettings(FindCurrentProcessTaskbarWnd());
}
bool IsExplorer() {
  wchar_t processPath[MAX_PATH];
  if (GetModuleFileName(NULL, processPath, MAX_PATH)) {
    const wchar_t* processName = wcsrchr(processPath, L'\\');
    if (processName && _wcsicmp(processName + 1, L"explorer.exe") == 0) {
      return true;
    }
  }
  return false;
}
using SetWindowPos_t = BOOL(WINAPI*)(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
SetWindowPos_t SetWindowPos_Original = nullptr;
std::wstring GetProcessExeName(DWORD processId) {
  std::wstring result = L"<unknown>";
  HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
  if (hProcess) {
    WCHAR path[MAX_PATH];
    DWORD size = ARRAYSIZE(path);
    if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
      std::wstring fullPath = path;
      size_t pos = fullPath.find_last_of(L"\\/");
      result = (pos != std::wstring::npos) ? fullPath.substr(pos + 1) : fullPath;
    }
    CloseHandle(hProcess);
  }
  return result;
}
BOOL WINAPI SetWindowPos_Hook(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags) {
  DWORD processId = 0;
  const bool userDefinedMoveFlyoutControlCenter =
      Wh_GetIntSetting(L"MoveFlyoutControlCenter") != 0;
  auto callOriginal = [&]() -> BOOL {
    return SetWindowPos_Original
        ? SetWindowPos_Original(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags)
        : FALSE;
  };
  if (!hWnd || !GetWindowThreadProcessId(hWnd, &processId)) {
    return callOriginal();
  }
  WCHAR className[256] = L"<unknown>";
  GetClassNameW(hWnd, className, ARRAYSIZE(className));
  const std::wstring windowClassName = className;
  const std::wstring processFileName = GetProcessExeName(processId);
  Wh_Log(L"[SetWindowPos] PID: %lu | EXE: %s | Class: %s | HWND: 0x%p | Pos: (%d,%d) Size: %dx%d Flags: 0x%08X",
         processId,
         processFileName.c_str(),
         windowClassName.c_str(),
         hWnd,
         X,
         Y,
         cx,
         cy,
         uFlags);
  // Fork addition: see g_languageIndicatorCentersXDip.
  if (!g_unloading && !(uFlags & SWP_NOMOVE) &&
      processId == GetCurrentProcessId() &&
      _wcsicmp(windowClassName.c_str(), L"Shell_InputSwitchTopLevelWindow") == 0 &&
      Wh_GetIntSetting(L"MoveFlyoutKeyboardLayout") != 0) {
    X = PlaceInputSwitchFlyoutXTai(hWnd, X, Y, cx, cy, uFlags);
  }
  if (!g_unloading && userDefinedMoveFlyoutControlCenter && _wcsicmp(processFileName.c_str(), L"ShellHost.exe") == 0 && _wcsicmp(windowClassName.c_str(), L"ControlCenterWindow") == 0) {
    HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    if (!monitor) {
      return callOriginal();
    }

    auto monitorName = GetMonitorName(monitor);
    if (monitorName.empty()) {
      Wh_Log(L"[SetWindowPos] Failed to resolve monitor name");
      return callOriginal();
    }

    int lastRecordedTrayRightMostEdgeForMonitor = Wh_GetIntValue((L"lastRightMostEdgeTray_" + monitorName).c_str(), -1);
    if (lastRecordedTrayRightMostEdgeForMonitor > 0) {
      UINT monitorDpiX = 96;
      UINT monitorDpiY = 96;
      if (FAILED(GetDpiForMonitor(monitor, MDT_DEFAULT, &monitorDpiX, &monitorDpiY)) ||
          monitorDpiX == 0) {
        monitorDpiX = 96;
        monitorDpiY = 96;
      }
      float dpiScale = static_cast<float>(monitorDpiX) / 96.0f;
      const int flyoutInnerPaddingPx = GetFlyoutInnerPaddingPx(dpiScale);
      X = static_cast<int>(lastRecordedTrayRightMostEdgeForMonitor * dpiScale + flyoutInnerPaddingPx - (Wh_GetIntSetting(L"AlignFlyoutInner") ? cx : (cx / 2.0f)));
      Wh_Log(L"[SetWindowPos] New X %d", X);
    } else {
      Wh_Log(L"[SetWindowPos] No reference state for monitor %s", monitorName.c_str());
    }
  }
  return callOriginal();
}
BOOL Wh_ModInit() {
  Wh_Log(L"======================================================");
  HMODULE moduleUser32 = GetModuleHandleW(L"user32.dll");
  if (moduleUser32) {
    auto pSetWindowPos = (SetWindowPos_t)GetProcAddress(moduleUser32, "SetWindowPos");
    if (pSetWindowPos) {
      if (WindhawkUtils::Wh_SetFunctionHookT(pSetWindowPos, SetWindowPos_Hook, &SetWindowPos_Original)) {
        Wh_Log(L"Successfully hooked SetWindowPos");
      } else {
        Wh_Log(L"Failed to hook SetWindowPos");
      }
    } else {
      Wh_Log(L"Failed to get address of SetWindowPos");
    }
  } else {
    Wh_Log(L"Failed to load user32.dll");
  }
  if (!IsExplorer()) {
    g_PartialMode = true;
    Wh_Log(L"Not explorer.exe; setting g_PartialMode to true");
    // StartDocked.dll
    HMODULE moduleStartDocked = GetModuleHandle(L"StartDocked.dll");
    if (moduleStartDocked) {
      WindhawkUtils::SYMBOL_HOOK StartDockedDll_hook[] = {{{LR"(private: void __cdecl StartDocked::StartSizingFrame::UpdateWindowRegion(class Windows::Foundation::Size))"}, &StartDocked__StartSizingFrame_UpdateWindowRegion_WithArgs_Original, StartDocked__StartSizingFrame_UpdateWindowRegion_WithArgs_Hook}};
      return WindhawkUtils::HookSymbols(moduleStartDocked, StartDockedDll_hook, ARRAYSIZE(StartDockedDll_hook));
    }
    return true;
  }
  g_unloading = false;
  g_worker_threads_stopping = false;
  InitMinimizeAnimationCorrectionTai();
  ArmInitialExplorerStyleApplyDelay();
  if (!Wh_ModInitTBIconSize()) {
    Wh_Log(L"Wh_ModInitTBIconSize failed");
    UninitMinimizeAnimationCorrectionTai();
    return FALSE;
  }
  if (!Wh_ModInitStartButtonPosition()) {
    Wh_Log(L"Wh_ModInitStartButtonPosition failed");
    UninitMinimizeAnimationCorrectionTai();
    return FALSE;
  }
  // Fork addition: see CActionCenterExperienceManager_GetViewPosition_Hook.
  HookNotificationCenterViewPositionIfLoadedTai();
  return TRUE;
}
void Wh_ModAfterInit() {
  if (g_PartialMode) {
    g_lastRecordedStartMenuWidth = Wh_GetIntValue(L"lastRecordedStartMenuWidth", g_lastRecordedStartMenuWidth);
    return;
  }
  Wh_ModAfterInitTBIconSize();
  ResetGlobalVars();
  LoadSettingsTBIconSize();
  LoadSettingsStartButtonPosition();
  UpdateGlobalSettings();
  ScheduleInitialExplorerStyleApply();
  // Fork addition: see CActionCenterExperienceManager_GetViewPosition_Hook.
  StartNotificationCenterHookWaitTai();
}
void Wh_ModBeforeUninit() {
  if (g_PartialMode) {
    return;
  }
  // Fork addition: see CActionCenterExperienceManager_GetViewPosition_Hook.
  StopNotificationCenterHookWaitTai();
  g_unloading = true;
  CleanupDebounce();
  UninitMinimizeAnimationCorrectionTai();
  Wh_ModBeforeUninitTBIconSize();
  Wh_ModBeforeUninitStartButtonPosition();
  RefreshSettings();
  HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
  if (hTaskbarWnd) {
    ApplySettings(hTaskbarWnd);
  }
}
void Wh_ModUninit() {
  if (g_PartialMode) {
    return;
  }
  // Fork addition: belt-and-braces. Do not rely on a final g_unloading
  // ApplyStyle pass reaching the region update -- it has several earlier
  // return paths.
  ClearAllTaskbarWindowRegionsTai();
  UninitMinimizeAnimationCorrectionTai();
  CleanupDebounce();
  Wh_ModUninitTBIconSize();
  ResetGlobalVars();
  ClearTaskbarStates();
  ClearLanguageIndicatorCentersTai();
  {
    std::lock_guard<std::recursive_mutex> settingsLock(g_settingsMutex);
    g_settings.compiledDividedAppPatterns.clear();
  }
  Wh_Log(L"... detached");
}
