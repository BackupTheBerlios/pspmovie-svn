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
    
    
}


void XferWin::refreshPSP()
{
    CPSPMovieLocalList psp_list(m_mount_point);
    for(CPSPMovieLocalList::CPSPMovieListIt i = psp_list.Begin(); i != psp_list.End(); i++) {
	CPSPMovie &m = i->second;
	new CXferListItem(listView_PSP, m.Id(), m.Name(), m.Size());
    }
}


void XferWin::refreshLocal()
{
    CPSPMovieLocalList psp_list(GetAppSettings()->TargetDir());
    for(CPSPMovieLocalList::CPSPMovieListIt i = psp_list.Begin(); i != psp_list.End(); i++) {
	CPSPMovie &m = i->second;
	new CXferListItem(listView_PSP, m.Id(), m.Name(), m.Size());
    }
}
