#include <ffmpeg/avformat.h>
#include <ffmpeg/avcodec.h>

#include <stdio.h>
#include <string.h>

class CAVInfo {
		bool m_have_vstream;
		bool m_codec_ok;
		
		int m_sec, m_usec;
		float m_fps;
		int m_frame_count;
		
		// same size as in libavformat
		char m_title[512];
	public:
		CAVInfo(const char *file);
		
		bool HaveVStream() { return m_have_vstream; }
		bool CodecOk() { return m_codec_ok; }
		int Sec() { return m_sec; }
		int Usec() { return m_usec; }
		
		float Fps() { return m_fps; }
		
		int FrameCount() { return m_frame_count; }
		
		const char *Title() { return &m_title[0]; }
};

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
	
	AVFormatContext *fctx;
	
	if ( av_open_input_file(&fctx, filename, 0, 0, 0) != 0 ) {
		printf("av_open_input_file - error\n");
		return;
	}
	if( av_find_stream_info(fctx) < 0) {
		printf("av_find_stream_info - error\n");
		return;
	}
	dump_format(fctx, 0, filename, false);

	if ( strlen(fctx->title) ) {
		strncpy(m_title, fctx->title, sizeof(m_title)-1);
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

int main()
{
	AV_Init();
	
	printf("can do PSP - [%s]\n", CanDoPSP() ? "yes" : "no");
	
	CAVInfo i("test.avi");
	printf("Info: %d.%d sec %d frames %f fps\n", i.Sec(), i.Usec(), i.FrameCount(), i.Fps());
	
	return 0;
}
