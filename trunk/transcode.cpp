#include <QtGui>
#include <QDialog>
#include <QFileDialog>

#include "pspmovie.h"
#include "transcode.h"
#include "avutils.h"

TranscodeDialog::TranscodeDialog(QWidget *parent) : QDialog(parent)
{
	m_avinfo = 0;
	ui.setupUi(this);
	ui.okButton->setEnabled(false);
}

TranscodeDialog::~TranscodeDialog()
{
}

void TranscodeDialog::on_browseButton_clicked()
{
	static QString last_used_dir = QDir::homePath();

    QFileDialog fd( this, "Source file", last_used_dir);
    fd.setFileMode(QFileDialog::ExistingFile);
    fd.setFilter( "Movies (*.avi *.mpg *.mpeg *.vob *.mov)" );
    
    QString fileName;
    if ( fd.exec() == QDialog::Accepted ) {
		fileName = fd.selectedFiles()[0];
		last_used_dir = fd.directory().path();
		ui.filenameEdit->setText(fileName);
    } else {
    	ui.okButton->setEnabled(false);
    }
}

void TranscodeDialog::on_filenameEdit_textChanged (const QString &s)
{
//	printf("TranscodeDialog::on_filenameEdit_changed()\n");
	if ( s.trimmed().length() == 0 ) {
    	//textLabel_Status->setText("No input file");
    	m_avinfo = 0;
		return;
	}
	QFileInfo fi(s);
	if ( !QFile::exists(s) || !fi.exists() ) {
    	//textLabel_Status->setText("Input file doesn't exists");
    	return;
	}
	if ( m_avinfo ) {
		delete m_avinfo;
		m_avinfo = 0;
	}
	m_avinfo = new CAVInfo(s.trimmed().toStdString().c_str());

	//m_thumbnail_time = 0;

    if ( !m_avinfo->HaveVStream() || !m_avinfo->HaveAStream() || !m_avinfo->CodecOk() ) {
    	//textLabel_Status->setText(m_avinfo->InputError());
    	delete m_avinfo;
    	m_avinfo = 0;
		return;
    }
    m_filename = s;
	//ui.thumbnailLabel->setPixmap(QImage::fromMimeSource("ok.png"));
	//textLabel_Status->setText("input file is OK");

	//slider_thm_time_valueChanged(1);
	ui.okButton->setEnabled(true);
	on_thumbnailSlider_valueChanged(0);
}

void TranscodeDialog::on_thumbnailSlider_valueChanged(int value)
{
	m_thumbnail_time = value * m_avinfo->Sec() / ui.thumbnailSlider->maximum();
	printf("image: %d -> %d (of %d)\n", value, m_thumbnail_time, m_avinfo->Sec());
    if ( m_avinfo->Seek(m_thumbnail_time) && m_avinfo->GetNextFrame() )  {
    	QImage img(m_avinfo->ImageData(), m_avinfo->W(), m_avinfo->H(),QImage::Format_RGB32);
		QImage scaled_img (img.scaled(ui.thumbnailLabel->width(), ui.thumbnailLabel->height()));
    	ui.thumbnailLabel->setPixmap(QPixmap::fromImage(scaled_img));
//    	lCDNumber_H->display((int)m_thumbnail_time / 3600);
//    	lCDNumber_M->display((int)(m_thumbnail_time / 60) % 60);
//    	lCDNumber_S->display((int)m_thumbnail_time % 60);
    }
}

CTranscode *TranscodeDialog::getJob()
{
	if ( !isOk() ) {
		return 0;
	}
	QString vrate = ui.VideoBitrateSel->currentText();
	QString arate = ui.AudioBitrateSel->currentText();
	CTranscode *job = new CTranscode(m_filename, m_thumbnail_time, arate, vrate, true);
	return job;
}
