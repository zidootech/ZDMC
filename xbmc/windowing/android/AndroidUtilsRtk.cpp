/*
 *  Copyright (C) 2011-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */
#include <stdlib.h>

#include <androidjni/SystemProperties.h>
#include <androidjni/Display.h>
#include <androidjni/View.h>
#include <androidjni/Window.h>
#include <androidjni/WindowManager.h>
#include <androidjni/Build.h>
#include <androidjni/System.h>

#include <EGL/egl.h>

#include <cmath>

#include "AndroidUtilsRtk.h"

#include "windowing/GraphicContext.h"
#include "utils/log.h"

#include "settings/Settings.h"
#include "settings/DisplaySettings.h"
#include "settings/SettingsComponent.h"
#include "settings/lib/SettingsManager.h"

#include "ServiceBroker.h"
#include "utils/StringUtils.h"
#include "platform/android/activity/XBMCApp.h"


enum {
    RES_AUTO = 0,
    RES_NTSC,
    RES_PAL,
    RES_480P_60HZ,
    RES_576P_50HZ,
    RES_720P_50HZ, //5
    RES_720P_60HZ,
    RES_1080I_50HZ,
    RES_1080I_60HZ,
    RES_1080P_50HZ,
    RES_1080P_60HZ, //10
    RES_3840X2160P_24HZ,
    RES_3840X2160P_25HZ,
    RES_3840X2160P_30HZ,
    RES_4096X2160P_24HZ,
    RES_1080P_24HZ, //15
    RES_720P_59HZ,
    RES_1080I_59HZ,
    RES_1080P_23HZ,
    RES_1080P_59HZ,
    RES_3840X2160P_23HZ, //20
    RES_3840X2160P_29HZ,
    RES_3840X2160P_60HZ,
    // New supported from nuplayer version
    RES_3840X2160P_50HZ,
    RES_4096X2160P_50HZ,
    RES_4096X2160P_60HZ, //25
    RES_4096X2160P_25HZ,
    RES_4096X2160P_30HZ,
    RES_3840X2160P_59HZ,
    RES_1080P_25HZ,
    RES_1080P_30HZ, //30
    RES_RTK_MAX,
    RES_720P_25HZ, // will set to 720p@50hz
    RES_720P_30HZ, // will set to 720p@60hz
    RES_1080I_25HZ, // will set to 1080i@50hz
    RES_1080I_30HZ, // will set to 1080i@60hz
    RES_MAX
};

typedef struct rtk_resolution_s {
    int width;
    int height;
    int interlaced;
    float fps;
} rtk_resolution_t;

// width, height, p/i(0/1), refresh reate
static rtk_resolution_t rtk_resolutions[] = {
  {0, 0, 0, 0}, // Auto detect
  {720, 480, 1, 60.0f}, // NTSC
  {720, 576, 1, 50.0f}, // PAL
  {720, 480, 0, 60.0f}, // 480P
  {720, 576, 0, 50.0f}, // 576P
  {1280, 720, 0, 50.0f}, // 720P@50HZ
  {1280, 720, 0, 60.0f}, // 720P@60HZ
  {1920, 1080, 1, 50.0f}, // 1080I@50HZ
  {1920, 1080, 1, 60.0f}, // 1080I@60HZ
  {1920, 1080, 0, 50.0f}, // 1080P@50HZ
  {1920, 1080, 0, 60.0f}, // 1080P@60HZ
  {3840, 2160, 0, 24.0f}, // 3840X2160P@24HZ
  {3840, 2160, 0, 25.0f}, // 3840X2160P@25HZ
  {3840, 2160, 0, 30.0f}, // 3840X2160P@30HZ
  {4096, 2160, 0, 24.0f}, // 4096X2160P@24HZ
  {1920, 1080, 0, 24.0f}, // 1080P@24HZ
  {1280, 720, 0, 59.94f},  // 720P@59HZ
  {1920, 1080, 1, 59.94f}, // 1080I@59HZ
  {1920, 1080, 0, 23.976f}, // 1080P@23HZ
  {1920, 1080, 0, 59.94f}, // 1080P@59HZ
  {3840, 2160, 0, 23.976f}, // 3840X2160P@23HZ
  {3840, 2160, 0, 29.97}, // 3840X2160P@29HZ
  {3840, 2160, 0, 60.0f},  // 3840X2160P@60HZ
  // New supported from nuplayer version
  {3840, 2160, 0, 50.0f},  // 3840X2160P@50HZ
  {4096, 2160, 0, 50.0f},  // 4096X2160P@50HZ
  {4096, 2160, 0, 60.0f},  // 4096X2160P@60HZ
  {4096, 2160, 0, 25.0f},  // 4096X2160P@25HZ
  {4096, 2160, 0, 30.0f},  // 4096X2160P@30HZ
  {3840, 2160, 0, 59.94f}, // 3840X2160P@59HZ
  {1920, 1080, 0, 25.0f},  // 1080P@25HZ
  {1920, 1080, 0, 30.0f},  // 1080P@30HZ
  {0, 0, 0, 0.0f},
  {1280, 720, 0, 25.0f},  //720p@25hz, will set to 720p@50hz
  {1280, 720, 0, 30.0f},  //720p@30hz, will set to 720p@60hz
  {1920, 1080, 1, 25.0f}, //1080i@25hz,  will set to 1080i@50hz
  {1920, 1080, 1, 30.0f}  //1080i@30HZ,  will set to 1080i@60hz
};

static const char *resolution_strings[] = {
  "AUTO",
  "NTSC",
  "PAL",
  "480P",
  "576P",
  "720P @ 50Hz",
  "720P @ 60Hz",
  "1080I @ 50Hz",
  "1080I @ 60Hz",
  "1080P @ 50Hz",
  "1080P @ 60Hz",
  "3840x2160P @ 24Hz",
  "3840x2160P @ 25Hz",
  "3840x2160P @ 30Hz",
  "4096x2160P @ 24Hz",
  "1080P @ 24Hz",
  "720P @ 59Hz",
  "1080I @ 59Hz",
  "1080P @ 23Hz",
  "1080P @ 59Hz",
  "3840x2160P @ 23Hz",
  "3840x2160P @ 29Hz",
  "3840x2160P @ 60Hz",
  // New supported from nuplayer version
  "3840x2160P @ 50Hz",
  "4096x2160P @ 50Hz",
  "4096x2160P @ 60Hz",
  "4096x2160P @ 25Hz",
  "4096x2160P @ 30Hz",
  "3840x2160P @ 59Hz",
  "1080P @ 25Hz",
  "1080P @ 30Hz",
  "RES_RTK_MAX",
  "720P @ 25HZ",
  "720P @ 30HZ",
  "1080I @ 25HZ",
  "1080I @ 30HZ"
};

static int supported_resolutions[RES_MAX];

static bool s_hasModeApi = false;
static std::vector<RESOLUTION_INFO> s_res_displayModes;
static RESOLUTION_INFO s_res_cur_displayMode;
static RESOLUTION_INFO m_fb_res; // frame buffer

static float currentRefreshRate()
{
  if (s_hasModeApi)
    return s_res_cur_displayMode.fRefreshRate;

  CJNIWindow window = CXBMCApp::getWindow();
  if (window)
  {
    float preferredRate = window.getAttributes().getpreferredRefreshRate();
    if (preferredRate > 20.0 && preferredRate < 70.0)
    {
      CLog::Log(LOGINFO, "CAndroidUtilsRtk: Preferred refresh rate: %f", preferredRate);
      return preferredRate;
    }
    CJNIView view(window.getDecorView());
    if (view) {
      CJNIDisplay display(view.getDisplay());
      if (display)
      {
        float reportedRate = display.getRefreshRate();
        if (reportedRate > 20.0 && reportedRate < 70.0)
        {
          CLog::Log(LOGINFO, "CAndroidUtilsRtk: Current display refresh rate: %f", reportedRate);
          return reportedRate;
        }
      }
    }
  }
  CLog::Log(LOGDEBUG, "CAndroidUtilsRtk:currentRefreshRate found no refresh rate");
  return 60.0;
}

static bool sysModeToResolution(int resolution, RESOLUTION_INFO *res)
{
  if (!res)
    return false;

  res->iWidth = 0;
  res->iHeight= 0;

  if (resolution < RES_NTSC|| resolution > RES_MAX - 1)
    return false;

  int w = rtk_resolutions[resolution].width;
  int h = rtk_resolutions[resolution].height;
  int p = rtk_resolutions[resolution].interlaced;
  float r = rtk_resolutions[resolution].fps;

  res->iWidth = w;
  res->iHeight= h;
  res->iScreenWidth = w;
  res->iScreenHeight= h;
  res->fRefreshRate = r;
  res->dwFlags = (p == 0) ? D3DPRESENTFLAG_PROGRESSIVE : D3DPRESENTFLAG_INTERLACED;

  res->bFullScreen   = true;
  res->iSubtitles    = (int)(0.965 * res->iHeight);
  res->fPixelRatio   = 1.0f;
  res->strMode       = StringUtils::Format("%dx%d @ %.2f%s - Full Screen", res->iScreenWidth, res->iScreenHeight, res->fRefreshRate,
                                           res->dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");
  std::string resolutionStr(resolution_strings[resolution]);
  CLog::Log(LOGDEBUG, "CAndroidUtilsRtk:sysModeToResolution, resolution = %s", resolutionStr.c_str());
  res->strId         = resolutionStr;

  return res->iWidth > 0 && res->iHeight> 0;
}

static void GuessResolution(int mode, std::vector<RESOLUTION_INFO> &resolutions)
{
  RESOLUTION_INFO res;
  //CLog::Log(LOGDEBUG, "CAndroidUtilsRtk:GuessResolution, mode = %d", mode);
  switch (mode) {
    case RES_720P_50HZ:
      if (sysModeToResolution(RES_720P_25HZ, &res) && (supported_resolutions[RES_720P_25HZ] == 0)) {
          res.iWidth = m_fb_res.iWidth;
          res.iHeight = m_fb_res.iHeight;
          res.strId = resolution_strings[RES_720P_25HZ];
          resolutions.push_back(res);
          supported_resolutions[RES_720P_25HZ]  = 1;
      }
      break;
    case RES_720P_60HZ:
      if (sysModeToResolution(RES_720P_30HZ, &res) && (supported_resolutions[RES_720P_30HZ] == 0)) {
          res.iWidth = m_fb_res.iWidth;
          res.iHeight = m_fb_res.iHeight;
          res.strId = resolution_strings[RES_720P_30HZ];
          resolutions.push_back(res);
          supported_resolutions[RES_720P_30HZ] = 1;
      }
      break;
   case RES_1080I_50HZ:
      if (sysModeToResolution(RES_1080I_25HZ, &res) && (supported_resolutions[RES_1080I_25HZ] == 0)) {
          res.iWidth = m_fb_res.iWidth;
          res.iHeight = m_fb_res.iHeight;
          res.strId = resolution_strings[RES_1080I_25HZ];
          resolutions.push_back(res);
          supported_resolutions[RES_1080I_25HZ]  = 1;
      }
      break;
    case RES_1080I_60HZ:
      if (sysModeToResolution(RES_1080I_30HZ, &res) && (supported_resolutions[RES_1080I_30HZ] == 0)) {
          res.iWidth = m_fb_res.iWidth;
          res.iHeight = m_fb_res.iHeight;
          res.strId = resolution_strings[RES_1080I_30HZ];
          resolutions.push_back(res);
          supported_resolutions[RES_1080I_30HZ] = 1;
      }
      break;
    case RES_1080P_24HZ:
      if (sysModeToResolution(RES_1080P_23HZ, &res) && (supported_resolutions[RES_1080P_23HZ] == 0)) {
          res.iWidth = m_fb_res.iWidth;
          res.iHeight = m_fb_res.iHeight;
          res.strId = resolution_strings[RES_1080P_23HZ];
          resolutions.push_back(res);
          supported_resolutions[RES_1080P_23HZ] = 1;
      }
      break;
    case RES_3840X2160P_24HZ:
      if (sysModeToResolution(RES_3840X2160P_23HZ, &res) && (supported_resolutions[RES_3840X2160P_23HZ] == 0)) {
          res.iWidth = m_fb_res.iWidth;
          res.iHeight = m_fb_res.iHeight;
          res.strId = resolution_strings[RES_3840X2160P_23HZ];
          resolutions.push_back(res);
          supported_resolutions[RES_3840X2160P_23HZ] = 1;
      }
      break;
    default:
      break;
  }
}

static void fetchDisplayModes()
{
  s_hasModeApi = false;
  s_res_displayModes.clear();

  CJNIDisplay display = CXBMCApp::getWindow().getDecorView().getDisplay();

  if (display)
  {
    CJNIDisplayMode m = display.getMode();
    if (m)
    {
      if (m.getPhysicalWidth() > m.getPhysicalHeight())   // Assume unusable if portrait is returned
      {
        s_hasModeApi = true;

        CLog::Log(LOGDEBUG, "CAndroidUtilsRtk: current mode: %d: %dx%d@%f", m.getModeId(), m.getPhysicalWidth(), m.getPhysicalHeight(), m.getRefreshRate());
#if 0
        s_res_cur_displayMode.strId = StringUtils::Format("%d", m.getModeId());
        s_res_cur_displayMode.iWidth = s_res_cur_displayMode.iScreenWidth = m.getPhysicalWidth();
        s_res_cur_displayMode.iHeight = s_res_cur_displayMode.iScreenHeight = m.getPhysicalHeight();
        s_res_cur_displayMode.fRefreshRate = m.getRefreshRate();
        s_res_cur_displayMode.dwFlags= D3DPRESENTFLAG_PROGRESSIVE;
        s_res_cur_displayMode.bFullScreen   = true;
        s_res_cur_displayMode.iSubtitles    = (int)(0.965 * s_res_cur_displayMode.iHeight);
        s_res_cur_displayMode.fPixelRatio   = 1.0f;
        s_res_cur_displayMode.strMode       = StringUtils::Format("%dx%d @ %.6f%s - Full Screen", s_res_cur_displayMode.iScreenWidth, s_res_cur_displayMode.iScreenHeight, s_res_cur_displayMode.fRefreshRate,
                                                                  s_res_cur_displayMode.dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");

        std::vector<CJNIDisplayMode> modes = display.getSupportedModes();
        for (auto m : modes)
        {
          CLog::Log(LOGDEBUG, "CAndroidUtilsRtk: available mode: %d: %dx%d@%f", m.getModeId(), m.getPhysicalWidth(), m.getPhysicalHeight(), m.getRefreshRate());

          RESOLUTION_INFO res;
          res.strId = StringUtils::Format("%d", m.getModeId());
          res.iWidth = res.iScreenWidth = m.getPhysicalWidth();
          res.iHeight = res.iScreenHeight = m.getPhysicalHeight();
          res.fRefreshRate = m.getRefreshRate();
          res.dwFlags= D3DPRESENTFLAG_PROGRESSIVE;
          res.bFullScreen   = true;
          res.iSubtitles    = (int)(0.965 * res.iHeight);
          res.fPixelRatio   = 1.0f;
          res.strMode       = StringUtils::Format("%dx%d @ %.6f%s - Full Screen", res.iScreenWidth, res.iScreenHeight, res.fRefreshRate,
                                                  res.dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");

          s_res_displayModes.push_back(res);
        }
#else
        m_fb_res.strId = StringUtils::Format("%d", m.getModeId());
        m_fb_res.iWidth = m_fb_res.iScreenWidth = m.getPhysicalWidth();
        m_fb_res.iHeight = m_fb_res.iScreenHeight = m.getPhysicalHeight();
        m_fb_res.fRefreshRate = m.getRefreshRate();
        m_fb_res.dwFlags= D3DPRESENTFLAG_PROGRESSIVE;
        m_fb_res.bFullScreen   = true;
        m_fb_res.iSubtitles    = (int)(0.965 * m_fb_res.iHeight);
        m_fb_res.fPixelRatio   = 1.0f;
        m_fb_res.strMode       = StringUtils::Format("%dx%d @ %.6f%s - Full Screen", m_fb_res.iScreenWidth, m_fb_res.iScreenHeight, m_fb_res.fRefreshRate,
                                                                  m_fb_res.dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");

        if (!CXBMCApp::checkIfHDMIPlugged()) {
          s_hasModeApi = false;
          return;
        }

        memset((void*)&supported_resolutions, 0, sizeof(supported_resolutions));

        std::vector<int> supporteds = CXBMCApp::getVideoFormat();
        int i = 0;
        int total_format_size = supporteds.size();
        int support_format_size = 0;
        for (i = 1; i < RES_RTK_MAX; i++)
        {
          RESOLUTION_INFO hdmi_res;
          if (supporteds.at(i) == 1) {
            support_format_size++;
          }
          if ((supporteds.at(i) == 1) && sysModeToResolution(i, &hdmi_res) && (supported_resolutions[i] == 0))
          {
            hdmi_res.iWidth = m_fb_res.iWidth;
            hdmi_res.iHeight = m_fb_res.iHeight;
            s_res_displayModes.push_back(hdmi_res);
            supported_resolutions[i] = 1;
            GuessResolution(i, s_res_displayModes);
          }
        }  
        CLog::Log(LOGINFO, "video format size %d, support %d", total_format_size, support_format_size);
        if (support_format_size == 0) {
            s_hasModeApi = false;
        }
#endif
      }
    }
  }
}

const std::string CAndroidUtilsRtk::SETTING_LIMITGUI = "videoscreen.limitgui";

CAndroidUtilsRtk::CAndroidUtilsRtk()
{
  std::string displaySize;
  m_width = m_height = 0;

  CXBMCApp::initHdmiManager();

  if (CJNIBase::GetSDKVersion() >= 24)
  {
    fetchDisplayModes();
    for (auto res : s_res_displayModes)
    {
      if (res.iWidth > m_width || res.iHeight > m_height)
      {
        m_width = res.iWidth;
        m_height = res.iHeight;
      }
    }
  }

  if (!m_width || !m_height)
  {
    // Property available on some devices
    displaySize = CJNISystemProperties::get("sys.display-size", "");
    if (!displaySize.empty())
    {
      std::vector<std::string> aSize = StringUtils::Split(displaySize, "x");
      if (aSize.size() == 2)
      {
        m_width = StringUtils::IsInteger(aSize[0]) ? atoi(aSize[0].c_str()) : 0;
        m_height = StringUtils::IsInteger(aSize[1]) ? atoi(aSize[1].c_str()) : 0;
      }
      CLog::Log(LOGDEBUG, "CAndroidUtilsRtk: display-size: %s(%dx%d)", displaySize.c_str(), m_width, m_height);
    }
  }

  CLog::Log(LOGDEBUG, "CAndroidUtilsRtk: maximum/current resolution: %dx%d", m_width, m_height);
  int limit = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CAndroidUtilsRtk::SETTING_LIMITGUI);
  switch (limit)
  {
    case 0: // auto
      m_width = 0;
      m_height = 0;
      break;

    case 9999:  // unlimited
      break;

    case 720:
      if (m_height > 720)
      {
        m_width = 1280;
        m_height = 720;
      }
      break;

    case 1080:
      if (m_height > 1080)
      {
        m_width = 1920;
        m_height = 1080;
      }
      break;
  }
  CLog::Log(LOGDEBUG, "CAndroidUtilsRtk: selected resolution: %dx%d", m_width, m_height);

  CServiceBroker::GetSettingsComponent()->GetSettings()->GetSettingsManager()->RegisterCallback(this, {
    CAndroidUtilsRtk::SETTING_LIMITGUI
  });
}

CAndroidUtilsRtk::~CAndroidUtilsRtk()
{
    CXBMCApp::deInitHdmiManager();
}

bool CAndroidUtilsRtk::GetNativeResolution(RESOLUTION_INFO *res) const
{
  EGLNativeWindowType nativeWindow = (EGLNativeWindowType)CXBMCApp::GetNativeWindow(30000);
  if (!nativeWindow)
    return false;

  if (!m_width || !m_height)
  {
    ANativeWindow_acquire(nativeWindow);
    m_width = ANativeWindow_getWidth(nativeWindow);
    m_height= ANativeWindow_getHeight(nativeWindow);
    ANativeWindow_release(nativeWindow);
    CLog::Log(LOGINFO,"CAndroidUtilsRtk: window resolution: %dx%d", m_width, m_height);
  }

  if (s_hasModeApi)
  {
    RESOLUTION_INFO hdmi_res;
    int resolution = CXBMCApp::getTVSystem();
    CLog::Log(LOGDEBUG, "CAndroidUtilsRtk:GetNativeResolution, resolution = %d", resolution);
    if (resolution > 0 && sysModeToResolution(resolution, &s_res_cur_displayMode))
    {
      m_curHdmiResolution = resolution;
      *res = s_res_cur_displayMode;
      res->iWidth = m_width;
      res->iHeight = m_height;
    }
    else
      *res = m_fb_res;
  }
  else
  {
    res->strId = "-1";
    res->fRefreshRate = currentRefreshRate();
    res->dwFlags= D3DPRESENTFLAG_PROGRESSIVE;
    res->bFullScreen   = true;
    res->iWidth = m_width;
    res->iHeight = m_height;
    res->fPixelRatio   = 1.0f;
    res->iScreenWidth  = res->iWidth;
    res->iScreenHeight = res->iHeight;
  }
  res->iSubtitles    = (int)(0.965 * res->iHeight);
  res->strMode       = StringUtils::Format("%dx%d @ %.6f%s - Full Screen", res->iScreenWidth, res->iScreenHeight, res->fRefreshRate,
                                           res->dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");
  CLog::Log(LOGINFO,"CAndroidUtilsRtk: Current resolution: %dx%d %s\n", res->iWidth, res->iHeight, res->strMode.c_str());
  return true;
}

bool CAndroidUtilsRtk::SetNativeResolution(const RESOLUTION_INFO &res)
{
  CLog::Log(LOGDEBUG, "CAndroidUtilsRtk: SetNativeResolution: %s: %dx%d %dx%d@%f", res.strId.c_str(), res.iWidth, res.iHeight, res.iScreenWidth, res.iScreenHeight, res.fRefreshRate);
  bool ret = true;
#if 0
  if (s_hasModeApi)
  {
    CXBMCApp::SetDisplayMode(atoi(res.strId.c_str()), res.fRefreshRate);
    s_res_cur_displayMode = res;
  }
  else
    CXBMCApp::SetRefreshRate(res.fRefreshRate);
#else
  switch((int)(res.fRefreshRate*10))
  {
    default:
    case 600:
      switch(res.iScreenWidth)
      {
        default:
        case 720:
          if (!isHdmiConnected())
          {
            // output cvbs
            if (res.iScreenHeight == 480)
              ret = SetDisplayResolution(RES_NTSC); // NTSC
            else
              ret = SetDisplayResolution(RES_PAL); // PAL
          }
          else
          {
            if (res.iScreenHeight == 480)
            {
              if (res.dwFlags & D3DPRESENTFLAG_INTERLACED)
                ret = SetDisplayResolution(RES_NTSC); // 480i / NTSC  
              else
                ret = SetDisplayResolution(RES_480P_60HZ); // 480p@60hz
            }
            else
            {
              if (res.dwFlags & D3DPRESENTFLAG_INTERLACED)
                ret = SetDisplayResolution(RES_PAL); // 576i / PAL
              else
                ret = SetDisplayResolution(RES_720P_60HZ);  // hack : no 576p@60hz, set to 720p@60hz
            }
          }
          break;
        case 1280:
          ret = SetDisplayResolution(RES_720P_60HZ); 
          break;
        case 1920:
          if (res.dwFlags & D3DPRESENTFLAG_INTERLACED)
            ret = SetDisplayResolution(RES_1080I_60HZ);
          else
            ret = SetDisplayResolution(RES_1080P_60HZ);
          break;
        case 3840:
            ret = SetDisplayResolution(RES_3840X2160P_60HZ);
            break;
        case 4096:
            ret = SetDisplayResolution(RES_4096X2160P_60HZ);
            break;
      }
      break;
    case 599:
      switch(res.iScreenWidth)
      {
         case 3480:
           ret = SetDisplayResolution(RES_3840X2160P_59HZ);
           break;
         case 1280:
           ret = SetDisplayResolution(RES_720P_59HZ);
           break;
        default:
        case 1920:    
          if (res.dwFlags & D3DPRESENTFLAG_INTERLACED)
            ret = SetDisplayResolution(RES_1080I_59HZ);
          else
            ret = SetDisplayResolution(RES_1080P_59HZ);
          break;
      }
    case 500:
      switch(res.iScreenWidth)
      {
        default:
        case 1280:
          ret = SetDisplayResolution(RES_720P_50HZ);
          break;
        case 1920:
          if (res.dwFlags & D3DPRESENTFLAG_INTERLACED)
            ret = SetDisplayResolution(RES_1080I_50HZ);
          else
            ret = SetDisplayResolution(RES_1080P_50HZ);
          break;
        case 3840:
          ret = SetDisplayResolution(RES_3840X2160P_50HZ);
          break;
        case 4096:
          ret = SetDisplayResolution(RES_4096X2160P_50HZ);
          break;
      }
      break;
    case 300:
      switch(res.iScreenWidth)
      {
        case 3840:
          ret = SetDisplayResolution(RES_3840X2160P_30HZ);
          break;
        case 1280:
          ret = SetDisplayResolution(RES_720P_60HZ); // HACK: no 720@30HZ, set to 720p@60HZ  
          break;
        case 4096:
          ret = SetDisplayResolution(RES_4096X2160P_30HZ); 
          break;
        case 1920:
          ret = SetDisplayResolution(RES_1080P_30HZ); 
          break;
        default:
          if (res.dwFlags & D3DPRESENTFLAG_INTERLACED)  
            ret = SetDisplayResolution(RES_1080I_60HZ); // NO 1080I @ 30, set to 1080I @ 60
          else
            ret = SetDisplayResolution(RES_1080P_30HZ);
          break;
      }
      break;
    case 299:
      switch(res.iScreenWidth)
      {
        case 3840:
          ret = SetDisplayResolution(RES_3840X2160P_29HZ);
          break;
        default:
          ret = SetDisplayResolution(RES_1080P_60HZ); // no 1080 @ 29 and 1080 @ 30, set to 1080 @ 60
          break;
      }
      break;
    case 250:
      switch(res.iScreenWidth)
      {
        case 3840:
          ret = SetDisplayResolution(RES_3840X2160P_25HZ);
          break;
        case 1280:
          ret = SetDisplayResolution(RES_720P_50HZ); // no 720 @ 25, set to 720 @ 50
          break;
        case 4096:
          ret = SetDisplayResolution(RES_4096X2160P_25HZ);
          break;
        case 1920:
          ret = SetDisplayResolution(RES_1080P_25HZ);
          break;
        default: 
          if (res.dwFlags & D3DPRESENTFLAG_INTERLACED)
            ret = SetDisplayResolution(RES_1080I_50HZ);
          else
            ret = SetDisplayResolution(RES_1080P_25HZ);
          break;
      }
      break;
    case 240:
      switch(res.iScreenWidth)
      {
        case 3840:
          ret = SetDisplayResolution(RES_3840X2160P_24HZ);
          break;
        case 4096:
          ret = SetDisplayResolution(RES_4096X2160P_24HZ);
          break;
        default:
          ret = SetDisplayResolution(RES_1080P_24HZ);
          break;
      }
      break;
    case 239:
      switch(res.iScreenWidth)
      {
        case 3840:
          ret = SetDisplayResolution(RES_3840X2160P_23HZ);
          break;
        default:
          return SetDisplayResolution(RES_1080P_23HZ);
          break;
      }
      break;
  }
  s_res_cur_displayMode = res;
#endif
  CXBMCApp::SetBuffersGeometry(res.iWidth, res.iHeight, 0);

  return ret;
}

bool CAndroidUtilsRtk::ProbeResolutions(std::vector<RESOLUTION_INFO> &resolutions)
{
  RESOLUTION_INFO cur_res;
  bool ret = GetNativeResolution(&cur_res);

  CLog::Log(LOGDEBUG, "CAndroidUtilsRtk: ProbeResolutions: %dx%d", m_width, m_height);

  if (s_hasModeApi)
  {
    for(RESOLUTION_INFO res : s_res_displayModes)
    {
      if (m_width && m_height)
      {
        res.iWidth = std::min(res.iWidth, m_width);
        res.iHeight = std::min(res.iHeight, m_height);
        res.iSubtitles = static_cast<int>(0.965 * res.iHeight);
      }
      resolutions.push_back(res);
    }
    return true;
  }

  if (ret && cur_res.iWidth > 1 && cur_res.iHeight > 1)
  {
    std::vector<float> refreshRates;
    CJNIWindow window = CXBMCApp::getWindow();
    if (window)
    {
      CJNIView view = window.getDecorView();
      if (view)
      {
        CJNIDisplay display = view.getDisplay();
        if (display)
        {
          refreshRates = display.getSupportedRefreshRates();
        }
      }

      if (!refreshRates.empty())
      {
        for (unsigned int i = 0; i < refreshRates.size(); i++)
        {
          if (refreshRates[i] < 20.0 || refreshRates[i] > 70.0)
            continue;
          cur_res.fRefreshRate = refreshRates[i];
          cur_res.strMode      = StringUtils::Format("%dx%d @ %.6f%s - Full Screen", cur_res.iScreenWidth, cur_res.iScreenHeight, cur_res.fRefreshRate,
                                                 cur_res.dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");
          resolutions.push_back(cur_res);
        }
      }
    }
    if (resolutions.empty())
    {
      /* No valid refresh rates available, just provide the current one */
      resolutions.push_back(cur_res);
    }
    return true;
  }
  return false;
}

bool CAndroidUtilsRtk::UpdateDisplayModes()
{
  fetchDisplayModes();
  return true;
}

bool CAndroidUtilsRtk::IsHDRDisplay()
{
  CJNIWindow window = CXBMCApp::getWindow();
  bool ret = false;

  if (window)
  {
    CJNIView view = window.getDecorView();
    if (view)
    {
      CJNIDisplay display = view.getDisplay();
      if (display) {
        if (CJNIBase::GetSDKVersion() == 28) {
            ret = display.supportHdr();
        } else {
            ret = display.isHdr();
        }
      }
    }
  }
  CLog::Log(LOGDEBUG, "CAndroidUtilsRtk: IsHDRDisplay: %s", ret ? "true" : "false");
  return ret;
}

bool CAndroidUtilsRtk::IsDoviDisplay()
{
  CJNIWindow window = CXBMCApp::getWindow();
  bool ret = false;

  if (window)
  {
    CJNIView view = window.getDecorView();
    if (view)
    {
      CJNIDisplay display = view.getDisplay();
      if (display)
        ret = display.supportDovi();
    }
  }
  CLog::Log(LOGDEBUG, "CAndroidUtilsRtk: IsDoviDisplay: %s", ret ? "true" : "false");
  return ret;
}

void  CAndroidUtilsRtk::OnSettingChanged(const std::shared_ptr<const CSetting>& setting)
{
  const std::string &settingId = setting->GetId();
  /* Calibration (overscan / subtitles) are based on GUI size -> reset required */
  if (settingId == CAndroidUtilsRtk::SETTING_LIMITGUI)
    CDisplaySettings::GetInstance().ClearCalibrations();
}

bool CAndroidUtilsRtk::SetDisplayResolution(int resolution)
{
  CLog::Log(LOGDEBUG, "CAndroidUtilsRtk:SetDisplayResolution, resolution = %d", resolution);
  if ((resolution < RES_NTSC) || (resolution > RES_RTK_MAX - 1)) {
    return false;
  }

  if (m_curHdmiResolution == resolution)
    return true;

  if (!CXBMCApp::checkIfHDMIPlugged())
    return false;

  /*
  std::vector<int> supporteds = CXBMCApp::getVideoFormat();
  //CLog::Log(LOGDEBUG, "CAndroidUtilsRtk:SetDisplayResolution , empty %d, size %d ", supporteds.empty() ? 1 : 0, supporteds.size());
  if (supporteds.empty() || resolution > supporteds.size())
    return false;

  if (supporteds.at(resolution) == 0) {
     CLog::Log(LOGDEBUG, "CAndroidUtilsRtk:SetDisplayResolution, resolution %s is not supported", resolution_strings[resolution]);
    return false;
  }
  */
  if (supported_resolutions[resolution] == 0) {
    CLog::Log(LOGDEBUG, "CAndroidUtilsRtk:SetDisplayResolution, resolution %s is not supported", resolution_strings[resolution]);
    return false;
  }

  if (CXBMCApp::checkIfHDMIPlugged()) {
    //CLog::Log(LOGDEBUG, "CAndroidUtilsRtk:SetDisplayResolution, setTVSystem  = %d", resolution);
    CXBMCApp::setTVSystem(resolution);
  } else {
    return false;
  }
  
  m_curHdmiResolution = resolution;

  return true;
}

bool CAndroidUtilsRtk::isHdmiConnected()
{
  return CXBMCApp::checkIfHDMIPlugged();
}
