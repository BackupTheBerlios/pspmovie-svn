#include <QApplication>
#include <QtGui>

#include "mainwin.h"
#include "avutils.h"

#include "pspmovie.h"

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
// Transcoding job
//
int CTranscode::m_curr_id = 1001;

CTranscode::CTranscode(QString &src, uint32_t thumbnail_time,
			QString &s_bitrate, QString &v_bitrate, bool fix_aspect)
{
	CAVInfo in_info(src.toUtf8());
	m_input_ok = in_info.HaveVStream() && in_info.HaveAStream() && in_info.CodecOk();
	if ( !m_input_ok ) {
		m_input_error = in_info.InputError();
		return;
	}
	m_frame_count = in_info.FrameCount();
	m_being_run = false;
	m_src = src;
	m_thumbnail_time = thumbnail_time;
	
	m_fix_aspect = fix_aspect;
	
	m_id = m_curr_id++;
	
	if ( m_src.length() > 50 ) {
		QFileInfo fi(m_src);
		QString short_path = fi.absoluteDir().path().left(40) + QDir::convertSeparators(".../");
		QString short_name = fi.fileName();
		if ( fi.fileName().length() > 20 ) {
			short_name = fi.baseName().left(10) + "..." +
				fi.fileName().right(10);
		}
		m_short_src =  short_path + short_name;
	} else {
		m_short_src = m_src;
	}

	s_bitrate.remove("kbps");
	v_bitrate.remove("kbps");
	m_s_bitrate = s_bitrate.toInt();
	m_v_bitrate = v_bitrate.toInt();

	int s = in_info.Sec() % 60, ms = in_info.Usec() / 1000;
	int h = in_info.Sec() / 3600;
	int m = (in_info.Sec() - h*3600) / 60;
	m_str_duration = QString( "%1:%2:%3.%4" )
                    .arg( h ) .arg( m ) .arg( s ) .arg( ms );

	if ( fix_aspect ) {
		/*
		 * PSP have screen size 480 x 272 pixel, and can
		 * resize movie to fit the screen discarding aspect ratio
		 */
		int h, w;
		float ratio = (float)in_info.H() / (float)in_info.W();
		// adding horizontal/vertical strips. In this case movie is being optimized
		// for full-screen view mode (aspect ratio is discarded)
		if ( ratio >= (9.0/16.0) ) {
			// adding vertical strips. In this case movie is being optimized
			// for scaled view mode (aspect ratio is maintainted)
			h = 240;
			//w = m_in_info.W() * 240 / m_in_info.H();
			w = in_info.W() * 180 / in_info.H();
		} else {
			// h = H/W x 480 x (240/270)
			w = 320;
			h = in_info.H() * 160 * 8 / in_info.W() / 3;
		}
		m_v_padding = ((240 - h) / 2) & 0xfffe;
		m_h_padding = ((320 - w) / 2) & 0xfffe;
		
		if ( (h + 2 * m_v_padding) != 240 ) {
			h += 240 - (h + 2 * m_v_padding);
		}
		if ( (w + 2 * m_h_padding) != 320 ) {
			w += 320 - (w + 2 * m_h_padding);
		}
	} else {
		m_v_padding = 0;
		m_h_padding = 0;
	}
}

bool CTranscode::IsOK()
{
	return m_input_ok;
}

int CTranscode::TotalFrames()
{
	return m_frame_count;
}

void CTranscode::RunTranscode(CFFmpeg_Glue &ffmpeg, int (cb)(void *, int), void *ptr)
{
	m_being_run = true;
	QFileInfo fi(m_src);
	QString target_path = GetAppSettings()->TargetDir().filePath(fi.baseName() + ".mp4");
	
	//
	// Some tell, that other resolutions bisides 320x240 are possible. Never
	// found it to be true
	//
	int v_size = 240 - 2*m_v_padding;
	int h_size = 320 - 2*m_h_padding;
	ffmpeg.RunTranscode(m_src.toUtf8(), target_path.toUtf8(), m_s_bitrate, m_v_bitrate,
		v_size, h_size, m_v_padding, m_h_padding, 
		fi.baseName().toUtf8(), cb, ptr);
}

void CTranscode::RunThumbnail(CFFmpeg_Glue &)
{
	QFileInfo fi(m_src);
	QString target_path = GetAppSettings()->TargetDir().filePath(fi.baseName() + ".thm");

	CAVInfo m_in_info(m_src.toUtf8());
	m_in_info.Seek(m_thumbnail_time);
	m_in_info.GetNextFrame();

	QImage img(m_in_info.ImageData(), m_in_info.W(), m_in_info.H(),
		QImage::Format_RGB32);
	img.scaled(160, 120).save(target_path, "JPEG");
}

const QString CTranscode::Target()
{
	QString s = QString("%1 / %2 kbps") . arg(m_v_bitrate) . arg(m_s_bitrate);

	return s;
}

//
// Class representing transcoded file
//
int CPSPMovie::s_next_id = 1;

CPSPMovie::CPSPMovie(const QFileInfo &info) : m_dir(info.dir().path())
{
	QRegExp id_exp("M4V(\\d{5})", Qt::CaseInsensitive);
	if ( id_exp.exactMatch(info.baseName()) ) {
		// this file on PSP
		m_id = id_exp.cap(1).toInt();
		m_movie_name = info.completeBaseName().toUpper() + ".MP4";
		m_thmb_name = info.completeBaseName().toUpper() + ".THM";
	} else {
		m_id = s_next_id++;
		m_movie_name = info.completeBaseName() + ".mp4";
		m_thmb_name = info.completeBaseName() + ".thm";
	}

	//printf("Test thumbnail at [%s]\n", (const char *)m_dir.filePath(m_thmb_name));
	
	m_have_thumbnail = QFile::exists(m_dir.filePath(m_thmb_name));
	m_size = info.size();
//	printf("File [%d] [%s] with thumbnail [%s]\n", m_id, (const char *)m_movie_name.toUtf8(),
//	       m_have_thumbnail ? "yes" : "no");
	m_str_size = CastToXBytes(m_size);
	
	if ( m_have_thumbnail ) {
		m_icon = QImage(m_dir.filePath(m_thmb_name)).scaled(2*32, 2*24);
	}
	
	char title_buf[512];
	GetMP4Title(m_dir.absoluteFilePath(m_movie_name).toUtf8(), title_buf);
	m_movie_title = title_buf;

	if ( m_movie_title.length() < 4 ) {
		m_movie_title = m_movie_name;
	}
}

bool CPSPMovie::DoCopy(QWidget *parent, const QString &source, const QString &target)
{
	//printf("Copying [%s] -> [%s]\n", (const char *)source, (const char *)target);
	QFile src(source);
	QFile dst(target);
	if ( !src.open(QIODevice::ReadOnly | QIODevice::Unbuffered) ) {
		printf("ERROR: src open failed with error %d\n", src.error());
		return false;
	}
	if ( !dst.open(QIODevice::Truncate | QIODevice::WriteOnly | QIODevice::Unbuffered) ) {
		printf("ERROR: dst open failed with error %d\n", dst.error());
		return false;
	}
	sync();

	const int bufsize = 0x10000;

	QProgressDialog progress(QString("Copying file: ") + source,
		"Abort Copy", 0, src.size() / bufsize, parent);

	char *buffer = new char[bufsize];
	int curr_step = 0;
	while( !src.atEnd() ) {
		
	    progress.setValue(curr_step);
	    qApp->processEvents();
	
	    if ( progress.wasCanceled() ) {
	        break;
	    }
	    
		qint64 sz = src.read(buffer, bufsize);

		if ( sz == -1 ) {
			delete buffer;
			return false;
		}
		dst.write(buffer, sz);
		dst.flush();
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
		CAVInfo in_info(m_dir.filePath(m_movie_name).toUtf8());
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
	printf("Loading list from [%s]\n", (const char *)dir_path.toUtf8());

	// on vfat it's shown in lower case !
	QStringList name_filter;
	name_filter << "*.MP4" << "*.mp4";
	m_source_dir.setNameFilters(name_filter);
	QFileInfoList files(m_source_dir.entryInfoList(QDir::Files | QDir::NoSymLinks | QDir::Readable));
	for(QList<QFileInfo>::const_iterator it = files.begin(); it != files.end(); it++) {
    	CPSPMovie m(*it);
    	m_movie_set[m.Id()] = m;
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
	  printf("DEBUG: rename orig dir [%s] -> [%s]\n", (const char *)trg_dir.path().toUtf8(),
	  	(const char *)trg_dir_backup.path().toUtf8());
	  if ( !trg_dir.rename(trg_dir.path(), trg_dir_backup.path()) ) {
	    printf("DEBUG: dir failed rename\n");
	  }
	}
	trg_dir = QDir(mp_root.filePath("100MNV01"));
	sync();
	if ( !trg_dir.exists() ) {
	    printf("DEBUG: dir doesn't exist - will create\n");
		if ( !trg_dir.mkpath(trg_dir.path()) ) {
		    printf("DEBUG: dir failed create\n");
			return false;
		}
	}
	printf("DEBUG: transferring [%s] -> [%s]\n", (const char *)m.Name().toUtf8(), (const char *)trg_dir.path().toUtf8());
	if ( !m.TransferTo(parent, trg_dir.path(), free_idx) ) {
	  	printf("DEBUG: transfer failed\n");
		return false;
	}
	printf("Checking backup dir [%s]\n", (const char *)trg_dir_backup.path().toUtf8());
	if ( trg_dir_backup.exists() ) {
		QFileInfoList files(trg_dir_backup.entryInfoList(QDir::Files | QDir::Readable));
		for(QList<QFileInfo>::const_iterator it = files.begin(); it != files.end(); it++) {
			QString backup_src(it->filePath());
			QString backup_dst(trg_dir.filePath(it->fileName().toUpper()));
			printf("DEBUG: moving [%s] -> [%s]\n", (const char *)backup_src.toUtf8(), (const char *)backup_dst.toUtf8());
			if ( !QFile::rename(backup_src, backup_dst) ) {
				printf("OOps - rename failed: orig %s exists, target dir %s exists\n",
					QFile::exists(backup_src) ? "-" : "doesn't", trg_dir.exists() ? "-" : "doesn't");
//				if ( rename((const char *)backup_src.toUtf8(), (const char *)backup_dst.toUtf8()) ) {
//					perror("rename");
//				}
			}
			sync();
		}
		
		printf("Will remove [%s]\n", (const char *)trg_dir_backup.path().toUtf8());
		if  ( !trg_dir_backup.rmdir(trg_dir_backup.path()) ) {
			printf("remove failed\n");
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

//
// Application preferences
//
const CAppSettings *GetAppSettings()
{
	static CAppSettings app_settings;

	return &app_settings;
}

CAppSettings::CAppSettings(): m_settings("pspmovie")
{
	//m_settings.setPath(QSettings::NativeFormat, QSettings::UserScope, "pspmovie");
	
	// FIXME: set correct dir on Windows
	m_app_dir_path = QDir::cleanPath(QDir::homePath() + QDir::convertSeparators("/.pspmovie/"));
	//m_settings.insertSearchPath( QSettings::Unix, m_app_dir_path);
	
	QDir dir(m_app_dir_path);
	if ( !dir.exists() ) {
		if ( !dir.mkdir(m_app_dir_path) ) {
			QMessageBox::critical(0, "Error", "unable to create application directory");
			qApp->exit(-1);
		}
	}
	m_tmp_dir_path = QDir::cleanPath(m_app_dir_path + QDir::convertSeparators("/100MNV01/"));
	
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

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(pspmovie);

	//
	// init connection to ffmpeg lib
	//
	CFFmpeg_Glue g; 
	QApplication app(argc, argv);

 	if ( !CanDoPSP() ) {
		QMessageBox::critical(0, "ERROR: bad ffmpeg library",
			"FFMPEG library you have can not encode PSP format correctly\n"
			"You have version \"" FFMPEG_VERSION "\" of FFMPEG"
			);
		return -1;
	}

	MainWindow win(&g);
	win.show();
	app.exec();	
	return 0;
}
