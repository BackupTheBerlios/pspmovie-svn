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

#include "ffmpeg/avformat.h"
#include "ffmpeg/avcodec.h"

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
	m_codec_ok = false;
	m_title[0] = 0;

	if ( !filename ) {
		return;
	}
			
	AVFormatContext *fctx;
	if ( av_open_input_file(&fctx, filename, 0, 0, 0) != 0 ) {
		printf("av_open_input_file - error\n");
		return;
	}
	if( av_find_stream_info(fctx) < 0) {
		printf("av_find_stream_info - error\n");
		return;
	}

#ifdef AVLIB_TEST
	dump_format(fctx, 0, filename, false);
	printf("Title [%s] comment [%s]\n", fctx->title, fctx->comment);
#endif

 	m_sec = fctx->duration / AV_TIME_BASE;
    m_usec = fctx->duration % AV_TIME_BASE;
    m_width = m_height = 0;
	int videoStream = -1;
	for(int i = 0; i < fctx->nb_streams; i++) {
		if ( fctx->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO) {
			m_width = fctx->streams[i]->codec->width;
			m_height = fctx->streams[i]->codec->height;
	        videoStream = i;
	        m_have_vstream = true;
    	    break;
 		}
	}
	if(videoStream == -1) {
    	printf("Didn't find a video stream\n");
    	return ;
	}

	// Get a pointer to the codec context for the video stream
	AVStream *st = fctx->streams[videoStream];
	AVCodecContext *acctx = st->codec;
	m_fps = 1/av_q2d(st->time_base);
	/*
	if (st->r_frame_rate.den && st->r_frame_rate.num) {
		m_fps = av_q2d(st->r_frame_rate);
	} else {
		m_fps = 1/av_q2d(st->codec->time_base);
	}
	*/
	// Find the decoder for the video stream
	AVCodec *codec = avcodec_find_decoder(acctx->codec_id);
	m_codec_ok = codec != 0;
	
	m_frame_count = st->nb_frames ? st->nb_frames : int(m_sec * m_fps);
	
	//
	// If we have .mp4 file, attempt to read title
	//
	const char *ext = filename + strlen(filename) - 4;
	if ( !strcmp(ext, ".mp4") || !strcmp(ext, ".MP4") ) {
		ReadMP4(filename);
	}
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
			const char *abitrate, const char *vbitrate,
			const char *title,
			const char *size, const char *v_pad, const char *h_pad,
			int (*callback)(void *, int frame), void *uptr)
{
	const char *ffmpeg_opts[256];
	
	int i = 0;
	// "must" set
	ffmpeg_opts[i++] = "ffmpeg"; ffmpeg_opts[i++] = "-y";
	ffmpeg_opts[i++] = "-i"; ffmpeg_opts[i++] = infile;
	ffmpeg_opts[i++] = "-b"; ffmpeg_opts[i++] = vbitrate;
	ffmpeg_opts[i++] = "-ab"; ffmpeg_opts[i++] = abitrate;
	ffmpeg_opts[i++] = "-title"; ffmpeg_opts[i++] = title;
	ffmpeg_opts[i++] = "-s"; ffmpeg_opts[i++] = size;

	ffmpeg_opts[i++] = "-f"; ffmpeg_opts[i++] = "psp";
	ffmpeg_opts[i++] = "-r"; ffmpeg_opts[i++] = "29.970030";
	ffmpeg_opts[i++] = "-ar"; ffmpeg_opts[i++] = "24000";
	ffmpeg_opts[i++] = "-ac"; ffmpeg_opts[i++] = "2";

	if ( v_pad && strlen(v_pad) ) {
		ffmpeg_opts[i++] = "-padtop"; ffmpeg_opts[i++] = v_pad;
		ffmpeg_opts[i++] = "-padbottom"; ffmpeg_opts[i++] = v_pad;
	}
	if ( h_pad && strlen(h_pad) ) {
		ffmpeg_opts[i++] = "-padleft"; ffmpeg_opts[i++] = h_pad;
		ffmpeg_opts[i++] = "-padright"; ffmpeg_opts[i++] = h_pad;
	}
	
	// current call params
	
	ffmpeg_opts[i++] = outfile;
	
	ffmpeg_opts[i++] = 0;
	
	ffmpeg_main(i-1, (char **)ffmpeg_opts, callback, uptr);
	
	return true;
}

bool CFFmpeg_Glue::RunThumbnail(const char *infile, const char *outfile,
	const char *offset, const char *size, const char *v_pad)
{
	const char *ffmpeg_opts[256];
	
	int i = 0;
	// "must" set
	ffmpeg_opts[i++] = "ffmpeg"; ffmpeg_opts[i++] = "-y";
	ffmpeg_opts[i++] = "-i"; ffmpeg_opts[i++] = infile;
	ffmpeg_opts[i++] = "-s"; ffmpeg_opts[i++] = size;
	ffmpeg_opts[i++] = "-b"; ffmpeg_opts[i++] = "200";
	if ( v_pad && strlen(v_pad) ) {
		ffmpeg_opts[i++] = "-padtop"; ffmpeg_opts[i++] = v_pad;
		ffmpeg_opts[i++] = "-padbottom"; ffmpeg_opts[i++] = v_pad;
	}
	ffmpeg_opts[i++] = "-r"; ffmpeg_opts[i++] = "1";
	ffmpeg_opts[i++] = "-t"; ffmpeg_opts[i++] = "1";
	ffmpeg_opts[i++] = "-ss"; ffmpeg_opts[i++] = offset;
	ffmpeg_opts[i++] = "-an";
	ffmpeg_opts[i++] = "-f"; ffmpeg_opts[i++] = "mjpeg";

	ffmpeg_opts[i++] = outfile;
	
	ffmpeg_opts[i++] = 0;
	
	ffmpeg_main(i-1, (char **)ffmpeg_opts, 0, 0);

	return true;
}
