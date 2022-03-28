/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "CPUInfoPosix.h"

#include "ServiceBroker.h"
#include "settings/AdvancedSettings.h"
#include "settings/SettingsComponent.h"

bool CCPUInfoPosix::GetTemperature(CTemperature& temperature)
{
  return CheckUserTemperatureCommand(temperature);
}

bool CCPUInfoPosix::CheckUserTemperatureCommand(CTemperature& temperature)
{
  temperature.SetValid(false);

  auto settingComponent = CServiceBroker::GetSettingsComponent();
  if (!settingComponent)
    return false;

  auto advancedSettings = settingComponent->GetAdvancedSettings();
  if (!advancedSettings)
    return false;

  std::string cmd = advancedSettings->m_cpuTempCmd;

  if (cmd.empty())
    return false;

  int value = {};
  char scale = {};

  auto p = popen(cmd.c_str(), "r");
  if (p)
  {
    int ret = fscanf(p, "%d %c", &value, &scale);
    pclose(p);

    if (ret < 2)
      return false;
  }

  if (scale == 'C' || scale == 'c')
    temperature = CTemperature::CreateFromCelsius(value);
  else if (scale == 'F' || scale == 'f')
    temperature = CTemperature::CreateFromFahrenheit(value);
  else
    return false;

  temperature.SetValid(true);

  return true;
}
