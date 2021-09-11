/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "WinSystemAndroid.h"

#include "OSScreenSaverAndroid.h"
#include "ServiceBroker.h"
#include "WinEventsAndroid.h"
#include "addons/interfaces/platform/android/System.h"
#include "cores/RetroPlayer/process/android/RPProcessInfoAndroid.h"
#include "cores/RetroPlayer/rendering/VideoRenderers/RPRendererOpenGLES.h"
#include "cores/VideoPlayer/DVDCodecs/Audio/DVDAudioCodecAndroidMediaCodec.h"
#include "cores/VideoPlayer/DVDCodecs/Video/DVDVideoCodecAndroidMediaCodec.h"
#include "cores/VideoPlayer/Process/android/ProcessInfoAndroid.h"
#include "cores/VideoPlayer/VideoRenderers/HwDecRender/RendererMediaCodec.h"
#include "cores/VideoPlayer/VideoRenderers/HwDecRender/RendererMediaCodecSurface.h"
#include "guilib/DispResource.h"
#include "rendering/gles/ScreenshotSurfaceGLES.h"
#include "settings/DisplaySettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "system_egl.h"
#include "threads/SingleLock.h"
#include "utils/log.h"
#include "windowing/GraphicContext.h"
#include "windowing/Resolution.h"

#include "platform/android/activity/XBMCApp.h"
#include "platform/android/media/decoderfilter/MediaCodecDecoderFilterManager.h"
#include "platform/android/media/drm/MediaDrmCryptoSession.h"

#include <float.h>
#include <string.h>

#include <EGL/eglplatform.h>

using namespace KODI;

CWinSystemAndroid::CWinSystemAndroid()
{
  m_nativeDisplay = EGL_NO_DISPLAY;
  m_nativeWindow = nullptr;

  m_displayWidth = 0;
  m_displayHeight = 0;

  m_stereo_mode = RENDER_STEREO_MODE_OFF;

  m_dispResetTimer = new CTimer(this);

  m_android = nullptr;

  m_winEvents.reset(new CWinEventsAndroid());
}

CWinSystemAndroid::~CWinSystemAndroid()
{
  m_nativeWindow = nullptr;
  delete m_dispResetTimer, m_dispResetTimer = nullptr;
}

bool CWinSystemAndroid::InitWindowSystem()
{
  m_nativeDisplay = EGL_DEFAULT_DISPLAY;
#if UTILS_USE_RTK
  m_android = new CAndroidUtilsRtk();
#else
  m_android = new CAndroidUtils();
#endif
  m_decoderFilterManager = new(CMediaCodecDecoderFilterManager);
  CServiceBroker::RegisterDecoderFilterManager(m_decoderFilterManager);

  CDVDVideoCodecAndroidMediaCodec::Register();
  CDVDAudioCodecAndroidMediaCodec::Register();

  CLinuxRendererGLES::Register();
  RETRO::CRPProcessInfoAndroid::Register();
  RETRO::CRPProcessInfoAndroid::RegisterRendererFactory(new RETRO::CRendererFactoryOpenGLES);
  CRendererMediaCodec::Register();
  CRendererMediaCodecSurface::Register();
  ADDON::Interface_Android::Register();
  DRM::CMediaDrmCryptoSession::Register();
  VIDEOPLAYER::CProcessInfoAndroid::Register();

  CScreenshotSurfaceGLES::Register();

  return CWinSystemBase::InitWindowSystem();
}

bool CWinSystemAndroid::DestroyWindowSystem()
{
  CLog::Log(LOGINFO, "CWinSystemAndroid::%s", __FUNCTION__);

  delete m_android;
  m_android = nullptr;

  CServiceBroker::RegisterDecoderFilterManager(nullptr);
  delete m_decoderFilterManager;
  m_decoderFilterManager = nullptr;

  return true;
}

bool CWinSystemAndroid::CreateNewWindow(const std::string& name,
                                    bool fullScreen,
                                    RESOLUTION_INFO& res)
{
  RESOLUTION_INFO current_resolution;
  current_resolution.iWidth = current_resolution.iHeight = 0;
  RENDER_STEREO_MODE stereo_mode = CServiceBroker::GetWinSystem()->GetGfxContext().GetStereoMode();

  m_nWidth        = res.iWidth;
  m_nHeight       = res.iHeight;
  m_displayWidth  = res.iScreenWidth;
  m_displayHeight = res.iScreenHeight;
  m_fRefreshRate  = res.fRefreshRate;

  if ((m_bWindowCreated && m_android->GetNativeResolution(&current_resolution)) &&
    current_resolution.iWidth == res.iWidth && current_resolution.iHeight == res.iHeight &&
    current_resolution.iScreenWidth == res.iScreenWidth && current_resolution.iScreenHeight == res.iScreenHeight &&
    m_bFullScreen == fullScreen && current_resolution.fRefreshRate == res.fRefreshRate &&
    (current_resolution.dwFlags & D3DPRESENTFLAG_MODEMASK) == (res.dwFlags & D3DPRESENTFLAG_MODEMASK) &&
    m_stereo_mode == stereo_mode)
  {
    CLog::Log(LOGDEBUG, "CWinSystemAndroid::CreateNewWindow: No need to create a new window");
    return true;
  }

  m_dispResetTimer->Stop();
  m_HdmiModeTriggered = false;

  m_stereo_mode = stereo_mode;
  m_bFullScreen = fullScreen;

  m_nativeWindow = CXBMCApp::GetNativeWindow(2000);
  if (!m_nativeWindow)
  {
    CLog::Log(LOGERROR, "CWinSystemAndroid::CreateNewWindow: failed");
    return false;
  }
  m_android->SetNativeResolution(res);

  return true;
}

bool CWinSystemAndroid::DestroyWindow()
{
  CLog::Log(LOGINFO, "CWinSystemAndroid::%s", __FUNCTION__);
  m_nativeWindow = nullptr;
  m_bWindowCreated = false;
  return true;
}

void CWinSystemAndroid::UpdateResolutions()
{
  UpdateResolutions(true);
}

void CWinSystemAndroid::UpdateResolutions(bool bUpdateDesktopRes)
{
  CWinSystemBase::UpdateResolutions();

  std::vector<RESOLUTION_INFO> resolutions;
  if (!m_android->ProbeResolutions(resolutions) || resolutions.empty())
  {
    CLog::Log(LOGWARNING, "CWinSystemAndroid::%s failed.", __FUNCTION__);
  }

  const RESOLUTION_INFO resWindow = CDisplaySettings::GetInstance().GetResolutionInfo(RES_WINDOW);

  RESOLUTION_INFO resDesktop;
  if (bUpdateDesktopRes)
  {
    // ProbeResolutions includes already all resolutions.
    // Only get desktop resolution so we can replace Kodi's desktop res.
    RESOLUTION_INFO curDisplay;
    if (m_android->GetNativeResolution(&curDisplay))
      resDesktop = curDisplay;
  }
  else
  {
    // Do not replace Kodi's desktop res, just update the data.
    resDesktop = CDisplaySettings::GetInstance().GetResolutionInfo(RES_DESKTOP);
  }

  CDisplaySettings::GetInstance().ClearCustomResolutions();

  for (auto& res : resolutions)
  {
    CServiceBroker::GetWinSystem()->GetGfxContext().ResetOverscan(res);
    CDisplaySettings::GetInstance().AddResolutionInfo(res);

    if (resDesktop.iScreenWidth == res.iScreenWidth &&
        resDesktop.iScreenHeight == res.iScreenHeight &&
        (resDesktop.dwFlags & D3DPRESENTFLAG_MODEMASK) == (res.dwFlags & D3DPRESENTFLAG_MODEMASK) &&
        std::fabs(resDesktop.fRefreshRate - res.fRefreshRate) < FLT_EPSILON)
    {
      CDisplaySettings::GetInstance().GetResolutionInfo(RES_DESKTOP) = res;
    }

    if (resWindow.iScreenWidth == res.iScreenWidth &&
        resWindow.iScreenHeight == res.iScreenHeight &&
        (resWindow.dwFlags & D3DPRESENTFLAG_MODEMASK) == (res.dwFlags & D3DPRESENTFLAG_MODEMASK) &&
        std::fabs(resWindow.fRefreshRate - res.fRefreshRate) < FLT_EPSILON)
    {
      CDisplaySettings::GetInstance().GetResolutionInfo(RES_WINDOW) = res;
    }
  }

  CDisplaySettings::GetInstance().ApplyCalibrations();
}

void CWinSystemAndroid::OnTimeout()
{
  m_HdmiModeTriggered = true;
}

void CWinSystemAndroid::InitiateModeChange()
{
  int delay = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(
                  "videoscreen.delayrefreshchange") *
              100;

  if (delay < 2000)
    delay = 2000;
  m_dispResetTimer->Stop();
  m_dispResetTimer->Start(delay);

  SetHdmiState(false);
}

void CWinSystemAndroid::SetHdmiState(bool connected)
{
  CSingleLock lock(m_resourceSection);
  CLog::Log(LOGDEBUG, "CWinSystemAndroid::SetHdmiState: state: %d", static_cast<int>(connected));

  if (connected)
  {
    if (m_dispResetTimer->IsRunning())
    {
      // We stop the timer if OS supports HDMI_AUDIO_PLUG intent
      // and configured delay is smaller than the time HDMI_PLUG took.
      // Note that timer is always started with minimum of 2 seconds
      // regardless if the configured delay is smaller
      if (m_dispResetTimer->GetElapsedMilliseconds() >=
          CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(
              "videoscreen.delayrefreshchange") *
              100)
        m_dispResetTimer->Stop();
      else
        return;
    }

    for (auto resource : m_resources)
      resource->OnResetDisplay();
    m_dispResetTimer->Stop();
    m_HdmiModeTriggered = false;
  }
  else
  {
    for (auto resource : m_resources)
      resource->OnLostDisplay();
  }
}

void CWinSystemAndroid::UpdateDisplayModes()
{
  // re-fetch display modes
  m_android->UpdateDisplayModes();

  if (m_nativeWindow)
  {
    // update display settings
    UpdateResolutions(false);
  }
}

bool CWinSystemAndroid::Hide()
{
  return false;
}

bool CWinSystemAndroid::Show(bool raise)
{
  return false;
}

void CWinSystemAndroid::Register(IDispResource *resource)
{
  CSingleLock lock(m_resourceSection);
  m_resources.push_back(resource);
}

void CWinSystemAndroid::Unregister(IDispResource *resource)
{
  CSingleLock lock(m_resourceSection);
  std::vector<IDispResource*>::iterator i = find(m_resources.begin(), m_resources.end(), resource);
  if (i != m_resources.end())
    m_resources.erase(i);
}

void CWinSystemAndroid::MessagePush(XBMC_Event *newEvent)
{
  dynamic_cast<CWinEventsAndroid&>(*m_winEvents).MessagePush(newEvent);
}

bool CWinSystemAndroid::MessagePump()
{
  return m_winEvents->MessagePump();
}

bool CWinSystemAndroid::IsHDRDisplay()
{
  return m_android->IsHDRDisplay();
}

std::unique_ptr<WINDOWING::IOSScreenSaver> CWinSystemAndroid::GetOSScreenSaverImpl()
{
  std::unique_ptr<KODI::WINDOWING::IOSScreenSaver> ret(new COSScreenSaverAndroid());
  return ret;
}
