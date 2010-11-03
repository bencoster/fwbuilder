/*

                          Firewall Builder

                 Copyright (C) 2008 NetCitadel, LLC

  Author:  alek@codeminders.com
           refactoring and bugfixes: vadim@fwbuilder.org

  $Id$

  This program is free software which we release under the GNU General Public
  License. You may redistribute and/or modify this program under the terms
  of that license as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  To get a copy of the GNU General Public License, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#include "../../config.h"
#include "global.h"
#include "utils.h"

#include <fwbuilder/Cluster.h>
#include <fwbuilder/Firewall.h>
#include <fwbuilder/RuleSet.h>
#include <fwbuilder/Policy.h>
#include <fwbuilder/NAT.h>
#include <fwbuilder/Routing.h>
#include "fwbuilder/RuleSet.h"
#include "fwbuilder/Rule.h"
#include "fwbuilder/RuleElement.h"

#include "FWBSettings.h"
#include "UserWorkflow.h"
#include "FWBTree.h"
#include "FWObjectPropertiesFactory.h"
#include "FWWindow.h"
#include "ProjectPanel.h"
#include "RCS.h"
#include "RuleSetView.h"
#include "findDialog.h"
#include "events.h"
#include "ObjectTreeView.h"
#include "FWObjectClipboard.h"
#include "WorkflowIcons.h"
#include "FirewallCodeViewer.h"

#include <QtDebug>
#include <QMdiSubWindow>
#include <QMdiArea>
#include <QTimer>
#include <QStatusBar>
#include <QFileInfo>
#include <QApplication>
#include <QUndoStack>
#include <QUndoGroup>
#include <QScrollBar>
#include <QMessageBox>

#include <iostream>


using namespace Ui;
using namespace libfwbuilder;
using namespace std;



void ProjectPanel::initMain(FWWindow *main)
{
    mainW = main;
    mdiWindow = NULL;
    treeReloadPending = false;

    // mdiWindow changes state several times right after it is opened,
    // but we call saveState to store splitter position and its geometry
    // when state changes. Flag "ready" is false after ProjectPanel is created
    // and until FWWindow decides that ProjectPanel is ready for operation.
    // Do not load or save state if flag ready is false.
    ready = false;

    int total_width = DEFAULT_H_SPLITTER_POSITION;
    int total_height = DEFAULT_V_SPLITTER_POSITION;

    if (mainW)
    {
        total_width = mainW->width();
        total_height = mainW->height();
    }

    setMainSplitterPosition(DEFAULT_H_SPLITTER_POSITION,
                            total_width - DEFAULT_H_SPLITTER_POSITION);

    loading_state = false;
    oldState=-1;

    main->undoGroup->addStack(undoStack);

    connect(m_panel->treeDockWidget, SIGNAL(topLevelChanged(bool)),
            this, SLOT(topLevelChangedForTreePanel(bool)));
    connect(m_panel->treeDockWidget, SIGNAL(visibilityChanged(bool)),
            this, SLOT(visibilityChangedForTreePanel(bool)));

//    connect(m_panel->bottomDockWidget, SIGNAL(topLevelChanged(bool)),
//            this, SLOT(topLevelChangedForBottomPanel(bool)));

    fd  = new findDialog(this, this);
    fd->hide();

    m_panel->icons->setUpSignals(this);
}

void ProjectPanel::reset()
{
    undoStack->clear();
    delete rcs;
    rcs = NULL;
    firewalls.clear();
    visibleFirewall = NULL;
    visibleRuleSet = NULL;
    clearFirewallTabs();
    clearObjects();
    FWObjectClipboard::obj_clipboard->clear();
}

ProjectPanel::ProjectPanel(QWidget *parent):
    QWidget(parent), // , Qt::WindowSystemMenuHint|Qt::Window),
    mainW(0),
    rcs(0),
    systemFile(true),
    safeMode(false),
    editingStandardLib(false),
    editingTemplateLib(false),
    ruleSetRedrawPending(false),
    objdb(0),
    fd(0),
    autosaveTimer(new QTimer(static_cast<QObject*>(this))), ruleSetTabIndex(0),
    visibleFirewall(0),
    visibleRuleSet(0),
    lastFirewallIdx(-2),
    changingTabs(false),
    noFirewalls(tr("No firewalls defined")),
    m_panel(0),
    undoStack(0)
{
    if (fwbdebug) qDebug("ProjectPanel constructor");
    m_panel = new Ui::ProjectPanel_q();
    m_panel->setupUi(this);
    m_panel->om->setupProject(this);

    m_panel->toolbar->hide();

    undoStack = new QUndoStack(this);

    setWindowTitle(getPageTitle());

    if (fwbdebug) qDebug("New ProjectPanel  %p", this);

    connect(m_panel->topSplitter, SIGNAL(splitterMoved(int,int)),
            this, SLOT(splitterPositionChanged(int,int)));
}

ProjectPanel::~ProjectPanel()
{
    undoStack->clear();
    if (rcs) delete rcs;
    if (objdb) delete objdb;
    delete m_panel;
}

QString ProjectPanel::getPageTitle(bool file_path)
{
    QString default_caption = tr("Untitled");
    if (rcs)
    {
        QString caption;
        if (file_path) caption = rcs->getFileName(); // full path
        else
        {
            QFileInfo fi(rcs->getFileName());
            caption = fi.fileName();
        }
        if (rcs->isInRCS()) caption= caption + ", rev " + rcs->getSelectedRev();
        if (rcs->isRO()) caption = caption + " " + tr("(read-only)");
        if (caption.isEmpty()) return default_caption;
        return caption;
    }
    else return default_caption;
}

RuleElement* ProjectPanel::getRE(Rule* r, int col)
{
    string ret;
    switch (col)
    {
        case 0: ret=RuleElementSrc::TYPENAME; break;//Object
        case 1: ret=RuleElementDst::TYPENAME; break;//Object
        case 2: ret=RuleElementSrv::TYPENAME; break;//Object
        case 3: ret=RuleElementItf::TYPENAME; break;//Object
        case 4: ret=RuleElementInterval::TYPENAME; break;//Time
        default: return NULL;
    }

    return RuleElement::cast( r->getFirstByType(ret) );
}

void ProjectPanel::restoreRuleSetTab()
{
    if (fwbdebug) qDebug("ProjectPanel::()");
    m_panel->ruleSets->setCurrentIndex(ruleSetTabIndex);
    m_panel->toolbar->show();
}

void ProjectPanel::loadObjects()
{
    m_panel->om->loadObjects();
}

void ProjectPanel::loadObjects(FWObjectDatabase*)
{
    m_panel->om->loadObjects();
}

void ProjectPanel::clearObjects()
{
    m_panel->om->clearObjects();
}

void ProjectPanel::clearFirewallTabs()
{
    if (fwbdebug) qDebug() << "ProjectPanel::clearFirewallTabs";

    m_panel->ruleSets->hide();

    while (m_panel->ruleSets->count()!=0)
    {
        QWidget *p = m_panel->ruleSets->widget(0);
        m_panel->ruleSets->removeWidget(
            m_panel->ruleSets->widget(m_panel->ruleSets->indexOf(p)));
        delete p;
    }
    m_panel->rulesetname->setText("");
    m_panel->ruleSets->show();
    ruleSetViews.clear();
}

void ProjectPanel::closeRuleSetPanel()
{
    if (fwbdebug) qDebug() << "ProjectPanel::closeRuleSetPanel";
    clearFirewallTabs();
    visibleRuleSet = NULL;
}

void ProjectPanel::ensureObjectVisibleInRules(FWReference *obj)
{
    if (fwbdebug) qDebug() << "ProjectPanel::ensureObjectVisibleInRules";
    FWObject *p=obj;
    while (p && RuleSet::cast(p)==NULL ) p=p->getParent();
    if (p==NULL) return;  // something is broken
    // p is a pointer to RuleSet object @obj belongs to
    if (p != getCurrentRuleSet()) openRuleSet(p);
    getCurrentRuleSetView()->setFocus();
    getCurrentRuleSetView()->selectRE( obj );
}

RuleSetView * ProjectPanel::getCurrentRuleSetView()
{
    return dynamic_cast<RuleSetView*>(m_panel->ruleSets->currentWidget());
}


void ProjectPanel::reopenFirewall()
{
    if (fwbdebug)  qDebug("ProjectPanel::reopenFirewall()");

    time_t last_modified = db()->getTimeLastModified();
    if (fwbdebug)
        qDebug("ProjectPanel::reopenFirewall(): checkpoint 1: "
               "dirty=%d last_modified=%s",
               db()->isDirty(), ctime(&last_modified));

    if (ruleSetRedrawPending) return;

    int currentPage = m_panel->ruleSets->currentIndex();

    SelectionMemento memento;

    RuleSetView* rv = dynamic_cast<RuleSetView*>(m_panel->ruleSets->currentWidget());
    if (rv) rv->saveCurrentRowColumn(memento);

    last_modified = db()->getTimeLastModified();
    if (fwbdebug)
        qDebug("ProjectPanel::reopenFirewall(): checkpoint 2: "
               "dirty=%d last_modified=%s",
               db()->isDirty(), ctime(&last_modified));

    // since reopenFirewall deletes and recreates all RuleSetView
    // widgets, it causes significant amount of repaint and
    // flicker. Disable updates for the duration of operation to avoid
    // that.
    m_panel->ruleSets->setUpdatesEnabled(false);

    changingTabs = true;

    clearFirewallTabs();

    last_modified = db()->getTimeLastModified();
    if (fwbdebug)
        qDebug("ProjectPanel::reopenFirewall(): checkpoint 3: "
               "dirty=%d last_modified=%s",
               db()->isDirty(), ctime(&last_modified));

    if (visibleRuleSet==NULL) return ;

    for (int i =0 ; i < m_panel->ruleSets->count (); i++)
        m_panel->ruleSets->removeWidget(m_panel->ruleSets->widget(i));

    m_panel->rulesetname->setTextFormat(Qt::RichText);
    updateFirewallName();

    last_modified = db()->getTimeLastModified();
    if (fwbdebug)
        qDebug("ProjectPanel::reopenFirewall(): checkpoint 4: "
               "dirty=%d last_modified=%s",
               db()->isDirty(), ctime(&last_modified));

    RuleSetView* rulesetview =
        RuleSetView::getRuleSetViewByType(this, visibleRuleSet, NULL);
    if (rulesetview)
    {
        m_panel->ruleSets->addWidget(rulesetview);

        last_modified = db()->getTimeLastModified();
        if (fwbdebug)
            qDebug("ProjectPanel::reopenFirewall(): checkpoint 5: "
                   "dirty=%d last_modified=%s",
                   db()->isDirty(), ctime(&last_modified));

        m_panel->ruleSets->setCurrentIndex(currentPage);
        m_panel->toolbar->show();
        rv = dynamic_cast<RuleSetView*>(m_panel->ruleSets->currentWidget());
        rv->restoreCurrentRowColumn(memento);

        changingTabs = false;

        mainW->updateGlobalToolbar();

        m_panel->ruleSets->setUpdatesEnabled(true);
        m_panel->ruleSets->show();
    }
}

int  ProjectPanel::findFirewallInList(FWObject *f)
{
    vector<FWObject*>::iterator i;
    int n=0;
    for (i=firewalls.begin(); i!=firewalls.end(); i++,n++)
    {
        if ( (*i)->getId()==f->getId() ) return n;
    }
    return -1;
}

void ProjectPanel::updateFirewallName()
{
    if (visibleRuleSet==NULL) return ;
    QString name;
//     mw->buildEditorTitleAndIcon(visibleRuleSet, ObjectEditor::optNone,
//                                 &name, NULL, false);
//    name = "<b>" + name  + "</b>";
    FWObject *fw = visibleRuleSet->getParent();
    name = QString("Currently editing: <b>%1 / %2</b>")
           .arg(QString::fromUtf8(fw->getName().c_str()))
           .arg(QString::fromUtf8(visibleRuleSet->getName().c_str()));
    m_panel->rulesetname->setText(name );
}

void ProjectPanel::openRuleSet(FWObject * obj, bool immediately)
{
    //mw->blankEditor();
    visibleRuleSet = RuleSet::cast(obj);
    if (immediately) redrawRuleSets();
    else registerRuleSetRedrawRequest();
}

void ProjectPanel::selectRules()
{
//    `unselect();
    RuleSetView* rv = dynamic_cast<RuleSetView*>(
        m_panel->ruleSets->currentWidget());
    if (rv) rv->setFocus();
}

void ProjectPanel::unselectRules()
{
    bool havePolicies = (m_panel->ruleSets->count()!=0);

/* commented this out so that when I hit "Edit" in the object's pop-down
 * menu in a rule, ruleset wont lose focus when object editor is opened.
 * If rule set loses focus, the object's background turns from "selected" color
 * to white and user loses context (which object is shown in the object editor)
 */
    if (havePolicies)
    {
        RuleSetView* rv=dynamic_cast<RuleSetView*>(m_panel->ruleSets->currentWidget());

        if (rv && rv->getSelectedObject()!=getSelectedObject())
        {
            rv->clearFocus();
        }
    }
    mainW->disableActions(havePolicies);
}

void ProjectPanel::editCopy()
{
    if (fwbdebug)
        qDebug() << "ProjectPanel::editCopy()  isManipulatorSelected()="
                 << isManipulatorSelected();

    if (isManipulatorSelected()) copyObj();
    else
    {
        if (m_panel->ruleSets->count()!=0)
        {
            RuleSetView *rsv = dynamic_cast<RuleSetView*>(m_panel->ruleSets->currentWidget());
            if (rsv)
                rsv->copySelectedObject();
        }
    }
}

void ProjectPanel::editCut()
{
    if (fwbdebug)
        qDebug() << "ProjectPanel::editCut()  isManipulatorSelected()="
                 << isManipulatorSelected();

    if (isManipulatorSelected()) cutObj();
    else
    {
        if (m_panel->ruleSets->count()!=0)
        {
            RuleSetView *rsv = dynamic_cast<RuleSetView*>(m_panel->ruleSets->currentWidget());
            if (rsv)
                rsv->cutSelectedObject();
        }
    }
}

void ProjectPanel::editDelete()
{
    if (fwbdebug)
        qDebug() << "ProjectPanel::editDelete()  isManipulatorSelected()="
                 << isManipulatorSelected();

    if (isManipulatorSelected()) deleteObj();
}

void ProjectPanel::editPaste()
{
    if (fwbdebug)
        qDebug() << "ProjectPanel::editPaste()  isManipulatorSelected()="
                 << isManipulatorSelected();

    if (isManipulatorSelected()) pasteObj();
    else
    {
        if (m_panel->ruleSets->count()!=0)
        {
            RuleSetView *rsv = dynamic_cast<RuleSetView*>(m_panel->ruleSets->currentWidget());
            if (rsv)
                rsv->pasteObject();
        }
    }
}

QString ProjectPanel::getDestDir(const QString &fname)
{
    QString destdir = "";

    if (st->getWDir().isEmpty())
    {
        if (fname.isEmpty())
        {
            destdir = userDataDir.c_str();
        } else
        {
            QFileInfo fi(fname);
            if (fi.isDir()) destdir = fname;
            else
            {
                destdir = fi.canonicalPath();
            }
        }
    } else
    {
        destdir = st->getWDir();
    }
    return destdir;
}

void ProjectPanel::setFileName(const QString &fname)
{
    systemFile = false;
    rcs->setFileName(fname);
    db()->setFileName(fname.toLatin1().constData());

    //setWindowTitle(getPageTitle());
    QCoreApplication::postEvent(mw, new updateSubWindowTitlesEvent());
}

//wrapers for some ObjectManipulator functions

FWObject* ProjectPanel::getCurrentLib()
{
    return m_panel->om->getCurrentLib();
}

void ProjectPanel::updateObjectInTree(FWObject *obj, bool subtree)
{
    m_panel->om->updateObjectInTree(obj, subtree);
}

FWObject* ProjectPanel::createObject(const QString &objType,
                                     const QString &objName,
                                     FWObject *copyFrom)
{
    return m_panel->om->createObject(objType, objName, copyFrom);
}

FWObject* ProjectPanel::createObject(FWObject *parent,
                                     const QString &objType,
                                     const QString &objName,
                                     FWObject *copyFrom)
{
    return m_panel->om->createObject(parent, objType, objName, copyFrom);
}

void ProjectPanel::moveObject(FWObject *target,
                              FWObject *obj)
{
    m_panel->om->moveObject(target, obj);
}

void ProjectPanel::moveObject(const QString &targetLibName,
                              FWObject *obj)
{
    m_panel->om->moveObject(targetLibName, obj);
}

FWObject* ProjectPanel::pasteTo(FWObject *target, FWObject *obj)
{
    return m_panel->om->pasteTo(target, obj);
}

ObjectTreeView* ProjectPanel::getCurrentObjectTree()
{
    return m_panel->om->getCurrentObjectTree();
}

void ProjectPanel::findAllFirewalls (std::list<Firewall *> &fws)
{
    m_panel->om->findAllFirewalls(fws);
}

void ProjectPanel::showDeletedObjects(bool f)
{
    m_panel->om->showDeletedObjects(f);
}

void ProjectPanel::select()
{
    m_panel->om->select();
}

void ProjectPanel::unselect()
{
    m_panel->om->unselect();
}

void ProjectPanel::clearManipulatorFocus()
{
    m_panel->om->clearFocus();
}

void ProjectPanel::copyObj()
{
    m_panel->om->copyObj();
}

bool ProjectPanel::isManipulatorSelected()
{
    return m_panel->om->getCurrentObjectTree()->hasFocus();
}

void ProjectPanel::cutObj()
{
    m_panel->om->cutObj();
}

void ProjectPanel::pasteObj()
{
    m_panel->om->pasteObj();
}


void ProjectPanel::newObject()
{
    m_panel->om->newObject();
}

void ProjectPanel::deleteObj()
{
    m_panel->om->delObj();
}

FWObject* ProjectPanel::getSelectedObject()
{
    return m_panel->om->getSelectedObject();
}

void ProjectPanel::reopenCurrentItemParent()
{
    m_panel->om->reopenCurrentItemParent();
}

void ProjectPanel::back()
{
    m_panel->om->back();
}

void ProjectPanel::lockObject()
{
    m_panel->om->lockObject();
}

void ProjectPanel::unlockObject()
{
    m_panel->om->unlockObject();
}

void ProjectPanel::setFDObject(FWObject *o)
{
    fd->setObject(o);
    fd->show();
}
void ProjectPanel::resetFD()
{
    fd->reset();
}

void ProjectPanel::insertRule()
{
    if (visibleRuleSet==NULL || m_panel->ruleSets->count()==0) return;
    getCurrentRuleSetView()->insertRule();
}

void ProjectPanel::addRuleAfterCurrent()
{
    if (visibleRuleSet==NULL || m_panel->ruleSets->count()==0) return;
    getCurrentRuleSetView()->addRuleAfterCurrent();
}

void ProjectPanel::removeRule()
{
    if (visibleRuleSet==NULL || m_panel->ruleSets->count()==0) return;
    getCurrentRuleSetView()->removeRule();
}

void ProjectPanel::moveRule()
{
    if (visibleRuleSet==NULL || m_panel->ruleSets->count()==0) return;
    getCurrentRuleSetView()->moveRule();
}

void ProjectPanel::moveRuleUp()
{
    if (visibleRuleSet==NULL || m_panel->ruleSets->count()==0) return;
    getCurrentRuleSetView()->moveRuleUp();
}

void ProjectPanel::moveRuleDown()
{
    if (visibleRuleSet==NULL || m_panel->ruleSets->count()==0) return;
    getCurrentRuleSetView()->moveRuleDown();
}

void ProjectPanel::copyRule()
{
    if (visibleRuleSet==NULL || m_panel->ruleSets->count()==0) return;
    getCurrentRuleSetView()->copyRule();
}

void ProjectPanel::cutRule()
{
    if (visibleRuleSet==NULL || m_panel->ruleSets->count()==0) return;
    getCurrentRuleSetView()->cutRule();
}

void ProjectPanel::pasteRuleAbove()
{
    if (visibleRuleSet==NULL || m_panel->ruleSets->count()==0) return;
    getCurrentRuleSetView()->pasteRuleAbove();
}

void ProjectPanel::pasteRuleBelow()
{
    if (visibleRuleSet==NULL || m_panel->ruleSets->count()==0) return;
    getCurrentRuleSetView()->pasteRuleBelow();
}

bool ProjectPanel::editingLibrary()
{
    return (rcs!=NULL &&
        ( rcs->getFileName().endsWith(".fwl")) );
}

void ProjectPanel::createRCS(const QString &filename)
{
    rcs = new RCS(filename);
    systemFile = true;
}

RCS * ProjectPanel::getRCS()
{
    return rcs;
}

/*
 * This slot is connected to the "add rule" button in the mini-toolbar
 * at the top of the rule set view
 */
void ProjectPanel::addRule()
{
    if (visibleRuleSet==NULL) return ;
    getCurrentRuleSetView()->insertRule();
}

void ProjectPanel::compileThis()
{
    if (visibleRuleSet==NULL) return ;

    save();
    // see comment in FWWindow::compile()
    if (db()->isDirty()) return;

    wfl->registerFlag(UserWorkflow::COMPILE, true);

    set<Firewall*> fw;
    Firewall *f = Firewall::cast(visibleRuleSet->getParent());
    if (f)
    {
        fw.insert(f);
        mainW->compile(fw);
    }
}

void ProjectPanel::installThis()
{
    if (visibleRuleSet==NULL) return ;

    save();
    // see comment in FWWindow::compile()
    if (db()->isDirty()) return;

    wfl->registerFlag(UserWorkflow::INSTALL, true);

    set<Firewall*> fw;
    Firewall *f = Firewall::cast(visibleRuleSet->getParent());
    if (f)
    {
        fw.insert(f);
        mainW->install(fw);
    }
}

void ProjectPanel::inspectThis()
{
    if (visibleRuleSet==NULL) return;

    save();
    // see comment in FWWindow::compile()
    if (db()->isDirty()) return;

    Firewall *f = Firewall::cast(visibleRuleSet->getParent());
    set<Firewall*> fwlist;
    if (Cluster::isA(f))
    {
        std::list<Firewall*> cfws;
        Cluster::cast(f)->getMembersList(cfws);
        foreach(Firewall *fw, cfws)
            fwlist.insert(fw);
    }
    else
    {
        fwlist.insert(f);
    }

    this->inspect(fwlist);
}

void ProjectPanel::inspectAll()
{
    ObjectManipulator *om = this->findChild<ObjectManipulator*>();
    list<Firewall*> fws;
    om->findAllFirewalls(fws);
    set<Firewall*> fwset;
    foreach(Firewall *fw, fws)
    {
        if (Cluster::isA(fw))
        {
            std::list<Firewall*> cfws;
            Cluster::cast(fw)->getMembersList(cfws);
            foreach(Firewall *f, cfws)
                fwset.insert(f);
        }
        else
        {
            fwset.insert(fw);
        }
    }

    this->inspect(fwset);
}

void ProjectPanel::compile()
{
    if (mw->isEditorVisible() &&
        !mw->requestEditorOwnership(NULL,NULL,ObjectEditor::optNone,true))
        return;

    save();
    // see comment in FWWindow::compile()
    if (db()->isDirty()) return;

    //fileSave();
    wfl->registerFlag(UserWorkflow::COMPILE, true);
    mainW->compile();
}

void ProjectPanel::compile(set<Firewall*> vf)
{
    if (mw->isEditorVisible() &&
        !mw->requestEditorOwnership(NULL, NULL, ObjectEditor::optNone, true))
        return;

    save();
    // see comment in FWWindow::compile()
    if (db()->isDirty()) return;

    //fileSave();
    wfl->registerFlag(UserWorkflow::COMPILE, true);
    mainW->compile(vf);
}

void ProjectPanel::install(set<Firewall*> vf)
{
    save();
    // see comment in FWWindow::compile()
    if (db()->isDirty()) return;

    wfl->registerFlag(UserWorkflow::INSTALL, true);
    mainW->install(vf);
}

void ProjectPanel::install()
{
    save();
    // see comment in FWWindow::compile()
    if (db()->isDirty()) return;

    wfl->registerFlag(UserWorkflow::INSTALL, true);
    mainW->install();
}


void ProjectPanel::inspect(set<Firewall *> fws)
{
    if (fws.empty())
        return;

    QMessageBox messageBox(this);
    messageBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    messageBox.addButton(tr("Compile and Inspect files"), QMessageBox::AcceptRole);
    messageBox.setIcon(QMessageBox::Critical);

    set<Firewall*> needCompile;
    foreach(Firewall *fw, fws)
        if (fw->needsCompile())
            needCompile.insert(fw);

    if (!needCompile.empty())
    {
        QString text;
        QStringList names;
        foreach(Firewall *fw, needCompile)
            names.append(fw->getName().c_str());

        if (needCompile.size() > 1 && needCompile.size() < 5)
        {
            QString last = names.last();
            names.pop_back();
            QString firewalls = "\"" + names.join("\", \"") + "\" " + tr("and") + " \"" + last + "\"";
            text = tr("Firewall objects %1 have been modified and need to be recompiled.").arg(firewalls);
        }
        else
            if (needCompile.size() == 1)
                text = tr("Firewall object \"%1\" has been modified and needs to be recompiled.").arg(names.first());
            else
            {
                text = tr("%1 firewall objects have been modified and need to be recompiled.").arg(needCompile.size());
            }

        messageBox.setText(text);
        messageBox.exec();
        if (messageBox.result() == QMessageBox::Accepted)
        {
            this->compile(needCompile);
        }
        return;
    }

    QStringList files;

    QSet<Firewall*> filesMissing;
    Firewall *first_fw = NULL;
    foreach(Firewall *fw, fws)
    {
        if (first_fw == NULL) first_fw = fw;
        QString mainFile = FirewallInstaller::getGeneratedFileFullPath(fw);
        if (QFile::exists(mainFile))
        {
            instConf cnf;
            cnf.fwobj = fw;
            cnf.script = mainFile;
            QMap<QString, QString> res;
            FirewallInstaller(NULL, &cnf, "").readManifest(mainFile, &res);
            QStringList current_files = res.keys();
            foreach(QString file, current_files)
            {
                if (!QFile::exists(file))
                    filesMissing.insert(fw);
                else
                    files.append(file);
            }
        }
        else
            filesMissing.insert(fw);
    }

    if (!filesMissing.isEmpty())
    {
        QString text;
        QStringList names;
        foreach(Firewall *fw, filesMissing)
            names.append(fw->getName().c_str());

        if (filesMissing.size() > 1 && filesMissing.size() < 5)
        {
            QString last = names.last();
            names.pop_back();
            QString firewalls = "\"" + names.join("\", \"") + "\" " + tr("and") + " \"" + last + "\"";
            text = tr("Can not read generated files for the firewall objects %1. You need to compile them to create the files.").arg(firewalls);
        }
        else
            if (filesMissing.size() == 1)
                text = tr("Can not read generated files for the firewall objects %1. You need to compile it to create the files.").arg(names.first());
            else
            {
                text = tr("Can not read generated files for the %1 firewall objects. You need to compile then to create the files.").arg(filesMissing.size());
            }
        messageBox.setText(text);
        messageBox.exec();
        if (messageBox.result() == QMessageBox::Accepted)
        {
            this->compile(fws);
        }
        return;
    }

    if (files.empty())
        return;

    QString viewer_title;
    if (fws.size() > 1) viewer_title = tr("<b>Multiple firewalls</b>");
    else viewer_title = QString("<b>%1</b>").arg(first_fw->getName().c_str());

    FirewallCodeViewer *viewer =
        new FirewallCodeViewer(files, viewer_title, this);
    viewer->show();
}

void ProjectPanel::transferfw(set<Firewall*> vf)
{
    mainW->transferfw(vf);
}

void ProjectPanel::transferfw()
{
    mainW->transferfw();
}

QString ProjectPanel::printHeader()
{
    QString headerText = rcs->getFileName().section("/",-1,-1);
    if (rcs->isInRCS())
        headerText = headerText + ", rev " + rcs->getSelectedRev();
    return headerText;
}

void ProjectPanel::registerRuleSetRedrawRequest()
{
    if (!ruleSetRedrawPending)
    {
        ruleSetRedrawPending = true;
        //redrawRuleSets();
        QTimer::singleShot( 0, this, SLOT(redrawRuleSets()) );
    }
}

void ProjectPanel::redrawRuleSets()
{
    ruleSetRedrawPending = false;
    reopenFirewall();
}

void ProjectPanel::showEvent(QShowEvent *ev)
{
    if (fwbdebug) qDebug() << "ProjectPanel::showEvent " << this
                           << "title " << mdiWindow->windowTitle();

    m_panel->treeDockWidget->raise();
    QWidget::showEvent(ev);

    // we get this event when subsindow is maximized or restored
    loadState();
}

void ProjectPanel::hideEvent(QHideEvent *ev)
{
    if (fwbdebug) qDebug() << "ProjectPanel::hideEvent " << this
                           << "title " << mdiWindow->windowTitle();

    QWidget::hideEvent(ev);
}

void ProjectPanel::closeEvent(QCloseEvent * ev)
{
    if (fwbdebug) qDebug() << "ProjectPanel::closeEvent " << this
                           << "title " << mdiWindow->windowTitle();

    if (!saveIfModified() || !checkin(true))
    {
        ev->ignore();
        return;
    }

    saveState();
    fileClose();

    mw->updateWindowTitle();

    QTimer::singleShot( 0, mw, SLOT(projectWindowClosed()) );
}

QString ProjectPanel::getFileName()
{
    if (rcs!=NULL) return rcs->getFileName();
    else  return "";
}

void ProjectPanel::splitterMoved(int , int)
{
}

void ProjectPanel::resizeEvent(QResizeEvent*)
{
}

void ProjectPanel::registerTreeReloadRequest()
{
    treeReloadPending = true;
    QTimer::singleShot(0, this, SLOT(reloadTree()));
}

void ProjectPanel::reloadTree()
{
    if (treeReloadPending)
    {
        m_panel->om->reload();
        treeReloadPending = false;
    }
}

void ProjectPanel::registerObjectToUpdateInTree(FWObject *o, bool update_subtree)
{
    if (fwbdebug)
        qDebug() << "ProjectPanel::registerObjectToUpdateInTree()"
                 << "o=" << o->getName().c_str()
                 << "update_subtree=" << update_subtree
                 << "updateObjectsInTreePool.size()=" << updateObjectsInTreePool.size();
    if (updateObjectsInTreePool.find(o->getId()) == updateObjectsInTreePool.end())
    {
        updateObjectsInTreePool[o->getId()] = update_subtree;
        QTimer::singleShot(0, this, SLOT(updateObjectInTree()));
    }
}

void ProjectPanel::updateObjectInTree()
{
    if (fwbdebug)
        qDebug() << "ProjectPanel::updateObjectInTree()"
                 << "updateObjectsInTreePool.size()=" << updateObjectsInTreePool.size();

    while (updateObjectsInTreePool.size() > 0)
    {
        map<int, bool>::iterator it = updateObjectsInTreePool.begin();
        FWObject *obj = db()->findInIndex(it->first);
        m_panel->om->updateObjectInTree(obj, it->second);
        updateObjectsInTreePool.erase(it);
    }
    mdiWindow->update();
}

void ProjectPanel::registerModifiedObject(FWObject *o)
{
    if (fwbdebug)
        qDebug() << "ProjectPanel::registerModifiedObject "
                 << "lastModifiedTimestampChangePool.size()=" << lastModifiedTimestampChangePool.size()
                 << "o=" << o->getName().c_str();
    if (lastModifiedTimestampChangePool.find(o->getId()) == lastModifiedTimestampChangePool.end())
    {
        lastModifiedTimestampChangePool.insert(o->getId());
        QTimer::singleShot(0, this, SLOT(updateLastModifiedTimestampForAllFirewalls()));
    }
}

void ProjectPanel::updateLastModifiedTimestampForAllFirewalls()
{
    if (fwbdebug)
        qDebug() << "ProjectPanel::updateLastModifiedTimestampForAllFirewalls"
                 << "lastModifiedTimestampChangePool.size()="
                 << lastModifiedTimestampChangePool.size();

    if (lastModifiedTimestampChangePool.size() == 0) return;

    mw->showStatusBarMessage(tr("Searching for firewalls affected by the change..."));

    //QApplication::processEvents(QEventLoop::ExcludeUserInputEvents,100);

    QApplication::setOverrideCursor(QCursor( Qt::WaitCursor));

    set<Firewall*> firewalls_to_update;

    while (lastModifiedTimestampChangePool.size() > 0)
    {
        set<int>::iterator it = lastModifiedTimestampChangePool.begin();
        FWObject *obj = db()->findInIndex(*it);
        lastModifiedTimestampChangePool.erase(it);

        if (fwbdebug)
            qDebug() << "Modified object: " << obj->getName().c_str();

        if (FWBTree().isSystem(obj)) continue;

        list<Firewall *> fws = m_panel->om->findFirewallsForObject(obj);
        if (fws.size())
        {
            Firewall *f;
            for (list<Firewall *>::iterator i=fws.begin();
                 i!=fws.end();
                 ++i)
            {
                f = *i;
                if (f==obj) continue;

                firewalls_to_update.insert(f);
            }
        }
    }

    if (fwbdebug)
        qDebug() << "Will update " << firewalls_to_update.size() << " firewalls";

    for (set<Firewall*>::iterator it=firewalls_to_update.begin();
         it!=firewalls_to_update.end(); ++it)
    {
        Firewall *f = *it;
        // when user locks firewall object, this code tries to
        // update last_modified timestamp in it because it
        // depends on itself. Dont.
        if (f->isReadOnly()) continue;

        f->updateLastModifiedTimestamp();
        QCoreApplication::postEvent(
            mw, new updateObjectInTreeEvent(getFileName(), f->getId()));

        list<Cluster*> clusters = m_panel->om->findClustersUsingFirewall(f);
        if (clusters.size() != 0)
        {
            list<Cluster*>::iterator it;
            for (it=clusters.begin(); it!=clusters.end(); ++it)
            {
                Cluster *cl = *it;
                if (cl->isReadOnly()) continue;
                cl->updateLastModifiedTimestamp();
                QCoreApplication::postEvent(
                    mw, new updateObjectInTreeEvent(getFileName(), cl->getId()));
            }
        }
    }

    QApplication::restoreOverrideCursor();
}

void ProjectPanel::toggleViewTree(bool f)
{
    if (f) m_panel->treeDockWidget->show();
    else m_panel->treeDockWidget->hide();
}

/*
 * Signal QDockWidget::topLevelChanged is called after dock widget
 * is made floating or docked.
 */
void ProjectPanel::topLevelChangedForTreePanel(bool f)
{
    if (fwbdebug)
        qDebug() << "ProjectPanel::topLevelChangedForTreePanel  f=" << f;

    /*
     * QDockWidget object uses native decorators on Windows and Mac
     * and therefore gets window title bar there. On X11 QT emulates
     * title bar and allows dragging of the floating dock widget only
     * if its parent is QMainWindow. Here is a hack: we reparent the
     * widget in order to satisfy their requirements and make floating
     * panel widget draggable on all platforms. Need to reparent it
     * back and stick it into the layout of the ProjectPanel when it
     * is docked.
     */

    m_panel->treeDockWidget->blockSignals(true);

    if (f)  // window becomes detached
    {
        QString file_name = getFileName(); // full path
        QFileInfo fi(file_name);
        QString short_name = fi.fileName();
        m_panel->treeDockWidget->setParent(mw);
        m_panel->treeDockWidget->setWindowTitle(short_name);
        mw->addDockWidget(Qt::LeftDockWidgetArea, m_panel->treeDockWidget);
        m_panel->treeDockWidget->show();
        m_panel->treeDockWidget->blockSignals(false);
    } else
    {
#if QT_VERSION < 0x040500
// See bug #973 for details
        QTimer::singleShot(0, this, SLOT(setTreeDockPosition()));
#else
// Setting widget position here causes crash on Qt < 4.5.
        mw->removeDockWidget(m_panel->treeDockWidget);
        m_panel->treeDockWidget->setWindowTitle("");
        m_panel->treeDockWidget->setParent(m_panel->treeDockWidgetParentFrame);
        m_panel->treeDockWidgetParentFrame->layout()->addWidget(m_panel->treeDockWidget);
        m_panel->treeDockWidget->show();
        m_panel->treeDockWidget->blockSignals(false);
#endif
    }
    m_panel->treeDockWidget->setFloating(f);

    if (fwbdebug)
        qDebug() << "ProjectPanel::topLevelChangedForTreePanel check 1";

    if (!m_panel->treeDockWidget->isWindow())
    {
#if QT_VERSION > 0x040500
        loadMainSplitter();
#endif
    } else
    {
        // expand rules
        collapseTree();
        m_panel->treeDockWidget->widget()->update();
    }

    if (fwbdebug)
        qDebug() << "ProjectPanel::topLevelChangedForTreePanel check 2";
}

void ProjectPanel::visibilityChangedForTreePanel(bool f)
{
    if (fwbdebug)
        qDebug() << "ProjectPanel::visibilityChangedForTreePanel  f="
                 << f
                 << "isVisible()=" << m_panel->treeDockWidget->isVisible()
                 << "isWindow()=" << m_panel->treeDockWidget->isWindow()
                 << "mdiWindow->isMaximized()=" << mdiWindow->isMaximized();

    if (m_panel->treeDockWidget->isVisible() &&
        ! m_panel->treeDockWidget->isWindow())  // visible and not floating
    {
        loadMainSplitter();
    } else
    {
        // expand rules
        collapseTree();
        m_panel->treeDockWidget->widget()->update();
    }

}

void ProjectPanel::setActive()
{
    undoStack->setActive(true);
}

#if QT_VERSION < 0x040500
void ProjectPanel::setTreeDockPosition()
{
    mw->removeDockWidget(m_panel->treeDockWidget);
    m_panel->treeDockWidget->setParent(m_panel->treeDockWidgetParentFrame);
    m_panel->treeDockWidgetParentFrame->layout()->addWidget(m_panel->treeDockWidget);
    m_panel->treeDockWidget->blockSignals(false);
    m_panel->treeDockWidget->show();
    loadMainSplitter();
}
#endif

void ProjectPanel::splitterPositionChanged(int,int)
{
    saveMainSplitter();
}
