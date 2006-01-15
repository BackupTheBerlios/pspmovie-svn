#include <qapplication.h>
#include <qmessagebox.h>
#include <qregexp.h>
#include <qprogressdialog.h>
#include <qtimer.h>

#include <math.h>

#include "pspmovie.h"

#include "mainwin.h"
MainWin *g_main_win = 0;


QString CastToXBytes(unsigned long size)
{
    QString result;
    if ( size < 1024 ) {
		result.sprintf("%d bytes", size);
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

CTranscode::CTranscode(QString &src, QString &size,
			QString &s_bitrate, QString &v_bitrate, bool fix_aspect)
{
	m_src = src;
	m_size = size;
	m_s_bitrate = s_bitrate;
	m_v_bitrate = v_bitrate;
	m_fix_aspect = fix_aspect;
	
	m_id = m_curr_id++;
	
	if ( m_src.length() > 50 ) {
		QDir d(m_src);
		QString short_path = d.path().left(40) + QDir::convertSeparators(".../");
		QString short_name = d.dirName().left(20);
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
	QRegExp size_exp("[0-9]{3}x[0-9]{3}");
	if ( !size_exp.exactMatch(size) ) {
		Q_ASSERT(false);
	}
	
	s_bitrate.replace("kpbs", "", false);
	v_bitrate.replace("kpbs", "", false);
	CheckInput();
}

void CTranscode::ParseCheckInputTest()
{
	char *s = m_in_check_out_buff;
	printf("InParse = [%s]\n", s);
	//if ( s && !strncmp(s, "Input #0", strlen("Input #0")) ) {
	if ( s && strstr(s, "Input #0") ) {
		m_duration = -1;
		char *p = strstr(s, "Stream #0");
		if ( !p ) {
			// no stream ?
			return ;
		} else {
			p = strstr(p, "Video:");
			if ( !p ) {
				// aparantly no video ?
				return;
			}
			p += strlen("Video:");
		}
		QStringList tok = QStringList::split(",", p);
		//printf("size=[%s] fps=[%s]\n", (const char *)tok[2], (const char *)tok[3]);
		// parse size and fps
		const char *cp = tok[2];
		while( isspace(*cp) ) cp++;
		if ( sscanf(cp, "%dx%d", &m_width, &m_height) != 2 ) {
			return;
		}
		cp = tok[3];
		while( isspace(*cp) ) cp++;
		if ( sscanf(cp, "%f", &m_fps) != 1 ) {
			return;
		}
		// size and fps looks ok by now
		p = strstr(s, "Duration:");
		if ( !p ) {
			return;
		}
		p += strlen("Duration:");
		while( isspace(*p) ) p++;
		int h, m, s, ms;
		if ( sscanf(p, "%d:%d:%d.%d", &h, &m, &s, &ms) != 4 ) {
			return;
		}
		m_str_duration = QString( "%1:%2:%3.%4" )
                        .arg( h ) .arg( m ) .arg( s ) .arg( ms );
		//printf("all ok\n");
		m_duration = ms + 1000*(s + 60*(m + 60*h));
	}
}

void CTranscode::CheckInputCallback(void *ptr, const char *s)
{
	CTranscode *This = (CTranscode *)ptr;
	printf("InCheck = [%s]\n", s);
	if ( s ) {
		strcat(This->m_in_check_out_buff, s);
	} else {
		This->m_duration = -1;
	}
}

void CTranscode::CheckInput()
{

	CJobControlImp *proc = new CJobControlImp(CheckInputCallback, this);

	proc->AddProcessArg("ffmpeg");
	proc->AddProcessArg("-i");
	proc->AddProcessArg(m_src);
	
	m_duration = 0;
	
	m_in_check_buf_size = 0x10000;
	m_in_check_out_buff = new char[m_in_check_buf_size];\
	m_in_check_out_buff[0] = 0;
	if ( !proc->Start() ) {
		printf("start failed\n");
	}
	
	while ( !m_duration ) {
		qApp->processEvents();
	}
	ParseCheckInputTest();
	delete [] m_in_check_out_buff;
	
	printf("test done\n");
}

QProcess *CTranscode::Start(CJobControlImp *proc, const QString &dst)
{
	proc->AddProcessArg("ffmpeg");
	proc->AddProcessArg("-i");
	proc->AddProcessArg(m_src);
    
    // mandatory arguments
	proc->AddProcessArg("-y");
	proc->AddProcessArg("-f");
	proc->AddProcessArg("psp");
	proc->AddProcessArg("-r");
	proc->AddProcessArg("29.970030");
//	proc->AddProcessArg("29.97");
	proc->AddProcessArg("-ar");
	proc->AddProcessArg("24000");
	// rates:
	proc->AddProcessArg("-b");
	proc->AddProcessArg(m_v_bitrate);
	proc->AddProcessArg("-ab");
	proc->AddProcessArg(m_s_bitrate);
	// size
	proc->AddProcessArg("-s");
	proc->AddProcessArg(m_size);
	// title
	proc->AddProcessArg("-title");
	proc->AddProcessArg("PSP/"+m_src);

	QString target_path = GetAppSettings()->TargetDir().filePath(dst);
		
	proc->AddProcessArg(target_path);
    return proc->Start();
}

QProcess *CTranscode::StartThumbnail(CJobControlImp *proc, const QString &dst)
{
	proc->AddProcessArg("ffmpeg");
	proc->AddProcessArg("-i");
	proc->AddProcessArg(m_src);
    
	proc->AddProcessArg("-y");

	// size
	proc->AddProcessArg("-s");
	proc->AddProcessArg("160x90");
	// title
	proc->AddProcessArg("-title");
	proc->AddProcessArg(m_src);

	proc->AddProcessArg("-r");
	proc->AddProcessArg("1");
	proc->AddProcessArg("-t");
	proc->AddProcessArg("1");

	proc->AddProcessArg("-ss");
	proc->AddProcessArg("3:00.00");

	proc->AddProcessArg("-an");

	proc->AddProcessArg("-f");
	proc->AddProcessArg("mjpeg");

	QString target_path = GetAppSettings()->TargetDir().filePath(dst);
		
	proc->AddProcessArg(target_path);
    return proc->Start();
}

//
// Queue of pending and running jobs
//

CJobQueue g_job_queue;

CJobQueue::CJobQueue()
{
	m_curr_process = 0;
}

bool CJobQueue::Start()
{
	if ( m_queue.empty() ) {
		return false;
	}

	m_is_aborted = false;

	CTranscode &new_job = m_queue.front();

	CJobControlImp *job_ctrl = new CJobControlImp(UpdateTranscodeProgress, this);
	
	m_total_frames = new_job.TotalFrames();
	
	m_curr_file_id = GetAppSettings()->GetNewOutputNameIdx();
	QString target;
	target.sprintf("M4V%05d.MP4", m_curr_file_id);
	m_curr_process = new_job.Start(job_ctrl, target);
	
	return true;
}

bool CJobQueue::Abort()
{
	//m_curr_process->writeToStdin("q");
	m_curr_process->tryTerminate();
	QTimer::singleShot(500, m_curr_process, SLOT( kill() ) );
	
	g_main_win->updateProgress(0, 0);
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
			m_queue.erase(i);
			return true;
		}
	}
	return false;
}

void CJobQueue::ParseFfmpegOutputLine(const char *line)
{
	//printf("XXX0 => %s\n", line);
	if ( m_in_progress_out ) {
		char *p = strstr(line, "frame=");
		if ( !p ) {
			return;
		}
		p += strlen("frame=");
		while ( isspace(*p) ) p++;
		int frame;
		if ( sscanf(p, "%d", &frame) != 1 ) {
			return;
		}		
		int progress = (frame * 100) / m_total_frames;
		//printf("frame = %d progress = %d\n", frame, progress);
		g_main_win->updateProgress(progress, frame);
	} else {
		if ( strstr(line, "Press [q] to stop encoding") ) {
			m_in_progress_out = true;
		}
	}
}

void CJobQueue::UpdateTranscodeProgress(void *ptr, const char *outline)
{
	CJobQueue *This = (CJobQueue *)ptr;
	if ( outline ) {
		This->ParseFfmpegOutputLine(outline);
	} else {
		This->TranscodeProcessDone();
	}
}

void CJobQueue::TranscodeProcessDone()
{
	printf("TranscodeProcessDone\n");
	QString target;
	target.sprintf("M4V%05d.THM", m_curr_file_id);

	CTranscode &new_job = m_queue.front();

	CJobControlImp *job_ctrl = new CJobControlImp(UpdateThumbnailProgress, this);
	m_curr_process = new_job.StartThumbnail(job_ctrl, target);
}

void CJobQueue::UpdateThumbnailProgress(void *ptr, const char *outline)
{
	CJobQueue *This = (CJobQueue *)ptr;
	if ( outline ) {
	} else {
		This->ThumbnailProcessDone();
	}
}


void CJobQueue::ThumbnailProcessDone()
{
	g_main_win->updateProgress(0, 0);

	// current job done: remove from gui
	g_main_win->removeFromQueue(m_queue.front().Id());

	// done with it - remove from queue
	m_queue.pop_front();

	// start next
	if ( m_is_aborted || !Start() ) {
		g_main_win->enableStart(true);
	}
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

	printf("Tmp dir -> [%s]\n", (const char *)m_tmp_dir_path);
}

CAppSettings::~CAppSettings()
{
}

int CAppSettings::GetNewOutputNameIdx() const
{
	for(int i = 1 ; i < 999999; i++) {
		QString next_name;
		next_name.sprintf("M4V%05d.MP4", i);
		if ( !QFile::exists(GetAppSettings()->TargetDir().filePath(next_name)) ) {
			return i;
		}
	}
	return -1;
}

QString CAppSettings::Get_FFMPEG()
{
	QString ffmpeg;
#if defined(Q_WS_X11)
	// on unix ffmpeg is expected to be in the path
	return "ffmpeg";
#endif
#ifdef Q_WS_WIN
#endif
	return ffmpeg;
}

CPSPMovie::CPSPMovie(int id)
{
	m_id = id;

	m_thmb_name.sprintf("M4V%05d.THM", m_id);
	m_movie_name.sprintf("M4V%05d.MP4", m_id);
	
	m_have_thumbnail = QFile::exists(GetAppSettings()->TargetDir().filePath(m_thmb_name));
	QFile f(GetAppSettings()->TargetDir().filePath(m_movie_name));
	m_size = f.size();
}

CPSPMovie::CPSPMovie(QFileInfo *info) : m_dir(info->dirPath(TRUE))
{
	QRegExp id_exp("M4V(\\d{5})", FALSE);
	if ( !id_exp.exactMatch(info->baseName()) ) {
		m_id = -1;
		return;
	}
	m_id = id_exp.cap(1).toInt();
	m_movie_name = info->baseName().upper() + ".MP4";
	m_thmb_name = info->baseName().upper() + ".THM";

	m_have_thumbnail = QFile::exists(info->absFilePath() + m_thmb_name);
	m_size = info->size();
	
	m_str_size = CastToXBytes(m_size);
}

bool CPSPMovie::DoCopy(QWidget *parent, const QString &source, const QString &target)
{
	QFile src(source);
	QFile dst(target);
	if ( !src.open(IO_Raw | IO_ReadOnly ) ) {
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

bool CPSPMovie::TransferTo(QWidget *parent, const QString &target_dir)
{
	QDir trgdir(target_dir);
	// movie going first
	if ( !DoCopy(parent, m_dir.filePath(m_movie_name),
			trgdir.filePath(m_movie_name)) ) {
		return false;
	}
	if ( !DoCopy(parent, m_dir.filePath(m_thmb_name),
			trgdir.filePath(m_thmb_name)) ) {
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
	printf("Loading list from [%s]\n", (const char *)dir_path);

	// on vfat it's shown in lower case !
	m_source_dir.setNameFilter("M4V*.MP4 m4v*.mp4");
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
	CPSPMovie &m = m_movie_set[id];
	return m.TransferTo(parent, dest);
}

bool CPSPMovieLocalList::TransferPSP(QWidget *parent, int id, const QString &base)
{
	Q_ASSERT ( m_movie_set.count(id) );
	CPSPMovie &m = m_movie_set[id];
	
	QDir trg_dir(QDir::cleanDirPath(base + QDir::convertSeparators("/MP_ROOT/100MNV01")));
	QDir trg_dir_backup(QDir::cleanDirPath(base + QDir::convertSeparators("/MP_ROOT/100MNV01_BACK")));
	if ( trg_dir.exists() ) {
		trg_dir.rename(trg_dir.path(), trg_dir_backup.path());
	}
	if ( !trg_dir.mkdir(trg_dir.path()) ) {
		return false;
	}
	if ( !m.TransferTo(parent, trg_dir.path()) ) {
		return false;
	}
	if ( trg_dir_backup.exists() ) {
		const QFileInfoList *files = trg_dir_backup.entryInfoList(QDir::Files);
		if ( files ) {
			QFileInfoListIterator it(*files);
			QFileInfo *fi;
			while ( ( fi = it.current() ) != 0 ) {
				trg_dir.rename(fi->filePath(), trg_dir.filePath(fi->fileName()));
				++it;
			}
		}
		if  ( !trg_dir_backup.rmdir(trg_dir_backup.path()) ) {
			printf("remove failed\n");
		}
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
	
	app.setMainWidget(&mainwin);
	mainwin.show();
	
	GetAppSettings();
	
	return app.exec();
}

#include "pspmovie.moc"
