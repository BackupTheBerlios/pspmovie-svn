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
#include <qcombobox.h> 
#include <qcheckbox.h> 
#include <qlineedit.h> 
#include <qmessagebox.h>

#include "pspdetect.h"
#include "pspmovie.h"

class CXferListItem : public QListViewItem {
    int m_id;
public:
    CXferListItem(QListView *parent, int id, const QString &name, const QString &size) :
	    QListViewItem(parent, name, size)
    {
	m_id = id;
    }
};

void XferWin::init()
{
    char error_buff[256];
    char *mount_point = find_psp_mount(error_buff, sizeof(error_buff));
    if ( mount_point ) {
	m_mount_point = mount_point;
	lineEdit_Mount->setText(mount_point);
    } else {
	m_mount_point = "";
    }
    
    refreshPSP();
    refreshLocal();
}


void XferWin::refreshPSP()
{
    m_psp_list = new CPSPMovieLocalList(m_mount_point);
    for(CPSPMovieLocalList::CPSPMovieListIt i = m_psp_list->Begin(); i != m_psp_list->End(); i++) {
	CPSPMovie &m = i->second;
	new CXferListItem(listView_PSP, m.Id(), m.Name(), m.Size());
    }
}


void XferWin::refreshLocal()
{
    m_local_list = new CPSPMovieLocalList(GetAppSettings()->TargetDir().path());
    for(CPSPMovieLocalList::CPSPMovieListIt i = m_local_list->Begin(); i != m_local_list->End(); i++) {
	CPSPMovie &m = i->second;
	new CXferListItem(listView_Local, m.Id(), m.Name(), m.Size());
    }
}


void XferWin::toPSP_clicked()
{
    CXferListItem *it = (CXferListItem *)listView_Local->selectedItem();
    if ( !it ) {
	QMessageBox::information(this, "No file selected", "Please select file to transfer");
	return;
    }
}


void XferWin::toDesktop_clicked()
{

}


void XferWin::browsePSP_clicked()
{

}


void XferWin::deleteFile_clicked()
{

}
