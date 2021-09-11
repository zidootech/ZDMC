/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#define UTILS_USE_RTK 1

#if UTILS_USE_RTK
#include "AndroidUtilsRtk.h"
#else
#include "AndroidUtils.h"
#endif
#include "rendering/gles/RenderSystemGLES.h"
#include "system_egl.h"
#include "threads/CriticalSection.h"
#include "threads/Timer.h"
#include "windowing/WinSystem.h"

class CDecoderFilterManager;
class IDispResource;

class CWinSystemAndroid : public CWinSystemBase, public ITimerCallback
{
public:
  CWinSystemAndroid();
  ~CWinSystemAndroid() override;

  bool InitWindowSystem() override;
  bool DestroyWindowSystem() override;

  bool CreateNewWindow(const std::string& name,
                       bool fullScreen,
                       RESOLUTION_INFO& res) override;

  bool DestroyWindow() override;
  void UpdateResolutions() override;

  void InitiateModeChange();
  bool IsHdmiModeTriggered() const { return m_HdmiModeTriggered; };
  void SetHdmiState(bool connected);

  void UpdateDisplayModes();

  bool HasCursor() override { return false; };

  bool Hide() override;
  bool Show(bool raise = true) override;
  void Register(IDispResource *resource) override;
  void Unregister(IDispResource *resource) override;

  void MessagePush(XBMC_Event *newEvent);

  // winevents override
  bool MessagePump() override;
  bool IsHDRDisplay() override;

protected:
  std::unique_ptr<KODI::WINDOWING::IOSScreenSaver> GetOSScreenSaverImpl() override;
  void OnTimeout() override;

#if UTILS_USE_RTK
  CAndroidUtilsRtk *m_android;
#else
  CAndroidUtils *m_android;
#endif

  EGLDisplay m_nativeDisplay;
  EGLNativeWindowType m_nativeWindow;

  int m_displayWidth;
  int m_displayHeight;

  RENDER_STEREO_MODE m_stereo_mode;

  CTimer *m_dispResetTimer;

  CCriticalSection m_resourceSection;
  std::vector<IDispResource*> m_resources;
  CDecoderFilterManager *m_decoderFilterManager;

private:
  bool m_HdmiModeTriggered = false;
  void UpdateResolutions(bool bUpdateDesktopRes);
};
