/*
 *      Copyright (C) 2005-2013 Team XBMC
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

#include "RBP.h"
#if defined(TARGET_RASPBERRY_PI)

#include <assert.h>
#include "settings/Settings.h"
#include "settings/AdvancedSettings.h"
#include "utils/log.h"

#include "cores/omxplayer/OMXImage.h"

#include "guilib/GraphicContext.h"
#include "settings/DisplaySettings.h"

#include <sys/ioctl.h>
#include "rpi/rpi_user_vcsm.h"
#include "utils/TimeUtils.h"

#define MAJOR_NUM 100
#define IOCTL_MBOX_PROPERTY _IOWR(MAJOR_NUM, 0, char *)
#define DEVICE_FILE_NAME "/dev/vcio"

static int mbox_open();
static void mbox_close(int file_desc);

CRBP::CRBP()
{
  m_initialized     = false;
  m_omx_initialized = false;
  m_DllBcmHost      = new DllBcmHost();
  m_OMX             = new COMXCore();
  m_display = DISPMANX_NO_HANDLE;
  m_last_pll_adjust = 1.0;
  m_p = NULL;
  m_x = 0;
  m_y = 0;
  m_enabled = 0;
  m_mb = mbox_open();
  vcsm_init();
  m_vsync_count = 0;
  m_vsync_time = 0;
}

CRBP::~CRBP()
{
  Deinitialize();
  delete m_OMX;
  delete m_DllBcmHost;
}

void CRBP::InitializeSettings()
{
  if (m_initialized && g_advancedSettings.m_cacheMemSize == ~0U)
    g_advancedSettings.m_cacheMemSize = m_arm_mem < 256 ? 1024 * 1024 * 2 : 1024 * 1024 * 20;
}

bool CRBP::Initialize()
{
  CSingleLock lock(m_critSection);
  if (m_initialized)
    return true;

  m_initialized = m_DllBcmHost->Load();
  if(!m_initialized)
    return false;

  m_DllBcmHost->bcm_host_init();

  m_omx_initialized = m_OMX->Initialize();
  if(!m_omx_initialized)
    return false;

  char response[80] = "";
  m_arm_mem = 0;
  m_gpu_mem = 0;
  m_codec_mpg2_enabled = false;
  m_codec_wvc1_enabled = false;

  if (vc_gencmd(response, sizeof response, "get_mem arm") == 0)
    vc_gencmd_number_property(response, "arm", &m_arm_mem);
  if (vc_gencmd(response, sizeof response, "get_mem gpu") == 0)
    vc_gencmd_number_property(response, "gpu", &m_gpu_mem);

  if (vc_gencmd(response, sizeof response, "codec_enabled MPG2") == 0)
    m_codec_mpg2_enabled = strcmp("MPG2=enabled", response) == 0;
  if (vc_gencmd(response, sizeof response, "codec_enabled WVC1") == 0)
    m_codec_wvc1_enabled = strcmp("WVC1=enabled", response) == 0;

  if (m_gpu_mem < 128)
    setenv("V3D_DOUBLE_BUFFER", "1", 1);

  m_gui_resolution_limit = CSettings::GetInstance().GetInt("videoscreen.limitgui");
  if (!m_gui_resolution_limit)
    m_gui_resolution_limit = m_gpu_mem < 128 ? 720:1080;

  InitializeSettings();

  g_OMXImage.Initialize();
  m_omx_image_init = true;
  return true;
}

void CRBP::LogFirmwareVerison()
{
  char  response[1024];
  m_DllBcmHost->vc_gencmd(response, sizeof response, "version");
  response[sizeof(response) - 1] = '\0';
  CLog::Log(LOGNOTICE, "Raspberry PI firmware version: %s", response);
  CLog::Log(LOGNOTICE, "ARM mem: %dMB GPU mem: %dMB MPG2:%d WVC1:%d", m_arm_mem, m_gpu_mem, m_codec_mpg2_enabled, m_codec_wvc1_enabled);
  CLog::Log(LOGNOTICE, "cache.memorysize: %dMB",  g_advancedSettings.m_cacheMemSize >> 20);
  m_DllBcmHost->vc_gencmd(response, sizeof response, "get_config int");
  response[sizeof(response) - 1] = '\0';
  CLog::Log(LOGNOTICE, "Config:\n%s", response);
  m_DllBcmHost->vc_gencmd(response, sizeof response, "get_config str");
  response[sizeof(response) - 1] = '\0';
  CLog::Log(LOGNOTICE, "Config:\n%s", response);
}

static void vsync_callback_static(DISPMANX_UPDATE_HANDLE_T u, void *arg)
{
  CRBP *rbp = reinterpret_cast<CRBP*>(arg);
  rbp->VSyncCallback();
}

DISPMANX_DISPLAY_HANDLE_T CRBP::OpenDisplay(uint32_t device)
{
  CSingleLock lock(m_critSection);
  if (m_display == DISPMANX_NO_HANDLE)
  {
    m_display = vc_dispmanx_display_open( 0 /*screen*/ );
    int s = vc_dispmanx_vsync_callback(m_display, vsync_callback_static, (void *)this);
    assert(s == 0);
    init_cursor();
  }
  return m_display;
}

void CRBP::CloseDisplay(DISPMANX_DISPLAY_HANDLE_T display)
{
  CSingleLock lock(m_critSection);
  uninit_cursor();
  assert(display == m_display);
  int s = vc_dispmanx_vsync_callback(m_display, NULL, NULL);
  assert(s == 0);
  vc_dispmanx_display_close(m_display);
  m_display = DISPMANX_NO_HANDLE;
  m_last_pll_adjust = 1.0;
}

void CRBP::GetDisplaySize(int &width, int &height)
{
  CSingleLock lock(m_critSection);
  DISPMANX_MODEINFO_T info;
  if (m_display != DISPMANX_NO_HANDLE && vc_dispmanx_display_get_info(m_display, &info) == 0)
  {
    width = info.width;
    height = info.height;
  }
  else
  {
    width = 0;
    height = 0;
  }
}

unsigned char *CRBP::CaptureDisplay(int width, int height, int *pstride, bool swap_red_blue, bool video_only)
{
  DISPMANX_RESOURCE_HANDLE_T resource;
  VC_RECT_T rect;
  unsigned char *image = NULL;
  uint32_t vc_image_ptr;
  int stride;
  uint32_t flags = 0;

  if (video_only)
    flags |= DISPMANX_SNAPSHOT_NO_RGB|DISPMANX_SNAPSHOT_FILL;
  if (swap_red_blue)
    flags |= DISPMANX_SNAPSHOT_SWAP_RED_BLUE;
  if (!pstride)
    flags |= DISPMANX_SNAPSHOT_PACK;

  stride = ((width + 15) & ~15) * 4;

  CSingleLock lock(m_critSection);
  if (m_display != DISPMANX_NO_HANDLE)
  {
    image = new unsigned char [height * stride];
    resource = vc_dispmanx_resource_create( VC_IMAGE_RGBA32, width, height, &vc_image_ptr );

    vc_dispmanx_snapshot(m_display, resource, (DISPMANX_TRANSFORM_T)flags);

    vc_dispmanx_rect_set(&rect, 0, 0, width, height);
    vc_dispmanx_resource_read_data(resource, &rect, image, stride);
    vc_dispmanx_resource_delete( resource );
  }
  if (pstride)
    *pstride = stride;
  return image;
}

void CRBP::VSyncCallback()
{
  CSingleLock lock(m_vsync_lock);
  m_vsync_count++;
  m_vsync_time = CurrentHostCounter();
  m_vsync_cond.notifyAll();
}

uint32_t CRBP::WaitVsync(uint32_t target)
{
  CSingleLock vlock(m_vsync_lock);
  DISPMANX_DISPLAY_HANDLE_T display = m_display;
  XbmcThreads::EndTime delay(50);
  if (target == ~0U)
    target = m_vsync_count+1;
  while (!delay.IsTimePast())
  {
    if ((signed)(m_vsync_count - target) >= 0)
      break;
    if (!m_vsync_cond.wait(vlock, delay.MillisLeft()))
      break;
  }
  if ((signed)(m_vsync_count - target) < 0)
    CLog::Log(LOGDEBUG, "CRBP::%s no  vsync %d/%d display:%x(%x) delay:%d", __FUNCTION__, m_vsync_count, target, m_display, display, delay.MillisLeft());

  return m_vsync_count;
}

uint32_t CRBP::LastVsync(int64_t &time)
{
  CSingleLock lock(m_vsync_lock);
  time = m_vsync_time;
  return m_vsync_count;
}

uint32_t CRBP::LastVsync()
{
  int64_t time = 0;
  return LastVsync(time);
}

void CRBP::Deinitialize()
{
  if (m_omx_image_init)
    g_OMXImage.Deinitialize();

  if(m_omx_initialized)
    m_OMX->Deinitialize();

  if (m_display)
    CloseDisplay(m_display);

  m_DllBcmHost->bcm_host_deinit();

  if(m_initialized)
    m_DllBcmHost->Unload();

  m_omx_image_init  = false;
  m_initialized     = false;
  m_omx_initialized = false;
  uninit_cursor();
  delete m_p;
  m_p = NULL;
  if (m_mb)
    mbox_close(m_mb);
  m_mb = 0;
  vcsm_exit();
}

void CRBP::SuspendVideoOutput()
{
  CLog::Log(LOGDEBUG, "Raspberry PI suspending video output\n");
  char response[80];
  m_DllBcmHost->vc_gencmd(response, sizeof response, "display_power 0");
}

void CRBP::ResumeVideoOutput()
{
  char response[80];
  m_DllBcmHost->vc_gencmd(response, sizeof response, "display_power 1");
  CLog::Log(LOGDEBUG, "Raspberry PI resuming video output\n");
}

static int mbox_property(int file_desc, void *buf)
{
   int ret_val = ioctl(file_desc, IOCTL_MBOX_PROPERTY, buf);

   if (ret_val < 0)
   {
     CLog::Log(LOGERROR, "%s: ioctl_set_msg failed:%d", __FUNCTION__, ret_val);
   }
   return ret_val;
}

static int mbox_open()
{
   int file_desc;

   // open a char device file used for communicating with kernel mbox driver
   file_desc = open(DEVICE_FILE_NAME, 0);
   if (file_desc < 0)
     CLog::Log(LOGERROR, "%s: Can't open device file: %s (%d)", __FUNCTION__, DEVICE_FILE_NAME, file_desc);

   return file_desc;
}

static void mbox_close(int file_desc)
{
  close(file_desc);
}

static unsigned mem_lock(int file_desc, unsigned handle)
{
   int i=0;
   unsigned p[32];
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request

   p[i++] = 0x3000d; // (the tag id)
   p[i++] = 4; // (size of the buffer)
   p[i++] = 4; // (size of the data)
   p[i++] = handle;

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof *p; // actual size

   mbox_property(file_desc, p);
   return p[5];
}

unsigned mem_unlock(int file_desc, unsigned handle)
{
   int i=0;
   unsigned p[32];
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request

   p[i++] = 0x3000e; // (the tag id)
   p[i++] = 4; // (size of the buffer)
   p[i++] = 4; // (size of the data)
   p[i++] = handle;

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof *p; // actual size

   mbox_property(file_desc, p);
   return p[5];
}

unsigned int mailbox_set_cursor_info(int file_desc, int width, int height, int format, uint32_t buffer, int hotspotx, int hotspoty)
{
   int i=0;
   unsigned int p[32];
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request
   p[i++] = 0x00008010; // set cursor state
   p[i++] = 24; // buffer size
   p[i++] = 24; // data size

   p[i++] = width;
   p[i++] = height;
   p[i++] = format;
   p[i++] = buffer;           // ptr to VC memory buffer. Doesn't work in 64bit....
   p[i++] = hotspotx;
   p[i++] = hotspoty;

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof(*p); // actual size

   mbox_property(file_desc, p);
   return p[5];

}

unsigned int mailbox_set_cursor_position(int file_desc, int enabled, int x, int y)
{
   int i=0;
   unsigned p[32];
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request
   p[i++] = 0x00008011; // set cursor state
   p[i++] = 12; // buffer size
   p[i++] = 12; // data size

   p[i++] = enabled;
   p[i++] = x;
   p[i++] = y;

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof *p; // actual size

   mbox_property(file_desc, p);
   return p[5];
}

CGPUMEM::CGPUMEM(unsigned int numbytes, bool cached)
{
  m_numbytes = numbytes;
  m_vcsm_handle = vcsm_malloc_cache(numbytes, cached ? VCSM_CACHE_TYPE_HOST : VCSM_CACHE_TYPE_NONE, (char *)"CGPUMEM");
  assert(m_vcsm_handle);
  m_vc_handle = vcsm_vc_hdl_from_hdl(m_vcsm_handle);
  assert(m_vc_handle);
  m_arm = vcsm_lock(m_vcsm_handle);
  assert(m_arm);
  m_vc = mem_lock(g_RBP.GetMBox(), m_vc_handle);
  assert(m_vc);
}

CGPUMEM::~CGPUMEM()
{
  mem_unlock(g_RBP.GetMBox(), m_vc_handle);
  vcsm_unlock_ptr(m_arm);
  vcsm_free(m_vcsm_handle);
}

// Call this to clean and invalidate a region of memory
void CGPUMEM::Flush()
{
  struct vcsm_user_clean_invalid_s iocache = {};
  iocache.s[0].handle = m_vcsm_handle;
  iocache.s[0].cmd = 3; // clean+invalidate
  iocache.s[0].addr = (int) m_arm;
  iocache.s[0].size  = m_numbytes;
  vcsm_clean_invalid( &iocache );
}

#define T 0
#define W 0xffffffff
#define B 0xff000000

const static uint32_t default_cursor_pixels[] =
{
   B,B,B,B,B,B,B,B,B,T,T,T,T,T,T,T,
   B,W,W,W,W,W,W,B,T,T,T,T,T,T,T,T,
   B,W,W,W,W,W,B,T,T,T,T,T,T,T,T,T,
   B,W,W,W,W,B,T,T,T,T,T,T,T,T,T,T,
   B,W,W,W,W,W,B,T,T,T,T,T,T,T,T,T,
   B,W,W,B,W,W,W,B,T,T,T,T,T,T,T,T,
   B,W,B,T,B,W,W,W,B,T,T,T,T,T,T,T,
   B,B,T,T,T,B,W,W,W,B,T,T,T,T,T,T,
   B,T,T,T,T,T,B,W,W,W,B,T,T,T,T,T,
   T,T,T,T,T,T,T,B,W,W,W,B,T,T,T,T,
   T,T,T,T,T,T,T,T,B,W,W,W,B,T,T,T,
   T,T,T,T,T,T,T,T,T,B,W,W,W,B,T,T,
   T,T,T,T,T,T,T,T,T,T,B,W,W,W,B,T,
   T,T,T,T,T,T,T,T,T,T,T,B,W,W,W,B,
   T,T,T,T,T,T,T,T,T,T,T,T,B,W,B,T,
   T,T,T,T,T,T,T,T,T,T,T,T,T,B,T,T
};

#undef T
#undef W
#undef B

void CRBP::init_cursor()
{
  CLog::Log(LOGDEBUG, "%s", __FUNCTION__);
  if (!m_mb)
    return;
  if (!m_p)
    m_p = new CGPUMEM(64 * 64 * 4, false);
  if (m_p && m_p->m_arm && m_p->m_vc)
    set_cursor(default_cursor_pixels, 16, 16, 0, 0);
}

void CRBP::set_cursor(const void *pixels, int width, int height, int hotspot_x, int hotspot_y)
{
  if (!m_mb || !m_p || !m_p->m_arm || !m_p->m_vc || !pixels || width * height > 64 * 64)
    return;
  CLog::Log(LOGDEBUG, "%s %dx%d %p", __FUNCTION__, width, height, pixels);
  memcpy(m_p->m_arm, pixels, width * height * 4);
  unsigned int s = mailbox_set_cursor_info(m_mb, width, height, 0, m_p->m_vc, hotspot_x, hotspot_y);
  assert(s == 0);
}

void CRBP::update_cursor(int x, int y, bool enabled)
{
  if (!m_mb || !m_p || !m_p->m_arm || !m_p->m_vc)
    return;

  RESOLUTION res = g_graphicsContext.GetVideoResolution();
  CRect gui(0, 0, CDisplaySettings::GetInstance().GetResolutionInfo(res).iWidth, CDisplaySettings::GetInstance().GetResolutionInfo(res).iHeight);
  CRect display(0, 0, CDisplaySettings::GetInstance().GetResolutionInfo(res).iScreenWidth, CDisplaySettings::GetInstance().GetResolutionInfo(res).iScreenHeight);

  int x2 = x * display.Width()  / gui.Width();
  int y2 = y * display.Height() / gui.Height();

  if (g_graphicsContext.GetStereoMode() == RENDER_STEREO_MODE_SPLIT_HORIZONTAL)
    y2 *= 2;
  else if (g_graphicsContext.GetStereoMode() == RENDER_STEREO_MODE_SPLIT_VERTICAL)
    x2 *= 2;
  if (m_x != x2 || m_y != y2 || m_enabled != enabled)
    mailbox_set_cursor_position(m_mb, enabled, x2, y2);
  m_x = x2;
  m_y = y2;
  m_enabled = enabled;
}

void CRBP::uninit_cursor()
{
  if (!m_mb || !m_p || !m_p->m_arm || !m_p->m_vc)
    return;
  CLog::Log(LOGDEBUG, "%s", __FUNCTION__);
  mailbox_set_cursor_position(m_mb, 0, 0, 0);
}

double CRBP::AdjustHDMIClock(double adjust)
{
  char response[80];
  vc_gencmd(response, sizeof response, "hdmi_adjust_clock %f", adjust);
  char *p = strchr(response, '=');
  if (p)
    m_last_pll_adjust = atof(p+1);
  CLog::Log(LOGDEBUG, "CRBP::%s(%.4f) = %.4f", __func__, adjust, m_last_pll_adjust);
  return m_last_pll_adjust;
}

#include "cores/VideoPlayer/DVDClock.h"
#include "cores/VideoPlayer/DVDCodecs/DVDCodecUtils.h"
#include "cores/VideoPlayer/VideoRenderers/HwDecRender/MMALRenderer.h"
#include "utils/log.h"

extern "C"
{
  #include "libswscale/swscale.h"
  #include "libavutil/imgutils.h"
}

CPixelConverter::CPixelConverter() :
  m_renderFormat(RENDER_FMT_MMAL),
  m_width(0),
  m_height(0),
  m_swsContext(nullptr),
  m_buf(nullptr)
{
}

static struct {
  AVPixelFormat pixfmt;
  AVPixelFormat targetfmt;
} pixfmt_target_table[] =
{
   {AV_PIX_FMT_BGR0,      AV_PIX_FMT_BGR0},
   {AV_PIX_FMT_RGB565LE,  AV_PIX_FMT_RGB565LE},
   {AV_PIX_FMT_NONE,      AV_PIX_FMT_NONE}
};

static AVPixelFormat pixfmt_to_target(AVPixelFormat pixfmt)
{
  unsigned int i;
  for (i = 0; pixfmt_target_table[i].pixfmt != AV_PIX_FMT_NONE; i++)
    if (pixfmt_target_table[i].pixfmt == pixfmt)
      break;
  return pixfmt_target_table[i].targetfmt;
}

bool CPixelConverter::Open(AVPixelFormat pixfmt, AVPixelFormat targetfmt, unsigned int width, unsigned int height)
{
  targetfmt = pixfmt_to_target(pixfmt);
  CLog::Log(LOGDEBUG, "CPixelConverter::%s: pixfmt:%d(%s) targetfmt:%d(%s) %dx%d", __FUNCTION__, pixfmt, av_get_pix_fmt_name(pixfmt), targetfmt, av_get_pix_fmt_name(targetfmt), width, height);
  if (targetfmt == AV_PIX_FMT_NONE || width == 0 || height == 0)
  {
    CLog::Log(LOGERROR, "%s: Invalid target pixel format: %d", __FUNCTION__, targetfmt);
    assert(0);
    return false;
  }

  m_width = width;
  m_height = height;

  m_swsContext = sws_getContext(width, height, pixfmt,
                                width, height, targetfmt,
                                SWS_FAST_BILINEAR, NULL, NULL, NULL);
  if (!m_swsContext)
  {
    CLog::Log(LOGERROR, "%s: Failed to create swscale context", __FUNCTION__);
    return false;
  }

  if (targetfmt == AV_PIX_FMT_YUV420P)
    m_mmal_format = MMAL_ENCODING_I420;
  else if (targetfmt == AV_PIX_FMT_ARGB)
    m_mmal_format = MMAL_ENCODING_ARGB;
  else if (targetfmt == AV_PIX_FMT_RGBA)
    m_mmal_format = MMAL_ENCODING_RGBA;
  else if (targetfmt == AV_PIX_FMT_ABGR)
    m_mmal_format = MMAL_ENCODING_ABGR;
  else if (targetfmt == AV_PIX_FMT_BGRA || targetfmt == AV_PIX_FMT_BGR0)
    m_mmal_format = MMAL_ENCODING_BGRA;
  else if (targetfmt == AV_PIX_FMT_RGB24)
    m_mmal_format = MMAL_ENCODING_RGB24;
  else if (targetfmt == AV_PIX_FMT_BGR24)
    m_mmal_format = MMAL_ENCODING_BGR24;
  else if (targetfmt == AV_PIX_FMT_RGB565 || targetfmt == AV_PIX_FMT_RGB565LE)
    m_mmal_format = MMAL_ENCODING_RGB16;
  else if (targetfmt == AV_PIX_FMT_BGR565)
    m_mmal_format = MMAL_ENCODING_BGR16;
  else
    m_mmal_format = MMAL_ENCODING_UNKNOWN;

  if (m_mmal_format == MMAL_ENCODING_UNKNOWN)
    return false;

  /* Create dummy component with attached pool */
  m_pool = std::make_shared<CMMALPool>(MMAL_COMPONENT_DEFAULT_VIDEO_DECODER, false, MMAL_NUM_OUTPUT_BUFFERS, 0, MMAL_ENCODING_I420, MMALStateFFDec);

  return true;
}

// allocate a new picture (AV_PIX_FMT_YUV420P)
DVDVideoPicture* CPixelConverter::AllocatePicture(int iWidth, int iHeight)
{
  MMAL::CMMALYUVBuffer *omvb = nullptr;
  DVDVideoPicture* pPicture = new DVDVideoPicture;
  // gpu requirements
  int w = (iWidth + 31) & ~31;
  int h = (iHeight + 15) & ~15;
  if (pPicture)
  {
    assert(m_pool);
    m_pool->SetFormat(m_mmal_format, iWidth, iHeight, w, h, 0, nullptr);
    omvb = dynamic_cast<MMAL::CMMALYUVBuffer *>(m_pool->GetBuffer(500));
    if (!omvb || !omvb->mmal_buffer || !omvb->gmem || !omvb->gmem->m_arm)
    {
      CLog::Log(LOGERROR, "CPixelConverter::AllocatePicture, unable to allocate new video picture, out of memory.");
      delete pPicture;
      pPicture = NULL;
    }
    CGPUMEM *gmem = omvb->gmem;
    omvb->mmal_buffer->data = (uint8_t *)gmem->m_vc_handle;
    omvb->mmal_buffer->alloc_size = omvb->mmal_buffer->length = gmem->m_numbytes;
  }

  if (pPicture)
  {
    pPicture->MMALBuffer = omvb;
    pPicture->iWidth = iWidth;
    pPicture->iHeight = iHeight;
  }
  CLog::Log(LOGDEBUG, "CPixelConverter::AllocatePicture pic:%p omvb:%p mmal:%p %dx%d format:%.4s", pPicture, omvb, omvb ? omvb->mmal_buffer : nullptr, iWidth, iHeight, (char *)&m_mmal_format);
  return pPicture;
}

void CPixelConverter::FreePicture(DVDVideoPicture* pPicture)
{
  assert(pPicture);
  if (pPicture->MMALBuffer)
    pPicture->MMALBuffer->Release();
  delete pPicture;
}

void CPixelConverter::Dispose()
{
  m_pool->Close();
  m_pool = nullptr;

  if (m_swsContext)
  {
    sws_freeContext(m_swsContext);
    m_swsContext = nullptr;
  }

  if (m_buf)
  {
    FreePicture(m_buf);
    m_buf = nullptr;
  }
}

bool CPixelConverter::Decode(const uint8_t* pData, unsigned int size)
{
  if (pData == nullptr || size == 0 || m_swsContext == nullptr)
    return false;

  if (m_buf)
    FreePicture(m_buf);
  m_buf = AllocatePicture(m_width, m_height);
  if (!m_buf)
  {
    CLog::Log(LOGERROR, "%s: Failed to allocate picture of dimensions %dx%d", __FUNCTION__, m_width, m_height);
    return false;
  }

  uint8_t* dataMutable = const_cast<uint8_t*>(pData);

  const int stride = size / m_height;

  int bpp = 4;
  if (m_mmal_format == MMAL_ENCODING_ARGB || m_mmal_format == MMAL_ENCODING_RGBA || m_mmal_format == MMAL_ENCODING_ABGR || m_mmal_format == MMAL_ENCODING_BGRA)
    bpp = 4;
  else if (m_mmal_format == MMAL_ENCODING_RGB24 || m_mmal_format == MMAL_ENCODING_BGR24)
    bpp = 4;
  else if (m_mmal_format == MMAL_ENCODING_RGB16 || m_mmal_format == MMAL_ENCODING_BGR16)
    bpp = 2;
  else
  {
    CLog::Log(LOGERROR, "CPixelConverter::AllocatePicture, unknown format:%.4s", (char *)&m_mmal_format);
    return false;
  }

  MMAL::CMMALYUVBuffer *omvb = (MMAL::CMMALYUVBuffer *)m_buf->MMALBuffer;

  uint8_t* src[] =       { dataMutable,                       0, 0, 0 };
  int      srcStride[] = { stride,                            0, 0, 0 };
  uint8_t* dst[] =       { (uint8_t *)omvb->gmem->m_arm,      0, 0, 0 };
  int      dstStride[] = { (int)omvb->m_aligned_width * bpp,  0, 0, 0 };

  sws_scale(m_swsContext, src, srcStride, 0, m_height, dst, dstStride);

  CLog::Log(LOGDEBUG, "CPixelConverter::Decode pic:%p omvb:%p mmal:%p arm:%p %dx%d format:%.4s", m_buf, m_buf->MMALBuffer, nullptr, nullptr, m_buf->iWidth, m_buf->iHeight, "null");
  return true;
}

void CPixelConverter::GetPicture(DVDVideoPicture& dvdVideoPicture)
{
  dvdVideoPicture.dts            = DVD_NOPTS_VALUE;
  dvdVideoPicture.pts            = DVD_NOPTS_VALUE;

  for (int i = 0; i < 4; i++)
  {
    dvdVideoPicture.data[i]      = nullptr;
    dvdVideoPicture.iLineSize[i] = 0;
  }

  dvdVideoPicture.iFlags         = 0; // *not* DVP_FLAG_ALLOCATED
  dvdVideoPicture.color_matrix   = 4; // CONF_FLAGS_YUVCOEF_BT601
  dvdVideoPicture.color_range    = 0; // *not* CONF_FLAGS_YUV_FULLRANGE
  dvdVideoPicture.iWidth         = m_width;
  dvdVideoPicture.iHeight        = m_height;
  dvdVideoPicture.iDisplayWidth  = m_width; // TODO: Update if aspect ratio changes
  dvdVideoPicture.iDisplayHeight = m_height;
  dvdVideoPicture.format         = m_renderFormat;
  dvdVideoPicture.MMALBuffer     = m_buf->MMALBuffer;

  MMAL::CMMALYUVBuffer *omvb = (MMAL::CMMALYUVBuffer *)m_buf->MMALBuffer;

  // need to flush ARM cache so GPU can see it
  omvb->gmem->Flush();

  CLog::Log(LOGDEBUG, "CPixelConverter::GetPicture pic:%p omvb:%p mmal:%p arm:%p %dx%d format:%.4s", m_buf, m_buf->MMALBuffer, omvb->mmal_buffer, omvb->gmem->m_arm, dvdVideoPicture.iWidth, dvdVideoPicture.iHeight, (char *)&omvb->m_encoding);
}

#endif
