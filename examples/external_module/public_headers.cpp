#include <ffmedia/ffmedia.hpp>

#include <ffmedia/base/ff_log.h>
#include <ffmedia/base/ff_synchronize.hpp>
#include <ffmedia/base/ff_type.hpp>
#include <ffmedia/base/media_buffer.hpp>
#include <ffmedia/base/pixel_fmt.hpp>
#include <ffmedia/base/video_buffer.hpp>

#include <ffmedia/module/base_config.h>
#include <ffmedia/module/ff_media_consumer.hpp>
#include <ffmedia/module/ff_media_hookable.hpp>
#include <ffmedia/module/ff_media_parameter.hpp>
#include <ffmedia/module/ff_media_parameter_helpers.hpp>
#include <ffmedia/module/ff_media_producer.hpp>
#include <ffmedia/module/ffmedia_abi.hpp>
#include <ffmedia/module/media_channel.hpp>
#include <ffmedia/module/module_app.hpp>
#include <ffmedia/module/module_media.hpp>

#include <ffmedia/module/vi/module_cam.hpp>
#include <ffmedia/module/vi/module_fileReader.hpp>
#include <ffmedia/module/vi/module_memReader.hpp>
#include <ffmedia/module/vi/module_rtmpClient.hpp>
#include <ffmedia/module/vi/module_rtspClient.hpp>

#include <ffmedia/module/vp/module_mppdec.hpp>
#include <ffmedia/module/vp/module_mppenc.hpp>
#include <ffmedia/module/vp/module_rga.hpp>
#include <ffmedia/module/vp/module_videoStack.hpp>

#include <ffmedia/module/vo/module_drmDisplay.hpp>
#include <ffmedia/module/vo/module_fileWriter.hpp>
#include <ffmedia/module/vo/module_gb28181Client.hpp>
#include <ffmedia/module/vo/module_rtmpServer.hpp>
#include <ffmedia/module/vo/module_rtspServer.hpp>

#if AUDIO_SUPPORT
#include <ffmedia/module/vi/module_alsaCapture.hpp>
#include <ffmedia/module/vp/module_aacdec.hpp>
#include <ffmedia/module/vp/module_aacenc.hpp>
#include <ffmedia/module/vo/module_alsaPlayBack.hpp>
#endif

#if FFMPEG_SUPPORT
#include <ffmedia/module/vi/module_ffmpegDemux.hpp>
#include <ffmedia/module/vo/module_ffmpegMux.hpp>
#endif

#if OPENGL_SUPPORT
#include <ffmedia/module/vp/module_imageProcessor.hpp>
#include <ffmedia/module/vo/module_rendererVideo.hpp>
#endif

#if INFERENCE_SUPPORT
#include <ffmedia/module/vp/module_inference.hpp>
#endif

int ffmedia_public_headers_compile()
{
    return FFMEDIA_MODULE_ABI_VERSION > 0 ? 0 : 1;
}
