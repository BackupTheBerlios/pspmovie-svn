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


#endif /*AVUTILS_H_*/
