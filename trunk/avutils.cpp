#include <ffmpeg/avformat.h>
#include <ffmpeg/avcodec.h>

#include <stdio.h>
#include <string.h>

#include "avutils.h"

#include "ffmpeg_glue.h"

void AV_Init()
{
	av_register_all();
}

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

	if ( strlen(fctx->comment) ) {
		strncpy(m_title, fctx->comment, sizeof(m_title)-1);
		m_title[sizeof(m_title)-1] = 0;
	} else {
		m_title[0] = 0;
	}
	
 	m_sec = fctx->duration / AV_TIME_BASE;
    m_usec = fctx->duration % AV_TIME_BASE;
    
	int videoStream = -1;
	for(int i = 0; i < fctx->nb_streams; i++) {
		if ( fctx->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO) {
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
}

#ifdef AVLIB_TEST
uint32_t my_mp4ff_read(void *user_data, void *buffer, uint32_t length)
{
	FILE *f = (FILE *)user_data;
	size_t count = fread(buffer, length, 1, f);
	return count;
}

uint32_t my_mp4ff_seek(void *user_data, uint64_t position)
{
	FILE *f = (FILE *)user_data;
	return fseek(f, position, SEEK_SET);
}

void mp4fftest(const char *file)
{
	printf("mp4fftest for [%s]\n", file);

	mp4ff_callback_t cb;
	cb.read = my_mp4ff_read;
	cb.seek = my_mp4ff_seek;
	cb.write = 0;
	cb.truncate = 0;
	
	FILE *f = fopen(file, "r");
	if (!f) {
		printf("fopen failed\n");
		return;
	}
	cb.user_data = f;
	
	mp4ff_t *mp4 = mp4ff_open_read(&cb);
	
	for(int i = 0; i < mp4ff_meta_get_num_items(mp4); i++) {
		char *item, *value;
		mp4ff_meta_get_by_index(mp4, i, &item, &value);
		printf("META: [%s] => [%s]\n", item, value);
	}
	printf("mp4fftest done\n");
}

void mp4test(const char *file)
{
	printf("mp4test for [%s]\n", file);
	char *info = MP4FileInfo(file, 0);
	printf("Info = [%s]\n", info);
	MP4FileHandle mp4File = MP4Modify(file, MP4_DETAILS_ERROR);
	if ( !mp4File ) {
		printf("MP4Read failed\n");
		return;
	}
	int i = 0;
	const char *name;
	uint32_t vsize;
	uint8_t *value;
	while ( MP4GetMetadataByIndex(mp4File, i, &name, &value, &vsize) ) {
		printf("META: [%s] => [%s]\n", name, value);
		i++;
	}
	char *metaname;
	if ( MP4GetMetadataName(mp4File, &metaname) ) {
		printf("META NAME:\n");
	}
	MP4Dump(mp4File, stdout, 0);
	
	printf("mp4test done\n");
}

int main(int argc, char *argv[])
{
	AV_Init();
	
	printf("can do PSP - [%s]\n", CanDoPSP() ? "yes" : "no");
	
	const char *file = argc > 1 ? argv[1] : "test.avi";
	
	//mp4fftest(file);
	mp4test(file);
	
	//CAVInfo i(file);
	//printf("Info: title %s\n", i.Title());
	//printf("Info: %d.%d sec %d frames %f fps\n", i.Sec(), i.Usec(), i.FrameCount(), i.Fps());
	
	return 0;
}
#endif

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
			int (*callback)(void *, int frame), void *uptr)
{
	const char *ffmpeg_opts[256];
	
	int i = 0;
	// "must" set
	ffmpeg_opts[i++] = "ffmpeg"; ffmpeg_opts[i++] = "-y";
	ffmpeg_opts[i++] = "-f"; ffmpeg_opts[i++] = "psp";
	ffmpeg_opts[i++] = "-r"; ffmpeg_opts[i++] = "29.970030";
	ffmpeg_opts[i++] = "-ar"; ffmpeg_opts[i++] = "24000";

	ffmpeg_opts[i++] = "-s"; ffmpeg_opts[i++] = "320x240";

	// current call params
	ffmpeg_opts[i++] = "-i"; ffmpeg_opts[i++] = infile;
	ffmpeg_opts[i++] = "-b"; ffmpeg_opts[i++] = vbitrate;
	ffmpeg_opts[i++] = "-ab"; ffmpeg_opts[i++] = abitrate;
	ffmpeg_opts[i++] = "-title"; ffmpeg_opts[i++] = title;
	
	ffmpeg_opts[i++] = outfile;
	
	ffmpeg_opts[i++] = 0;
	
	ffmpeg_main(i-1, (char **)ffmpeg_opts, callback, uptr);
	
	return true;
}

bool CFFmpeg_Glue::RunThumbnail(const char *infile, const char *outfile, const char *offset)
{
	const char *ffmpeg_opts[256];
	
	int i = 0;
	// "must" set
	ffmpeg_opts[i++] = "ffmpeg"; ffmpeg_opts[i++] = "-y";
	ffmpeg_opts[i++] = "-i"; ffmpeg_opts[i++] = infile;
	ffmpeg_opts[i++] = "-s"; ffmpeg_opts[i++] = "160x90";
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
