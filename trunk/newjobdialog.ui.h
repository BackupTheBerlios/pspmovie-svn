/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/
#include <qfiledialog.h> 

void NewJobDialog::Browse_clicked()
{
    QFileDialog fd( this, "Source file", true);
    fd.setMode(QFileDialog::ExistingFile);
    fd.addFilter( "Movies (*.avi *.mpg *.mpeg *.vob *.mov)" );
    
    QString fileName;
    if ( fd.exec() == QDialog::Accepted ) {
		fileName = fd.selectedFile();
		lineEdit_File->setText(fileName);
    }
}




void NewJobDialog::slider_thm_time_valueChanged(int v)
{
    m_thumbnail_time = m_avinfo->Sec() * v / 100;
    if ( m_avinfo->Seek(m_thumbnail_time) && m_avinfo->GetNextFrame() )  {
    	QImage img(m_avinfo->ImageData(), m_avinfo->W(), m_avinfo->H(),
    		32, 0, 0, QImage::LittleEndian);
    	pixmapLabel_Thumbnail->setPixmap(img);
    	lCDNumber_H->display((int)m_thumbnail_time / 3600);
    	lCDNumber_M->display((int)(m_thumbnail_time / 60) % 60);
    	lCDNumber_S->display((int)m_thumbnail_time % 60);
    }
}


void NewJobDialog::lineEdit_File_textChanged( const QString &s)
{
	pixmapLabel_Status->setPixmap(QImage::fromMimeSource("cancel.png"));
	if ( s.stripWhiteSpace().length() == 0 ) {
    	textLabel_Status->setText("No input file");
    	m_avinfo = 0;
		return;
	}
	QFileInfo fi(s);
	if ( !QFile::exists(s) || !fi.exists() ) {
    	textLabel_Status->setText("Input file doesn't exists");
    	return;
	}
	if ( m_avinfo ) {
		delete m_avinfo;
	}
	
	m_avinfo = new CAVInfo(s);
	m_thumbnail_time = 0;

    if ( !m_avinfo->HaveVStream() || !m_avinfo->HaveAStream() || !m_avinfo->CodecOk() ) {
    	textLabel_Status->setText(m_avinfo->InputError());
		return;
    }
	pixmapLabel_Status->setPixmap(QImage::fromMimeSource("ok.png"));
	textLabel_Status->setText("input file is OK");

	slider_thm_time_valueChanged(1);
}


void NewJobDialog::init()
{
	m_avinfo = 0;
}


void NewJobDialog::destroy()
{
	if ( m_avinfo ) {
		delete m_avinfo;
	}
	m_avinfo = 0;
}
