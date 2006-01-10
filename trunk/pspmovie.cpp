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

	QString target_path = QDir::cleanDirPath(GetAppSettings()->TargetDir() +
		QDir::convertSeparators("/") + dst);
		
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

	QString target_path = QDir::cleanDirPath(GetAppSettings()->TargetDir() +
		QDir::convertSeparators("/") + dst);
		
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
	target.sprintf("M4V%5d.MP4", m_curr_file_id);
	m_curr_process = new_job.Start(job_ctrl, target);
	
	return true;
}

bool CJobQueue::Abort()
{
	m_curr_process->writeToStdin("q");
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
	target.sprintf("M4V%5d.THM", m_curr_file_id);

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
	
	m_app_dir = QDir::cleanDirPath(QDir::homeDirPath() + QDir::convertSeparators("/.pspmovie/"));
	m_settings.insertSearchPath( QSettings::Unix, m_app_dir);
	
	QDir dir(m_app_dir);
	if ( !dir.exists() ) {
		if ( !dir.mkdir(m_app_dir) ) {
			QMessageBox::critical(0, "Error", "unable to create application directory");
			qApp->exit(-1);
		}
	}
	m_tmp_dir = QDir::cleanDirPath(m_app_dir + QDir::convertSeparators("/100MNV01/"));
	QDir tmp_dir(m_tmp_dir);
	if ( !tmp_dir.exists() ) {
		if ( !tmp_dir.mkdir(m_tmp_dir) ) {
			QMessageBox::critical(0, "Error", "unable to create output directory");
			m_tmp_dir = QString(0);
		}
	}
}

CAppSettings::~CAppSettings()
{
}

int CAppSettings::GetNewOutputNameIdx() const
{
	for(int i = 0 ; i < 999999; i++) {
		QString next_name = QString("M4V%1.MP4").arg(i, 6, 10);
		if ( !QFile::exists(next_name) ) {
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

int main( int argc, char **argv )
{
	QApplication app(argc, argv);
	MainWin mainwin;
	g_main_win = &mainwin;
	
	app.setMainWidget(&mainwin);
	mainwin.show();
	
	return app.exec();
}

#include "pspmovie.moc"
