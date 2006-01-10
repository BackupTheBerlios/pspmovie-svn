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

#include "pspmovie.h"

#include "newjobdialog.h"
#include "xferwin.h"

XferWin *xfer_win = 0;

class CQueueListViewItem : public QListViewItem {
    int m_id;
public:
    CQueueListViewItem(QListView *parent, CTranscode &data) : 
	    QListViewItem(parent, data.ShortName(), data.StrDuration())
    {
	m_id = data.Id();
    }
    int rtti()
    {
	return m_id;
    }
};

void MainWin::newJob()
{
    NewJobDialog dlg(this);
    if ( dlg.exec() != QDialog::Accepted ) {
	return;
    }
    
    QString src = dlg.lineEdit_File->text();
    QString title = dlg.lineEdit_Title->text();
    QString size = dlg.comboBox_TargetSize->currentText();
    QString snd_bitrate = dlg.comboBox_TargetSound->currentText();
    QString video_bitrate = dlg.comboBox_TargetVideo->currentText();
    bool fix_aspect = dlg.checkBox_FixAspect->isChecked();
    
    CTranscode new_job(src, title, size, snd_bitrate, video_bitrate, fix_aspect);
    if ( g_job_queue.Add(new_job) ) {
	new CQueueListViewItem(listView_Queue, new_job);
    }
}


void MainWin::startQueue()
{
    progressBar_Encode->setProgress(0);
    lCDNumber_Frame->display(0);
    lineEdit_CurrFile->setText("");
    
    if ( g_job_queue.IsEmpty() ) {
	newJob();
	if  ( g_job_queue.IsEmpty() ) {
	    return;
	}
    }
    // update gui before item polled out of queue
    CTranscode *job = g_job_queue.NextJob();
    lineEdit_CurrFile->setText(job->ShortName());
    lCDNumber_TotalFrames->display(job->TotalFrames());
    if ( g_job_queue.Start() ) {
	CQueueListViewItem *i = (CQueueListViewItem *)listView_Queue->firstChild();
	delete i;
	
	ActionStart->setEnabled(false);
	ActionAbort->setEnabled(true);
    } else {
	lineEdit_CurrFile->setText("");
	lCDNumber_TotalFrames->display(0);
    }
    
}


void MainWin::abortQueue()
{
    ActionStart->setEnabled(true);
    ActionAbort->setEnabled(false);
    
    g_job_queue.Abort();

}


void MainWin::updateProgress( int percent, int frame )
{
    progressBar_Encode->setProgress(percent);
    lCDNumber_Frame->display(frame);
}


void MainWin::deleteQueue()
{
    CQueueListViewItem *i = (CQueueListViewItem *)listView_Queue->selectedItem();
    if ( !i ) {
	QMessageBox::information(this, "No job selected",
				 "Please select job you want to remove from queue");
    } else {
	 bool res = g_job_queue.Remove(i->rtti());
	 Q_ASSERT(res);
	delete i;
    }
}


void MainWin::startXfer()
{
    if ( !xfer_win ) {
	xfer_win = new XferWin(this);
    }
    xfer_win->show();
}
