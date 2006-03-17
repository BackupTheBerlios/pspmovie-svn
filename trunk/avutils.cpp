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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * FFMPEG have a "feature" - it can't parse headers of MP4
 * files properly. Actually, all "atom" parsing thing is
 * missing. So, even it can generate correct header with
 * title, it can't read it later.
 * That's why mpeg4ip (libmp4v2) is used.
 */
#include <inttypes.h>

// mp4 need this
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;

#include <mp4.h>

#include "avutils.h"

#include "ffmpeg_glue.h"

bool CanDoPSP()
{
	if ( !avcodec_find_encoder(CODEC_ID_AAC) || !avcodec_find_encoder(CODEC_ID_MPEG4)) {
		return false;
	}
	AVOutputFormat *fmt = guess_format("psp", 0, 0);
	if ( !fmt || strcmp(fmt->name, "psp") ) {
		return false;
	}
	return true;
}

CAVInfo::CAVInfo(const char *filename)
{
	m_have_vstream = false;
	m_have_astream = false;
	m_codec_ok = false;
	m_title[0] = 0;
	m_img_data = 0;
	m_pFrameRGB = m_pFrame = 0;
	m_fctx = 0;
	
	if ( !filename ) {
		return;
	}
			
	if ( av_open_input_file(&m_fctx, filename, 0, 0, 0) != 0 ) {
		printf("av_open_input_file - error\n");
		m_stream_error = "Unable to open input file";
		return;
	}
	if( av_find_stream_info(m_fctx) < 0) {
		printf("av_find_stream_info - error\n");
		m_stream_error = "Input file have unrecognized format";
		return;
	}

 	m_sec = m_fctx->duration / AV_TIME_BASE;
    m_usec = m_fctx->duration % AV_TIME_BASE;
    m_width = m_height = 0;
	m_videoStream = -1;
	for(int i = 0; i < m_fctx->nb_streams; i++) {
		if ( m_fctx->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO) {
			m_width = m_fctx->streams[i]->codec->width;
			m_height = m_fctx->streams[i]->codec->height;
	        m_videoStream = i;
	        m_have_vstream = true;
 		}
		if ( m_fctx->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO) {
	        m_have_astream = true;
		}
		if ( m_have_vstream && m_have_astream ) {
    	    break;
		}
	}
	if(!m_have_vstream) {
    	printf("Didn't find a video stream\n");
		m_stream_error = "Input file have no video stream";
    	return ;
	}
	if(!m_have_astream) {
    	printf("Didn't find a audio stream\n");
		m_stream_error = "Input file have no audio stream";
    	return ;
	}

	// Get a pointer to the codec context for the video stream
	m_st = m_fctx->streams[m_videoStream];
	m_acctx = m_st->codec;
	m_fps = 1/av_q2d(m_st->time_base);
	if ( m_fps > 1000 ) {
		m_fps = av_q2d(m_st->r_frame_rate);
	}
	printf("Stream with %f fps\n", m_fps);
	/*
	if (st->r_frame_rate.den && st->r_frame_rate.num) {
		m_fps = av_q2d(st->r_frame_rate);
	} else {
		m_fps = 1/av_q2d(st->codec->time_base);
	}
	*/
	// Find the decoder for the video stream
	m_codec = avcodec_find_decoder(m_acctx->codec_id);
	if(m_codec && (avcodec_open(m_acctx, m_codec) == 0)) {
        m_codec_ok = true;
	} else {
		return;
	}
	m_frame_count = m_st->nb_frames ? m_st->nb_frames : int( (m_sec + 1) * m_fps);
	
    // Allocate an AVFrame structure
	m_pFrame = avcodec_alloc_frame();

	int numBytes = avpicture_get_size(PIX_FMT_RGBA32, m_acctx->width, m_acctx->height);
	m_img_data = new uint8_t[numBytes];
    m_pFrameRGB = avcodec_alloc_frame();

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    avpicture_fill((AVPicture *)m_pFrameRGB, m_img_data, PIX_FMT_RGBA32,
		m_acctx->width, m_acctx->height);
	
	//
	// If we have .mp4 file, attempt to read title
	//
	const char *ext = filename + strlen(filename) - 4;
	if ( !strcmp(ext, ".mp4") || !strcmp(ext, ".MP4") ) {
		ReadMP4(filename);
	}
}

CAVInfo::~CAVInfo()
{
	if ( m_fctx ) {
		av_close_input_file(m_fctx);
	}
	if ( m_pFrameRGB ) {
	    av_free(m_pFrameRGB);
	}

    if ( m_pFrame ) {
	    av_free(m_pFrame);
    }
    if ( m_img_data ) {
    	delete [] m_img_data;
    }
}

bool CAVInfo::Seek(int secs)
{
	int res = av_seek_frame(m_fctx, m_videoStream, (int)(secs * m_fps), 0);
	return res == 0;
}

bool CAVInfo::GetNextFrame()
{
    AVPacket packet;
    int frameFinished;
    
	while( av_read_frame(m_fctx, &packet) >=0 ) {
	    // Is this a packet from the video stream?
	    if(packet.stream_index == m_videoStream) {
	        // Decode video frame
	        avcodec_decode_video(m_acctx, m_pFrame, &frameFinished, 
	            packet.data, packet.size);
	
	        // Did we get a video frame?
	        if(frameFinished) {
	            // Convert the image from its native format to RGB
	            img_convert((AVPicture *)m_pFrameRGB, PIX_FMT_RGBA32, 
	                (AVPicture*)m_pFrame, m_acctx->pix_fmt, m_acctx->width, 
	                m_acctx->height);
				return true;
	            // Process the video frame (save to disk etc.)
	            //DoSomethingWithTheImage(pFrameRGB);
	        }
	    }
	
	    // Free the packet that was allocated by av_read_frame
	    av_free_packet(&packet);
	}
	return false;
}

uint32_t read_be32(uint8_t *data)
{
	uint32_t val = (data[0] << 24) | (data[1] << 16) |
		(data[2] << 8) | data[3];
	return val;
}

int16_t read_be16(uint8_t *data)
{
	uint16_t val = (data[0] << 8) | data[1];
	return val;
}

int MP4_moov_uuid_parse(uint8_t *uuid_data, uint32_t data_len, char *title)
{
	uint8_t *data = uuid_data;
	
	uint32_t uuid_len = read_be32(data);
	data += sizeof(uint32_t);
	if ( uuid_len != data_len ) {
		return 0;
	}
	//printf("uuid len = %08lx\n", uuid_len);
	
	// "MTDT" tag expected
	uint32_t tag = read_be32(data);
	if ( tag != 0x4d544454 ) {
		return 0;
	}
	data += sizeof(uint32_t);
	//printf("tag = %08lx\n", tag);
	if ( read_be16(data) != 0x0001 ) {
		return 0;
	}
	data += sizeof(uint16_t);
	uint16_t title_len = (read_be16(data) - 10) / 2;
	data += sizeof(uint16_t);

	if ( read_be32(data) != 0x00000001 ) {
		return 0;
	}
	data += sizeof(uint32_t);
	
	uint16_t lang_code = read_be16(data);
	data += sizeof(uint16_t);
	
	if ( read_be16(data) != 0x0001 ) {
		return 0;
	}
	data += sizeof(uint16_t);

	//printf("title len = %04lx (%d) \n", title_len, title_len);
	if ( title_len > (data_len - (data - uuid_data)) ) {
		title_len = data_len - (data - uuid_data) - 1;
	}

	for(uint16_t i = 0; i < title_len; i++) {
		uint16_t wc = read_be16(data);
		//printf("\twc=%04x\n", wc);
		data += sizeof(uint16_t);
		title[i] = (char)(wc & 0xff);
	}
	title[title_len] = 0;

	return 1;
}

void CAVInfo::ReadMP4(const char *file)
{
	MP4FileHandle mp4File = MP4Read(file, MP4_DETAILS_ERROR);
	if ( !mp4File ) {
		return;
	}
	uint32_t vsize;
	uint8_t *value;
	char *pname = "moov.uuid.data";

	MP4GetBytesProperty(mp4File, pname, &value, &vsize);
	if ( vsize ) {
		MP4_moov_uuid_parse(value, vsize, m_title);
		free(value);
	}
	MP4Close(mp4File);
}

CFFmpeg_Glue::CFFmpeg_Glue()
{
	ffmpeg_init();
}

CFFmpeg_Glue::~CFFmpeg_Glue()
{
	ffmpeg_deinit();
}

bool CFFmpeg_Glue::IsValidVersion()
{
	if ( !avcodec_find_encoder(CODEC_ID_AAC) || !avcodec_find_encoder(CODEC_ID_MPEG4)) {
		return false;
	}
	AVOutputFormat *fmt = guess_format("psp", 0, 0);
	if ( !fmt || strcmp(fmt->name, "psp") ) {
		return false;
	}
	return true;
}

bool CFFmpeg_Glue::RunTranscode(
			const char *infile, const char *outfile,
			int abitrate, int vbitrate,
			int v_size, int h_size,
			int v_pad, int h_pad,
			const char *title,
			int (*callback)(void *, int frame), void *uptr)
{
	ffmpeg_do_transcode((char *)infile, (char *)outfile,
		abitrate, vbitrate, v_size, h_size, v_pad, h_pad,
		(char *)title, callback, uptr);
		
	return true;
}

// generate thumbnail by ffmpeg call. Don't think it's needed
//bool CFFmpeg_Glue::RunThumbnail(const char *infile, const char *outfile,
//	const char *offset, const char *size, const char *v_pad, const char *h_pad)
//{
//
//	return true;
//}
