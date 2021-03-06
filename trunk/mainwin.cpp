#include <QtGui>

#include "mainwin.h"
#include "xferwin.h"
#include "transcode.h"
#include "pspmovie.h"
#include "pspdetect.h"

class CFFmpeg_Glue;

MainWindow::MainWindow(CFFmpeg_Glue *gl) : QMainWindow(0)
{
	m_current_job = 0;
	m_ffmpeg = gl;
	ui.setupUi(this);
	
	ui.lcdNumberFrames->display(0);
	ui.lcdNumberFramesTotal->display(0);
	ui.progressBar->setValue(0);
	m_current_job = 0;
}

MainWindow::~MainWindow()
{
}

void MainWindow::on_transcodeButton_clicked()
{
	//
	// Check if transcode already running, so it's "Cancel" click
	//
	if ( m_current_job ) {
		if ( QMessageBox::question(this, "PSPMovie", tr("Abort transcoding ?"),
				QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes ) {
			m_stop_transcode = true;
		}
		return;		
	}
	
	TranscodeDialog dlg;
	if ( !dlg.exec() ) {
		return;
	}
	//
	// begin transcoding
	//
	CTranscode *job = dlg.getJob();
	if ( !job ) {
		return;
	}
	m_current_job = job;
	m_stop_transcode = false;

	ui.lcdNumberFramesTotal->display(m_current_job->TotalFrames());
	ui.lcdNumberFrames->display(0);
	ui.progressBar->setValue(0);
	m_last_update_frame = 0, m_last_update_time = time(0);
	
	ui.transcodeButton->setIcon(QPixmap(":/mainwin/images/cancel.png").scaled(64,64));
	ui.transcodeButton->setToolTip(tr("Stop transcoding"));
	ui.transcodeButton->setStatusTip(tr("Stop transcoding"));
	
	job->RunTranscode(*m_ffmpeg, UpdateTranscodeProgress, this);
	job->RunThumbnail(*m_ffmpeg);
	
	ui.lcdNumberFrames->display(0);
	ui.lcdNumberFramesTotal->display(0);
	ui.progressBar->setValue(0);
	m_last_update_frame = 0, m_last_update_time = time(0);
	
	delete m_current_job;
	m_current_job = 0;
	ui.transcodeButton->setIcon(QPixmap(":/mainwin/images/folder_video.png").scaled(128,128));
	ui.transcodeButton->setToolTip(tr("Transcode movie to PSP format"));
	ui.transcodeButton->setStatusTip(tr("Transcode movie to PSP format"));
}

int MainWindow::UpdateTranscodeProgress(void *p, int num_of_frames_done)
{
	MainWindow *This = (MainWindow *)p;
	num_of_frames_done /= 2;
	//printf("this=%p, progress=%d of %d\r", This, progress, This->m_current_job->TotalFrames());
	if ( ((num_of_frames_done - This->m_last_update_frame) > 100) || ((time(0) - This->m_last_update_time) > 0) ) { 
		This->ui.progressBar->setValue(num_of_frames_done*100/This->m_current_job->TotalFrames());
		This->ui.lcdNumberFrames->display(num_of_frames_done);
		This->m_last_update_frame = num_of_frames_done;
		qApp->processEvents();
		This->m_last_update_time = time(0);
	}
	
	return !This->m_stop_transcode;
}

void MainWindow::on_xferButton_clicked()
{
	char *psp_mount_path = find_psp_mount(0, 0);
	QString q_psp_mount_path(psp_mount_path);
	if ( !psp_mount_path ) {
		q_psp_mount_path = QFileDialog::getExistingDirectory(
			this, "Choose destination directory", 0,
			QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
		if ( q_psp_mount_path.isEmpty() ) {
			return;
		}
	} else {
		free(psp_mount_path);
	}
	XferDialog dlg(q_psp_mount_path, this);
	if ( !dlg.exec() ) {
		return;
	}
}

void MainWindow::closeEvent(QCloseEvent * event)
{
	if ( m_current_job ) {
		if ( QMessageBox::warning(this, "PSPMovie", tr("Transcoding is still running\n"
				"Stop transcoding and quit the program ?"), "Yes", "No", 0, 1) ) {
			event->ignore();
		} else {
			m_stop_transcode = true;
		}
	}
}
