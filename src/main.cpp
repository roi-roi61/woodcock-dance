#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <shellapi.h>
#include <gdiplus.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <random>
#include <string>
#include <vector>

#include "resource.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")

namespace {

constexpr wchar_t kWindowClass[] = L"WoodcockDance.LayeredWindow";
constexpr wchar_t kApplicationName[] = L"Woodcock Dance";
constexpr wchar_t kSingleInstanceMutex[] = L"Local\\WoodcockDance.SingleInstance";
constexpr wchar_t kRegistryPath[] = L"Software\\WoodcockDance";
constexpr wchar_t kRegistrySize[] = L"Size";
constexpr wchar_t kRegistryStartup[] = L"Startup";
constexpr wchar_t kRunValue[] = L"WoodcockDance";

constexpr UINT_PTR kAnimationTimer = 1;
constexpr UINT kAnimationTimerMs = 33;
constexpr int kHotKeyId = 31;
constexpr UINT kTrayMessage = WM_APP + 31;

constexpr ULONGLONG kTraverseDurationMs = 3ULL * 60ULL * 1000ULL;
constexpr double kDancePeriodMs = 3500.0;
constexpr int kBottomMargin = 6;
constexpr int kMaxBirdCount = 18;
constexpr int kAssetCount = 7;
constexpr int kFlockGap = 16;
constexpr UINT kMaxAssetDimension = 2048;
constexpr size_t kMaxAssetPixels = 4U * 1024U * 1024U;
constexpr ULONGLONG kMaxExternalAssetBytes = 16ULL * 1024ULL * 1024ULL;

constexpr std::array<const wchar_t*, kAssetCount> kAssetFiles{
  L"woodcock-base.png",
  L"variant-01-backpack.png",
  L"variant-02-scarf.png",
  L"variant-03-cafe.png",
  L"variant-04-classic.png",
  L"variant-05-skate.png",
  L"variant-06-hoverboard.png"
};

constexpr std::array<int, kAssetCount> kAssetResourceIds{
  IDR_WOODCOCK_BASE,
  IDR_VARIANT_BACKPACK,
  IDR_VARIANT_SCARF,
  IDR_VARIANT_CAFE,
  IDR_VARIANT_CLASSIC,
  IDR_VARIANT_SKATE,
  IDR_VARIANT_HOVERBOARD
};

constexpr UINT kMenuToggle = 41001;
constexpr UINT kMenuSmall = 41002;
constexpr UINT kMenuMedium = 41003;
constexpr UINT kMenuLarge = 41004;
constexpr UINT kMenuStartup = 41005;
constexpr UINT kMenuExit = 41006;

constexpr int kGroupBirdCount = 3;
constexpr int kGroupGap = 16;
constexpr ULONGLONG kGroupDanceMs = 10000;

struct Sprite {
  int width = 0;
  int height = 0;
  std::vector<BYTE> pixels;
};

struct Geometry {
  double x = 0.0;
};

struct AnimatedFrame {
  int width = 0;
  int height = 0;
  int offsetX = 0;
  int offsetY = 0;
  std::vector<BYTE> pixels;
};

std::wstring ModulePath() {
  for (DWORD capacity = MAX_PATH; capacity <= 32768; capacity *= 2) {
    std::vector<wchar_t> buffer(capacity, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), capacity);
    if (length == 0) return L"";
    if (length < capacity - 1) {
      return std::wstring(buffer.data(), length);
    }
  }
  return L"";
}

std::wstring ModuleDirectory() {
  const std::wstring path = ModulePath();
  if (path.empty()) return L".";

  const std::wstring::size_type slash = path.find_last_of(L"\\/");
  return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}

bool ReadRegistryDword(const wchar_t* name, DWORD& value) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
    return false;
  }

  DWORD type = 0;
  DWORD bytes = sizeof(value);
  const LONG result = RegQueryValueExW(
    key,
    name,
    nullptr,
    &type,
    reinterpret_cast<LPBYTE>(&value),
    &bytes
  );
  RegCloseKey(key);
  return result == ERROR_SUCCESS && type == REG_DWORD && bytes == sizeof(value);
}

bool WriteRegistryDword(const wchar_t* name, DWORD value) {
  HKEY key = nullptr;
  DWORD disposition = 0;
  if (RegCreateKeyExW(
        HKEY_CURRENT_USER,
        kRegistryPath,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        nullptr,
        &key,
        &disposition) != ERROR_SUCCESS) {
    return false;
  }

  const LONG result = RegSetValueExW(
    key,
    name,
    0,
    REG_DWORD,
    reinterpret_cast<const BYTE*>(&value),
    sizeof(value)
  );
  RegCloseKey(key);
  return result == ERROR_SUCCESS;
}

bool SetAutoStart(bool enabled) {
  HKEY key = nullptr;
  if (enabled) {
    const std::wstring modulePath = ModulePath();
    if (modulePath.empty()) return false;

    if (RegOpenKeyExW(
          HKEY_CURRENT_USER,
          L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
          0,
          KEY_SET_VALUE,
          &key) != ERROR_SUCCESS) {
      return false;
    }

    std::wstring command = L"\"" + modulePath + L"\" --autostart";
    if (command.size() >
        static_cast<size_t>(std::numeric_limits<DWORD>::max()) / sizeof(wchar_t) - 1U) {
      RegCloseKey(key);
      return false;
    }
    const LONG result = RegSetValueExW(
      key,
      kRunValue,
      0,
      REG_SZ,
      reinterpret_cast<const BYTE*>(command.c_str()),
      static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))
    );
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
  }

  const LONG openResult = RegOpenKeyExW(
    HKEY_CURRENT_USER,
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
    0,
    KEY_SET_VALUE,
    &key
  );
  if (openResult != ERROR_SUCCESS) {
    return openResult == ERROR_FILE_NOT_FOUND;
  }

  const LONG result = RegDeleteValueW(key, kRunValue);
  RegCloseKey(key);
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

bool IsAutoStart() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_READ,
        &key) != ERROR_SUCCESS) {
    return false;
  }

  wchar_t value[1024]{};
  DWORD type = 0;
  DWORD bytes = sizeof(value);
  const LONG result = RegQueryValueExW(
    key,
    kRunValue,
    nullptr,
    &type,
    reinterpret_cast<LPBYTE>(value),
    &bytes
  );
  RegCloseKey(key);
  return result == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ);
}

BYTE Premultiply(BYTE channel, BYTE alpha) {
  return static_cast<BYTE>((static_cast<unsigned int>(channel) * alpha + 127U) / 255U);
}

class BitmapLock final {
public:
  BitmapLock(Gdiplus::Bitmap& bitmap, Gdiplus::BitmapData& data)
    : bitmap_(&bitmap), data_(&data) {}

  ~BitmapLock() {
    if (bitmap_) bitmap_->UnlockBits(data_);
  }

  BitmapLock(const BitmapLock&) = delete;
  BitmapLock& operator=(const BitmapLock&) = delete;

private:
  Gdiplus::Bitmap* bitmap_;
  Gdiplus::BitmapData* data_;
};

bool IsExternalAssetUsable(const std::wstring& path) {
  WIN32_FILE_ATTRIBUTE_DATA attributes{};
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes)) {
    return false;
  }
  if ((attributes.dwFileAttributes &
       (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
    return false;
  }

  ULARGE_INTEGER fileSize{};
  fileSize.HighPart = attributes.nFileSizeHigh;
  fileSize.LowPart = attributes.nFileSizeLow;
  return fileSize.QuadPart <= kMaxExternalAssetBytes;
}

bool LoadSpriteFromBitmap(Gdiplus::Bitmap& bitmap, Sprite& result, bool tightCrop) {
  if (bitmap.GetLastStatus() != Gdiplus::Ok) return false;

  const UINT sourceWidth = bitmap.GetWidth();
  const UINT sourceHeight = bitmap.GetHeight();
  if (sourceWidth == 0 || sourceHeight == 0 ||
      sourceWidth > kMaxAssetDimension || sourceHeight > kMaxAssetDimension ||
      static_cast<size_t>(sourceWidth) > kMaxAssetPixels / sourceHeight) {
    return false;
  }

  const size_t pixelCount = static_cast<size_t>(sourceWidth) * sourceHeight;
  const size_t byteCount = pixelCount * 4U;

  Gdiplus::Rect rectangle(0, 0, static_cast<INT>(sourceWidth), static_cast<INT>(sourceHeight));
  Gdiplus::BitmapData data{};
  if (bitmap.LockBits(
        &rectangle,
        Gdiplus::ImageLockModeRead,
        PixelFormat32bppARGB,
        &data) != Gdiplus::Ok) {
    return false;
  }

  BitmapLock lock(bitmap, data);
  if (data.Scan0 == nullptr || data.Stride == 0 ||
      data.Stride == std::numeric_limits<int>::min()) {
    return false;
  }
  const size_t rowBytes = static_cast<size_t>(sourceWidth) * 4U;
  const size_t rowStride = static_cast<size_t>(data.Stride < 0 ? -data.Stride : data.Stride);
  if (rowStride < rowBytes || rowStride > static_cast<size_t>(kMaxAssetDimension) * 4U) {
    return false;
  }

  std::vector<BYTE> pixels;
  try {
    pixels.assign(byteCount, 0);
  } catch (const std::bad_alloc&) {
    return false;
  }
  const int stride = data.Stride;
  const BYTE* base = static_cast<const BYTE*>(data.Scan0);
  for (UINT y = 0; y < sourceHeight; ++y) {
    const BYTE* sourceRow = stride >= 0
      ? base + static_cast<size_t>(y) * rowStride
      : base + static_cast<size_t>(sourceHeight - 1U - y) * rowStride;
    BYTE* destinationRow = pixels.data() + static_cast<size_t>(y) * sourceWidth * 4U;
    std::memcpy(destinationRow, sourceRow, static_cast<size_t>(sourceWidth) * 4U);
  }
  constexpr int backgroundRed = 99;
  constexpr int backgroundGreen = 169;
  constexpr int backgroundBlue = 232;
  constexpr int backgroundTolerance = 24;
  int minX = static_cast<int>(sourceWidth);
  int minY = static_cast<int>(sourceHeight);
  int maxX = -1;
  int maxY = -1;

  for (UINT y = 0; y < sourceHeight; ++y) {
    for (UINT x = 0; x < sourceWidth; ++x) {
      BYTE* pixel = pixels.data() + (static_cast<size_t>(y) * sourceWidth + x) * 4U;
      if (pixel[3] <= 8) {
        continue;
      }
      const int blue = pixel[0];
      const int green = pixel[1];
      const int red = pixel[2];
      const int dr = red - backgroundRed;
      const int dg = green - backgroundGreen;
      const int db = blue - backgroundBlue;
      const int distanceSquared = dr * dr + dg * dg + db * db;

      if (distanceSquared <= backgroundTolerance * backgroundTolerance) {
        pixel[3] = 0;
        pixel[0] = 0;
        pixel[1] = 0;
        pixel[2] = 0;
        continue;
      }

      minX = std::min(minX, static_cast<int>(x));
      minY = std::min(minY, static_cast<int>(y));
      maxX = std::max(maxX, static_cast<int>(x));
      maxY = std::max(maxY, static_cast<int>(y));
    }
  }

  if (maxX < 0 || maxY < 0) return false;

  const int padding = tightCrop ? 0 : 2;
  const int cropX = std::max(0, minX - padding);
  const int cropY = std::max(0, minY - padding);
  const int cropRight = std::min(static_cast<int>(sourceWidth) - 1, maxX + padding);
  const int cropBottom = std::min(static_cast<int>(sourceHeight) - 1, maxY + padding);
  result.width = cropRight - cropX + 1;
  result.height = cropBottom - cropY + 1;
  try {
    result.pixels.assign(static_cast<size_t>(result.width) * result.height * 4U, 0);
  } catch (const std::bad_alloc&) {
    result = {};
    return false;
  }

  for (int y = 0; y < result.height; ++y) {
    for (int x = 0; x < result.width; ++x) {
      const BYTE* sourcePixel = pixels.data() +
        (static_cast<size_t>(cropY + y) * sourceWidth + cropX + x) * 4U;
      BYTE* destinationPixel = result.pixels.data() +
        (static_cast<size_t>(y) * result.width + x) * 4U;
      destinationPixel[3] = sourcePixel[3];
      destinationPixel[0] = Premultiply(sourcePixel[0], sourcePixel[3]);
      destinationPixel[1] = Premultiply(sourcePixel[1], sourcePixel[3]);
      destinationPixel[2] = Premultiply(sourcePixel[2], sourcePixel[3]);
    }
  }

  return true;
}

bool LoadSprite(const std::wstring& path, Sprite& result, bool tightCrop) {
  if (!IsExternalAssetUsable(path)) return false;
  try {
    Gdiplus::Bitmap bitmap(path.c_str(), FALSE);
    return LoadSpriteFromBitmap(bitmap, result, tightCrop);
  } catch (const std::bad_alloc&) {
    result = {};
    return false;
  }
}

bool LoadEmbeddedSprite(HINSTANCE instance, int resourceId, Sprite& result, bool tightCrop) {
  HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
  if (!resource) return false;

  HGLOBAL loaded = LoadResource(instance, resource);
  const DWORD resourceSize = SizeofResource(instance, resource);
  const void* resourceData = loaded ? LockResource(loaded) : nullptr;
  if (!resourceData || resourceSize == 0) return false;

  HGLOBAL streamMemory = GlobalAlloc(GMEM_MOVEABLE, resourceSize);
  if (!streamMemory) return false;

  void* destination = GlobalLock(streamMemory);
  if (!destination) {
    GlobalFree(streamMemory);
    return false;
  }
  std::memcpy(destination, resourceData, resourceSize);
  GlobalUnlock(streamMemory);

  IStream* stream = nullptr;
  if (CreateStreamOnHGlobal(streamMemory, TRUE, &stream) != S_OK) {
    GlobalFree(streamMemory);
    return false;
  }

  bool success = false;
  {
    try {
      Gdiplus::Bitmap bitmap(stream, FALSE);
      success = LoadSpriteFromBitmap(bitmap, result, tightCrop);
    } catch (const std::bad_alloc&) {
      result = {};
      success = false;
    }
  }
  stream->Release();
  return success;
}

Sprite ScaleSprite(const Sprite& source, int targetHeight) {
  Sprite result;
  if (source.width <= 0 || source.height <= 0 || source.pixels.empty()) return result;

  result.height = std::max(1, targetHeight);
  const double scaledWidth =
    static_cast<double>(source.width) * result.height / source.height;
  if (!std::isfinite(scaledWidth) || scaledWidth < 1.0 ||
      scaledWidth > static_cast<double>(kMaxAssetDimension)) {
    return {};
  }
  result.width = std::max(1, static_cast<int>(std::lround(scaledWidth)));
  try {
    result.pixels.assign(static_cast<size_t>(result.width) * result.height * 4U, 0);
  } catch (const std::bad_alloc&) {
    return {};
  }

  for (int y = 0; y < result.height; ++y) {
    const int sourceY = std::min(source.height - 1, y * source.height / result.height);
    for (int x = 0; x < result.width; ++x) {
      const int sourceX = std::min(source.width - 1, x * source.width / result.width);
      const BYTE* sourcePixel = source.pixels.data() +
        (static_cast<size_t>(sourceY) * source.width + sourceX) * 4U;
      BYTE* destinationPixel = result.pixels.data() +
        (static_cast<size_t>(y) * result.width + x) * 4U;
      std::memcpy(destinationPixel, sourcePixel, 4U);
    }
  }
  return result;
}

struct Point2D {
  double x = 0.0;
  double y = 0.0;
};

AnimatedFrame BuildDanceFrame(const Sprite& source, double clockMs) {
  AnimatedFrame frame;
  if (source.width <= 0 || source.height <= 0 || source.pixels.empty()) return frame;

  constexpr double pi = 3.14159265358979323846;
  const double phase = std::fmod(std::max(0.0, clockMs), kDancePeriodMs) / kDancePeriodMs * 2.0 * pi;
  const double centerX = static_cast<double>(source.width) * 0.5;
  const double centerY = static_cast<double>(source.height) * 0.5;

  // Keep the original right-facing sprite and animate it with nearest-neighbor
  // transforms so the pixel-art edges remain crisp.
  const double swayX = std::sin(phase + 0.35) * static_cast<double>(source.width) * 0.04;
  const double bobY = std::sin(phase) * static_cast<double>(source.height) * 0.08;
  const double rotation = std::sin(phase + 0.90) * 6.0 * pi / 180.0;
  const double squash = std::sin(phase * 2.0 + 0.20) * 0.04;
  const double scaleX = 1.0 + squash;
  const double scaleY = 1.0 - squash * 0.85;
  const double cosine = std::cos(rotation);
  const double sine = std::sin(rotation);

  const auto transform = [&](double x, double y) {
    const double localX = (x - centerX) * scaleX;
    const double localY = (y - centerY) * scaleY;
    return Point2D{
      centerX + swayX + cosine * localX - sine * localY,
      centerY + bobY + sine * localX + cosine * localY
    };
  };

  const std::array<Point2D, 4> corners{
    transform(0.0, 0.0),
    transform(static_cast<double>(source.width), 0.0),
    transform(0.0, static_cast<double>(source.height)),
    transform(static_cast<double>(source.width), static_cast<double>(source.height))
  };

  double minX = std::numeric_limits<double>::max();
  double minY = std::numeric_limits<double>::max();
  double maxX = std::numeric_limits<double>::lowest();
  double maxY = std::numeric_limits<double>::lowest();
  for (const Point2D& point : corners) {
    minX = std::min(minX, point.x);
    minY = std::min(minY, point.y);
    maxX = std::max(maxX, point.x);
    maxY = std::max(maxY, point.y);
  }

  frame.offsetX = static_cast<int>(std::floor(minX));
  frame.offsetY = static_cast<int>(std::floor(minY));
  const int right = static_cast<int>(std::ceil(maxX));
  const int bottom = static_cast<int>(std::ceil(maxY));
  frame.width = std::max(1, right - frame.offsetX);
  frame.height = std::max(1, bottom - frame.offsetY);
  frame.pixels.assign(static_cast<size_t>(frame.width) * frame.height * 4U, 0);

  for (int y = 0; y < frame.height; ++y) {
    const double outputY = static_cast<double>(frame.offsetY + y) + 0.5;
    for (int x = 0; x < frame.width; ++x) {
      const double outputX = static_cast<double>(frame.offsetX + x) + 0.5;
      const double deltaX = outputX - (centerX + swayX);
      const double deltaY = outputY - (centerY + bobY);
      const double sourceXUnrounded =
        (cosine * deltaX + sine * deltaY) / scaleX + centerX;
      const double sourceYUnrounded =
        (-sine * deltaX + cosine * deltaY) / scaleY + centerY;
      const int sourceX = static_cast<int>(std::floor(sourceXUnrounded));
      const int sourceY = static_cast<int>(std::floor(sourceYUnrounded));
      if (sourceX < 0 || sourceY < 0 || sourceX >= source.width || sourceY >= source.height) {
        continue;
      }

      const BYTE* sourcePixel = source.pixels.data() +
        (static_cast<size_t>(sourceY) * source.width + sourceX) * 4U;
      BYTE* destinationPixel = frame.pixels.data() +
        (static_cast<size_t>(y) * frame.width + x) * 4U;
      std::memcpy(destinationPixel, sourcePixel, 4U);
    }
  }

  return frame;
}

class LayerSurface {
public:
  ~LayerSurface() { Reset(); }

  bool Ensure(int width, int height) {
    if (width_ == width && height_ == height && bitmap_ && bits_) return true;
    Reset();

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    dc_ = CreateCompatibleDC(nullptr);
    if (!dc_) return false;

    void* rawBits = nullptr;
    bitmap_ = CreateDIBSection(
      dc_,
      &info,
      DIB_RGB_COLORS,
      &rawBits,
      nullptr,
      0
    );
    bits_ = static_cast<BYTE*>(rawBits);
    if (!bitmap_ || !bits_) {
      Reset();
      return false;
    }

    oldObject_ = SelectObject(dc_, bitmap_);
    width_ = width;
    height_ = height;
    return true;
  }

  void Clear() {
    if (bits_ && width_ > 0 && height_ > 0) {
      std::memset(bits_, 0, static_cast<size_t>(width_) * height_ * 4U);
    }
  }

  HDC dc() const { return dc_; }
  HBITMAP bitmap() const { return bitmap_; }
  BYTE* bits() const { return bits_; }
  int width() const { return width_; }
  int height() const { return height_; }

private:
  void Reset() {
    if (dc_ && oldObject_) SelectObject(dc_, oldObject_);
    if (bitmap_) DeleteObject(bitmap_);
    if (dc_) DeleteDC(dc_);
    dc_ = nullptr;
    bitmap_ = nullptr;
    bits_ = nullptr;
    oldObject_ = nullptr;
    width_ = 0;
    height_ = 0;
  }

  HDC dc_ = nullptr;
  HBITMAP bitmap_ = nullptr;
  HGDIOBJ oldObject_ = nullptr;
  BYTE* bits_ = nullptr;
  int width_ = 0;
  int height_ = 0;
};

void DrawAnimatedFrame(
  LayerSurface& surface,
  const AnimatedFrame& frame,
  int destinationX,
  int destinationY
) {
  if (!surface.bits() || frame.width <= 0 || frame.height <= 0) return;

  const int firstX = std::max(0, -destinationX);
  const int firstY = std::max(0, -destinationY);
  const int lastX = std::min(frame.width, surface.width() - destinationX);
  const int lastY = std::min(frame.height, surface.height() - destinationY);
  if (firstX >= lastX || firstY >= lastY) return;

  for (int y = firstY; y < lastY; ++y) {
    for (int x = firstX; x < lastX; ++x) {
      const BYTE* sourcePixel = frame.pixels.data() +
        (static_cast<size_t>(y) * frame.width + x) * 4U;
      if (sourcePixel[3] == 0) continue;
      BYTE* destinationPixel = surface.bits() +
        (static_cast<size_t>(destinationY + y) * surface.width() + destinationX + x) * 4U;
      std::memcpy(destinationPixel, sourcePixel, 4U);
    }
  }
}

class WoodcockApp {
public:
  explicit WoodcockApp(HINSTANCE instance)
    : instance_(instance) {}

  ~WoodcockApp() {
    if (trayAdded_) {
      Shell_NotifyIconW(NIM_DELETE, &tray_);
    }
    if (hotKeyRegistered_ && hwnd_) {
      UnregisterHotKey(hwnd_, kHotKeyId);
    }
  }

  int Run() {
    if (!Initialize()) return 1;

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
  }

private:
  bool Initialize() {
    LoadPreferences();

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = StaticWindowProc;
    windowClass.lpszClassName = kWindowClass;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
      return false;
    }

    hwnd_ = CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
      kWindowClass,
      kApplicationName,
      WS_POPUP,
      0,
      0,
      1,
      1,
      nullptr,
      nullptr,
      instance_,
      this
    );
    if (!hwnd_) return false;

    const std::wstring assetDirectory = ModuleDirectory() + L"\\assets\\";
    for (int index = 0; index < kAssetCount; ++index) {
      const std::wstring assetPath = assetDirectory + kAssetFiles[index];
      const bool loadedExternal = LoadSprite(assetPath, sourceSprites_[index], index == 0);
      if (loadedExternal) {
        scaledSprites_[index] = ScaleSprite(sourceSprites_[index], spriteSize_);
      }
      if (!loadedExternal || scaledSprites_[index].pixels.empty()) {
        if (!LoadEmbeddedSprite(instance_, kAssetResourceIds[index], sourceSprites_[index], index == 0)) {
          std::wstring message = L"assets\\";
          message += kAssetFiles[index];
          message += L"を読み込めませんでした。実行ファイルとassetsフォルダーを同じ場所に置いてください。";
          MessageBoxW(nullptr, message.c_str(), kApplicationName, MB_OK | MB_ICONERROR);
          return false;
        }
        scaledSprites_[index] = ScaleSprite(sourceSprites_[index], spriteSize_);
      }
      if (scaledSprites_[index].pixels.empty()) {
        std::wstring message = L"assets\\";
        message += kAssetFiles[index];
        message += L"を安全に表示できるサイズへ変換できませんでした。";
        MessageBoxW(nullptr, message.c_str(), kApplicationName, MB_OK | MB_ICONERROR);
        return false;
      }
    }

    RefreshWorkArea();
    RefillVariantPool();
    ConfigureCycle(true, GetTickCount64());
    AddTrayIcon();
    hotKeyRegistered_ = RegisterHotKey(hwnd_, kHotKeyId, MOD_CONTROL | MOD_ALT, 'W') == TRUE;
    if (!hotKeyRegistered_) {
      MessageBoxW(
        nullptr,
        L"Ctrl+Alt+Wを登録できませんでした。別のアプリが同じショートカットを使用している可能性があります。",
        kApplicationName,
        MB_OK | MB_ICONWARNING
      );
    }
    SetTimer(hwnd_, kAnimationTimer, kAnimationTimerMs, nullptr);
    Render();
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    return true;
  }

  void LoadPreferences() {
    DWORD storedSize = 0;
    if (ReadRegistryDword(kRegistrySize, storedSize) &&
        (storedSize == 96 || storedSize == 144 || storedSize == 192)) {
      spriteSize_ = static_cast<int>(storedSize);
    } else {
      // Keep startup quiet: use the recommended medium size without showing
      // a first-run selection dialog. The tray menu can change it later.
      spriteSize_ = 144;
      WriteRegistryDword(kRegistrySize, static_cast<DWORD>(spriteSize_));
    }

    DWORD startup = 0;
    if (!ReadRegistryDword(kRegistryStartup, startup)) {
      const int choice = MessageBoxW(
        nullptr,
        L"WindowsにログオンしたときにWoodcock Danceを自動起動しますか？\n\n"
        L"選択は保存され、後からタスクトレイのメニューで変更できます。",
        kApplicationName,
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1
      );
      startup = choice == IDYES ? 1U : 0U;
      const bool startupSaved = SetAutoStart(startup != 0) &&
        WriteRegistryDword(kRegistryStartup, startup);
      if (!startupSaved) {
        MessageBoxW(
          nullptr,
          L"自動起動の設定を保存できませんでした。タスクトレイから再試行できます。",
          kApplicationName,
          MB_OK | MB_ICONWARNING
        );
      }
    } else if (!SetAutoStart(startup != 0)) {
      MessageBoxW(
        nullptr,
        L"保存済みの自動起動設定を反映できませんでした。",
        kApplicationName,
        MB_OK | MB_ICONWARNING
      );
    }
  }

  bool RefreshWorkArea() {
    const RECT previous = workArea_;
    POINT primary{0, 0};
    HMONITOR monitor = MonitorFromPoint(primary, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info{sizeof(info)};
    if (monitor && GetMonitorInfoW(monitor, &info)) {
      workArea_ = info.rcWork;
      return !EqualRect(&previous, &workArea_);
    }
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea_, 0);
    return !EqualRect(&previous, &workArea_);
  }

  void ReconfigureCycleForWorkArea(ULONGLONG now) {
    const double previousElapsed = hidden_
      ? pausedElapsedMs_
      : static_cast<double>(CurrentElapsed(now));
    const ULONGLONG previousDuration = cycleDurationMs_;
    const double progress = previousDuration == 0
      ? 0.0
      : std::clamp(
          previousElapsed / static_cast<double>(previousDuration),
          0.0,
          1.0
        );
    if (!RefreshWorkArea()) return;

    ConfigureCycle(false, now);
    const double newElapsed = progress * static_cast<double>(cycleDurationMs_);
    if (hidden_) {
      pausedElapsedMs_ = newElapsed;
    } else {
      const ULONGLONG elapsed = static_cast<ULONGLONG>(std::max(0.0, newElapsed));
      traverseStart_ = now >= elapsed ? now - elapsed : now;
    }
  }

  void RefillVariantPool() {
    variantPool_.clear();
    for (int index = 1; index < kAssetCount; ++index) {
      variantPool_.push_back(index);
    }
    std::shuffle(variantPool_.begin(), variantPool_.end(), randomEngine_);
    variantCursor_ = 0;
  }

  int NextVariant() {
    if (variantCursor_ >= variantPool_.size()) {
      RefillVariantPool();
    }
    return variantPool_[variantCursor_++];
  }

  int FlockSpan() const {
    if (flockIds_.empty()) return 0;

    int span = scaledSprites_[flockIds_.front()].width;
    for (size_t index = 1; index < flockIds_.size(); ++index) {
      span += kFlockGap + scaledSprites_[flockIds_[index]].width;
    }
    return span;
  }

  int DanceHorizontalPadding() const {
    int largestDimension = 0;
    for (const Sprite& sprite : scaledSprites_) {
      largestDimension = std::max(largestDimension, std::max(sprite.width, sprite.height));
    }
    return std::max(8, static_cast<int>(std::ceil(largestDimension * 0.18)) + 4);
  }

  void ConfigureCycle(bool initial, ULONGLONG now) {
    (void)initial;
    const double visibleStart = static_cast<double>(workArea_.left);
    const double fullExit = static_cast<double>(workArea_.right + FlockSpan() + DanceHorizontalPadding());
    const double visibleDistance = std::max(
      1.0,
      static_cast<double>(workArea_.right - workArea_.left)
    );
    pixelsPerMs_ = visibleDistance / static_cast<double>(kTraverseDurationMs);
    cycleStartX_ = visibleStart;
    cycleEndX_ = fullExit;
    cycleDurationMs_ = std::max(
      1ULL,
      static_cast<ULONGLONG>(std::ceil((cycleEndX_ - cycleStartX_) / pixelsPerMs_))
    );
    traverseStart_ = now;
  }

  double CurrentElapsed(ULONGLONG now) {
    if (now < traverseStart_) {
      traverseStart_ = now;
    }

    while (cycleDurationMs_ > 0 && now - traverseStart_ >= cycleDurationMs_) {
      const ULONGLONG nextCycleStart = traverseStart_ + cycleDurationMs_;
      if (flockIds_.size() < kMaxBirdCount) {
        flockIds_.push_back(NextVariant());
      }
      ConfigureCycle(false, nextCycleStart);
    }

    return static_cast<double>(now - traverseStart_);
  }

  Geometry GeometryAt(double elapsedMs) const {
    Geometry geometry{};
    geometry.x = std::min(
      cycleEndX_,
      cycleStartX_ + std::max(0.0, elapsedMs) * pixelsPerMs_
    );
    return geometry;
  }

  void BeginGroupDance(ULONGLONG now) {
    if (groupMode_ || hidden_ || flockIds_.size() != 1 || scaledSprites_[0].width <= 0) return;
    groupMode_ = true;
    groupEnd_ = now + kGroupDanceMs;
    Render();
  }

  void EndGroupDance() {
    if (!groupMode_) return;
    groupMode_ = false;
    Render();
  }

  void ToggleHidden() {
    if (!hwnd_) return;
    const ULONGLONG now = GetTickCount64();
    if (hidden_) {
      hidden_ = false;
      const ULONGLONG pausedElapsed = static_cast<ULONGLONG>(std::max(0.0, pausedElapsedMs_));
      traverseStart_ = now >= pausedElapsed ? now - pausedElapsed : now;
      ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
      Render();
      return;
    }

    pausedElapsedMs_ = CurrentElapsed(now);
    groupMode_ = false;
    hidden_ = true;
    ShowWindow(hwnd_, SW_HIDE);
  }

  void ChangeSize(int newSize) {
    if (newSize != 96 && newSize != 144 && newSize != 192) return;
    if (spriteSize_ == newSize) return;
    const ULONGLONG now = GetTickCount64();
    const double currentX = hidden_ || cycleDurationMs_ == 0
      ? GeometryAt(pausedElapsedMs_).x
      : GeometryAt(CurrentElapsed(now)).x;
    spriteSize_ = newSize;
    WriteRegistryDword(kRegistrySize, static_cast<DWORD>(spriteSize_));
    groupMode_ = false;
    for (int index = 0; index < kAssetCount; ++index) {
      scaledSprites_[index] = ScaleSprite(sourceSprites_[index], spriteSize_);
    }
    ConfigureCycle(false, now);
    cycleStartX_ = std::clamp(currentX, cycleStartX_, cycleEndX_);
    cycleDurationMs_ = std::max(
      1ULL,
      static_cast<ULONGLONG>(std::ceil((cycleEndX_ - cycleStartX_) / pixelsPerMs_))
    );
    if (hidden_) pausedElapsedMs_ = 0.0;
    Render();
  }

  void AddTrayIcon() {
    tray_ = {};
    tray_.cbSize = sizeof(tray_);
    tray_.hWnd = hwnd_;
    tray_.uID = 1;
    tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    tray_.uCallbackMessage = kTrayMessage;
    tray_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpynW(tray_.szTip, kApplicationName, ARRAYSIZE(tray_.szTip));
    trayAdded_ = Shell_NotifyIconW(NIM_ADD, &tray_) == TRUE;
  }

  void ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenuW(menu, MF_STRING, kMenuToggle, hidden_ ? L"表示" : L"非表示");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (spriteSize_ == 96 ? MF_CHECKED : 0), kMenuSmall, L"小さい（96px）");
    AppendMenuW(menu, MF_STRING | (spriteSize_ == 144 ? MF_CHECKED : 0), kMenuMedium, L"中くらい（144px）");
    AppendMenuW(menu, MF_STRING | (spriteSize_ == 192 ? MF_CHECKED : 0), kMenuLarge, L"目立つ（192px）");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (IsAutoStart() ? MF_CHECKED : 0), kMenuStartup, L"Windows起動時に自動起動");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"終了");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, hwnd_, nullptr);
    PostMessageW(hwnd_, WM_NULL, 0, 0);
    DestroyMenu(menu);
  }

  bool HitTest(POINT screenPoint) const {
    POINT local = screenPoint;
    ScreenToClient(hwnd_, &local);
    return local.x >= 0 && local.y >= 0 &&
      local.x < surface_.width() && local.y < surface_.height();
  }

  void Render() {
    if (!hwnd_ || hidden_ || flockIds_.empty() || scaledSprites_[0].pixels.empty()) return;

    const ULONGLONG now = GetTickCount64();
    const double elapsed = CurrentElapsed(now);
    const Geometry geometry = GeometryAt(elapsed);
    std::array<AnimatedFrame, kAssetCount> frames{};
    for (int index = 0; index < kAssetCount; ++index) {
      frames[index] = BuildDanceFrame(scaledSprites_[index], static_cast<double>(now));
    }

    struct RenderItem {
      int assetIndex = 0;
      int worldX = 0;
    };

    std::vector<RenderItem> items;
    items.reserve(groupMode_ ? kGroupBirdCount : flockIds_.size());
    int worldMinX = std::numeric_limits<int>::max();
    int worldMaxX = std::numeric_limits<int>::lowest();
    int windowHeight = 0;

    const auto addItem = [&](int assetIndex, int anchorX) {
      const AnimatedFrame& frame = frames[assetIndex];
      if (frame.pixels.empty()) return;
      const int worldX = anchorX + frame.offsetX;
      items.push_back(RenderItem{assetIndex, worldX});
      worldMinX = std::min(worldMinX, worldX);
      worldMaxX = std::max(worldMaxX, worldX + frame.width);
      windowHeight = std::max(windowHeight, frame.height);
    };

    const int baseX = static_cast<int>(std::lround(geometry.x));
    if (groupMode_) {
      const int birdStep = scaledSprites_[0].width + kGroupGap;
      for (int index = 0; index < kGroupBirdCount; ++index) {
        addItem(0, baseX + (index - 1) * birdStep);
      }
    } else {
      int followerAnchorX = baseX;
      for (size_t index = 0; index < flockIds_.size(); ++index) {
        const int assetIndex = flockIds_[index];
        if (index > 0) {
          followerAnchorX -= kFlockGap + scaledSprites_[assetIndex].width;
        }
        addItem(assetIndex, followerAnchorX);
      }
    }

    if (items.empty() || worldMinX >= worldMaxX || windowHeight <= 0) return;

    const int windowX = worldMinX;
    const int windowWidth = worldMaxX - worldMinX;
    if (!surface_.Ensure(windowWidth, windowHeight)) return;
    surface_.Clear();

    for (const RenderItem& item : items) {
      const AnimatedFrame& frame = frames[item.assetIndex];
      const int destinationX = item.worldX - windowX;
      const int destinationY = windowHeight - frame.height;
      DrawAnimatedFrame(surface_, frame, destinationX, destinationY);
    }

    const int windowY = workArea_.bottom - kBottomMargin - windowHeight;
    POINT destination{windowX, windowY};
    SIZE size{windowWidth, windowHeight};
    POINT sourcePoint{0, 0};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    HDC screen = GetDC(nullptr);
    if (screen) {
      UpdateLayeredWindow(
        hwnd_,
        screen,
        &destination,
        &size,
        surface_.dc(),
        &sourcePoint,
        0,
        &blend,
        ULW_ALPHA
      );
      ReleaseDC(nullptr, screen);
    }
    SetWindowPos(hwnd_, HWND_TOPMOST, windowX, windowY, windowWidth, windowHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
  }

  void OnTimer() {
    if (hidden_) return;
    const ULONGLONG now = GetTickCount64();
    if (groupMode_ && now >= groupEnd_) {
      EndGroupDance();
      return;
    }
    ReconfigureCycleForWorkArea(now);
    Render();
  }

  LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
      case WM_NCHITTEST: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        return HitTest(point) ? HTCLIENT : HTTRANSPARENT;
      }
      case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
      case WM_LBUTTONUP:
        BeginGroupDance(GetTickCount64());
        return 0;
      case WM_RBUTTONUP:
      case WM_CONTEXTMENU:
        ShowTrayMenu();
        return 0;
      case WM_TIMER:
        if (wParam == kAnimationTimer) OnTimer();
        return 0;
      case WM_HOTKEY:
        if (wParam == kHotKeyId) ToggleHidden();
        return 0;
      case WM_COMMAND:
        switch (LOWORD(wParam)) {
          case kMenuToggle:
            ToggleHidden();
            return 0;
          case kMenuSmall:
            ChangeSize(96);
            return 0;
          case kMenuMedium:
            ChangeSize(144);
            return 0;
          case kMenuLarge:
            ChangeSize(192);
            return 0;
          case kMenuStartup: {
            const bool enabled = !IsAutoStart();
            if (SetAutoStart(enabled) &&
                WriteRegistryDword(kRegistryStartup, enabled ? 1U : 0U)) {
              return 0;
            }
            MessageBoxW(
              nullptr,
              L"自動起動の設定を変更できませんでした。",
              kApplicationName,
              MB_OK | MB_ICONWARNING
            );
            return 0;
          }
          case kMenuExit:
            DestroyWindow(hwnd_);
            return 0;
          default:
            break;
        }
        break;
      case WM_DISPLAYCHANGE:
      case WM_SETTINGCHANGE:
      case WM_DPICHANGED:
        ReconfigureCycleForWorkArea(GetTickCount64());
        Render();
        return 0;
      case kTrayMessage:
        if (lParam == WM_RBUTTONUP) {
          ShowTrayMenu();
          return 0;
        }
        if (lParam == WM_LBUTTONDBLCLK) {
          ToggleHidden();
          return 0;
        }
        break;
      case WM_ERASEBKGND:
        return 1;
      case WM_DESTROY:
        KillTimer(hwnd_, kAnimationTimer);
        PostQuitMessage(0);
        return 0;
      default:
        break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
  }

  static LRESULT CALLBACK StaticWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    WoodcockApp* app = reinterpret_cast<WoodcockApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
      const CREATESTRUCTW* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
      app = reinterpret_cast<WoodcockApp*>(create->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
      app->hwnd_ = hwnd;
    }
    return app ? app->HandleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
  }

  HINSTANCE instance_ = nullptr;
  HWND hwnd_ = nullptr;
  NOTIFYICONDATAW tray_{};
  bool trayAdded_ = false;
  bool hotKeyRegistered_ = false;
  bool hidden_ = false;
  bool groupMode_ = false;
  ULONGLONG groupEnd_ = 0;
  ULONGLONG traverseStart_ = 0;
  ULONGLONG cycleDurationMs_ = 0;
  double cycleStartX_ = 0.0;
  double cycleEndX_ = 0.0;
  double pixelsPerMs_ = 0.0;
  double pausedElapsedMs_ = 0.0;
  int spriteSize_ = 144;
  RECT workArea_{0, 0, 0, 0};
  std::array<Sprite, kAssetCount> sourceSprites_{};
  std::array<Sprite, kAssetCount> scaledSprites_{};
  std::vector<int> flockIds_{0};
  std::vector<int> variantPool_;
  size_t variantCursor_ = 0;
  std::mt19937 randomEngine_{static_cast<std::mt19937::result_type>(GetTickCount())};
  LayerSurface surface_;
};

void SetPerMonitorDpiAwareness() {
  using SetProcessDpiAwarenessContextFunction = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  if (!user32) return;

  auto function = reinterpret_cast<SetProcessDpiAwarenessContextFunction>(
    GetProcAddress(user32, "SetProcessDpiAwarenessContext")
  );
  if (function) function(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  HANDLE instanceMutex = CreateMutexW(nullptr, TRUE, kSingleInstanceMutex);
  if (!instanceMutex) return 1;
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(instanceMutex);
    return 0;
  }
  if (FindWindowW(kWindowClass, nullptr)) {
    CloseHandle(instanceMutex);
    return 0;
  }

  SetPerMonitorDpiAwareness();

  Gdiplus::GdiplusStartupInput startupInput;
  ULONG_PTR gdiplusToken = 0;
  if (Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr) != Gdiplus::Ok) {
    CloseHandle(instanceMutex);
    return 1;
  }

  WoodcockApp application(instance);
  const int result = application.Run();
  Gdiplus::GdiplusShutdown(gdiplusToken);
  CloseHandle(instanceMutex);
  return result;
}
