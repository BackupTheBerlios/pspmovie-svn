#ifndef AVUTILS_H_
#define AVUTILS_H_

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
		CAVInfo() { /* for stl */ }
		
		bool HaveVStream() { return m_have_vstream; }
		bool CodecOk() { return m_codec_ok; }
		int Sec() { return m_sec; }
		int Usec() { return m_usec; }
		
		float Fps() { return m_fps; }
		
		int FrameCount() { return m_frame_count; }
		
		const char *Title() { return &m_title[0]; }
};

void AV_Init();

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
		bool RunTranscode(
			const char *infile, const char *outfile,
			const char *abitrate, const char *vbitrate,
			const char *title,
			int (*callback)(void *, int frame), void *uptr);
		//
		// Call to create thumbnail image.
		// offset have firmat hh:mm:ss.SS
		//
		bool RunThumbnail(const char *infile, const char *outfile, const char *offset);
};

#endif /*AVUTILS_H_*/
