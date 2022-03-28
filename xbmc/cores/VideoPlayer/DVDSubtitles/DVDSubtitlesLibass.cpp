/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DVDSubtitlesLibass.h"

#include "ServiceBroker.h"
#include "cores/VideoPlayer/Interface/TimingConstants.h"
#include "filesystem/File.h"
#include "filesystem/SpecialProtocol.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "threads/SingleLock.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"
#include "windowing/GraphicContext.h"

namespace
{
std::string GetDefaultFontPath(std::string& font)
{
  std::string fontSources[]{"special://home/media/Fonts/", "special://xbmc/media/Fonts/"};

  for (const auto& path : fontSources)
  {
    auto fontPath = URIUtils::AddFileToFolder(path, font);
    if (XFILE::CFile::Exists(fontPath))
    {
      return CSpecialProtocol::TranslatePath(fontPath).c_str();
    }
  }
  CLog::Log(LOGERROR, "CDVDSubtitlesLibass: Could not find font {} in font sources", font);
  return "";
}
} // namespace

static void libass_log(int level, const char *fmt, va_list args, void *data)
{
  if(level >= 5)
    return;
  std::string log = StringUtils::FormatV(fmt, args);
  CLog::Log(LOGDEBUG, "CDVDSubtitlesLibass: [ass] %s", log.c_str());
}

CDVDSubtitlesLibass::CDVDSubtitlesLibass()
{
  // Setting the font directory to the user font dir. This is the directory
  // where user defined fonts are located (and where mkv fonts are extracted to)
  const std::string strPath = "special://home/media/Fonts/";

  CLog::Log(LOGINFO, "CDVDSubtitlesLibass: Creating ASS library structure");
  m_library = ass_library_init();
  if(!m_library)
    return;

  ass_set_message_cb(m_library, libass_log, this);

  CLog::Log(LOGINFO, "CDVDSubtitlesLibass: Initializing ASS library font settings");

  const std::shared_ptr<CSettings> settings = CServiceBroker::GetSettingsComponent()->GetSettings();
  bool overrideFont = settings->GetBool(CSettings::SETTING_SUBTITLES_OVERRIDEASSFONTS);
  if (!overrideFont)
  {
    // libass uses fontconfig (system lib) which is not wrapped
    // so translate the path before calling into libass
    ass_set_fonts_dir(m_library, CSpecialProtocol::TranslatePath(strPath).c_str());
    ass_set_extract_fonts(m_library, 1);
    ass_set_style_overrides(m_library, nullptr);
  }

  CLog::Log(LOGINFO, "CDVDSubtitlesLibass: Initializing ASS Renderer");

  m_renderer = ass_renderer_init(m_library);

  if(!m_renderer)
    return;

  ass_set_margins(m_renderer, 0, 0, 0, 0);
  ass_set_use_margins(m_renderer, 0);
  ass_set_font_scale(m_renderer, 1);

  // libass uses fontconfig (system lib) which is not wrapped
  // so translate the path before calling into libass
  std::string forcedFont = settings->GetString(CSettings::SETTING_SUBTITLES_FONT);
  ass_set_fonts(m_renderer, GetDefaultFontPath(forcedFont).c_str(), "Arial", overrideFont ? 0 : 1,
                nullptr, 1);
}

CDVDSubtitlesLibass::~CDVDSubtitlesLibass()
{
  if(m_track)
    ass_free_track(m_track);
  ass_renderer_done(m_renderer);
  ass_library_done(m_library);
}

/*Decode Header of SSA, needed to properly decode demux packets*/
bool CDVDSubtitlesLibass::DecodeHeader(char* data, int size)
{
  CSingleLock lock(m_section);
  if(!m_library || !data)
    return false;

  if(!m_track)
  {
    CLog::Log(LOGINFO, "CDVDSubtitlesLibass: Creating new ASS track");
    m_track = ass_new_track(m_library) ;
  }

  ass_process_codec_private(m_track, data, size);
  return true;
}

bool CDVDSubtitlesLibass::DecodeDemuxPkt(const char* data, int size, double start, double duration)
{
  CSingleLock lock(m_section);
  if(!m_track)
  {
    CLog::Log(LOGERROR, "CDVDSubtitlesLibass: No SSA header found.");
    return false;
  }

  //! @bug libass isn't const correct
  ass_process_chunk(m_track, const_cast<char*>(data), size, DVD_TIME_TO_MSEC(start), DVD_TIME_TO_MSEC(duration));
  return true;
}

bool CDVDSubtitlesLibass::CreateTrack(char* buf, size_t size)
{
  CSingleLock lock(m_section);
  if(!m_library)
  {
    CLog::Log(LOGERROR, "CDVDSubtitlesLibass: %s - No ASS library struct", __FUNCTION__);
    return false;
  }

  CLog::Log(LOGINFO, "SSA Parser: Creating m_track from SSA buffer");

  m_track = ass_read_memory(m_library, buf, size, 0);
  if(m_track == NULL)
    return false;

  return true;
}

ASS_Image* CDVDSubtitlesLibass::RenderImage(int frameWidth, int frameHeight, int videoWidth, int videoHeight, int sourceWidth, int sourceHeight,
                                            double pts, int useMargin, double position, int *changes)
{
  CSingleLock lock(m_section);
  if(!m_renderer || !m_track)
  {
    CLog::Log(LOGERROR, "CDVDSubtitlesLibass: %s - Missing ASS structs(m_track or m_renderer)", __FUNCTION__);
    return NULL;
  }

  double sar = static_cast<double>(sourceWidth) / static_cast<double>(sourceHeight);
  double dar = static_cast<double>(videoWidth) / static_cast<double>(videoHeight);
  ass_set_frame_size(m_renderer, frameWidth, frameHeight);
  ass_set_storage_size(m_renderer, sourceWidth, sourceHeight);
  int topmargin = (frameHeight - videoHeight) / 2;
  int leftmargin = (frameWidth - videoWidth) / 2;
  ass_set_margins(m_renderer, topmargin, topmargin, leftmargin, leftmargin);
  ass_set_use_margins(m_renderer, useMargin);
  ass_set_line_position(m_renderer, position);
  ass_set_pixel_aspect(m_renderer, dar / sar);
  return ass_render_frame(m_renderer, m_track, DVD_TIME_TO_MSEC(pts), changes);
}

ASS_Event* CDVDSubtitlesLibass::GetEvents()
{
  CSingleLock lock(m_section);
  if(!m_track)
  {
    CLog::Log(LOGERROR, "CDVDSubtitlesLibass: %s -  Missing ASS structs(m_track)", __FUNCTION__);
    return NULL;
  }
  return m_track->events;
}

int CDVDSubtitlesLibass::GetNrOfEvents()
{
  CSingleLock lock(m_section);
  if(!m_track)
    return 0;
  return m_track->n_events;
}
