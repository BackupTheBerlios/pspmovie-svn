#ifndef JOBCONTROL_H_
#define JOBCONTROL_H_

#include <qobject.h>
#include <qprocess.h>
#include <qsettings.h>

QString CastToXBytes(unsigned long size);

class CJobControlImp;

#include "avutils.h"

class CTranscode {
		// user choices from gui
		QString m_src;
		QString m_short_src;
		bool m_fix_aspect;
		
		// output stream params
		QString m_size;
		QString m_s_bitrate, m_v_bitrate;
		
		// input stream params
		CAVInfo m_in_info;
		/*
		int m_width, m_height;
		// duration is in msec. frame count is 
		// estimation from "duration x fps" rounded up
		int m_duration, m_frame_count;
		float m_fps;
		*/
		QString m_str_duration;
		
		// for lookup in job queue
		int m_id;
		static int m_curr_id;
	public:
	
		CTranscode(QString &src, QString &size,
			QString &s_bitrate, QString &v_bitrate, bool fix_aspect);
		
		bool IsOK();

		int TotalFrames();
		
		void RunTranscode(CFFmpeg_Glue &, int (cb)(void *, int), void *);
		void RunThumbnail(CFFmpeg_Glue &);
		
		int Id() { return m_id; }
		
		// for display in gui
		QString StrDuration() { return m_str_duration; }
		QString ShortName() { return m_short_src; }
		QString Target()
		{
			return m_size + " / " + m_s_bitrate + " / " + m_v_bitrate;
		}
};

class CJobQueue {
		CFFmpeg_Glue m_ffmpeg;
		
		std::list<CTranscode> m_queue;
		
		//
		// current ffmpeg process info
		QProcess *m_curr_process;
		bool m_in_progress_out;
		
		bool m_is_aborted;
		
		int m_total_frames;
		
		static int UpdateTranscodeProgress(void *This, int frame);

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
		
		
};

class CPSPMovie {
		int m_id;
		static int s_next_id;
		
		unsigned long m_size;
		bool m_have_thumbnail;
		QString m_thmb_name, m_movie_name;
		QDir m_dir;
		QString m_str_size;
		bool DoCopy(QWidget *parent, const QString &source, const QString &target);

		QImage m_icon;
	public:
		CPSPMovie(QFileInfo *info);
		
		CPSPMovie() {  /* for stl */ }
		
		bool TransferTo(QWidget *parent, const QString &target_dir, int trg_idx);
		bool Delete();
		
		const QString &Name() { return m_movie_name; };
		const QString &Size() { return m_str_size; };
		
		int Id() { return m_id; }
		
		QImage &Icon() { return m_icon; }
};

class CPSPMovieLocalList {
		std::map<int, CPSPMovie> m_movie_set;
		QDir m_source_dir;
	public:
		CPSPMovieLocalList(const QString &dir);
		
		typedef std::map<int, CPSPMovie>::iterator CPSPMovieListIt;
		
		CPSPMovieListIt Begin() { return m_movie_set.begin(); }
		CPSPMovieListIt End() { return m_movie_set.end(); }
		
		bool Transfer(QWidget *parent, int id, const QString &dest);
		bool TransferPSP(QWidget *parent, int id, const QString &base);
		bool Delete(int id);
};

extern CJobQueue g_job_queue;

class CAppSettings {
		QSettings m_settings;
		
		QString m_app_dir_path;
		
		QString m_tmp_dir_path, m_psp_dir_path;
		QDir m_tmp_dir;
		
		QString m_ffmpeg_path;

	public:
		CAppSettings();
		~CAppSettings();
		
		QString ffmpeg() { return m_ffmpeg_path; }
		const QDir &TargetDir() const { return m_tmp_dir; } 
		
		int GetNewOutputNameIdx(const QDir &trg_dir) const;
			
};
const CAppSettings *GetAppSettings();

#endif /*JOBCONTROL_H_*/
