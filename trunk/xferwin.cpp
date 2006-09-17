#include <QtGui>

#include "xferwin.h"
#include "pspmovie.h"

class XferListItem : public QTableWidgetItem {
	public:
		CPSPMovie *m_data;

		XferListItem(CPSPMovie *data) : QTableWidgetItem(QTableWidgetItem::UserType+1)
		{
			m_data = data;
		}
};

void FillListControl(CPSPMovieLocalList *file_list, QTableWidget *list)
{
	list->clear();
	int char_width = list->fontMetrics().width('w');
	
	// icon size
	list->setColumnWidth(0, 32);
	
	const int file_name_width = 50;
	list->setColumnWidth(1, char_width*file_name_width);
	
	// file size
	list->setColumnWidth(2, 5*char_width);
	
	list->verticalHeader()->setHidden(true);
	
	//CPSPMovieLocalList file_list(src_dir);
	int row = 0;
	for(CPSPMovieLocalList::CPSPMovieListIt i = file_list->Begin(); i != file_list->End(); i++) {
        CPSPMovie &m = i->second;

        list->insertRow(row);

        QTableWidgetItem *it = new XferListItem(&m);
        printf("New item = %p\n", it);
        if ( !m.Icon().isNull() ) {
	        it->setIcon(QPixmap::fromImage(m.Icon()));
	        list->setItem(row, 0, it);
        }

        list->setItem(row, 1, new QTableWidgetItem(m.Title()));
        
        row++;
	}
}

XferDialog::XferDialog(const QString &psp_dir, QWidget *parent) : QDialog(parent)
{
	ui.setupUi(this);
	m_psp_dir = psp_dir;

	m_local_file_list = 0;
	m_psp_file_list = 0;

	RefreshPSP();
	RefreshLocal();
}

XferDialog::~XferDialog()
{
	delete m_local_file_list;
	delete m_psp_file_list;
}

void XferDialog::RefreshPSP()
{
	if ( m_psp_file_list ) {
		delete m_psp_file_list;
	}
	QDir dir(m_psp_dir);
	m_psp_file_list = new CPSPMovieLocalList(dir.absoluteFilePath("mp_root/100mnv01"));
	FillListControl(m_psp_file_list, ui.pspList);
}

void XferDialog::RefreshLocal()
{
	if ( m_local_file_list ) {
		delete m_local_file_list;
	}
	m_local_file_list = new CPSPMovieLocalList(GetAppSettings()->TargetDir().path());
	FillListControl(m_local_file_list, ui.localList);
}

void XferDialog::on_topspButton_clicked()
{
	QList<QTableWidgetItem *> selitems = ui.localList->selectedItems();
	if ( !selitems.empty() ) {
		for(QList<QTableWidgetItem *>::iterator i = selitems.begin(); i != selitems.end(); i++) {
			if ( (*i)->type() == (QTableWidgetItem::UserType+1) ) {
				XferListItem *it = (XferListItem *)(*i);
				printf("zhopaPSP [%s]\n", (const char *)it->m_data->Name().toUtf8());

				m_local_file_list->TransferPSP(this, it->m_data->Id(), m_psp_dir);
			}
		}
	}
	RefreshPSP();
}

void XferDialog::on_topcButton_clicked()
{
	QList<QTableWidgetItem *> selitems = ui.pspList->selectedItems();
	if ( !selitems.empty() ) {
		for(QList<QTableWidgetItem *>::iterator i = selitems.begin(); i != selitems.end(); i++) {
			if ( (*i)->type() == (QTableWidgetItem::UserType+1) ) {
				XferListItem *it = (XferListItem *)(*i);
				printf("zhopaPC [%s]\n", (const char *)it->m_data->Name().toUtf8());

				m_psp_file_list->Transfer(this, it->m_data->Id(), GetAppSettings()->TargetDir().path());
			}
		}
	}
	RefreshLocal();
}

void DeleteFromList(QTableWidget *list)
{
	QList<QTableWidgetItem *> selitems = list->selectedItems();
	if ( !selitems.empty() ) {
		for(QList<QTableWidgetItem *>::iterator i = selitems.begin(); i != selitems.end(); i++) {
			if ( (*i)->type() == (QTableWidgetItem::UserType+1) ) {
				XferListItem *it = (XferListItem *)(*i);
				printf("zhopa [%s]\n", (const char *)it->m_data->Name().toUtf8());
				it->m_data->Delete();
			}
		}
	}
}

void XferDialog::on_deleteButton_clicked()
{
	DeleteFromList(ui.pspList);
	DeleteFromList(ui.localList);
}
