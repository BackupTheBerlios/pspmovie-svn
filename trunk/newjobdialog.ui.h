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
    printf("curr value = %d\n", v);
    uint32_t seek_time = m_avinfo->Sec() * v / 100;
    if ( m_avinfo->Seek(seek_time) && m_avinfo->GetNextFrame() )  {
    	QImage img(m_avinfo->ImageData(), m_avinfo->W(), m_avinfo->H(),
    		32, 0, 0, QImage::LittleEndian);
    	//img.save("th-frame.bmp", "BMP");
    	pixmapLabel_Thumbnail->setPixmap(img);
    }
}


void NewJobDialog::lineEdit_File_textChanged( const QString &s)
{
	m_avinfo = new CAVInfo(s);
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
