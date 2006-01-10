#ifndef JOBCONTROL_H_
#define JOBCONTROL_H_

#include <qobject.h>
#include <qprocess.h>
#include <qsettings.h>

class CJobControlImp;
class CTranscode {
		// user choices from gui
		QString m_src, m_title;
		QString m_short_src;
		bool m_fix_aspect;
		
		// output stream params
		QString m_size;
		QString m_s_bitrate, m_v_bitrate;
		
		// input stream params
		int m_width, m_height;
		// duration is in msec. frame count is 
		// estimation from "duration x fps" rounded up
		int m_duration, m_frame_count;
		QString m_str_duration;
		float m_fps;
		
		static void CheckInputCallback(void *, const char *);
		void CheckInput();
		
		// for lookup in job queue
		int m_id;
		static int m_curr_id;
	public:
	
		CTranscode(QString &src, QString &title, QString &size,
			QString &s_bitrate, QString &v_bitrate, bool fix_aspect);
		
		bool IsOK() { return m_duration != -1; }

		int Duration() { return m_duration; }
		
		int TotalFrames() { return (int)((m_duration * m_fps) / 1000.0); }
		
		QProcess *Start(CJobControlImp *ctrl, const QString &dst);

		int Id() { return m_id; }
		
		// for display in gui
		QString StrDuration() { return m_str_duration; }
		QString ShortName() { return m_short_src; }
};

class CJobQueue {
		std::list<CTranscode> m_queue;
		
		//
		// current ffmpeg process info
		QProcess *m_curr_process;
		
		bool m_in_progress_out;
		
		int m_total_frames;
		
		void ParseFfmpegOutputLine(const char *line);
		
		static void UpdateProgress(void *, const char *);
	public:
		CJobQueue();
		
		bool Add(CTranscode &job);
		bool Remove(int job_id);
		
		CTranscode *NextJob()
		{ 
			return IsEmpty() ? 0 : &m_queue.front();
		}
		
		bool Start();
		bool Abort();
		
		bool IsEmpty() { return m_queue.empty(); }
		
		
		void CurrProcessDone();
};

extern CJobQueue g_job_queue;

class CAppSettings {
		QSettings m_settings;
		
		QString m_app_dir;
		
		QString m_tmp_dir;
		QString m_psp_dir;
		
		QString m_ffmpeg_path;

		QString Get_FFMPEG();
	public:
		CAppSettings();
		~CAppSettings();
		
		QString ffmpeg() { return m_ffmpeg_path; }
		const QString TargetDir() const { return m_tmp_dir; } 
		
		int GetNewOutputNameIdx() const;
			
};
const CAppSettings *GetAppSettings();

#endif /*JOBCONTROL_H_*/
