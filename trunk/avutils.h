// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
//

#ifndef AVUTILS_H_
#define AVUTILS_H_

#include "ffmpeg/libavformat/avformat.h"
#include "ffmpeg/libavcodec/avcodec.h"

int GetMP4Title(const char *file, char *title_buf);

class CAVInfo {
		bool m_have_vstream, m_have_astream;
		bool m_codec_ok;
		char *m_stream_error;
		
		int m_sec, m_usec;
		int m_width, m_height;
		float m_fps;
		int m_frame_count;
		
		AVFormatContext *m_fctx;
		AVStream *m_st;
		AVCodecContext *m_acctx;
		AVCodec *m_codec;
		int m_videoStream;
		AVFrame *m_pFrame, *m_pFrameRGB;
		unsigned char *m_img_data;
		
		// same size as in libavformat
		char m_title[512];

		// call to mpeg4ip to read title
		void ReadMP4(const char *file);
	public:
		CAVInfo(const char *file);
		CAVInfo()
		{
			/* for stl */ 
			m_img_data = 0;
			m_fctx = 0;
			m_st = 0;
			m_acctx = 0;
			m_codec = 0;
			m_pFrame = 0;
			m_pFrameRGB = 0;
		}
		~CAVInfo();
		
		bool HaveVStream() { return m_have_vstream; }
		bool HaveAStream() { return m_have_astream; }
		bool CodecOk() { return m_codec_ok; }
		const char *InputError() { return m_stream_error; }
		
		int Sec() { return m_sec; }
		int Usec() { return m_usec; }
		
		float Fps() { return m_fps; }
		int W() { return m_width; }
		int H() { return m_height; }
		
		int FrameCount() { return m_frame_count; }
		
		bool Seek(int secs);
		bool GetNextFrame();
		uint8_t *ImageData() { return (uint8_t *)m_img_data; }
		
		const char *Title() { return &m_title[0]; }
};


bool CanDoPSP();

/*
 * Instead of running ffmpeg in separate process and parse its
 * output, hoping for the best, I will take few files from ffmpeg
 * source code and "glue" them here.
 * ffmpeg.c contains actual encoder loop, which is so fucked-up, I
 * could not imagine. That's why I can't just rewrite it in cpp and
 * be happy.
 */
class CFFmpeg_Glue {
	public:
		CFFmpeg_Glue();
		~CFFmpeg_Glue();
		
		//
		// Check if version is ok and can do PSP encoding
		//
		bool IsValidVersion();
		
		//
		// Call to encoder loop
		//
//		bool RunTranscode(
//			const char *infile, const char *outfile,
//			const char *abitrate, const char *vbitrate,
//			const char *title,
//			const char *size, const char *v_pad, const char *h_pad,
//			int (*callback)(void *, int frame), void *uptr);
		bool RunTranscode(
			const char *infile, const char *outfile,
			int abitrate, int vbitrate,
			int v_size, int h_size,
			int v_pad, int h_pad,
			const char *title,
			int (*callback)(void *, int frame), void *uptr);

		//
		// Call to create thumbnail image.
		// offset have firmat hh:mm:ss.SS
		//
		bool RunThumbnail(const char *infile, const char *outfile,
			const char *offset, const char *size, const char *v_pad, const char *h_pad);
};

#endif /*AVUTILS_H_*/
