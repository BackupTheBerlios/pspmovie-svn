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
#include <qfiledialog.h>
#include <qlistview.h>

#include "pspdetect.h"
#include "pspmovie.h"

#include "mainwin.h"

class CXferListItem : public QListViewItem {
    int m_id;
public:
    CXferListItem(QListView *parent, int id, const QString &name, const QString &size) :
	    QListViewItem(parent, "", name, size)
    {
	m_id = id;
    }
    int Id() { return m_id; }
};

void XferWin::init()
{
    
    m_local_list = 0;
    m_psp_list = 0;
    
}


void XferWin::refreshData()
{
    checkPSP();
    
    refreshPSP();
    refreshLocal();
}


void XferWin::checkPSP()
{
    char error_buff[256];
    char *mount_point = find_psp_mount(error_buff, sizeof(error_buff));
    if ( mount_point ) {
	m_mount_point = QDir::convertSeparators(QDir::cleanDirPath(mount_point));
	groupBox_PSP->setTitle("PSP location (detected)");
	groupBoxTarget->setTitle("Sony PSP");
	free(mount_point);
    } else {
	groupBox_PSP->setTitle("Target location (PSP not detected)");
	groupBoxTarget->setTitle("Destination");
	m_mount_point = QDir::homeDirPath();
    }
    lineEdit_Mount->setText(m_mount_point);
}


CPSPMovieLocalList * XferWin::refreshList( const QString &dir, QListView *listview )
{
    CPSPMovieLocalList *file_list = new CPSPMovieLocalList(dir);
    listview->clear();
    for(CPSPMovieLocalList::CPSPMovieListIt i = file_list->Begin(); i != file_list->End(); i++) {
	CPSPMovie &m = i->second;
	CXferListItem *it = new CXferListItem(listview, m.Id(), m.Name(), m.Size());
	if ( !m.Icon().isNull() ) {
	    QPixmap pm(m.Icon());
	    it->setPixmap(0, pm);
	}
    }
    return file_list;
}


void XferWin::refreshPSP()
{
    if ( m_psp_list ) {
	delete m_psp_list;
    }
    QDir mp_root(QDir(m_mount_point).filePath("MP_ROOT"));
    m_psp_list = refreshList(mp_root.filePath("100MNV01"), listView_PSP);
}


void XferWin::refreshLocal()
{
    if ( m_local_list ) {
	delete m_local_list;
    }
    m_local_list = refreshList(GetAppSettings()->TargetDir().path(), listView_Local);
}


void XferWin::toPSP_clicked()
{
    QListViewItemIterator it(listView_Local, QListViewItemIterator::Selected );
    while ( it.current() ) {
	CXferListItem *item = (CXferListItem *)it.current();
//	if ( !m_local_list->Transfer(this, item->Id(), m_mount_point) ) {
//	    break;
//	}
	if ( !m_local_list->TransferPSP(this, item->Id(), m_mount_point) ) {
	    break;
	}
	++it;
    }
    refreshPSP();
}


void XferWin::toDesktop_clicked()
{
    QListViewItemIterator it(listView_PSP, QListViewItemIterator::Selected );
    while ( it.current() ) {
	CXferListItem *item = (CXferListItem *)it.current();
	if ( !m_psp_list->Transfer(this, item->Id(), GetAppSettings()->TargetDir().path()) ) {
	    break;
	}
	++it;
    }

    refreshLocal();
}


void XferWin::browsePSP_clicked()
{
    QString s = QFileDialog::getExistingDirectory(
                    QDir::homeDirPath(),
                    this);
    if ( !s.isNull() ) {
	m_mount_point = s;
	lineEdit_Mount->setText(m_mount_point);
	refreshPSP();
    }
}


void XferWin::deleteSelected( QListView *listview, CPSPMovieLocalList *list)
{
    QListViewItemIterator it(listview, QListViewItemIterator::Selected );
    while ( it.current() ) {
	CXferListItem *item = (CXferListItem *)it.current();
	if ( !list->Delete(item->Id()) ) {
	    break;
	}
	++it;
    }
}

void XferWin::deleteFile_clicked()
{
    deleteSelected( listView_PSP, m_psp_list);
    deleteSelected( listView_Local, m_local_list);
    refreshPSP();
    refreshLocal();
}

void XferWin::closeEvent( QCloseEvent * e )
{
    //((MainWindow *)parentWidget())->m_xfer_win = 0;
     e->accept();
}

void XferWin::listView_Local_selectionChanged()
{
    listView_PSP->clearSelection();
}


void XferWin::listView_PSP_selectionChanged()
{
    listView_Local->clearSelection();
}


