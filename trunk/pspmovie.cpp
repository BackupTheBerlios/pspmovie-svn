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

#include <qapplication.h>
#include <qmessagebox.h>
#include <qregexp.h>
#include <qprogressdialog.h>
#include <qtimer.h>
#include <qimage.h>

#include <math.h>
#include <unistd.h>

#include "ffmpeg/avformat.h"
#include "ffmpeg/avcodec.h"

#include "pspmovie.h"

#include "mainwin.h"
#include "xferwin.h"
MainWin *g_main_win = 0;


QString CastToXBytes(unsigned long size)
{
    QString result;
    if ( size < 1024 ) {
		result.sprintf("%d bytes", (int)size);
    } else if ( size < 1048576 ) {
		result.sprintf("%.02f KB", size / 1024.0);
    } else if ( size < 1073741824 ) {
		result.sprintf("%.02f MB", size / 1048576.0);
    } else {
		result.sprintf("%.02f GB", size / 1073741824.0);
    }
    return result;
}
//
// Used to bind between process and Qt framework
//
class CJobControlImp: public QObject {
    Q_OBJECT

    QProcess *m_proc;
    void (*m_callback)(void *, const char *);
    void *m_callback_data;
	public:
		CJobControlImp(void (*callback)(void *, const char *), void *callback_data);
		~CJobControlImp();
		
		void AddProcessArg(const QString &s);
		
		QProcess *Start();
		
	public slots:
	    void ReadFromStdout();
	    void ReadFromStderr();
	    void ProcessDone();
};


CJobControlImp::CJobControlImp(void (*callback)(void *, const char *), void *callback_data)
{
 	m_proc = new QProcess(this);

	m_callback = callback;
	m_callback_data = callback_data;

    connect(m_proc, SIGNAL(readyReadStdout()), this, SLOT(ReadFromStdout()));
    connect(m_proc, SIGNAL(readyReadStderr()), this, SLOT(ReadFromStderr()));
    connect(m_proc, SIGNAL(processExited()), this, SLOT(ProcessDone()));
}

CJobControlImp::~CJobControlImp()
{
	if ( m_proc ) {
		delete m_proc;
	}
}

void CJobControlImp::AddProcessArg(const QString &s)
{
	if ( m_proc ) {
		m_proc->addArgument(s);
	}
}

QProcess *CJobControlImp::Start()
{
	if ( m_proc->start() ) {
		return m_proc;
	}
	return 0;
}

void CJobControlImp::ReadFromStdout()
{
	QByteArray out = m_proc->readStdout();
	if ( m_callback && out.count() ) {
		m_callback(m_callback_data, (const char *)out);
	}
}

void CJobControlImp::ReadFromStderr()
{
	QByteArray out = m_proc->readStderr();
	if ( m_callback && out.count() ) {
		m_callback(m_callback_data, (const char *)out);
	}
}

void CJobControlImp::ProcessDone()
{
	if ( m_callback ) {
		m_callback(m_callback_data, 0);
	}
}

//
// Transcoding job
//
int CTranscode::m_curr_id = 1001;

CTranscode::CTranscode(QString &src,
			QString &s_bitrate, QString &v_bitrate, bool fix_aspect) : m_in_info(src)
{
	if ( !IsOK() ) {
		return;
	}
	m_being_run = false;
	m_src = src;

	// some tell, that other resolution is possible. Never find it to
	// be true	
	m_size = "320x240";

	m_s_bitrate = s_bitrate;
	m_v_bitrate = v_bitrate;
	m_fix_aspect = fix_aspect;
	
	m_id = m_curr_id++;
	
	if ( m_src.length() > 50 ) {
		QFileInfo fi(m_src);
		QString short_path = fi.dirPath().left(40) + QDir::convertSeparators(".../");
		QString short_name = fi.fileName();
		if ( fi.fileName().length() > 20 ) {
			short_name = fi.baseName(true).left(10) + "..." +
				fi.fileName().right(10);
		}
		m_short_src =  short_path + short_name;
	} else {
		m_short_src = m_src;
	}

	QRegExp rate_exp("[0-9]{2,3}kbps");
	if ( !rate_exp.exactMatch(s_bitrate) ) {
		Q_ASSERT(false);
	}
	if ( !rate_exp.exactMatch(v_bitrate) ) {
		Q_ASSERT(false);
	}
	
	s_bitrate.replace("kpbs", "", false);
	v_bitrate.replace("kpbs", "", false);

	int s = m_in_info.Sec() % 60, ms = m_in_info.Usec() / 1000;
	int h = m_in_info.Sec() / 3600;
	int m = (m_in_info.Sec() - h*3600) / 60;
	m_str_duration = QString( "%1:%2:%3.%4" )
                    .arg( h ) .arg( m ) .arg( s ) .arg( ms );

	if ( fix_aspect ) {
		/*
		 * PSP have screen size 480 x 272 pixel, and can
		 * resize movie to fit the screen discarding aspect ratio
		 */
		int h, w;
		float ratio = (float)m_in_info.H() / (float)m_in_info.W();
		if ( ratio >= (9.0/16.0) ) {
			// adding vertical strips. In this case movie is being optimized
			// for scaled view mode (aspect ratio is maintainted)
			h = 240;
			w = m_in_info.W() * 240 / m_in_info.H();
		} else {
			// adding horizontal strips. In this case movie is being optimized
			// for full-screen view mode (aspect ratio is discarded)
			// h = H/W x 480 x (240/270)
			w = 320;
			h = m_in_info.H() * 160 * 8 / m_in_info.W() / 3;
		}
		int pad_v = ((240 - h) / 2) & 0xfffe;
		int pad_h = ((320 - w) / 2) & 0xfffe;
		
		if ( (h + 2 * pad_v) != 240 ) {
			h += 240 - (h + 2 * pad_v);
		}
		if ( (w + 2 * pad_h) != 320 ) {
			w += 320 - (w + 2 * pad_h);
		}

		m_size = QString("%1x%2") .arg(w) . arg(h);
		if (  pad_v ) {
			m_v_padding = QString("%1") . arg(pad_v);
		}
		if (  pad_h ) {
			m_h_padding = QString("%1") . arg(pad_h);
		}
		// thumbnail aspect
		if ( ratio < (120.0/160.0) ) {
			int t_pad_v = ((120 - m_in_info.H() * 160 / m_in_info.W()) / 2) & 0xfffe;
			m_th_v_padding = QString("%1") . arg(t_pad_v);
			m_th_size = QString("160x%1") . arg(120 - 2 * t_pad_v);
		} else {
			int t_pad_h = ((160 - m_in_info.W() * 120 / m_in_info.H()) / 2) & 0xfffe;
			m_th_h_padding = QString("%1") . arg(t_pad_h);
			m_th_size = QString("%1x120") . arg(160 - 2 * t_pad_h);
		}
	}
}

bool CTranscode::IsOK()
{
	return m_in_info.HaveVStream() && m_in_info.CodecOk();
}

int CTranscode::TotalFrames()
{
	return m_in_info.FrameCount();
}

void CTranscode::RunTranscode(CFFmpeg_Glue &ffmpeg, int (cb)(void *, int), void *ptr)
{
	m_being_run = true;
	QFileInfo fi(m_src);
	QString target_path = GetAppSettings()->TargetDir().filePath(fi.baseName(true) + ".mp4");
	ffmpeg.RunTranscode(m_src, target_path, m_s_bitrate, m_v_bitrate, fi.baseName(true),
		m_size, m_v_padding, m_h_padding, cb, ptr);
}

void CTranscode::RunThumbnail(CFFmpeg_Glue &ffmpeg)
{
	QFileInfo fi(m_src);
	QString target_path = GetAppSettings()->TargetDir().filePath(fi.baseName(true) + ".thm");
	
	int thm_off = m_in_info.Sec() / 10;
	if ( thm_off > 20 ) {
		thm_off = 20;
	}
	QString thm_time;
	thm_time = QString("%1").arg(thm_off);
	ffmpeg.RunThumbnail(m_src, target_path, thm_time, m_th_size, m_th_v_padding, m_th_h_padding);
}


//
// Queue of pending and running jobs
//

CJobQueue g_job_queue;

CJobQueue::CJobQueue()
{
}

bool CJobQueue::Start()
{
	if ( m_queue.empty() ) {
		return false;
	}

	m_is_aborted = false;

	do {
		// update gui controls
		g_main_win->enableStart(false);

		CTranscode &new_job = m_queue.front();
		m_total_frames = new_job.TotalFrames();
	
		m_update_interval = m_total_frames / 100;
		m_update_countdown = m_update_interval;
		m_last_update = time(0);
		
		new_job.RunTranscode(m_ffmpeg, UpdateTranscodeProgress, this);
		new_job.RunThumbnail(m_ffmpeg);
	
		// current job done: remove from gui
		g_main_win->removeFromQueue(m_queue.front().Id());
	
		// done with it - remove from queue
		m_queue.pop_front();
	} while ( !m_is_aborted && !m_queue.empty() );
	
	g_main_win->enableStart(true);
	g_main_win->m_xfer_win->refreshData();
	
	return true;
}

bool CJobQueue::Abort()
{
	m_is_aborted = true;
	
	g_main_win->enableStart(true);

	return true;
}

bool CJobQueue::Add(CTranscode &job)
{
	if ( job.IsOK() ) {
		m_queue.push_back(job);
		return true;
	} else {
		QMessageBox::critical(qApp->mainWidget(),
			"Error", "Unrecognized file format");
		return false;
	}
}

bool CJobQueue::Remove(int job_id)
{
	for(std::list<CTranscode>::iterator i = m_queue.begin(); i != m_queue.end(); i++) {
		if ( i->Id() == job_id ) {
			if ( i->IsRunning() ) {
				QMessageBox::critical(qApp->mainWidget(),
					"Error", "You can not remove running job");
				return false;
			} else {
				m_queue.erase(i);
				return true;
			}
		}
	}
	return false;
}

int CJobQueue::UpdateTranscodeProgress(void *ptr, int frame)
{
	CJobQueue *This = (CJobQueue *)ptr;

	time_t curr_time = time(0);
	if ( !(--This->m_update_countdown) || (curr_time - This->m_last_update) ) {
		int progress = (frame * 100) / This->m_total_frames;
		g_main_win->updateProgress(progress, frame);
		This->m_update_countdown = This->m_update_interval;
		This->m_last_update = time(0);
	}
	qApp->processEvents();
	if ( ! qApp->mainWidget()->isShown() ) {
		return 0;
	}
	return This->m_is_aborted ? 0 : 1;
}

//
// Application preferences
//
const CAppSettings *GetAppSettings()
{
	static CAppSettings app_settings;

	return &app_settings;
}

CAppSettings::CAppSettings()
{
	m_settings.setPath("pspmovie.berlios.de", "pspmovie");
	
	m_app_dir_path = QDir::cleanDirPath(QDir::homeDirPath() + QDir::convertSeparators("/.pspmovie/"));
	m_settings.insertSearchPath( QSettings::Unix, m_app_dir_path);
	
	QDir dir(m_app_dir_path);
	if ( !dir.exists() ) {
		if ( !dir.mkdir(m_app_dir_path) ) {
			QMessageBox::critical(0, "Error", "unable to create application directory");
			qApp->exit(-1);
		}
	}
	m_tmp_dir_path = QDir::cleanDirPath(m_app_dir_path + QDir::convertSeparators("/100MNV01/"));
	
	m_tmp_dir.setPath(m_tmp_dir_path);
	if ( !m_tmp_dir.exists() ) {
		if ( !m_tmp_dir.mkdir(m_tmp_dir_path) ) {
			QMessageBox::critical(0, "Error", "unable to create output directory");
			m_tmp_dir_path = QString(0);
		}
	}

	//printf("Tmp dir -> [%s]\n", (const char *)m_tmp_dir_path);
}

CAppSettings::~CAppSettings()
{
}

int CAppSettings::GetNewOutputNameIdx(const QDir &trg_dir) const
{
	for(int i = 1 ; i < 999999; i++) {
		QString next_name;
		next_name.sprintf("M4V%05d.MP4", i);
		QFileInfo fi(trg_dir.filePath(next_name));
		//printf("DEBUG: testing [%s] - ", (const char *)trg_dir.filePath(next_name));
		if ( !fi.exists() ) {
			//printf("not found, id=%d\n", i);
			return i;
		}
		//printf("found\n");
	}
	return -1;
}

//
// Class representing transcoded file
//
int CPSPMovie::s_next_id = 1;

CPSPMovie::CPSPMovie(QFileInfo *info) : m_dir(info->dirPath(TRUE))
{
	QRegExp id_exp("M4V(\\d{5})", FALSE);
	if ( id_exp.exactMatch(info->baseName()) ) {
		// this file on PSP
		m_id = id_exp.cap(1).toInt();
		m_movie_name = info->baseName().upper() + ".MP4";
		m_thmb_name = info->baseName().upper() + ".THM";
	} else {
		m_id = s_next_id++;
		m_movie_name = info->baseName(true) + ".mp4";
		m_thmb_name = info->baseName(true) + ".thm";
	}

	//printf("Test thumbnail at [%s]\n", (const char *)m_dir.filePath(m_thmb_name));
	
	m_have_thumbnail = QFile::exists(m_dir.filePath(m_thmb_name));
	m_size = info->size();
//	printf("File [%d] [%s] with thumbnail [%s]\n", m_id, (const char *)m_movie_name,
//	       m_have_thumbnail ? "yes" : "no");
	m_str_size = CastToXBytes(m_size);
	
	if ( m_have_thumbnail ) {
		m_icon = QImage(m_dir.filePath(m_thmb_name)).smoothScale(2*32, 2*24);
	}
}

bool CPSPMovie::DoCopy(QWidget *parent, const QString &source, const QString &target)
{
	//printf("Copying [%s] -> [%s]\n", (const char *)source, (const char *)target);
	QFile src(source);
	QFile dst(target);
	if ( !src.open(IO_Raw | IO_ReadOnly) ) {
		return false;
	}
	if ( !dst.open(IO_Raw | IO_WriteOnly | IO_Truncate) ) {
		return false;
	}
	sync();

	const int bufsize = 0x10000;
	int num_of_steps = (src.size() / bufsize) + 1;
	QProgressDialog progress(QString("Copying file: ") + source,
		"Abort Copy", num_of_steps, parent, "progress", TRUE);

	char *buffer = new char[bufsize];
	int curr_step = 0;
	while( !src.atEnd() ) {
		
	    progress.setProgress(curr_step);
	    qApp->processEvents();
	
	    if ( progress.wasCanceled() ) {
	        break;
	    }
	    
		Q_LONG sz = src.readBlock(buffer, bufsize);
		if ( sz == -1 ) {
			delete buffer;
			return false;
		}
		dst.writeBlock(buffer, sz);
		curr_step++;
	}
	sync();
	delete buffer;

	return true;
}

bool CPSPMovie::TransferTo(QWidget *parent, const QString &target_dir, int trg_idx)
{
	QDir trgdir(target_dir);

	QString trg_movie, trg_thmb;
	if ( trg_idx == -1 ) {
		// try to extract title
		CAVInfo in_info(m_dir.filePath(m_movie_name));
		QString src_title(in_info.Title());
		if ( src_title.length() > 3 ) {
			trg_movie = src_title + ".mp4";
			trg_thmb = src_title + ".thm";
		} else {
			trg_movie.sprintf("from_PSP_%05d.mp4", s_next_id);
			trg_thmb.sprintf("from_PSP_%05d.thm", s_next_id);
		}
		trg_idx = s_next_id++;
	} else {
		trg_movie.sprintf("M4V%05d.MP4", trg_idx);
		trg_thmb.sprintf("M4V%05d.THM", trg_idx);
	}
	
	// movie going first
	if ( !DoCopy(parent, m_dir.filePath(m_movie_name),
			trgdir.filePath(trg_movie)) ) {
		return false;
	}
	if ( m_have_thumbnail && !DoCopy(parent, m_dir.filePath(m_thmb_name),
			trgdir.filePath(trg_thmb)) ) {
		return false;
	}

	return true;
}

bool CPSPMovie::Delete()
{
	if ( !QFile::remove(m_dir.filePath(m_movie_name)) ) {
		return false;
	}
	if ( !QFile::remove(m_dir.filePath(m_thmb_name)) ) {
		return false;
	}
	return true;
}

CPSPMovieLocalList::CPSPMovieLocalList(const QString &dir_path) : m_source_dir(dir_path)
{
	//printf("Loading list from [%s]\n", (const char *)dir_path);

	// on vfat it's shown in lower case !
	m_source_dir.setNameFilter("*.MP4;*.mp4");
	const QFileInfoList *files = m_source_dir.entryInfoList(QDir::Files | QDir::NoSymLinks | QDir::Readable);
	if ( files ) {
		QFileInfoListIterator it(*files);
		QFileInfo *fi;
	    while ( ( fi = it.current() ) != 0 ) {
	    	CPSPMovie m(fi);
	    	m_movie_set[m.Id()] = m;
	    	++it;
	    }
	}
}

bool CPSPMovieLocalList::Transfer(QWidget *parent, int id, const QString &dest)
{
	Q_ASSERT ( m_movie_set.count(id) );
	//printf("DEBUG: will copy file %d\n", id);
	CPSPMovie &m = m_movie_set[id];
	return m.TransferTo(parent, dest, -1);
}

bool CPSPMovieLocalList::TransferPSP(QWidget *parent, int id, const QString &base)
{
	Q_ASSERT ( m_movie_set.count(id) );
	CPSPMovie &m = m_movie_set[id];
	
	//printf("DEBUG: copy to [%s]\n", (const char *)base);
	QDir mount_base(base);
	QDir mp_root(mount_base.filePath("MP_ROOT"));
	if ( !mp_root.exists() && !mp_root.mkdir(mp_root.path()) ) {
		return false;
	}
	QDir trg_dir(mp_root.filePath("100MNV01"));

	// get free index before moving directory
	int free_idx = GetAppSettings()->GetNewOutputNameIdx(trg_dir.path());

	QDir trg_dir_backup(mp_root.filePath("100MNV01_BACK"));
	if ( trg_dir.exists() ) {
	  //printf("DEBUG: rename [%s] -> [%s]\n", (const char *)trg_dir.path(),
		// (const char *)trg_dir_backup.path());
	  if ( !trg_dir.rename(trg_dir.path(), trg_dir_backup.path()) ) {
	    //printf("DEBUG: failed rename\n");
	  }
	}
	if ( !trg_dir.exists() && !trg_dir.mkdir(trg_dir.path()) ) {
		return false;
	}
	//printf("DEBUG: transferring [%s] -> [%s]\n", (const char *)m.Name(), (const char *)trg_dir.path());
	if ( !m.TransferTo(parent, trg_dir.path(), free_idx) ) {
	  //printf("DEBUG: transfer failed\n");
		return false;
	}
	//printf("Checking backup dir [%s]\n", (const char *)trg_dir_backup.path());
	if ( trg_dir_backup.exists() ) {
		const QFileInfoList *files = trg_dir_backup.entryInfoList(QDir::Files);
		if ( files ) {
			QFileInfoListIterator it(*files);
			QFileInfo *fi;
			while ( ( fi = it.current() ) != 0 ) {
				QString backup_src(fi->filePath());
				QString backup_dst(trg_dir.filePath(fi->fileName().upper()));
				//printf("DEBUG: moving [%s] -> [%s]\n", (const char *)backup_src, (const char *)backup_dst);
				if ( !trg_dir.rename(backup_src, backup_dst) ) {
					//printf("OOps - rename failed\n");
				}
				sync();
				++it;
			}
		}
		//printf("Will remove [%s]\n", (const char *)trg_dir_backup.path());
		if  ( !trg_dir_backup.rmdir(trg_dir_backup.path()) ) {
			//printf("remove failed\n");
		}
		sync();
	}
	return true;
}

bool CPSPMovieLocalList::Delete(int id)
{
	Q_ASSERT ( m_movie_set.count(id) );
	CPSPMovie &m = m_movie_set[id];
	if ( m.Delete() ) {
		m_movie_set.erase(id);
		return true;
	}
	return false;
}

int main( int argc, char **argv )
{
	QApplication app(argc, argv);

	MainWin mainwin;
	g_main_win = &mainwin;

	if ( !CanDoPSP() ) {
		QMessageBox::critical(0, "ERROR: bad ffmpeg library",
			"FFMPEG library you have can not encode PSP format correctly\n"
			"You have version \"" FFMPEG_VERSION "\" of FFMPEG"
			);
		return -1;
	}
	
	app.connect( &app, SIGNAL( lastWindowClosed() ), &app, SLOT( quit() ) );
	app.setMainWidget(&mainwin);
	mainwin.show();
	
	GetAppSettings();

	
	return app.exec();
}

#include "pspmovie.moc"
