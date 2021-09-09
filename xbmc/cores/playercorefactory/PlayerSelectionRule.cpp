/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlayerSelectionRule.h"

#include "ServiceBroker.h"
#include "URL.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/RegExp.h"
#include "utils/StreamDetails.h"
#include "utils/XBMCTinyXML.h"
#include "utils/XMLUtils.h"
#include "utils/log.h"
#include "video/VideoInfoTag.h"

#include <algorithm>
#include "utils/URIUtils.h"

CPlayerSelectionRule::CPlayerSelectionRule(TiXmlElement* pRule)
{
  Initialize(pRule);
}

CPlayerSelectionRule::~CPlayerSelectionRule()
{
  for (unsigned int i = 0; i < vecSubRules.size(); i++)
  {
    delete vecSubRules[i];
  }
  vecSubRules.clear();
}

void CPlayerSelectionRule::Initialize(TiXmlElement* pRule)
{
  m_name = XMLUtils::GetAttribute(pRule, "name");
  if (m_name.empty())
    m_name = "un-named";

  CLog::Log(LOGDEBUG, "CPlayerSelectionRule::Initialize: creating rule: %s", m_name.c_str());

  m_tInternetStream = GetTristate(pRule->Attribute("internetstream"));
  m_tRemote = GetTristate(pRule->Attribute("remote"));
  m_tAudio = GetTristate(pRule->Attribute("audio"));
  m_tVideo = GetTristate(pRule->Attribute("video"));
  m_tGame = GetTristate(pRule->Attribute("game"));

  m_tBD = GetTristate(pRule->Attribute("bd"));
  m_tDVD = GetTristate(pRule->Attribute("dvd"));
  m_tDVDFile = GetTristate(pRule->Attribute("dvdfile"));
  m_tDiscImage = GetTristate(pRule->Attribute("discimage"));
  if (m_tDiscImage < 0)
  {
    m_tDiscImage = GetTristate(pRule->Attribute("dvdimage"));
    if (m_tDiscImage >= 0)
      CLog::Log(LOGWARNING, "\"dvdimage\" tag is deprecated. use \"discimage\"");
  }

  m_protocols = XMLUtils::GetAttribute(pRule, "protocols");
  m_fileTypes = XMLUtils::GetAttribute(pRule, "filetypes");
  m_mimeTypes = XMLUtils::GetAttribute(pRule, "mimetypes");
  m_fileName = XMLUtils::GetAttribute(pRule, "filename");

  m_audioCodec = XMLUtils::GetAttribute(pRule, "audiocodec");
  m_audioChannels = XMLUtils::GetAttribute(pRule, "audiochannels");
  m_videoCodec = XMLUtils::GetAttribute(pRule, "videocodec");
  m_videoResolution = XMLUtils::GetAttribute(pRule, "videoresolution");
  m_videoAspect = XMLUtils::GetAttribute(pRule, "videoaspect");

  m_bStreamDetails = m_audioCodec.length() > 0 || m_audioChannels.length() > 0 ||
    m_videoCodec.length() > 0 || m_videoResolution.length() > 0 || m_videoAspect.length() > 0;

  if (m_bStreamDetails && !CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_MYVIDEOS_EXTRACTFLAGS))
  {
      CLog::Log(LOGWARNING, "CPlayerSelectionRule::Initialize: rule: %s needs media flagging, which is disabled", m_name.c_str());
  }

  m_playerName = XMLUtils::GetAttribute(pRule, "player");

  TiXmlElement* pSubRule = pRule->FirstChildElement("rule");
  while (pSubRule)
  {
    vecSubRules.push_back(new CPlayerSelectionRule(pSubRule));
    pSubRule = pSubRule->NextSiblingElement("rule");
  }
}

int CPlayerSelectionRule::GetTristate(const char* szValue)
{
  if (szValue)
  {
    if (StringUtils::CompareNoCase(szValue, "true") == 0)
      return 1;
    if (StringUtils::CompareNoCase(szValue, "false") == 0)
      return 0;
  }
  return -1;
}

bool CPlayerSelectionRule::CompileRegExp(const std::string& str, CRegExp& regExp)
{
  return !str.empty() && regExp.RegComp(str.c_str());
}

bool CPlayerSelectionRule::MatchesRegExp(const std::string& str, CRegExp& regExp)
{
  return regExp.RegFind(str, 0) == 0;
}

void CPlayerSelectionRule::GetPlayers(const CFileItem& item, std::vector<std::string>&validPlayers, std::vector<std::string>&players)
{
  CLog::Log(LOGDEBUG, "CPlayerSelectionRule::GetPlayers: considering rule: %s", m_name.c_str());
  bool force_local = false;

  if (item.m_strTitle.compare("Play with 3d mode") == 0)
  {
    // User want to play 3d, force to play with local
    force_local = true;
  }

  if (m_name.compare("local")==0)
  {
    if (!force_local)
    {
      if (!CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_VIDEOPLAYER_ENABLEEXTERNALPLAYER))
      {
        return;
      }
    }

    // only accept local and samba, nfs
    std::string mainFile = item.GetPath();
    CURL url(item.GetPath());
    CLog::Log(LOGDEBUG, "CPlayerSelectionRule::GetPlayers - origin path = %s ", mainFile.c_str());
    CLog::Log(LOGDEBUG, "CPlayerSelectionRule::GetPlayers - origin protocol = %s ", url.GetProtocol().c_str());
    bool accept = false;
    if (url.IsLocal() || mainFile.compare(0, 8, "/storage") == 0)
    {
      // local file
      accept = true;
    }
    else if (url.IsProtocol("smb") || url.IsProtocol("nfs"))
    {
      // prefix with smb:// or nfs://
      accept = true;
    }
    else  if (url.IsProtocol("bluray"))
    {
      CURL base(url.GetHostName());
      if (base.IsLocal() || base.IsProtocol("smb") || base.IsProtocol("nfs"))
      {
        // prefix with bluray:///storage , bluray://smb://, bluray://nfs://
        accept = true;
      }
      else if (base.IsProtocol("udf"))
      {
        CURL base1(base.GetHostName());
        if (base1.IsLocal() || base1.IsProtocol("smb") || base1.IsProtocol("nfs"))
        {
          // prefix with bluray://udf:///storage, bluray://udf://smb://, bluray://udf://nfs://
          accept = true;
        }
        else
        {
          mainFile = URIUtils::AddFileToFolder(base1.Get(), base1.GetFileName());
        }
      }
      else
      {
        mainFile = URIUtils::AddFileToFolder(base.Get(), base.GetFileName());
      }
    }
    else if (url.IsProtocol("udf"))
    {
      CURL base(url.GetHostName());
      if (base.IsLocal() || base.IsProtocol("smb") || base.IsProtocol("nfs"))
      {
        // prefix with udf:///storage, udf://smb://, udf://nfs://
        accept = true;
      }
      else
      {
        mainFile = URIUtils::AddFileToFolder(base.Get(), base.GetFileName());
      }
    }
    else
    {
       mainFile = URIUtils::AddFileToFolder(url.Get(), url.GetFileName());
    }

    if (!accept) {
       if (mainFile.compare(0, 8, "/storage") == 0)
        accept = true;
    }

    if (!accept)
    {
      CLog::Log(LOGDEBUG, "CPlayerSelectionRule::GetPlayers -local player can't play media =%s", mainFile.c_str());
      return;
    }
  }

  if (m_bStreamDetails && !item.HasVideoInfoTag())
    return;
  if (m_tAudio >= 0 && (m_tAudio > 0) != item.IsAudio())
    return;
  if (m_tVideo >= 0 && (m_tVideo > 0) != item.IsVideo())
    return;
  if (m_tGame >= 0 && (m_tGame > 0) != item.IsGame())
    return;
  if (m_tInternetStream >= 0 && (m_tInternetStream > 0) != item.IsInternetStream())
    return;
  if (m_tRemote >= 0 && (m_tRemote > 0) != item.IsRemote())
    return;

  if (m_tBD >= 0 && (m_tBD > 0) != (item.IsBDFile() && item.IsOnDVD()))
    return;
  if (m_tDVD >= 0 && (m_tDVD > 0) != item.IsDVD())
    return;
  if (m_tDVDFile >= 0 && (m_tDVDFile > 0) != item.IsDVDFile())
    return;
  if (m_tDiscImage >= 0 && (m_tDiscImage > 0) != item.IsDiscImage())
    return;

  CRegExp regExp(false, CRegExp::autoUtf8);

  if (m_bStreamDetails)
  {
    if (!item.GetVideoInfoTag()->HasStreamDetails())
    {
      CLog::Log(LOGDEBUG, "CPlayerSelectionRule::GetPlayers: cannot check rule: %s, no StreamDetails", m_name.c_str());
      return;
    }

    CStreamDetails streamDetails = item.GetVideoInfoTag()->m_streamDetails;

    if (CompileRegExp(m_audioCodec, regExp) && !MatchesRegExp(streamDetails.GetAudioCodec(), regExp))
      return;

    std::stringstream itoa;
    itoa << streamDetails.GetAudioChannels();
    std::string audioChannelsstr = itoa.str();

    if (CompileRegExp(m_audioChannels, regExp) && !MatchesRegExp(audioChannelsstr, regExp))
      return;

    if (CompileRegExp(m_videoCodec, regExp) && !MatchesRegExp(streamDetails.GetVideoCodec(), regExp))
      return;

    if (CompileRegExp(m_videoResolution, regExp) &&
        !MatchesRegExp(CStreamDetails::VideoDimsToResolutionDescription(streamDetails.GetVideoWidth(), streamDetails.GetVideoHeight()), regExp))
      return;

    if (CompileRegExp(m_videoAspect, regExp) &&
        !MatchesRegExp(CStreamDetails::VideoAspectToAspectDescription(streamDetails.GetVideoAspect()),  regExp))
      return;
  }

  CURL url(item.GetDynPath());

  if (CompileRegExp(m_fileTypes, regExp) && !MatchesRegExp(url.GetFileType(), regExp))
    return;

  if (CompileRegExp(m_protocols, regExp) && !MatchesRegExp(url.GetProtocol(), regExp))
    return;

  if (CompileRegExp(m_mimeTypes, regExp) && !MatchesRegExp(item.GetMimeType(), regExp))
    return;

  if (CompileRegExp(m_fileName, regExp) && !MatchesRegExp(item.GetDynPath(), regExp))
    return;

  CLog::Log(LOGDEBUG, "CPlayerSelectionRule::GetPlayers: matches rule: %s", m_name.c_str());

  for (unsigned int i = 0; i < vecSubRules.size(); i++)
    vecSubRules[i]->GetPlayers(item, validPlayers, players);

  if (std::find(validPlayers.begin(), validPlayers.end(), m_playerName) != validPlayers.end())
  {
    CLog::Log(LOGDEBUG, "CPlayerSelectionRule::GetPlayers: adding player: %s for rule: %s", m_playerName.c_str(), m_name.c_str());
    players.push_back(m_playerName);
  }
}


