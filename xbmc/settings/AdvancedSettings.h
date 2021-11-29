/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "pictures/PictureScalingAlgorithm.h"
#include "settings/lib/ISettingCallback.h"
#include "settings/lib/ISettingsHandler.h"
#include "utils/SortUtils.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#define CACHE_BUFFER_MODE_INTERNET 0
#define CACHE_BUFFER_MODE_ALL 1
#define CACHE_BUFFER_MODE_TRUE_INTERNET 2
#define CACHE_BUFFER_MODE_NONE 3
#define CACHE_BUFFER_MODE_NETWORK 4

class CAppParamParser;
class CProfileManager;
class CSettingsManager;
class CVariant;
struct IntegerSettingOption;

class TiXmlElement;
namespace ADDON
{
  class IAddon;
}

class DatabaseSettings
{
public:
  DatabaseSettings() { Reset(); }
  void Reset()
  {
    type.clear();
    host.clear();
    port.clear();
    user.clear();
    pass.clear();
    name.clear();
    key.clear();
    cert.clear();
    ca.clear();
    capath.clear();
    ciphers.clear();
    compression = false;
  };
  std::string type;
  std::string host;
  std::string port;
  std::string user;
  std::string pass;
  std::string name;
  std::string key;
  std::string cert;
  std::string ca;
  std::string capath;
  std::string ciphers;
  bool compression;
};

struct TVShowRegexp
{
  bool byDate;
  std::string regexp;
  int defaultSeason;
  TVShowRegexp(bool d, const std::string& r, int s = 1):
    regexp(r)
  {
    byDate = d;
    defaultSeason = s;
  }
};

struct RefreshOverride
{
  float fpsmin;
  float fpsmax;

  float refreshmin;
  float refreshmax;

  bool  fallback;
};


struct RefreshVideoLatency
{
  float refreshmin;
  float refreshmax;

  float delay;
};

typedef std::vector<TVShowRegexp> SETTINGS_TVSHOWLIST;

class CAdvancedSettings : public ISettingCallback, public ISettingsHandler
{
  public:
    CAdvancedSettings();

    void OnSettingsLoaded() override;
    void OnSettingsUnloaded() override;

    void OnSettingChanged(const std::shared_ptr<const CSetting>& setting) override;

    void Initialize(const CAppParamParser &params, CSettingsManager& settingsMgr);
    void Uninitialize(CSettingsManager& settingsMgr);
    bool Initialized() const { return m_initialized; };
    void AddSettingsFile(const std::string &filename);
    bool Load(const CProfileManager &profileManager);

    static void GetCustomTVRegexps(TiXmlElement *pRootElement, SETTINGS_TVSHOWLIST& settings);
    static void GetCustomRegexps(TiXmlElement *pRootElement, std::vector<std::string> &settings);
    static void GetCustomExtensions(TiXmlElement *pRootElement, std::string& extensions);

    std::string m_audioDefaultPlayer;
    float m_audioPlayCountMinimumPercent;
    bool m_VideoPlayerIgnoreDTSinWAV;
    float m_limiterHold;
    float m_limiterRelease;

    bool  m_omlSync = true;

    float m_videoSubsDelayRange;
    float m_videoAudioDelayRange;
    bool m_videoUseTimeSeeking;
    int m_videoTimeSeekForward;
    int m_videoTimeSeekBackward;
    int m_videoTimeSeekForwardBig;
    int m_videoTimeSeekBackwardBig;
    int m_videoPercentSeekForward;
    int m_videoPercentSeekBackward;
    int m_videoPercentSeekForwardBig;
    int m_videoPercentSeekBackwardBig;
    std::vector<int> m_seekSteps;
    std::string m_videoPPFFmpegPostProc;
    bool m_videoVDPAUtelecine;
    bool m_videoVDPAUdeintSkipChromaHD;
    bool m_musicUseTimeSeeking;
    int m_musicTimeSeekForward;
    int m_musicTimeSeekBackward;
    int m_musicTimeSeekForwardBig;
    int m_musicTimeSeekBackwardBig;
    int m_musicPercentSeekForward;
    int m_musicPercentSeekBackward;
    int m_musicPercentSeekForwardBig;
    int m_musicPercentSeekBackwardBig;
    int m_videoIgnoreSecondsAtStart;
    float m_videoIgnorePercentAtEnd;
    float m_audioApplyDrc;

    int   m_videoVDPAUScaling;
    float m_videoNonLinStretchRatio;
    bool  m_videoEnableHighQualityHwScalers;
    float m_videoAutoScaleMaxFps;
    std::vector<RefreshOverride> m_videoAdjustRefreshOverrides;
    std::vector<RefreshVideoLatency> m_videoRefreshLatency;
    float m_videoDefaultLatency;
    int  m_videoCaptureUseOcclusionQuery;
    bool m_DXVACheckCompatibility;
    bool m_DXVACheckCompatibilityPresent;
    int  m_videoFpsDetect;
    float m_maxTempo;
    bool m_videoPreferStereoStream = false;

    std::string m_videoDefaultPlayer;
    float m_videoPlayCountMinimumPercent;

    float m_slideshowBlackBarCompensation;
    float m_slideshowZoomAmount;
    float m_slideshowPanAmount;

    int m_songInfoDuration;
    int m_logLevel;
    int m_logLevelHint;
    std::string m_cddbAddress;
    bool m_addSourceOnTop; //!< True to put 'add source' buttons on top

    //airtunes + airplay
    int m_airTunesPort;
    int m_airPlayPort;

    bool m_handleMounting;

    bool m_fullScreenOnMovieStart;
    std::string m_cachePath;
    std::string m_videoCleanDateTimeRegExp;
    std::vector<std::string> m_videoCleanStringRegExps;
    std::vector<std::string> m_videoExcludeFromListingRegExps;
    std::vector<std::string> m_allExcludeFromScanRegExps;
    std::vector<std::string> m_moviesExcludeFromScanRegExps;
    std::vector<std::string> m_tvshowExcludeFromScanRegExps;
    std::vector<std::string> m_audioExcludeFromListingRegExps;
    std::vector<std::string> m_audioExcludeFromScanRegExps;
    std::vector<std::string> m_pictureExcludeFromListingRegExps;
    std::vector<std::string> m_videoStackRegExps;
    std::vector<std::string> m_folderStackRegExps;
    std::vector<std::string> m_trailerMatchRegExps;
    SETTINGS_TVSHOWLIST m_tvshowEnumRegExps;
    std::string m_tvshowMultiPartEnumRegExp;
    typedef std::vector< std::pair<std::string, std::string> > StringMapping;
    StringMapping m_pathSubstitutions;
    int m_remoteDelay; ///< \brief number of remote messages to ignore before repeating
    bool m_bScanIRServer;

    bool m_playlistAsFolders;
    bool m_detectAsUdf;

    unsigned int m_fanartRes; ///< \brief the maximal resolution to cache fanart at (assumes 16x9)
    unsigned int m_imageRes;  ///< \brief the maximal resolution to cache images at (assumes 16x9)
    CPictureScalingAlgorithm::Algorithm m_imageScalingAlgorithm;

    int m_sambaclienttimeout;
    std::string m_sambadoscodepage;
    bool m_sambastatfiles;

    bool m_bHTTPDirectoryStatFilesize;

    bool m_bFTPThumbs;
    bool m_bShoutcastArt;

    std::string m_musicThumbs;
    std::vector<std::string> m_musicArtistExtraArt;
    std::vector<std::string> m_musicAlbumExtraArt;

    int m_iMusicLibraryRecentlyAddedItems;
    int m_iMusicLibraryDateAdded;
    bool m_bMusicLibraryAllItemsOnBottom;
    bool m_bMusicLibraryCleanOnUpdate;
    bool m_bMusicLibraryArtistSortOnUpdate;
    bool m_bMusicLibraryUseISODates;
    std::string m_strMusicLibraryAlbumFormat;
    bool m_prioritiseAPEv2tags;
    std::string m_musicItemSeparator;
    std::vector<std::string> m_musicArtistSeparators;
    std::string m_videoItemSeparator;
    std::vector<std::string> m_musicTagsFromFileFilters;

    bool m_bVideoLibraryAllItemsOnBottom;
    int m_iVideoLibraryRecentlyAddedItems;
    bool m_bVideoLibraryCleanOnUpdate;
    bool m_bVideoLibraryUseFastHash;
    bool m_bVideoLibraryImportWatchedState;
    bool m_bVideoLibraryImportResumePoint;
    std::vector<std::string> m_videoEpisodeExtraArt;
    std::vector<std::string> m_videoTvShowExtraArt;
    std::vector<std::string> m_videoTvSeasonExtraArt;
    std::vector<std::string> m_videoMovieExtraArt;
    std::vector<std::string> m_videoMovieSetExtraArt;
    std::vector<std::string> m_videoMusicVideoExtraArt;

    bool m_bVideoScannerIgnoreErrors;
    int m_iVideoLibraryDateAdded;

    std::set<std::string> m_vecTokens;

    int m_iEpgUpdateCheckInterval;  // seconds
    int m_iEpgCleanupInterval;      // seconds
    int m_iEpgActiveTagCheckInterval; // seconds
    int m_iEpgRetryInterruptedUpdateInterval; // seconds
    int m_iEpgUpdateEmptyTagsInterval; // seconds
    bool m_bEpgDisplayUpdatePopup;
    bool m_bEpgDisplayIncrementalUpdatePopup;

    // EDL Commercial Break
    bool m_bEdlMergeShortCommBreaks;
    int m_iEdlMaxCommBreakLength;   // seconds
    int m_iEdlMinCommBreakLength;   // seconds
    int m_iEdlMaxCommBreakGap;      // seconds
    int m_iEdlMaxStartGap;          // seconds
    int m_iEdlCommBreakAutowait;    // seconds
    int m_iEdlCommBreakAutowind;    // seconds

    int m_curlconnecttimeout;
    int m_curllowspeedtime;
    int m_curlretries;
    int m_curlKeepAliveInterval;    // seconds
    bool m_curlDisableIPV6;
    bool m_curlDisableHTTP2;

    std::string m_caTrustFile;

    bool m_fullScreen;
    bool m_startFullScreen;
    bool m_showExitButton; /* Ideal for appliances to hide a 'useless' button */
    bool m_canWindowed;
    bool m_splashImage;
    bool m_alwaysOnTop;  /* makes xbmc to run always on top .. osx/win32 only .. */
    int m_playlistRetries;
    int m_playlistTimeout;
    bool m_GLRectangleHack;
    int m_iSkipLoopFilter;

    bool m_bVirtualShares;
    bool m_bTry10bitOutput;

    std::string m_cpuTempCmd;
    std::string m_gpuTempCmd;

    /* PVR/TV related advanced settings */
    int m_iPVRTimeCorrection;     /*!< @brief correct all times (epg tags, timer tags, recording tags) by this amount of minutes. defaults to 0. */
    int m_iPVRInfoToggleInterval; /*!< @brief if there are more than 1 pvr gui info item available (e.g. multiple recordings active at the same time), use this toggle delay in milliseconds. defaults to 3000. */
    bool m_bPVRChannelIconsAutoScan; /*!< @brief automatically scan user defined folder for channel icons when loading internal channel groups */
    bool m_bPVRAutoScanIconsUserSet; /*!< @brief mark channel icons populated by auto scan as "user set" */
    int m_iPVRNumericChannelSwitchTimeout; /*!< @brief time in msecs after that a channel switch occurs after entering a channel number, if confirmchannelswitch is disabled */
    int m_iPVRTimeshiftThreshold; /*!< @brief time diff between current playing time and timeshift buffer end, in seconds, before a playing stream is displayed as timeshifting. */
    bool m_bPVRTimeshiftSimpleOSD; /*!< @brief use simple timeshift OSD (with progress only for the playing event instead of progress for the whole ts buffer). */
    SortDescription m_PVRDefaultSortOrder; /*!< @brief SortDecription used to store default recording sort type and sort order */

    DatabaseSettings m_databaseMusic; // advanced music database setup
    DatabaseSettings m_databaseVideo; // advanced video database setup
    DatabaseSettings m_databaseTV;    // advanced tv database setup
    DatabaseSettings m_databaseEpg;   /*!< advanced EPG database setup */

    bool m_useLocaleCollation;

    bool m_guiVisualizeDirtyRegions;
    int  m_guiAlgorithmDirtyRegions;
    bool m_guiSmartRedraw;
    unsigned int m_addonPackageFolderSize;

    unsigned int m_cacheMemSize;
    unsigned int m_cacheBufferMode;
    unsigned int m_cacheChunkSize;
    float m_cacheReadFactor;

    bool m_jsonOutputCompact;
    unsigned int m_jsonTcpPort;

    bool m_enableMultimediaKeys;
    std::vector<std::string> m_settingsFiles;
    void ParseSettingsFile(const std::string &file);

    float GetLatencyTweak(float refreshrate);
    bool m_initialized;

    void SetDebugMode(bool debug);

    //! \brief Toggles dirty-region visualization
    void ToggleDirtyRegionVisualization() { m_guiVisualizeDirtyRegions = !m_guiVisualizeDirtyRegions; };

    // runtime settings which cannot be set from advancedsettings.xml
    std::string m_videoExtensions;
    std::string m_discStubExtensions;
    std::string m_subtitlesExtensions;
    std::string m_musicExtensions;
    std::string m_pictureExtensions;

    std::string m_stereoscopicregex_3d;
    std::string m_stereoscopicregex_sbs;
    std::string m_stereoscopicregex_tab;

    /*!< @brief position behavior of ass subtitles when setting "subtitle position on screen" set to "fixed"
    True to show at the fixed position set in video calibration
    False to show at the bottom of video (default) */
    bool m_videoAssFixedWorks;

    bool m_openGlDebugging;

    std::string m_userAgent;
    uint32_t m_nfsTimeout;
    int m_nfsRetries;

  private:
    void Initialize();
    void Clear();
    void SetExtraArtwork(const TiXmlElement* arttypes, std::vector<std::string>& artworkMap);
    void MigrateOldArtSettings();
};
