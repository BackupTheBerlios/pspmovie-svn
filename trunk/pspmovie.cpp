#include <qapplication.h>
#include <qmessagebox.h>
#include <qregexp.h>

#include <math.h>

#include "pspmovie.h"

#include "mainwin.h"
MainWin *g_main_win = 0;

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
int CTranscode::m_curr_id = 1001; // Qt like it this way in rtti()

CTranscode::CTranscode(QString &src, QString &title, QString &size,
			QString &s_bitrate, QString &v_bitrate, bool fix_aspect)
{
	m_src = src;
	m_title = title;
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

void CTranscode::CheckInputCallback(void *ptr, const char *s)
{
	CTranscode *This = (CTranscode *)ptr;
	//printf("XXX0 = [%s]\n", s);
	if ( s && !strncmp(s, "Input #0", strlen("Input #0")) ) {
		This->m_duration = -1;
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
		if ( sscanf(cp, "%dx%d", &This->m_width, &This->m_height) != 2 ) {
			return;
		}
		cp = tok[3];
		while( isspace(*cp) ) cp++;
		if ( sscanf(cp, "%f", &This->m_fps) != 1 ) {
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
		This->m_str_duration = QString( "%1:%2:%3.%4" )
                        .arg( h ) .arg( m ) .arg( s ) .arg( ms );
		//printf("all ok\n");
		This->m_duration = ms + 1000*(s + 60*(m + 60*h));
	}
}

void CTranscode::CheckInput()
{

	CJobControlImp *proc = new CJobControlImp(CheckInputCallback, this);

	proc->AddProcessArg("ffmpeg");
	proc->AddProcessArg("-i");
	proc->AddProcessArg(m_src);
	
	m_duration = 0;
	if ( !proc->Start() ) {
		printf("start failed\n");
	}
	
	while ( !m_duration ) {
		qApp->processEvents();
	}
	//printf("test done\n");
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
	proc->AddProcessArg(m_title);

	QString target_path = GetAppSettings()->TargetDir().filePath(dst);
		
	//proc->AddProcessArg("MP_ROOT/100MNV01/M4V00010.MP4");
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
	proc->AddProcessArg(m_title);

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
	m_curr_process->writeToStdin("q");
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
	// done with it - remove from queue
	m_queue.pop_front();

	g_main_win->updateProgress(0, 0);
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

CPSPMovie::CPSPMovie(QFileInfo *info)
{
	QRegExp id_exp("M4V(\\d{5})");
	if ( !id_exp.exactMatch(info->baseName()) ) {
		m_id = -1;
		return;
	}
	m_id = id_exp.cap(1).toInt();
	m_movie_name = info->baseName() + ".MP4";
	m_thmb_name = info->baseName() + ".THM";
	m_have_thumbnail = QFile::exists(info->absFilePath() + m_thmb_name);
	m_size = info->size();
}

bool CPSPMovie::DoCopy(const QString &source, const QString &target)
{
	QFile src(source);
	QFile dst(target);
	if ( src.open(IO_Raw) ) {
		return false;
	}
	if ( dst.open(IO_Raw | IO_Truncate) ) {
		return false;
	}
	char buffer[4096];
	while( !src.atEnd() ) {
		Q_LONG sz = src.readBlock(buffer, sizeof(buffer));
		if ( sz == -1 ) {
			return false;
		}
		dst.writeBlock(buffer, sz);
	}
	return true;
}

bool CPSPMovie::TransferTo(const QString &target_dir)
{
	// movie going first
	if ( !DoCopy(GetAppSettings()->TargetDir().filePath(m_movie_name),
			target_dir + m_movie_name) ) {
		return false;
	}
	if ( !DoCopy(GetAppSettings()->TargetDir().filePath(m_thmb_name),
			target_dir + m_thmb_name) ) {
		return false;
	}

	return true;
}

CPSPMovieLocalList::CPSPMovieLocalList(const QString &dir_path)
{
	QDir dir(dir_path);
	dir.setNameFilter("M4V*.MP4");
	const QFileInfoList *files = dir.entryInfoList(QDir::Files | QDir::NoSymLinks | QDir::Readable);
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

void CPSPMovieLocalList::Transfer(int id, const QString &dest)
{
	Q_ASSERT ( m_movie_set.count(id) );
	CPSPMovie &m = m_movie_set[id];
	m.TransferTo(dest);
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
