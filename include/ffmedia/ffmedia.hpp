#pragma once

// Stable convenience header for the installed FFMedia C++ SDK. Optional
// modules are included only when the corresponding library feature was built.
#include <module/base_config.h>
#include <module/ffmedia_abi.hpp>
#include <module/module_app.hpp>
#include <module/module_media.hpp>

#include <module/vi/module_cam.hpp>
#include <module/vi/module_fileReader.hpp>
#include <module/vi/module_memReader.hpp>
#include <module/vi/module_rtmpClient.hpp>
#include <module/vi/module_rtspClient.hpp>

#include <module/vp/module_mppdec.hpp>
#include <module/vp/module_mppenc.hpp>
#include <module/vp/module_rga.hpp>
#include <module/vp/module_videoStack.hpp>

#include <module/vo/module_drmDisplay.hpp>
#include <module/vo/module_fileWriter.hpp>
#include <module/vo/module_gb28181Client.hpp>
#include <module/vo/module_rtmpServer.hpp>
#include <module/vo/module_rtspServer.hpp>

#if AUDIO_SUPPORT
#include <module/vi/module_alsaCapture.hpp>
#include <module/vp/module_aacdec.hpp>
#include <module/vp/module_aacenc.hpp>
#include <module/vo/module_alsaPlayBack.hpp>
#endif

#if FFMPEG_SUPPORT
#include <module/vi/module_ffmpegDemux.hpp>
#include <module/vo/module_ffmpegMux.hpp>
#endif

#if OPENGL_SUPPORT
#include <module/vp/module_imageProcessor.hpp>
#include <module/vo/module_rendererVideo.hpp>
#endif

#if INFERENCE_SUPPORT
#include <module/vp/module_inference.hpp>
#endif
