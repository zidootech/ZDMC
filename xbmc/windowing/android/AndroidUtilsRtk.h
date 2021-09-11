/*
 *  Copyright (C) 2011-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>
#include <vector>

#include <androidjni/Display.h>

#include "settings/lib/ISettingCallback.h"
#include "windowing/Resolution.h"

class CAndroidUtilsRtk : public ISettingCallback
{
public:
  CAndroidUtilsRtk();
  ~CAndroidUtilsRtk();
  bool GetNativeResolution(RESOLUTION_INFO *res) const;
  bool SetNativeResolution(const RESOLUTION_INFO &res);
  bool ProbeResolutions(std::vector<RESOLUTION_INFO> &resolutions);
  bool UpdateDisplayModes();
  bool IsHDRDisplay();
  bool IsDoviDisplay();

  // Implementation of ISettingCallback
  static const std::string SETTING_LIMITGUI;
  void OnSettingChanged(const std::shared_ptr<const CSetting>& setting) override;

protected:
  mutable int m_width;
  mutable int m_height;

  mutable int m_curHdmiResolution;
  bool SetDisplayResolution(int resolution);
  bool isHdmiConnected();
};
