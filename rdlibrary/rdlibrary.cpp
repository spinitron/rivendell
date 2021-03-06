// rdlibrary.cpp
//
// The Library Utility for Rivendell.
//
//   (C) Copyright 2002-2010,2016-2018 Fred Gleason <fredg@paravelsystems.com>
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License version 2 as
//   published by the Free Software Foundation.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public
//   License along with this program; if not, write to the Free Software
//   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <qapplication.h>
#include <qwindowsstyle.h>
#include <qeventloop.h>
#include <qwidget.h>
#include <qpainter.h>
#include <qsqlpropertymap.h>
#include <qmessagebox.h>
#include <qlabel.h>
#include <qtextcodec.h>
#include <qtranslator.h>
#include <qlabel.h>
#include <qlistview.h>
#include <qprogressdialog.h>
#include <qtooltip.h>
#include <curl/curl.h>

#include <dbversion.h>
#include <rd.h>
#include <rdadd_cart.h>
#include <rdapplication.h>
#include <rdaudio_port.h>
#include <rdcart_search_text.h>
#include <rdcheck_daemons.h>
#include <rdconf.h>
#include <rdescape_string.h>
#include <rdmixer.h>
#include <rdprofile.h>
#include <rdtextvalidator.h>

#include "cart_tip.h"
#include "cdripper.h"
#include "disk_ripper.h"
#include "edit_cart.h"
#include "filter.h"
#include "globals.h"
#include "list_reports.h"
#include "rdlibrary.h"
#include "validate_cut.h"

//
// Global Resources
//
RDAudioPort *rdaudioport_conf;
DiskGauge *disk_gauge;
RDCut *cut_clipboard=NULL;
bool audio_changed;

//
// Prototypes
//
void SigHandler(int signo);

//
// Icons
//
#include "../icons/play.xpm"
#include "../icons/rml5.xpm"
#include "../icons/track_cart.xpm"
#include "../icons/rdlibrary-22x22.xpm"

MainWidget::MainWidget(QWidget *parent)
  :QWidget(parent)
{
  QString err_msg;

  lib_resize=false;
  profile_ripping=false;
  lib_edit_pending=false;
  lib_user_changed=false;

  //
  // Fix the Window Size
  //
  setMinimumWidth(sizeHint().width());
  setMinimumHeight(sizeHint().height());

  //
  // Initialize LibCurl
  //
  curl_global_init(CURL_GLOBAL_ALL);

  //
  // Generate Fonts
  //
  QFont default_font("Helvetica",12,QFont::Normal);
  default_font.setPixelSize(12);
  qApp->setFont(default_font);
  QFont button_font=QFont("Helvetica",12,QFont::Bold);
  button_font.setPixelSize(12);
  QFont filter_font=QFont("Helvetica",16,QFont::Bold);
  filter_font.setPixelSize(16);

  //
  // Create Icons
  //
  lib_playout_map=new QPixmap(play_xpm);
  lib_macro_map=new QPixmap(rml5_xpm);
  lib_track_cart_map=new QPixmap(track_cart_xpm);
  lib_rivendell_map=new QPixmap(rdlibrary_22x22_xpm);
  setIcon(*lib_rivendell_map);

  //
  // Progress Dialog
  //
  lib_progress_dialog=
    new QProgressDialog(tr("Please Wait..."),"Cancel",10,this,
			"lib_progress_dialog",false,
			Qt::WStyle_Customize|Qt::WStyle_NormalBorder);
  lib_progress_dialog->setCaption(" ");
  QLabel *label=new QLabel(tr("Please Wait..."),lib_progress_dialog);
  label->setAlignment(AlignCenter);
  label->setFont(filter_font);
  lib_progress_dialog->setLabel(label);
  lib_progress_dialog->setCancelButton(NULL);
  lib_progress_dialog->setMinimumDuration(2000);

  //
  // Ensure that the system daemons are running
  //
  RDInitializeDaemons();

  //
  // Open the Database
  //
  rda=new RDApplication("RDLibrary","rdlibrary",RDLIBRARY_USAGE,this);
  if(!rda->open(&err_msg)) {
    QMessageBox::critical(this,"RDLibrary - "+tr("Error"),err_msg);
    exit(1);
  }

  //
  // Read Command Options
  //
  for(unsigned i=0;i<rda->cmdSwitch()->keys();i++) {
    if(rda->cmdSwitch()->key(i)=="--profile-ripping") {
      profile_ripping=true;
      rda->cmdSwitch()->setProcessed(i,true);
    }
    if(!rda->cmdSwitch()->processed(i)) {
      QMessageBox::critical(this,"RDLibrary - "+tr("Error"),
			    tr("Unknown command option")+": "+
			    rda->cmdSwitch()->key(i));
      exit(2);
    }
  }

  SetCaption("");
  lib_import_path=RDGetHomeDir();

  //
  // Allocate Global Resources
  //
  lib_filter_mode=rda->station()->filterMode();
  rdaudioport_conf=new RDAudioPort(rda->config()->stationName(),
				   rda->libraryConf()->inputCard());
  connect(rda,SIGNAL(userChanged()),this,SLOT(userData()));
  connect(rda->ripc(),SIGNAL(notificationReceived(RDNotification *)),
	  this,SLOT(notificationReceivedData(RDNotification *)));
  rda->ripc()->
    connectHost("localhost",RIPCD_TCP_PORT,rda->config()->password());
  cut_clipboard=NULL;
  lib_user_timer=new QTimer(this);
  connect(lib_user_timer,SIGNAL(timeout()),this,SLOT(userData()));

  //
  // CAE Connection
  //
  connect(rda->cae(),SIGNAL(isConnected(bool)),
	  this,SLOT(caeConnectedData(bool)));
  rda->cae()->connectHost();

  //
  // Load Audio Assignments
  //
  RDSetMixerPorts(rda->config()->stationName(),rda->cae());

  //
  // Filter
  //
  lib_filter_edit=new QLineEdit(this);
  lib_filter_edit->setFont(default_font);
  lib_filter_label=new QLabel(lib_filter_edit,tr("Filter:"),this);
  lib_filter_label->setFont(button_font);
  lib_filter_label->setAlignment(AlignVCenter|AlignRight);
  connect(lib_filter_edit,SIGNAL(textChanged(const QString &)),
	  this,SLOT(filterChangedData(const QString &)));
  connect(lib_filter_edit,SIGNAL(returnPressed()),
	  this,SLOT(searchClickedData()));

  //
  // Filter Search Button
  //
  lib_search_button=new QPushButton(tr("&Search"),this);
  lib_search_button->setFont(button_font);
  connect(lib_search_button,SIGNAL(clicked()),this,SLOT(searchClickedData()));
  switch(lib_filter_mode) {
    case RDStation::FilterSynchronous:
      lib_search_button->hide();
      break;

    case RDStation::FilterAsynchronous:
      break;
  }

  //
  // Filter Clear Button
  //
  lib_clear_button=new QPushButton(tr("&Clear"),this);
  lib_clear_button->setFont(button_font);
  lib_clear_button->setDisabled(true);
  connect(lib_clear_button,SIGNAL(clicked()),this,SLOT(clearClickedData()));

  //
  // Group Filter
  //
  lib_group_box=new QComboBox(this);
  lib_group_box->setFont(default_font);
  lib_group_label=new QLabel(lib_group_box,tr("Group:"),this);
  lib_group_label->setFont(button_font);
  lib_group_label->setAlignment(AlignVCenter|AlignRight);
  connect(lib_group_box,SIGNAL(activated(const QString &)),
	  this,SLOT(groupActivatedData(const QString &)));

  //
  // Scheduler Codes Filter
  //
  lib_codes_box=new QComboBox(this);
  lib_codes_box->setFont(default_font);
  lib_codes_label=new QLabel(lib_codes_box,tr("Scheduler Code:"),this);
  lib_codes_label->setFont(button_font);
  lib_codes_label->setAlignment(AlignVCenter|AlignRight);
  connect(lib_codes_box,SIGNAL(activated(const QString &)),
	  this,SLOT(groupActivatedData(const QString &)));

  //
  // Show Allow Cart Drags Checkbox
  //
  lib_allowdrag_box=new QCheckBox(this);
  lib_allowdrag_box->setChecked(false);
  lib_allowdrag_label=
    new QLabel(lib_allowdrag_box,tr("Allow Cart Dragging"),this);
  lib_allowdrag_label->setFont(button_font);
  lib_allowdrag_label->setAlignment(AlignVCenter|AlignLeft);
  connect(lib_allowdrag_box,SIGNAL(stateChanged(int)),
	  this,SLOT(dragsChangedData(int)));
  if(!rda->station()->enableDragdrop()) {
    lib_allowdrag_box->hide();
    lib_allowdrag_label->hide();
  }

  //
  // Show Audio Carts Checkbox
  //
  lib_showaudio_box=new QCheckBox(this);
  lib_showaudio_box->setChecked(true);
  lib_showaudio_label=new QLabel(lib_showaudio_box,tr("Show Audio Carts"),this);
  lib_showaudio_label->setFont(button_font);
  lib_showaudio_label->setAlignment(AlignVCenter|AlignLeft);
  connect(lib_showaudio_box,SIGNAL(stateChanged(int)),
	  this,SLOT(audioChangedData(int)));

  //
  // Show Macro Carts Checkbox
  //
  lib_showmacro_box=new QCheckBox(this);
  lib_showmacro_box->setChecked(true);
  lib_showmacro_label=new QLabel(lib_showmacro_box,tr("Show Macro Carts"),this);
  lib_showmacro_label->setFont(button_font);
  lib_showmacro_label->setAlignment(AlignVCenter|AlignLeft);
  connect(lib_showmacro_box,SIGNAL(stateChanged(int)),
	  this,SLOT(macroChangedData(int)));

  //
  // Show Cart Notes Checkbox
  //
  lib_shownotes_box=new QCheckBox(this);
  lib_shownotes_box->setChecked(true);
  lib_shownotes_label=
    new QLabel(lib_shownotes_box,tr("Show Note Bubbles"),this);
  lib_shownotes_label->setFont(button_font);
  lib_shownotes_label->setAlignment(AlignVCenter|AlignLeft);

  //
  // Show Matches Checkbox
  //
  lib_showmatches_box=new QCheckBox(this);
  lib_showmatches_label=
    new QLabel(lib_showmatches_box,tr("Show Only First ")+
	       QString().sprintf("%d",RD_LIMITED_CART_SEARCH_QUANTITY)+
	       tr(" Matches"),this);
  lib_showmatches_label->setFont(button_font);
  lib_showmatches_label->setAlignment(AlignVCenter|AlignLeft);
  connect(lib_showmatches_box,SIGNAL(stateChanged(int)),
	  this,SLOT(searchLimitChangedData(int)));

  //
  // Cart List
  //
  lib_cart_list=new LibListView(this);
  lib_cart_list->setFont(default_font);
  lib_cart_list->setAllColumnsShowFocus(true);
  lib_cart_list->setItemMargin(5);
  lib_cart_list->setSelectionMode(QListView::Extended);
  lib_cart_tip=new CartTip(lib_cart_list->viewport());
  connect(lib_cart_list,
	  SIGNAL(doubleClicked(QListViewItem *,const QPoint &,int)),
	  this,
	  SLOT(cartDoubleclickedData(QListViewItem *,const QPoint &,int)));
  connect(lib_cart_list,SIGNAL(pressed(QListViewItem *)),
	  this,SLOT(cartClickedData(QListViewItem *)));
  connect(lib_cart_list,SIGNAL(onItem(QListViewItem *)),
	  this,SLOT(cartOnItemData(QListViewItem *)));
  lib_cart_list->addColumn("");
  lib_cart_list->setColumnAlignment(0,Qt::AlignHCenter);
  lib_cart_list->addColumn(tr("CART"));
  lib_cart_list->setColumnAlignment(1,Qt::AlignHCenter);

  lib_cart_list->addColumn(tr("GROUP"));
  lib_cart_list->setColumnAlignment(2,Qt::AlignHCenter);

  lib_cart_list->addColumn(tr("LENGTH"));
  lib_cart_list->setColumnAlignment(3,Qt::AlignRight);
  lib_cart_list->setColumnSortType(3,RDListView::TimeSort);

  lib_cart_list->addColumn(tr("TITLE"));

  lib_cart_list->addColumn(tr("ARTIST"));

  lib_cart_list->addColumn(tr("START"));
  lib_cart_list->setColumnAlignment(6,Qt::AlignHCenter);

  lib_cart_list->addColumn(tr("END"));
  lib_cart_list->setColumnAlignment(7,Qt::AlignHCenter);

  lib_cart_list->addColumn(tr("ALBUM"));

  lib_cart_list->addColumn(tr("LABEL"));

  lib_cart_list->addColumn(tr("COMPOSER"));

  lib_cart_list->addColumn(tr("CONDUCTOR"));

  lib_cart_list->addColumn(tr("PUBLISHER"));

  lib_cart_list->addColumn(tr("CLIENT"));

  lib_cart_list->addColumn(tr("AGENCY"));

  lib_cart_list->addColumn(tr("USER DEFINED"));

  lib_cart_list->addColumn(tr("CUTS"));
  lib_cart_list->setColumnAlignment(16,Qt::AlignRight);

  lib_cart_list->addColumn(tr("LAST CUT PLAYED"));
  lib_cart_list->setColumnAlignment(17,Qt::AlignHCenter);

  lib_cart_list->addColumn(tr("ENFORCE LENGTH"));
  lib_cart_list->setColumnAlignment(18,Qt::AlignHCenter);

  lib_cart_list->addColumn(tr("PRESERVE PITCH"));
  lib_cart_list->setColumnAlignment(19,Qt::AlignHCenter);

  lib_cart_list->addColumn(tr("LENGTH DEVIATION"));
  lib_cart_list->setColumnAlignment(20,Qt::AlignHCenter);

  lib_cart_list->addColumn(tr("OWNED BY"));
  lib_cart_list->setColumnAlignment(21,Qt::AlignHCenter);

  //
  // Add Button
  //
  lib_add_button=new QPushButton(this);
  lib_add_button->setFont(button_font);
  lib_add_button->setText(tr("&Add"));
  connect(lib_add_button,SIGNAL(clicked()),this,SLOT(addData()));

  //
  // Edit Button
  //
  lib_edit_button=new QPushButton(this);
  lib_edit_button->setFont(button_font);
  lib_edit_button->setText(tr("&Edit"));
  connect(lib_edit_button,SIGNAL(clicked()),this,SLOT(editData()));

  //
  // Delete Button
  //
  lib_delete_button=new QPushButton(this);
  lib_delete_button->setFont(button_font);
  lib_delete_button->setText(tr("&Delete"));
  connect(lib_delete_button,SIGNAL(clicked()),this,SLOT(deleteData()));

  //
  // Disk Gauge
  //
  disk_gauge=new DiskGauge(rda->system()->sampleRate(),
			   rda->libraryConf()->defaultChannels(),this);

  //
  // Rip Button
  //
  lib_rip_button=new QPushButton(this);
  lib_rip_button->setFont(button_font);
  lib_rip_button->setText(tr("&Rip\nCD"));
  connect(lib_rip_button,SIGNAL(clicked()),this,SLOT(ripData()));

  //
  // Reports Button
  //
  lib_reports_button=new QPushButton(this);
  lib_reports_button->setFont(button_font);
  lib_reports_button->setText(tr("Re&ports"));
  connect(lib_reports_button,SIGNAL(clicked()),this,SLOT(reportsData()));

  //
  // Close Button
  //
  lib_close_button=new QPushButton(this);
  lib_close_button->setFont(button_font);
  lib_close_button->setText(tr("&Close"));
  connect(lib_close_button,SIGNAL(clicked()),this,SLOT(quitMainWidget()));

  // 
  // Setup Signal Handling 
  //
  ::signal(SIGCHLD,SigHandler);

  //
  // Load Data
  //
  switch(rda->libraryConf()->limitSearch()) {
  case RDLibraryConf::LimitNo:
    lib_showmatches_box->setChecked(false);
    break;

  case RDLibraryConf::LimitYes:
    lib_showmatches_box->setChecked(true);
    break;

  case RDLibraryConf::LimitPrevious:
    lib_showmatches_box->setChecked(rda->libraryConf()->searchLimited());
    break;
  }

  lib_resize=true;

  LoadGeometry();
}


QSize MainWidget::sizeHint() const
{
  return QSize(800,600);
}


QSizePolicy MainWidget::sizePolicy() const
{
  return QSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
}


void MainWidget::caeConnectedData(bool state)
{
  if(state) {
    std::vector<int> cards;
    cards.push_back(rda->libraryConf()->inputCard());
    cards.push_back(rda->libraryConf()->outputCard());
    rda->cae()->enableMetering(&cards);
  }
}


void MainWidget::userData()
{
  QString sql;
  RDSqlQuery *q;

  if(lib_edit_pending) {
    lib_user_changed=true;
    return;
  }

  SetCaption(rda->ripc()->user());

  lib_group_box->clear();
  lib_group_box->insertItem(tr("ALL"));
  sql=QString("select GROUP_NAME from USER_PERMS where ")+
    "USER_NAME=\""+RDEscapeString(rda->user()->name())+"\" order by GROUP_NAME";
  q=new RDSqlQuery(sql);
  while(q->next()) {
    lib_group_box->insertItem(q->value(0).toString());
  }
  delete q;

  if(lib_group_box->count()==1) {
    lib_add_button->setDisabled(true);
    lib_edit_button->setDisabled(true);
    lib_delete_button->setDisabled(true);
    lib_rip_button->setDisabled(true);
  }
  else {
    lib_add_button->setEnabled(rda->user()->createCarts());
    lib_edit_button->setEnabled(true);
    lib_delete_button->setEnabled(rda->user()->deleteCarts());
    lib_rip_button->setEnabled(rda->user()->editAudio());
  }

  lib_codes_box->clear();
  lib_codes_box->insertItem(tr("ALL"));
  sql=QString().sprintf("select CODE from SCHED_CODES");
  q=new RDSqlQuery(sql);
  while(q->next()) {
    lib_codes_box->insertItem(q->value(0).toString());
  }
  delete q;
  lib_search_button->setDisabled(true);
  groupActivatedData(lib_group_box->currentText());
}


void MainWidget::filterChangedData(const QString &str)
{
  lib_search_button->setEnabled(true);
  if(lib_filter_mode!=RDStation::FilterSynchronous) {
    return;
  }
  searchClickedData();
}


void MainWidget::searchClickedData()
{
  lib_search_button->setDisabled(true);
  if(lib_filter_edit->text().isEmpty()) {
    lib_clear_button->setDisabled(true);
  }
  else {
    lib_clear_button->setEnabled(true);
  }
  RefreshList();
}


void MainWidget::clearClickedData()
{
  lib_filter_edit->clear();
  filterChangedData("");
}


void MainWidget::groupActivatedData(const QString &str)
{
  if(str!=tr("ALL")) {
    lib_default_group=str;
  }
  filterChangedData("");
}


void MainWidget::addData()
{
  QString sql;
  RDSqlQuery *q;
  int cart_num;
  RDCart::Type cart_type=RDCart::All;
  QString cart_title;

  LockUser();

  RDAddCart *add_cart=new RDAddCart(&lib_default_group,&cart_type,&cart_title,
				    rda->user()->name(),rda->system(),this);
  if((cart_num=add_cart->exec())<0) {
    delete add_cart;
    UnlockUser();
    return;
  }
  delete add_cart;
  sql=QString("insert into CART set ")+
    QString().sprintf("NUMBER=%u,TYPE=%d,",cart_num,cart_type)+
    "GROUP_NAME=\""+RDEscapeString(lib_default_group)+"\","+
    "TITLE=\""+RDEscapeString(cart_title)+"\"";
  q=new RDSqlQuery(sql);
  delete q;
  
  EditCart *cart=
    new EditCart(cart_num,&lib_import_path,true,profile_ripping,this);
  if(cart->exec() <0) {
    RDCart *rdcart=new RDCart(cart_num);
    rdcart->remove(rda->station(),rda->user(),rda->config());
    delete rdcart;
  } 
  else {
    RDListViewItem *item=new RDListViewItem(lib_cart_list);
    item->setText(1,QString().sprintf("%06u",cart_num));
    RefreshLine(item);
    SendNotification(RDNotification::AddAction,cart_num);
    QListViewItemIterator it(lib_cart_list);
    while(it.current()) {
      lib_cart_list->setSelected(it.current(),false);
      ++it;
    }
    lib_cart_list->setSelected(item,true);
    lib_cart_list->ensureItemVisible(item);
  }
  delete cart;

  UnlockUser();
}



void MainWidget::editData()
{
  int sel_count=0;
  QListViewItemIterator *it;

  LockUser();

  it=new QListViewItemIterator(lib_cart_list);
  while(it->current()) {
    if (it->current()->isSelected()) {
      sel_count++;
    }
    ++(*it);
  }
  delete it;

  if(sel_count==0) {
    UnlockUser();
    return;
  }
  if(sel_count==1) { //single edit
    it=new QListViewItemIterator(lib_cart_list);
    while(!it->current()->isSelected()) {
      ++(*it);
    }
    RDListViewItem *item=(RDListViewItem *)it->current();

    EditCart *edit_cart=new EditCart(item->text(1).toUInt(),&lib_import_path,
				     false,profile_ripping,this);
    edit_cart->exec();
    RefreshLine(item);
    cartOnItemData(item);
    SendNotification(RDNotification::ModifyAction,item->text(1).toUInt());
    delete edit_cart;
    delete it;
  }
  else { //multi edit
    if(rda->user()->modifyCarts()) {
      EditCart *edit_cart=
	new EditCart(0,&lib_import_path,false,profile_ripping,this,"",
				       lib_cart_list);
    
      edit_cart->exec();
      delete edit_cart;
    
      it=new QListViewItemIterator(lib_cart_list);
      while(it->current()) {
        if (it->current()->isSelected()) {
          RefreshLine((RDListViewItem *)it->current());
	  SendNotification(RDNotification::ModifyAction,
			   it->current()->text(1).toUInt());
        }
        ++(*it);
      }
      delete it;
    }
  }
  UnlockUser();
}


void MainWidget::deleteData()
{
  QString filename;
  QString sql;
  RDSqlQuery *q;
  QString str;
  int sel_count=0;
  QListViewItemIterator *it;
  bool del_flag;

  LockUser();

  it=new QListViewItemIterator(lib_cart_list);
  while(it->current()) {
    if (it->current()->isSelected()) {
      sel_count++;
    }
    ++(*it);
  }
  delete it;

  if(sel_count==0) {
    UnlockUser();
    return;
  }

  str=QString(tr("Are you sure you want to delete cart(s)"));
  if(QMessageBox::question(this,tr("Delete Cart(s)"),str,QMessageBox::Yes,QMessageBox::No)!=
     QMessageBox::Yes) {
    UnlockUser();
    return;
  }
  it=new QListViewItemIterator(lib_cart_list);
  while(it->current()) {
    if (it->current()->isSelected()) {
    del_flag=true;
    RDListViewItem *item=(RDListViewItem *)it->current();
  sql=QString().sprintf("select CUT_NAME from RECORDINGS where \
                         (CUT_NAME like \"%06u_%%\")||(MACRO_CART=%u)",
			item->text(1).toUInt(),item->text(1).toUInt());
  q=new RDSqlQuery(sql);
  if(q->first()) {
      QString str=QString().sprintf(tr("Cart %06u is used in one or more RDCatch events!\n\
Do you still want to delete it?"),item->text(1).toUInt());
      if(QMessageBox::warning(this,tr("RDCatch Event Exists"),str,
			        QMessageBox::Yes,QMessageBox::No)==QMessageBox::No) {
        del_flag=false;
    }
  }
  delete q;
  if(cut_clipboard!=NULL) {
    if(item->text(1).toUInt()==cut_clipboard->cartNumber()) {
      	QString str=QString().sprintf(tr("Deleting cart %06u will also empty the clipboard.\n\
      	Do you still want to proceed?"),item->text(1).toUInt());
        switch(QMessageBox::question(this,tr("Empty Clipboard"),str,
				  QMessageBox::Yes,
				  QMessageBox::No)) {
	  case QMessageBox::No:
	  case QMessageBox::NoButton:
                del_flag=false;

	  default:
	    break;
      }
      delete cut_clipboard;
      cut_clipboard=NULL;
    }
  }
  if(del_flag && item->text(21).isEmpty()) {
    RDCart *rdcart=new RDCart(item->text(1).toUInt());
    if(!rdcart->remove(rda->station(),rda->user(),rda->config())) {
      QMessageBox::warning(this,tr("RDLibrary"),tr("Unable to delete audio!"));
      return;
    }
    SendNotification(RDNotification::DeleteAction,rdcart->number());
    delete rdcart;
    delete item;
  } 
  else {
    ++(*it);
  } 
    }
    else {
      ++(*it);
    } 
  }
  delete it;

  UnlockUser();
}


void MainWidget::ripData()
{
  LockUser();
  QString group=lib_group_box->currentText();
  QString schedcode=lib_codes_box->currentText();
  DiskRipper *dialog=new DiskRipper(&lib_filter_text,&group,&schedcode,
				    profile_ripping,this);
  if(dialog->exec()==0) {
    for(int i=0;i<lib_group_box->count();i++) {
      if(lib_group_box->text(i)==*group) {
	lib_filter_edit->setText(lib_filter_text);
	lib_group_box->setCurrentItem(i);
	groupActivatedData(lib_group_box->currentText());
      }
    }
  }
  delete dialog;
  if(!UnlockUser()) {
    RefreshList();
  }
}


void MainWidget::reportsData()
{
  LockUser();
  ListReports *lr=
    new ListReports(lib_filter_edit->text(),GetTypeFilter(),
		    lib_group_box->currentText(),lib_codes_box->currentText(),
		    this);
  lr->exec();
  delete lr;
  UnlockUser();
}


void MainWidget::cartOnItemData(QListViewItem *item)
{
  if((!lib_shownotes_box->isChecked())||(item==NULL)) {
    return;
  }
  lib_cart_tip->
    setCartNumber(lib_cart_list->itemRect(item),item->text(1).toUInt());
}


void MainWidget::cartClickedData(QListViewItem *item)
{
  int del_count=0;
  int sel_count=0;
  QListViewItemIterator *it;

  it=new QListViewItemIterator(lib_cart_list);
  while(it->current()) {
    if (it->current()->isSelected()) {
      sel_count++;
      if(it->current()->text(21).isEmpty()) {
        del_count++;
      }
    }
    ++(*it);
  }
  delete it;
  
  if(del_count>0) {
    lib_delete_button->setEnabled(rda->user()->deleteCarts());
  } 
  else {
    lib_delete_button->setEnabled(false);
  }
  if(sel_count>1) {
    if(del_count==0) {
      lib_edit_button->setEnabled(false);
    }
    else {
      lib_edit_button->setEnabled(rda->user()->modifyCarts());
    }
  } 
  else {
    lib_edit_button->setEnabled(true);
  }
}


void MainWidget::cartDoubleclickedData(QListViewItem *,const QPoint &,int)
{
  editData();
}


void MainWidget::audioChangedData(int state)
{
  filterChangedData("");
}


void MainWidget::macroChangedData(int state)
{
  filterChangedData("");
}


void MainWidget::searchLimitChangedData(int state)
{
  rda->libraryConf()->setSearchLimited(state);
  filterChangedData("");
}


void MainWidget::dragsChangedData(int state)
{
  if(state) {
    lib_cart_list->setSelectionMode(QListView::Single);
  }
  else {
    lib_cart_list->setSelectionMode(QListView::Extended);
  }
}


void MainWidget::notificationReceivedData(RDNotification *notify)
{
  RDListViewItem *item=NULL;
  QString sql;
  RDSqlQuery *q;

  if(notify->type()==RDNotification::CartType) {
    unsigned cartnum=notify->id().toUInt();
    switch(notify->action()) {
    case RDNotification::AddAction:
      sql=QString("select CART.NUMBER from CART ")+
	"left join CUTS on CART.NUMBER=CUTS.CART_NUMBER "+
	WhereClause()+
	QString().sprintf(" && CART.NUMBER=%u ",cartnum);
      q=new RDSqlQuery(sql);
      if(q->first()) {
	item=new RDListViewItem(lib_cart_list);
	item->setText(1,QString().sprintf("%06u",cartnum));
	RefreshLine(item);
      }
      delete q;
      break;

    case RDNotification::ModifyAction:
      if((item=(RDListViewItem *)lib_cart_list->
	  findItem(QString().sprintf("%06u",cartnum),1))!=NULL) {
	RefreshLine(item);
      }
      break;

    case RDNotification::DeleteAction:
      if(lib_edit_pending) {
	lib_deleted_carts.push_back(cartnum);
      }
      else {
	if((item=(RDListViewItem *)lib_cart_list->findItem(QString().sprintf("%06u",cartnum),1))!=NULL) {
	  delete item;
	}
      }
      break;

    case RDNotification::NoAction:
    case RDNotification::LastAction:
      break;
    }
  }
}


void MainWidget::quitMainWidget()
{
  SaveGeometry();
  exit(0);
}


void MainWidget::closeEvent(QCloseEvent *e)
{
  quitMainWidget();
}


void MainWidget::resizeEvent(QResizeEvent *e)
{
  if(lib_resize) {
    switch(lib_filter_mode) {
    case RDStation::FilterSynchronous:
      lib_filter_edit->setGeometry(70,10,e->size().width()-170,20);
      break;

    case RDStation::FilterAsynchronous:
      lib_search_button->setGeometry(e->size().width()-180,10,80,50);
      lib_filter_edit->setGeometry(70,10,e->size().width()-260,20);
      break;
    }
    lib_clear_button->setGeometry(e->size().width()-90,10,80,50);
    lib_filter_label->setGeometry(10,10,55,20);
    lib_group_box->setGeometry(70,40,120,20);
    lib_group_label->setGeometry(10,40,55,20);
    lib_codes_box->setGeometry(330,40,120,20);
    lib_codes_label->setGeometry(195,40,130,20);
    lib_allowdrag_box->setGeometry(470,42,15,15);
    lib_allowdrag_label->setGeometry(490,40,130,20);
    lib_showaudio_box->setGeometry(70,67,15,15);
    lib_showaudio_label->setGeometry(90,65,130,20);
    lib_showmacro_box->setGeometry(230,67,15,15);
    lib_showmacro_label->setGeometry(250,65,130,20);
    lib_shownotes_box->setGeometry(390,67,15,15);
    lib_shownotes_label->setGeometry(410,65,130,20);
    lib_showmatches_box->setGeometry(550,67,15,15);
    lib_showmatches_label->setGeometry(570,65,200,20);
    lib_cart_list->
      setGeometry(10,90,e->size().width()-20,e->size().height()-155);
    lib_add_button->setGeometry(10,e->size().height()-60,80,50);
    lib_edit_button->setGeometry(100,e->size().height()-60,80,50);
    lib_delete_button->setGeometry(190,e->size().height()-60,80,50);
    disk_gauge->setGeometry(285,e->size().height()-55,
			    e->size().width()-585,
			    disk_gauge->sizeHint().height());
    lib_rip_button->
      setGeometry(e->size().width()-290,e->size().height()-60,80,50);
    lib_reports_button->
      setGeometry(e->size().width()-200,e->size().height()-60,80,50);
    lib_close_button->setGeometry(e->size().width()-90,e->size().height()-60,
				  80,50);
  }
}


void MainWidget::RefreshList()
{
  RDSqlQuery *q;
  QString sql;
  RDListViewItem *l=NULL;
  QString type_filter;
  QDateTime current_datetime(QDate::currentDate(),QTime::currentTime());
  unsigned cartnum=0;
  RDCart::Validity validity=RDCart::NeverValid;
  QDateTime end_datetime;

  lib_cart_list->clear();

  type_filter=GetTypeFilter();
  if(type_filter.isEmpty()) {
    return;
  }
  sql=QString("select ")+
    "CART.NUMBER,"+             // 00
    "CART.FORCED_LENGTH,"+      // 01
    "CART.TITLE,"+              // 02
    "CART.ARTIST,"+             // 03
    "CART.ALBUM,"+              // 04
    "CART.LABEL,"+              // 05
    "CART.CLIENT,"+             // 06
    "CART.AGENCY,"+             // 07
    "CART.USER_DEFINED,"+       // 08
    "CART.COMPOSER,"+           // 09
    "CART.PUBLISHER,"+          // 10
    "CART.CONDUCTOR,"+          // 11
    "CART.GROUP_NAME,"+         // 12
    "CART.START_DATETIME,"+     // 13
    "CART.END_DATETIME,"+       // 14
    "CART.TYPE,"+               // 15
    "CART.CUT_QUANTITY,"+       // 16
    "CART.LAST_CUT_PLAYED,"+    // 17
    "CART.ENFORCE_LENGTH,"+     // 18
    "CART.PRESERVE_PITCH,"+     // 19
    "CART.LENGTH_DEVIATION,"+   // 20
    "CART.OWNER,"+              // 21
    "CART.VALIDITY,"+           // 22
    "GROUPS.COLOR,"+            // 23
    "CUTS.LENGTH,"+             // 24  offsets begin here
    "CUTS.EVERGREEN,"+          // 25
    "CUTS.START_DATETIME,"+     // 26
    "CUTS.END_DATETIME,"+       // 27
    "CUTS.START_DAYPART,"+      // 28
    "CUTS.END_DAYPART,"+        // 29
    "CUTS.MON,"+                // 30
    "CUTS.TUE,"+                // 31
    "CUTS.WED,"+                // 32
    "CUTS.THU,"+                // 33
    "CUTS.FRI,"+                // 34
    "CUTS.SAT,"+                // 35
    "CUTS.SUN "+                // 36
    "from CART left join GROUPS on CART.GROUP_NAME=GROUPS.NAME "+
    "left join CUTS on CART.NUMBER=CUTS.CART_NUMBER";
  sql+=WhereClause();
  sql+=" order by CART.NUMBER";
  if(lib_showmatches_box->isChecked()) {
    sql+=QString().sprintf(" limit %d",RD_LIMITED_CART_SEARCH_QUANTITY);
  }
  q=new RDSqlQuery(sql);
  int step=0;
  int count=0;
  lib_progress_dialog->setTotalSteps(q->size()/RDLIBRARY_STEP_SIZE);
  lib_progress_dialog->setProgress(0);
  while(q->next()) {
    end_datetime=q->value(14).toDateTime();
    if(q->value(0).toUInt()==cartnum) {
      if((RDCart::Type)q->value(15).toUInt()==RDCart::Macro) {
	validity=RDCart::AlwaysValid;
      }
      else {
	validity=ValidateCut(q,24,validity,current_datetime);
      }
    }
    else {
      //
      // Write availability color
      //
      UpdateItemColor(l,validity,q->value(14).toDateTime(),current_datetime);

      //
      // Start a new entry
      //
      if((RDCart::Type)q->value(15).toUInt()==RDCart::Macro) {
	validity=RDCart::AlwaysValid;
      }
      else {
	validity=ValidateCut(q,24,RDCart::NeverValid,current_datetime);
      }
      l=new RDListViewItem(lib_cart_list);
      switch((RDCart::Type)q->value(15).toUInt()) {
      case RDCart::Audio:
	if(q->value(21).isNull()) {
	  l->setPixmap(0,*lib_playout_map);
	}
	else {
	  l->setPixmap(0,*lib_track_cart_map);
	}
	break;
	
      case RDCart::Macro:
	l->setPixmap(0,*lib_macro_map);
	l->setBackgroundColor(backgroundColor());
	break;
	
      case RDCart::All:
	break;
      }
      l->setText(1,QString().sprintf("%06d",q->value(0).toUInt()));
      l->setText(2,q->value(12).toString());
      l->setTextColor(2,q->value(23).toString(),QFont::Bold);
      l->setText(3,RDGetTimeLength(q->value(1).toUInt()));
      l->setText(4,q->value(2).toString());
      l->setText(5,q->value(3).toString());
      if(!q->value(13).toDateTime().isNull()) {
	l->setText(6,q->value(13).toDateTime().
		   toString("MM/dd/yyyy - hh:mm:ss"));
      }
      if(!q->value(14).toDateTime().isNull()) {
	l->setText(7,q->value(14).toDateTime().
		   toString("MM/dd/yyyy - hh:mm:ss"));
      }
      else {
	l->setText(7,"TFN");
      }
      l->setText(8,q->value(4).toString());
      l->setText(9,q->value(5).toString());
      l->setText(10,q->value(9).toString());
      l->setText(11,q->value(11).toString());
      l->setText(12,q->value(10).toString());
      l->setText(13,q->value(6).toString());
      l->setText(14,q->value(7).toString());
      l->setText(15,q->value(8).toString());
      l->setText(16,q->value(16).toString());
      l->setText(17,q->value(17).toString());
      l->setText(18,q->value(18).toString());
      l->setText(19,q->value(19).toString());
      l->setText(20,q->value(20).toString());
      l->setText(21,q->value(21).toString());
      if(q->value(18).toString()=="Y") {
	l->setTextColor(3,QColor(RDLIBRARY_ENFORCE_LENGTH_COLOR),QFont::Bold);
      }
      else {
	if((q->value(20).toUInt()>RDLIBRARY_MID_LENGTH_LIMIT)&&
	   (q->value(18).toString()=="N")) {
	  if(q->value(20).toUInt()>RDLIBRARY_MAX_LENGTH_LIMIT) {
	    l->setTextColor(3,QColor(RDLIBRARY_MAX_LENGTH_COLOR),QFont::Bold);
	  }
	  else {
	    l->setTextColor(3,QColor(RDLIBRARY_MID_LENGTH_COLOR),QFont::Bold);
	  }
	}
	else {
	  l->setTextColor(3,QColor(black),QFont::Normal);
	}
      }
    }
    cartnum=q->value(0).toUInt();
    if(count++>RDLIBRARY_STEP_SIZE) {
      lib_progress_dialog->setProgress(++step);
      count=0;
      qApp->eventLoop()->processEvents(QEventLoop::ExcludeUserInput);
    }
  }
  UpdateItemColor(l,validity,end_datetime,current_datetime);
  lib_progress_dialog->reset();
  delete q;
}


QString MainWidget::WhereClause() const
{
  QString sql="";
  QString type_filter=GetTypeFilter();

  QString schedcode="";
  if(lib_codes_box->currentText()!=tr("ALL")) {
    schedcode=lib_codes_box->currentText();
  }
  if(lib_group_box->currentText()==QString(tr("ALL"))) {
    sql+=QString(" where ")+
      RDAllCartSearchText(lib_filter_edit->text(),schedcode,
			  rda->user()->name(),true)+" && "+type_filter;

  }
  else {
    sql+=QString(" where ")+
      RDCartSearchText(lib_filter_edit->text(),lib_group_box->currentText(),
		       schedcode,true)+" && "+type_filter;      
  }

  return sql;
}


void SigHandler(int signo)
{
  pid_t pLocalPid;

  switch(signo) {
      case SIGCHLD:
	pLocalPid=waitpid(-1,NULL,WNOHANG);
	while(pLocalPid>0) {
	  pLocalPid=waitpid(-1,NULL,WNOHANG);
	}
	ripper_running=false;
	import_active=false;
	signal(SIGCHLD,SigHandler);
	break;
  }
}


void MainWidget::RefreshLine(RDListViewItem *item)
{
  RDCart::Validity validity=RDCart::NeverValid;
  QDateTime current_datetime(QDate::currentDate(),QTime::currentTime());
  QString sql=QString().sprintf("select CART.FORCED_LENGTH,CART.TITLE,\
                                 CART.ARTIST,\
                                 CART.ALBUM,CART.LABEL,\
                                 CART.CLIENT,\
                                 CART.AGENCY,CART.USER_DEFINED,\
                                 CART.COMPOSER,CART.CONDUCTOR,CART.PUBLISHER,\
                                 CART.GROUP_NAME,CART.START_DATETIME,\
                                 CART.END_DATETIME,CART.TYPE,\
                                 CART.CUT_QUANTITY,CART.LAST_CUT_PLAYED,\
                                 CART.ENFORCE_LENGTH,\
                                 CART.PRESERVE_PITCH,\
                                 CART.LENGTH_DEVIATION,CART.OWNER,\
                                 CART.VALIDITY,GROUPS.COLOR,CUTS.LENGTH,\
                                 CUTS.EVERGREEN,CUTS.START_DATETIME,\
                                 CUTS.END_DATETIME,CUTS.START_DAYPART,\
                                 CUTS.END_DAYPART,CUTS.MON,CUTS.TUE,\
                                 CUTS.WED,CUTS.THU,CUTS.FRI,CUTS.SAT,CUTS.SUN \
                                 from CART left join GROUPS on \
                                 CART.GROUP_NAME=GROUPS.NAME left join \
                                 CUTS on CART.NUMBER=CUTS.CART_NUMBER \
                                 where CART.NUMBER=%u",
				item->text(1).toUInt());
  RDSqlQuery *q=new RDSqlQuery(sql);
  while(q->next()) {
    if((RDCart::Type)q->value(14).toUInt()==RDCart::Macro) {
      validity=RDCart::AlwaysValid;
    }
    else {
      validity=ValidateCut(q,23,validity,current_datetime);
    }
    switch((RDCart::Type)q->value(14).toUInt()) {
	case RDCart::Audio:
	  if(q->value(20).isNull()) {
	    item->setPixmap(0,*lib_playout_map);
	  }
	  else {
	    item->setPixmap(0,*lib_track_cart_map);
	  }
	  if(q->value(0).toUInt()==0) {
	    item->setBackgroundColor(RD_CART_ERROR_COLOR);
	  }
	  else {
	    UpdateItemColor(item,validity,
			    q->value(13).toDateTime(),current_datetime);
	  }
	  break;

	case RDCart::Macro:
	  item->setPixmap(0,*lib_macro_map);
	  break;

    case RDCart::All:
	  break;
    }
    item->setText(2,q->value(11).toString());
    item->setTextColor(2,q->value(22).toString(),QFont::Bold);
    item->setText(3,RDGetTimeLength(q->value(0).toUInt()));
    item->setText(4,q->value(1).toString());
    item->setText(5,q->value(2).toString());
    item->setText(8,q->value(3).toString());
    item->setText(9,q->value(4).toString());
    item->setText(10,q->value(8).toString());
    item->setText(11,q->value(9).toString());

    item->setText(12,q->value(10).toString());
    item->setText(13,q->value(5).toString());
    item->setText(14,q->value(6).toString());
    if(!q->value(12).toDateTime().isNull()) {
      item->setText(6,q->value(12).toDateTime().
		    toString("MM/dd/yyyy - hh:mm:ss"));
    }
    else {
      item->setText(6,"");
    }
    if(!q->value(13).toDateTime().isNull()) {
      item->setText(7,q->value(13).toDateTime().
		    toString("MM/dd/yyyy - hh:mm:ss"));
    }
    else {
      item->setText(7,tr("TFN"));
    }
    item->setText(16,q->value(15).toString());
    item->setText(17,q->value(16).toString());
    item->setText(18,q->value(17).toString());
    item->setText(19,q->value(18).toString());
    item->setText(20,q->value(19).toString());
    item->setText(21,q->value(20).toString());
    if(q->value(17).toString()=="Y") {
      item->setTextColor(3,QColor(RDLIBRARY_ENFORCE_LENGTH_COLOR),QFont::Bold);
    }
    else {
      if((q->value(19).toUInt()>RDLIBRARY_MID_LENGTH_LIMIT)&&
	 (q->value(17).toString()=="N")) {
	if(q->value(19).toUInt()>RDLIBRARY_MAX_LENGTH_LIMIT) {
	  item->setTextColor(3,QColor(RDLIBRARY_MAX_LENGTH_COLOR),QFont::Bold);
	}
	else {
	  item->setTextColor(3,QColor(RDLIBRARY_MID_LENGTH_COLOR),QFont::Bold);
	}
      }
      else {
	item->setTextColor(3,QColor(black),QFont::Normal);
      }
    }
  }
  delete q;
}


void MainWidget::UpdateItemColor(RDListViewItem *item,
				 RDCart::Validity validity,
				 const QDateTime &end_datetime,
				 const QDateTime &current_datetime)
{
  if(item!=NULL) {
    switch(validity) {
      case RDCart::NeverValid:
	item->setBackgroundColor(RD_CART_ERROR_COLOR);
	break;
	
      case RDCart::ConditionallyValid:
	if(end_datetime.isValid()&&
	   (end_datetime<current_datetime)) {
	  item->setBackgroundColor(RD_CART_ERROR_COLOR);
	}
	else {
	  item->setBackgroundColor(RD_CART_CONDITIONAL_COLOR);
	}
	break;
	
      case RDCart::FutureValid:
	item->setBackgroundColor(RD_CART_FUTURE_COLOR);
	break;
	
      case RDCart::AlwaysValid:
	item->setBackgroundColor(palette().color(QPalette::Active,
					      QColorGroup::Base));
	break;
	
      case RDCart::EvergreenValid:
	item->setBackgroundColor(RD_CART_EVERGREEN_COLOR);
	break;
    }
  }
}


void MainWidget::SetCaption(QString user)
{
  QString str1;
  QString str2;

  str1=QString("RDLibrary")+" v"+VERSION+" - "+tr("Host")+":";
  str2=tr("User")+":";
  setCaption(str1+" "+rda->config()->stationName()+", "+str2+" "+user);
}


QString MainWidget::GetTypeFilter() const
{
  QString type_filter;

  if(lib_showaudio_box->isChecked()) {
    if(lib_showmacro_box->isChecked()) {
      type_filter="((TYPE=1)||(TYPE=2)||(TYPE=3))";
    }
    else {
      type_filter="((TYPE=1)||(TYPE=3))";
    }
  }
  else {
    if(lib_showmacro_box->isChecked()) {
      type_filter="(TYPE=2)";
    }
  }
  return type_filter;
}

QString MainWidget::GeometryFile() {
  bool home_found = false;
  QString home = RDGetHomeDir(&home_found);
  if (home_found) {
    return home + "/" + RDLIBRARY_GEOMETRY_FILE;
  } else {
    return NULL;
  }
}

void MainWidget::LoadGeometry()
{
  QString geometry_file = GeometryFile();
  if(geometry_file==NULL) {
    return;
  }
  RDProfile *profile=new RDProfile();
  profile->setSource(geometry_file);
  resize(profile->intValue("RDLibrary","Width",sizeHint().width()),
	 profile->intValue("RDLibrary","Height",sizeHint().height()));
  lib_shownotes_box->
    setChecked(profile->boolValue("RDLibrary","ShowNoteBubbles",true));
  lib_allowdrag_box->
    setChecked(profile->boolValue("RDLibrary","AllowCartDragging",false));

  delete profile;
}


void MainWidget::SaveGeometry()
{
  QString geometry_file = GeometryFile();
  if(geometry_file==NULL) {
    return;
  }
  FILE *file=fopen(geometry_file,"w");
  if(file==NULL) {
    return;
  }
  fprintf(file,"[RDLibrary]\n");
  fprintf(file,"Width=%d\n",geometry().width());
  fprintf(file,"Height=%d\n",geometry().height());
  fprintf(file,"ShowNoteBubbles=");
  if(lib_shownotes_box->isChecked()) {
    fprintf(file,"Yes\n");
  }
  else {
    fprintf(file,"No\n");
  }
  fprintf(file,"AllowCartDragging=");
  if(lib_allowdrag_box->isChecked()) {
    fprintf(file,"Yes\n");
  }
  else {
    fprintf(file,"No\n");
  }

  fclose(file);
}


void MainWidget::LockUser()
{
  lib_edit_pending=true;
}


bool MainWidget::UnlockUser()
{

  RDListViewItem *item=NULL;

  //
  // Process Deleted Carts
  //
  for(unsigned i=0;i<lib_deleted_carts.size();i++) {
    if((item=(RDListViewItem *)lib_cart_list->findItem(QString().sprintf("%06u",lib_deleted_carts.at(i)),1))!=NULL) {
      delete item;
    }
  }

  //
  // Process User Change
  //
  bool ret=lib_user_changed;
  lib_edit_pending=false;
  if(lib_user_changed) {
    lib_user_timer->start(0,true);
    lib_user_changed=false;
  }

  return ret;
}


void MainWidget::SendNotification(RDNotification::Action action,
				  unsigned cartnum)
{
  RDNotification *notify=
    new RDNotification(RDNotification::CartType,action,QVariant(cartnum));
  rda->ripc()->sendNotification(*notify);
  delete notify;
}


int main(int argc,char *argv[])
{
  QApplication a(argc,argv);
  
  //
  // Load Translations
  //
  QTranslator qt(0);
  qt.load(QString(QTDIR)+QString("/translations/qt_")+QTextCodec::locale(),
	  ".");
  a.installTranslator(&qt);

  QTranslator rd(0);
  rd.load(QString(PREFIX)+QString("/share/rivendell/librd_")+
	     QTextCodec::locale(),".");
  a.installTranslator(&rd);

  QTranslator rdhpi(0);
  rdhpi.load(QString(PREFIX)+QString("/share/rivendell/librdhpi_")+
	     QTextCodec::locale(),".");
  a.installTranslator(&rdhpi);

  QTranslator tr(0);
  tr.load(QString(PREFIX)+QString("/share/rivendell/rdlibrary_")+
	     QTextCodec::locale(),".");
  a.installTranslator(&tr);

  //
  // Start Event Loop
  //
  MainWidget *w=new MainWidget();
  a.setMainWidget(w);
  w->show();
  return a.exec();
}
