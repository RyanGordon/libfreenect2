/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2014 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */

#include <libfreenect2/rgb_packet_processor.h>

#include <opencv2/opencv.hpp>
#include <jpeglib.h>
#include <dlfcn.h>
#include <sys/time.h>

namespace libfreenect2
{

void abort_jpeg_error(j_decompress_ptr info, const char *msg)
{
  jpeg_abort_decompress(info);
  throw std::runtime_error(msg);
}

void my_error_exit(j_common_ptr info)
{
  char buffer[JMSG_LENGTH_MAX];
  info->err->format_message(info, buffer);
  abort_jpeg_error((j_decompress_ptr)info, buffer);
}

struct tegra_source_mgr {
  struct jpeg_source_mgr pub;
  void *_unknown;
  /* Nvidia libjpeg.so writes beyond struct end to here.
	 It was a PITA to track this down from random segfaults */
  void *_buffer;
};

/* The JPEG frame extracted now still has some garbage after the end of the
 * actual JPEG image.  Tegra hardware acceleration can't handle garbage at
 * tail. We need to detect the end and remove the garbage.
 *
 * The following format is obtained by experiments.
 */
struct kinect2_jpeg_tail {
// JPEG EOI: ff d9
// char _pad_0xa5[]; //0-3 bytes alignment
// char _filler[filler_length] = "ZZZZ...";
  uint32_t _marker_9999;
  uint32_t _sequence2; //the same as sequence?
  uint32_t _filler_length;
  uint32_t _unknown1; //always zero (?)
  uint32_t _unknown2; //always zero (?)
  uint32_t _unknown3; //incrementing
  uint32_t _unknown4;
  uint32_t _unknown5; //0x3f80____
  uint32_t _marker_BBBB;
  uint32_t _buffer_length_again;
  uint32_t _unknown6; //0x3f800000
  uint32_t _unknown7; //always zero (?)
  uint32_t _unknown8; //always zero (?)
  uint32_t _unknown9; //always zero (?)
};

class TegraJpegRgbPacketProcessorImpl
{
private:
  struct jpeg_decompress_struct dinfo;

  struct jpeg_error_mgr jerr;

#define DEFINE_DL_FUNCTION(func) decltype(&func) _##func;
DEFINE_DL_FUNCTION(jpeg_std_error)
DEFINE_DL_FUNCTION(jpeg_CreateDecompress)
DEFINE_DL_FUNCTION(jpeg_destroy_decompress)
DEFINE_DL_FUNCTION(jpeg_mem_src)
DEFINE_DL_FUNCTION(jpeg_read_header)
DEFINE_DL_FUNCTION(jpeg_abort_decompress)
DEFINE_DL_FUNCTION(jpeg_start_decompress)
DEFINE_DL_FUNCTION(jpeg_read_scanlines)
DEFINE_DL_FUNCTION(jpeg_finish_decompress)
public:

  Frame *frame;

  double timing_acc;
  double timing_acc_n;

  double timing_current_start;

  TegraJpegRgbPacketProcessorImpl()
  {
    /* This fixed path is the Tegra accelerated libjpeg. */
    void *tegra_libjpeg = dlopen("/usr/lib/arm-linux-gnueabihf/tegra/libjpeg.so", RTLD_LAZY | RTLD_LOCAL);
#define IMPORT(func) *(void **)&_##func = dlsym(tegra_libjpeg, #func);
IMPORT(jpeg_std_error)
IMPORT(jpeg_CreateDecompress)
#define jpeg_CreateDecompress _jpeg_CreateDecompress
IMPORT(jpeg_destroy_decompress)
IMPORT(jpeg_mem_src)
IMPORT(jpeg_read_header)
IMPORT(jpeg_abort_decompress)
IMPORT(jpeg_start_decompress)
IMPORT(jpeg_read_scanlines)
IMPORT(jpeg_finish_decompress)

    dinfo.err = _jpeg_std_error(&jerr);
    jerr.error_exit = my_error_exit;

    jpeg_create_decompress(&dinfo);

    /* manually allocate to protect the tail (_buffer) */
    tegra_source_mgr *src = new tegra_source_mgr;
    dinfo.src = &src->pub;

    newFrame();

    size_t width = frame->width;
    size_t height = frame->height;
    size_t pitch = frame->width * frame->bytes_per_pixel;

    timing_acc = 0.0;
    timing_acc_n = 0.0;
    timing_current_start = 0.0;
  }

  ~TegraJpegRgbPacketProcessorImpl()
  {
    delete dinfo.src;
    _jpeg_destroy_decompress(&dinfo);
    //delete frame;
  }

  void newFrame()
  {
    frame = new Frame(1920, 1080, 4);

  }

  void startTiming()
  {
    timing_current_start = cv::getTickCount();
  }

  void stopTiming()
  {
    timing_acc += (cv::getTickCount() - timing_current_start) / cv::getTickFrequency();
    timing_acc_n += 1.0;

    if(timing_acc_n >= 100.0)
    {
      double avg = (timing_acc / timing_acc_n);
      std::cout << "[TegraJpegRgbPacketProcessor] avg. time: " << (avg * 1000) << "ms -> ~" << (1.0/avg) << "Hz" << std::endl;
      timing_acc = 0.0;
      timing_acc_n = 0.0;
    }
  }


  void decompress(unsigned char *buf, size_t len)
  {
    /* Nvidia libjpeg.so reads and writes dinfo.src->_buffer.
     * Manually fill in this pointer to avoid allocation on it.
     */
    tegra_source_mgr *src = (tegra_source_mgr *)dinfo.src;
    src->_buffer = buf;
    _jpeg_mem_src(&dinfo, buf, len);
    _jpeg_read_header(&dinfo, TRUE);

    /* Not clear if these have real effect on accelerated decoding.
     * There might be no penalty with enabling.
     */
    dinfo.dct_method = JDCT_FASTEST;
    dinfo.do_fancy_upsampling = FALSE;
    dinfo.do_block_smoothing = FALSE;

    if (dinfo.progressive_mode)
      abort_jpeg_error(&dinfo, "Tegra HW doesn't support progressive JPEG; use TurboJPEG");

    if (!dinfo.tegra_acceleration)
      abort_jpeg_error(&dinfo, "Tegra HW acceleration is disabled");

    if (frame->width != dinfo.image_width || frame->height != dinfo.image_height)
      abort_jpeg_error(&dinfo, "image dimensions does not match preset");

    dinfo.out_color_space = JCS_RGBA_8888;

    _jpeg_start_decompress(&dinfo);

    /* TegraJPEG jpeg_start_decompress does not reset output_scanline.
     * We have to clear this otherwise dinfo is messed up for
     * jpeg_read_scanlines with loop.
     */
    dinfo.output_scanline = 0;

    /* Hardware acceleration returns the entire surface at once.
     * The normal way with software decoding uses jpeg_read_scanlines with loop.
     */
    if (jpeg_read_scanlines(&dinfo, NULL, 0) != dinfo.output_height)
      abort_jpeg_error(&dinfo, "Incomplete decoding result");

    /* Empirically: 1 surface for RGBA; 3 surfaces for YUV */
    size_t pitch = dinfo.jpegTegraMgr->pitch[0];
    unsigned char *surface = dinfo.jpegTegraMgr->buff[0];
    if (!pitch || !surface)
      abort_jpeg_error(&dinfo, "Empty result buffer");

    if (frame-> width * frame->height * frame->bytes_per_pixel != pitch * dinfo.output_height)
      abort_jpeg_error(&dinfo, "buffer size mismatch");

    /* memcpy is very slow, and no reason not to use zero-copy here */
    //memcpy(frame->data, surface, pitch * dinfo.output_height);
    *(void **)frame->data = surface;

    _jpeg_finish_decompress(&dinfo);
  }

  bool isEoi(const unsigned char *p, size_t len)
  {
    if (len < 2)
      return false;
    return p[len - 2] == 0xff && p[len - 1] == 0xd9;
  }

  bool findJpegEoi(const RgbPacket &p, size_t &jlen)
  {
    jlen = p.jpeg_buffer_length;

    if (isEoi(p.jpeg_buffer, jlen))
      return true;

    size_t tail_size = sizeof(kinect2_jpeg_tail);

    if (jlen < tail_size)
      return false;
    jlen -= tail_size;

    unsigned char *tail;
    tail = p.jpeg_buffer + jlen;
    uint32_t filler_length = ((kinect2_jpeg_tail *)tail)->_filler_length;

    if (jlen < filler_length)
      return false;
    jlen -= filler_length;

    for (size_t i = 0; i < 4; i++, jlen--)
      if (isEoi(p.jpeg_buffer, jlen))
        return true;

    return false;
  }
};

TegraJpegRgbPacketProcessor::TegraJpegRgbPacketProcessor() :
    impl_(new TegraJpegRgbPacketProcessorImpl())
{
}

TegraJpegRgbPacketProcessor::~TegraJpegRgbPacketProcessor()
{
  delete impl_;
}

void TegraJpegRgbPacketProcessor::process(const RgbPacket &packet)
{
  if(listener_ != 0)
  {
    impl_->startTiming();
    /* Nvidia libjpeg can't handle garbage after JPEG EOI */
    size_t actual_jpeg_length;
    bool r = impl_->findJpegEoi(packet, actual_jpeg_length);
    if (r) {
      try {
        impl_->decompress(packet.jpeg_buffer, actual_jpeg_length);
        if (listener_->onNewFrame(Frame::Color, impl_->frame))
        {
          impl_->newFrame();
        }
      }
      catch (const std::runtime_error &err)
      {
        std::cerr << "jpeg error " << err.what() << std::endl;
      }

    }
    else
    {
      std::cerr << "[TegraJpegRgbPacketProcessor::doProcess] Can't find JPEG EOI" << std::endl;
    }

    impl_->stopTiming();
  }
}

} /* namespace libfreenect2 */
