/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileDirectoryFactory.h"

#if defined(HAS_ISO9660PP)
#include "ISO9660Directory.h"
#endif
#if defined(HAS_UDFREAD)
#include "UDFDirectory.h"
#endif
#include "RSSDirectory.h"
#include "UDFDirectory.h"
#include "utils/URIUtils.h"
#if defined(TARGET_ANDROID)
#include "platform/android/filesystem/APKDirectory.h"
#endif
#include "AudioBookFileDirectory.h"
#include "Directory.h"
#include "FileItem.h"
#include "PlaylistFileDirectory.h"
#include "ServiceBroker.h"
#include "SmartPlaylistDirectory.h"
#include "URL.h"
#include "XbtDirectory.h"
#include "ZipDirectory.h"
#include "addons/AudioDecoder.h"
#include "addons/VFSEntry.h"
#include "playlists/PlayListFactory.h"
#include "playlists/SmartPlayList.h"
#include "utils/StringUtils.h"
#include "utils/log.h"

using namespace ADDON;
using namespace XFILE;
using namespace PLAYLIST;

CFileDirectoryFactory::CFileDirectoryFactory(void) = default;

CFileDirectoryFactory::~CFileDirectoryFactory(void) = default;

// return NULL + set pItem->m_bIsFolder to remove it completely from list.
IFileDirectory* CFileDirectoryFactory::Create(const CURL& url, CFileItem* pItem, const std::string& strMask)
{
  if (url.IsProtocol("stack")) // disqualify stack as we need to work with each of the parts instead
    return NULL;

  std::string strExtension = URIUtils::GetExtension(url);
  StringUtils::ToLower(strExtension);
  if (!strExtension.empty() && CServiceBroker::IsBinaryAddonCacheUp() &&
      !StringUtils::EndsWith(strExtension, "stream"))
  {
    std::vector<AddonInfoPtr> addonInfos;
    CServiceBroker::GetAddonMgr().GetAddonInfos(addonInfos, true, ADDON_AUDIODECODER);
    for (const auto& addonInfo : addonInfos)
    {
      if (CAudioDecoder::HasTracks(addonInfo))
      {
        auto exts = StringUtils::Split(CAudioDecoder::GetExtensions(addonInfo), "|");
        if (std::find(exts.begin(), exts.end(), strExtension) != exts.end())
        {
          CAudioDecoder* result = new CAudioDecoder(addonInfo);
          if (!result->CreateDecoder() || !result->ContainsFiles(url))
          {
            delete result;
            CLog::Log(LOGINFO,
                      "CFileDirectoryFactory::{}: Addon '{}' support extension '{}' but creation "
                      "failed (seems not supported), trying other addons and Kodi",
                      __func__, addonInfo->ID(), strExtension);
            continue;
          }
          return result;
        }
      }
    }
  }

  if (!strExtension.empty() && CServiceBroker::IsBinaryAddonCacheUp())
  {
    for (const auto& vfsAddon : CServiceBroker::GetVFSAddonCache().GetAddonInstances())
    {
      if (vfsAddon->HasFileDirectories())
      {
        auto exts = StringUtils::Split(vfsAddon->GetExtensions(), "|");
        if (std::find(exts.begin(), exts.end(), strExtension) != exts.end())
        {
          CVFSEntryIFileDirectoryWrapper* wrap = new CVFSEntryIFileDirectoryWrapper(vfsAddon);
          if (wrap->ContainsFiles(url))
          {
            if (wrap->m_items.Size() == 1)
            {
              // one STORED file - collapse it down
              *pItem = *wrap->m_items[0];
            }
            else
            {
              // compressed or more than one file -> create a dir
              pItem->SetPath(wrap->m_items.GetPath());
            }

            // Check for folder, if yes return also wrap.
            // Needed to fix for e.g. RAR files with only one file inside
            pItem->m_bIsFolder = URIUtils::HasSlashAtEnd(pItem->GetPath());
            if (pItem->m_bIsFolder)
              return wrap;
          }
          else
          {
            pItem->m_bIsFolder = true;
          }

          delete wrap;
          return nullptr;
        }
      }
    }
  }

  if (pItem->IsRSS())
    return new CRSSDirectory();


  if (pItem->IsDiscImage())
  {
#if defined(HAS_ISO9660PP)
    CISO9660Directory* iso = new CISO9660Directory();
    if (iso->Exists(pItem->GetURL()))
      return iso;

    delete iso;
#endif

#if defined(HAS_UDFREAD)
    return new CUDFDirectory();
#endif

    return nullptr;
  }

#if defined(TARGET_ANDROID)
  if (url.IsFileType("apk"))
  {
    CURL zipURL = URIUtils::CreateArchivePath("apk", url);

    CFileItemList items;
    CDirectory::GetDirectory(zipURL, items, strMask, DIR_FLAG_DEFAULTS);
    if (items.Size() == 0) // no files
      pItem->m_bIsFolder = true;
    else if (items.Size() == 1 && items[0]->m_idepth == 0 && !items[0]->m_bIsFolder)
    {
      // one STORED file - collapse it down
      *pItem = *items[0];
    }
    else
    { // compressed or more than one file -> create a apk dir
      pItem->SetURL(zipURL);
      return new CAPKDirectory;
    }
    return NULL;
  }
#endif
  if (url.IsFileType("zip"))
  {
    CURL zipURL = URIUtils::CreateArchivePath("zip", url);

    CFileItemList items;
    CDirectory::GetDirectory(zipURL, items, strMask, DIR_FLAG_DEFAULTS);
    if (items.Size() == 0) // no files
      pItem->m_bIsFolder = true;
    else if (items.Size() == 1 && items[0]->m_idepth == 0 && !items[0]->m_bIsFolder)
    {
      // one STORED file - collapse it down
      *pItem = *items[0];
    }
    else
    { // compressed or more than one file -> create a zip dir
      pItem->SetURL(zipURL);
      return new CZipDirectory;
    }
    return NULL;
  }
  if (url.IsFileType("xbt"))
  {
    CURL xbtUrl = URIUtils::CreateArchivePath("xbt", url);
    pItem->SetURL(xbtUrl);

    return new CXbtDirectory();
  }
  if (url.IsFileType("xsp"))
  { // XBMC Smart playlist - just XML renamed to XSP
    // read the name of the playlist in
    CSmartPlaylist playlist;
    if (playlist.OpenAndReadName(url))
    {
      pItem->SetLabel(playlist.GetName());
      pItem->SetLabelPreformatted(true);
    }
    IFileDirectory* pDir=new CSmartPlaylistDirectory;
    return pDir; // treat as directory
  }
  if (CPlayListFactory::IsPlaylist(url))
  { // Playlist file
    // currently we only return the directory if it contains
    // more than one file.  Reason is that .pls and .m3u may be used
    // for links to http streams etc.
    IFileDirectory *pDir = new CPlaylistFileDirectory();
    CFileItemList items;
    if (pDir->GetDirectory(url, items))
    {
      if (items.Size() > 1)
        return pDir;
    }
    delete pDir;
    return NULL;
  }

  if (pItem->IsAudioBook())
  {
    if (!pItem->HasMusicInfoTag() || pItem->m_lEndOffset <= 0)
    {
      std::unique_ptr<CAudioBookFileDirectory> pDir(new CAudioBookFileDirectory);
      if (pDir->ContainsFiles(url))
        return pDir.release();
    }
    return NULL;
  }
  return NULL;
}

