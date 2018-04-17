/*
 *      Copyright (C) 2011-2014 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "system.h"
#include <EGL/egl.h>
#include "EGLNativeTypeRtkAndroid.h"
#include "utils/log.h"
#include "guilib/gui3d.h"
#include "platform/android/activity/XBMCApp.h"
#include "platform/android/jni/Build.h"
#include "platform/android/jni/RtkHdmiManager.h"
#include "utils/StringUtils.h"
#include "utils/SysfsUtils.h"

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

static char *resolution_strings[] = {
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

bool CEGLNativeTypeRtkAndroid::CheckCompatibility()
{
  CLog::Log(LOGDEBUG, "RTKEGL:CheckCompatibility");
  if (StringUtils::StartsWithNoCase(CJNIBuild::HARDWARE, "kylin"))  // Realtek
  {
    if (SysfsUtils::Has("/sys/devices/virtual/switch/hdmi/state"))
      return true;
    else
      CLog::Log(LOGERROR, "RTKEGL: no r on /sys/devices/virtual/switch/hdmi/state");
  }	

  return false;
}

bool CEGLNativeTypeRtkAndroid::SysModeToResolution(int resolution, RESOLUTION_INFO *res) const
{
  if (!res)
    return false;

  res->iWidth = 0;
  res->iHeight= 0;

  if(resolution < RES_NTSC|| resolution > RES_MAX - 1)
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

  res->iScreen       = 0;
  res->bFullScreen   = true;
  res->iSubtitles    = (int)(0.965 * res->iHeight);
  res->fPixelRatio   = 1.0f;
  res->strMode       = StringUtils::Format("%dx%d @ %.2f%s - Full Screen", res->iScreenWidth, res->iScreenHeight, res->fRefreshRate,
                                           res->dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");
  std::string resolutionStr(resolution_strings[resolution]);
  CLog::Log(LOGDEBUG, "RTKEGL:SysModeToResolution, resolution = %s", resolutionStr.c_str());
  res->strId         = resolutionStr;

  return res->iWidth > 0 && res->iHeight> 0;
}

bool CEGLNativeTypeRtkAndroid::GetNativeResolution(RESOLUTION_INFO *res) const
{
  CEGLNativeTypeAndroid::GetNativeResolution(&m_fb_res);

  RESOLUTION_INFO hdmi_res;
  int resolution = -1;
  CJNIRtkHdmiManager *hdmiManager = new CJNIRtkHdmiManager();
  if (hdmiManager != NULL)
    resolution = hdmiManager->getTVSystem();
  CLog::Log(LOGDEBUG, "RTKEGL:GetNativeResolution, resolution = %d", resolution);
  if (resolution > 0 && SysModeToResolution(resolution, &hdmi_res))
  {
    m_curHdmiResolution = resolution;
    *res = hdmi_res;
    res->iWidth = m_fb_res.iWidth;
    res->iHeight = m_fb_res.iHeight;
  }
  else
    *res = m_fb_res;

  return true;
}

bool CEGLNativeTypeRtkAndroid::SetNativeResolution(const RESOLUTION_INFO &res)
{

  CLog::Log(LOGDEBUG, "RTKEGL:SetNativeResolution, res = %s", 
    StringUtils::Format("%dx%d%s @ %.2f", res.iScreenWidth, res.iScreenHeight, res.dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "p", res.fRefreshRate).c_str());
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
              return SetDisplayResolution(RES_NTSC); // NTSC
            else
              return SetDisplayResolution(RES_PAL); // PAL
          }
          else
          {
            if (res.iScreenHeight == 480)
            {
              if (res.dwFlags & D3DPRESENTFLAG_INTERLACED)
                return SetDisplayResolution(RES_NTSC); // 480i / NTSC  
              else
                return SetDisplayResolution(RES_480P_60HZ); // 480p@60hz
            }
            else
            {
              if (res.dwFlags & D3DPRESENTFLAG_INTERLACED)
                return SetDisplayResolution(RES_PAL); // 576i / PAL
              else
                return SetDisplayResolution(RES_720P_60HZ);  // hack : no 576p@60hz, set to 720p@60hz
            }
          }
          break;
        case 1280:
          return SetDisplayResolution(RES_720P_60HZ); 
          break;
        case 1920:
          if (res.dwFlags & D3DPRESENTFLAG_INTERLACED)
            return SetDisplayResolution(RES_1080I_60HZ);
          else
            return SetDisplayResolution(RES_1080P_60HZ);
          break;
        case 3840:
            return SetDisplayResolution(RES_3840X2160P_60HZ);
            break;
        case 4096:
            return SetDisplayResolution(RES_4096X2160P_60HZ);
            break;
      }
      break;
    case 599:
      switch(res.iScreenWidth)
      {
         case 3480:
           return SetDisplayResolution(RES_3840X2160P_59HZ);
           break;
         case 1280:
           return SetDisplayResolution(RES_720P_59HZ);
           break;
        default:
        case 1920:    
          if (res.dwFlags & D3DPRESENTFLAG_INTERLACED)
            return SetDisplayResolution(RES_1080I_59HZ);
          else
            return SetDisplayResolution(RES_1080P_59HZ);
          break;
      }
    case 500:
      switch(res.iScreenWidth)
      {
        default:
        case 1280:
          return SetDisplayResolution(RES_720P_50HZ);
          break;
        case 1920:
          if (res.dwFlags & D3DPRESENTFLAG_INTERLACED)
            return SetDisplayResolution(RES_1080I_50HZ);
          else
            return SetDisplayResolution(RES_1080P_50HZ);
          break;
        case 3840:
          return SetDisplayResolution(RES_3840X2160P_50HZ);
          break;
        case 4096:
          return SetDisplayResolution(RES_4096X2160P_50HZ);
          break;
      }
      break;
    case 300:
      switch(res.iScreenWidth)
      {
        case 3840:
          return SetDisplayResolution(RES_3840X2160P_30HZ);
          break;
        case 1280:
          return SetDisplayResolution(RES_720P_60HZ); // HACK: no 720@30HZ, set to 720p@60HZ  
          break;
        case 4096:
          return SetDisplayResolution(RES_4096X2160P_30HZ); 
          break;
        case 1920:
          return SetDisplayResolution(RES_1080P_30HZ); 
          break;
        default:
          if (res.dwFlags & D3DPRESENTFLAG_INTERLACED)  
            return SetDisplayResolution(RES_1080I_60HZ); // NO 1080I @ 30, set to 1080I @ 60
          else
            return SetDisplayResolution(RES_1080P_30HZ);
          break;
      }
      break;
    case 299:
      switch(res.iScreenWidth)
      {
        case 3840:
          return SetDisplayResolution(RES_3840X2160P_29HZ);
          break;
        default:
          return SetDisplayResolution(RES_1080P_60HZ); // no 1080 @ 29 and 1080 @ 30, set to 1080 @ 60
          break;
      }
      break;
    case 250:
      switch(res.iScreenWidth)
      {
        case 3840:
          return SetDisplayResolution(RES_3840X2160P_25HZ);
          break;
        case 1280:
          return SetDisplayResolution(RES_720P_50HZ); // no 720 @ 25, set to 720 @ 50
          break;
        case 4096:
          return SetDisplayResolution(RES_4096X2160P_25HZ);
          break;
        case 1920:
          return SetDisplayResolution(RES_1080P_25HZ);
          break;
        default: 
          if (res.dwFlags & D3DPRESENTFLAG_INTERLACED)
            return SetDisplayResolution(RES_1080I_50HZ);
          else
            return SetDisplayResolution(RES_1080P_25HZ);
          break;
      }
      break;
    case 240:
      switch(res.iScreenWidth)
      {
        case 3840:
          return SetDisplayResolution(RES_3840X2160P_24HZ);
          break;
        case 4096:
          return SetDisplayResolution(RES_4096X2160P_24HZ);
          break;
        default:
          return SetDisplayResolution(RES_1080P_24HZ);
          break;
      }
      break;
    case 239:
      switch(res.iScreenWidth)
      {
        case 3840:
          return SetDisplayResolution(RES_3840X2160P_23HZ);
          break;
        default:
          return SetDisplayResolution(RES_1080P_23HZ);
          break;
      }
      break;
  }

  return false;
}

void CEGLNativeTypeRtkAndroid::GuessResolution(int mode, std::vector<RESOLUTION_INFO> &resolutions)
{
    RESOLUTION_INFO res;
    //CLog::Log(LOGDEBUG, "RTKEGL:GuessResolution, mode = %d", mode);
    switch (mode) {
      case RES_720P_50HZ:
        if (SysModeToResolution(RES_720P_25HZ, &res) && (supported_resolutions[RES_720P_25HZ] == 0)) {
            res.iWidth = m_fb_res.iWidth;
            res.iHeight = m_fb_res.iHeight;
            resolutions.push_back(res);
            supported_resolutions[RES_720P_25HZ]  = 1;
        }
        break;
      case RES_720P_60HZ:
        if (SysModeToResolution(RES_720P_30HZ, &res) && (supported_resolutions[RES_720P_30HZ] == 0)) {
            res.iWidth = m_fb_res.iWidth;
            res.iHeight = m_fb_res.iHeight;
            resolutions.push_back(res);
            supported_resolutions[RES_720P_30HZ] = 1;
        }
        break;
     case RES_1080I_50HZ:
        if (SysModeToResolution(RES_1080I_25HZ, &res) && (supported_resolutions[RES_1080I_25HZ] == 0)) {
            res.iWidth = m_fb_res.iWidth;
            res.iHeight = m_fb_res.iHeight;
            resolutions.push_back(res);
            supported_resolutions[RES_1080I_25HZ]  = 1;
        }
        break;
      case RES_1080I_60HZ:
        if (SysModeToResolution(RES_1080I_30HZ, &res) && (supported_resolutions[RES_1080I_30HZ] == 0)) {
            res.iWidth = m_fb_res.iWidth;
            res.iHeight = m_fb_res.iHeight;
            resolutions.push_back(res);
            supported_resolutions[RES_1080I_30HZ] = 1;
        }
        break;
      case RES_1080P_24HZ:
        if (SysModeToResolution(RES_1080P_23HZ, &res) && (supported_resolutions[RES_1080P_23HZ] == 0)) {
            res.iWidth = m_fb_res.iWidth;
            res.iHeight = m_fb_res.iHeight;
            resolutions.push_back(res);
            supported_resolutions[RES_1080P_23HZ] = 1;
        }
        break;
      case RES_3840X2160P_24HZ:
        if (SysModeToResolution(RES_3840X2160P_23HZ, &res) && (supported_resolutions[RES_3840X2160P_23HZ] == 0)) {
            res.iWidth = m_fb_res.iWidth;
            res.iHeight = m_fb_res.iHeight;
            resolutions.push_back(res);
            supported_resolutions[RES_3840X2160P_23HZ] = 1;
        }
        break;
      default:
        break;
    }
}

bool CEGLNativeTypeRtkAndroid::ProbeResolutions(std::vector<RESOLUTION_INFO> &resolutions)
{
  CLog::Log(LOGDEBUG, "RTKEGL:ProbeResolutions");
  CEGLNativeTypeAndroid::GetNativeResolution(&m_fb_res);

  CJNIRtkHdmiManager *hdmiManager = new CJNIRtkHdmiManager();
  if (hdmiManager == NULL)
    return false;

  memset((void*)&supported_resolutions, 0, sizeof(supported_resolutions));

  RESOLUTION_INFO hdmi_res;
  RESOLUTION_INFO guess_res;
  std::vector<int> supporteds = hdmiManager->getVideoFormat();
  int i = 0;
  for (i = 1; i < RES_RTK_MAX; i++)
  {
    if ((supporteds.at(i) == 1) && SysModeToResolution(i, &hdmi_res) && (supported_resolutions[i] == 0))
    {
      hdmi_res.iWidth = m_fb_res.iWidth;
      hdmi_res.iHeight = m_fb_res.iHeight;
      resolutions.push_back(hdmi_res);
      supported_resolutions[i] = 1;
      GuessResolution(i, resolutions);
    }
  }
  return resolutions.size() > 0;

}

bool CEGLNativeTypeRtkAndroid::GetPreferredResolution(RESOLUTION_INFO *res) const
{
  return GetNativeResolution(res);
}

bool CEGLNativeTypeRtkAndroid::SetDisplayResolution(int resolution)
{
  CLog::Log(LOGDEBUG, "RTKEGL:SetDisplayResolution, resolution = %d", resolution);
  if ((resolution < RES_NTSC) || (resolution > RES_RTK_MAX - 1)) {
    return false;
  }

  if (m_curHdmiResolution == resolution)
    return true;

  CJNIRtkHdmiManager *hdmiManager = new CJNIRtkHdmiManager();
  if (hdmiManager == NULL)
    return false;

  /*
  std::vector<int> supporteds = hdmiManager->getVideoFormat();
  //CLog::Log(LOGDEBUG, "RTKEGL:SetDisplayResolution , empty %d, size %d ", supporteds.empty() ? 1 : 0, supporteds.size());
  if (supporteds.empty() || resolution > supporteds.size())
    return false;

  if (supporteds.at(resolution) == 0) {
     CLog::Log(LOGDEBUG, "RTKEGL:SetDisplayResolution, resolution %s is not supported", resolution_strings[resolution]);
    return false;
  }
  */
  if (supported_resolutions[resolution] == 0) {
    CLog::Log(LOGDEBUG, "RTKEGL:SetDisplayResolution, resolution %s is not supported", resolution_strings[resolution]);
    return false;
  }

  if (hdmiManager->checkIfHDMIPlugged()) {
    //CLog::Log(LOGDEBUG, "RTKEGL:SetDisplayResolution, setTVSystem  = %d", resolution);
    hdmiManager->setTVSystem(resolution);
  } else {
    return false;
  }
  
  m_curHdmiResolution = resolution;

  return true;
}

bool CEGLNativeTypeRtkAndroid::isHdmiConnected()
{
  CJNIRtkHdmiManager *hdmiManager = new CJNIRtkHdmiManager();
  if (hdmiManager == NULL)
    return false;

  return hdmiManager->checkIfHDMIPlugged();
}
