#ifndef TRANSCODE_FORM_H
#define TRANSCODE_FORM_H

#include "ui_transcode.h"

class CAVInfo;
class CTranscode;
class TranscodeDialog : public QDialog {
		Q_OBJECT
	public:
		TranscodeDialog(QWidget *parent = 0);
		~TranscodeDialog();
	
		CTranscode *getJob();
    private slots:
    	void on_browseButton_clicked();
    	void on_filenameEdit_textChanged (const QString &);
    	void on_thumbnailSlider_valueChanged(int);
    private:
    	bool isOk() { return m_avinfo != 0; }
    	
    	QString m_filename;
		Ui::TranscodeDialog ui;
		CAVInfo *m_avinfo;
		int m_thumbnail_time;
};

#endif
