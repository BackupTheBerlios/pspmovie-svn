#ifndef MAINWIN_FORM_H
#define MAINWIN_FORM_H

#include "ui_mainwin.h"

class CFFmpeg_Glue;
class CTranscode;

class MainWindow : public QMainWindow {
		Q_OBJECT
	public:
		MainWindow(CFFmpeg_Glue *gl);
		~MainWindow();
	
    private slots:
    	void on_transcodeButton_clicked();
    	void on_xferButton_clicked();
    	
    private:
		Ui::MainWindow ui;
		bool m_stop_transcode;
		
		CFFmpeg_Glue *m_ffmpeg;

		CTranscode *m_current_job;
		
		void closeEvent(QCloseEvent * event);
		
		static int UpdateTranscodeProgress(void *, int);
};

#endif
