#ifndef XFERWIN_FORM_H
#define XFERWIN_FORM_H

#include "ui_xferwin.h"

class CPSPMovieLocalList;

class XferDialog : public QDialog {
		Q_OBJECT
	public:
		XferDialog(const QString &dir, QWidget *parent = 0);
		~XferDialog();
	CPSPMovieLocalList *m_local_file_list, *m_psp_file_list;
	
    private slots:
    	void on_topspButton_clicked();
    	void on_topcButton_clicked();
    	void on_deleteButton_clicked();
	private:
		QString m_psp_dir;
		Ui::XferDialog ui;
		
		void RefreshPSP();
		void RefreshLocal();
};

#endif
