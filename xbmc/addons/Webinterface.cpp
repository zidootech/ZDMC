/*
 *  Copyright (C) 2015-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "Webinterface.h"

#include "ServiceBroker.h"
#include "addons/AddonManager.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

using namespace ADDON;

CWebinterface::CWebinterface(const AddonInfoPtr& addonInfo)
  : CAddon(addonInfo, ADDON_WEB_INTERFACE),
    m_type(WebinterfaceTypeStatic),
    m_entryPoint(WEBINTERFACE_DEFAULT_ENTRY_POINT)
{
  // determine the type of the webinterface
  std::string webinterfaceType = Type(ADDON_WEB_INTERFACE)->GetValue("@type").asString();
  if (StringUtils::EqualsNoCase(webinterfaceType, "wsgi"))
    m_type = WebinterfaceTypeWsgi;
  else if (!webinterfaceType.empty() && !StringUtils::EqualsNoCase(webinterfaceType, "static") && !StringUtils::EqualsNoCase(webinterfaceType, "html"))
    CLog::Log(LOGWARNING, "CWebinterface::CWebinterface: Addon \"{}\" has specified an unsupported type \"{}\"", ID(), webinterfaceType);

  // determine the entry point of the webinterface
  std::string entry = Type(ADDON_WEB_INTERFACE)->GetValue("@entry").asString();
  if (!entry.empty())
    m_entryPoint = entry;
}

std::string CWebinterface::GetEntryPoint(const std::string &path) const
{
  if (m_type == WebinterfaceTypeWsgi)
    return LibPath();

  return URIUtils::AddFileToFolder(path, m_entryPoint);
}

std::string CWebinterface::GetBaseLocation() const
{
  if (m_type == WebinterfaceTypeWsgi)
    return "/addons/" + ID();

  return "";
}
