/*

                          Firewall Builder

                 Copyright (C) 2003 NetCitadel, LLC

  Author:  Vadim Kurland     vadim@fwbuilder.org

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
#include "platforms.h"
#include "events.h"

#include "ObjectManipulator.h"
#include "ObjectEditor.h"
#include "ObjectTreeViewItem.h"
#include "ObjectTreeView.h"
#include "newGroupDialog.h"
#include "FWObjectClipboard.h"
#include "FindObjectWidget.h"
#include "interfaceProperties.h"
#include "interfacePropertiesObjectFactory.h"
#include "FWCmdChange.h"
#include "FWCmdAddObject.h"
#include "FWCmdMoveObject.h"
#include "FWBTree.h"
#include "FWWindow.h"
#include "ProjectPanel.h"
#include "ConfirmDeleteObjectDialog.h"

#include "fwbuilder/Cluster.h"
#include "fwbuilder/FWObject.h"
#include "fwbuilder/IPv6.h"
#include "fwbuilder/Library.h"
#include "fwbuilder/Policy.h"
#include "fwbuilder/Resources.h"

#include <QMessageBox>
#include <QTextEdit>
#include <QTime>
#include <QtDebug>

#include <memory>
#include <algorithm>


using namespace std;
using namespace libfwbuilder;

/*
 * TODO: make this signal/slot. Dialogs just emit signal
 * 'updateObject_sign', which objectManipulator should have connected
 * to its slot which would do what updateObjName does now, and more.
 */
void ObjectManipulator::updateObjName(FWObject *obj,
                                      const QString &oldName,
                                      bool  askForAutorename)
{
    if (fwbdebug)
        qDebug() << "ObjectManipulator::updateObjName  changing name "
                 << oldName.toLatin1()
                 << "->"
                 << QString::fromUtf8(obj->getName().c_str());
    
    if (oldName == QString::fromUtf8(obj->getName().c_str())) return;
    
    //if (obj!=getSelectedObject()) openObject(obj);
    
    QTreeWidgetItem *itm = allItems[obj];
    assert(itm!=NULL);
    
    if ((QString::fromUtf8(obj->getName().c_str())!=oldName) &&
        (Host::isA(obj) || Firewall::isA(obj) || Interface::isA(obj)))
    {
        autorename(obj, askForAutorename);
    }

    QCoreApplication::postEvent(
        mw, new updateObjectEverywhereEvent(m_project->getFileName(), obj->getId()));
}

void ObjectManipulator::autorename(FWObject *obj, bool ask)
{
    if (Host::isA(obj) || Firewall::isA(obj) || Cluster::isA(obj))
    {
        QString dialog_txt = tr(
"The name of the object '%1' has changed. The program can also "
"rename IP address objects that belong to this object, "
"using standard naming scheme 'host_name:interface_name:ip'. "
"This makes it easier to distinguish what host or a firewall "
"given IP address object belongs to when it is used in "
"the policy or NAT rule. The program also renames MAC address "
"objects using scheme 'host_name:interface_name:mac'. "
"Do you want to rename child IP and MAC address objects now? "
"(If you click 'No', names of all address objects that belong to "
"%2 will stay the same.)")
            .arg(QString::fromUtf8(obj->getName().c_str()))
            .arg(QString::fromUtf8(obj->getName().c_str()));

        if (!ask || QMessageBox::warning(
                this,"Firewall Builder", dialog_txt,
                tr("&Yes"), tr("&No"), QString::null,
                0, 1 )==0 )
        {
            list<FWObject*> il = obj->getByType(Interface::TYPENAME);
            for (list<FWObject*>::iterator i=il.begin(); i!=il.end(); ++i)
                autorename(*i, false);
        }
    }
 
    if (Interface::isA(obj))
    {
        list<FWObject*> subinterfaces = obj->getByType(Interface::TYPENAME);
        if (obj->getByType(IPv4::TYPENAME).size() ||
            obj->getByType(IPv6::TYPENAME).size() ||
            obj->getByType(physAddress::TYPENAME).size() ||
            subinterfaces.size())
        {
            QString dialog_txt = tr(
                "The name of the interface '%1' has changed. The program can also "
                "rename IP address objects that belong to this interface, "
                "using standard naming scheme 'host_name:interface_name:ip'. "
                "This makes it easier to distinguish what host or a firewall "
                "given IP address object belongs to when it is used in "
                "the policy or NAT rule. The program also renames MAC address "
                "objects using scheme 'host_name:interface_name:mac'. "
                "Do you want to rename child IP and MAC address objects now? "
                "(If you click 'No', names of all address objects that belong to "
                "interface '%2' will stay the same.)")
                .arg(QString::fromUtf8(obj->getName().c_str()))
                .arg(QString::fromUtf8(obj->getName().c_str()));

            if (!ask || QMessageBox::warning(
                    this, "Firewall Builder", dialog_txt,
                    tr("&Yes"), tr("&No"), QString::null,
                    0, 1 )==0 )
            {
                list<FWObject*> vlans;
                for (list<FWObject*>::iterator j=subinterfaces.begin();
                     j!=subinterfaces.end(); ++j)
                {
                    Interface *intf = Interface::cast(*j);
                    if (intf->getOptionsObject()->getStr("type") == "8021q")
                        vlans.push_back(intf);
                }

                if (vlans.size()) autorenameVlans(vlans);

                for (list<FWObject*>::iterator j=subinterfaces.begin();
                     j!=subinterfaces.end(); ++j)
                    autorename(*j, false);

                list<FWObject*> obj_list = obj->getByType(IPv4::TYPENAME);
                autorename(obj_list, IPv4::TYPENAME, "ip");
                obj_list = obj->getByType(IPv6::TYPENAME);
                autorename(obj_list, IPv6::TYPENAME, "ip6");
                obj_list = obj->getByType(physAddress::TYPENAME);
                autorename(obj_list, physAddress::TYPENAME, "mac");

            }

        }
    }
}

void ObjectManipulator::autorename(list<FWObject*> &obj_list,
                                   const string &objtype,
                                   const string &namesuffix)
{
    for (list<FWObject*>::iterator j=obj_list.begin(); j!=obj_list.end(); ++j)
    {
        FWObject *obj = *j;
        QString old_name = obj->getName().c_str();
        FWObject *parent = obj->getParent();
        QString name = getStandardName(parent, objtype, namesuffix);
        name = makeNameUnique(parent, name, objtype.c_str());
        if (name != old_name)
        {
            FWCmdChange* cmd = new FWCmdChangeName(m_project, obj);
            FWObject* new_state = cmd->getNewState();
            new_state->setName(string(name.toUtf8()));
            m_project->undoStack->push(cmd);
        }
    }
}

void ObjectManipulator::autorenameVlans(list<FWObject*> &obj_list)
{
    for (list<FWObject*>::iterator j=obj_list.begin(); j!=obj_list.end(); ++j)
    {
        FWObject *obj = *j;
        FWObject *parent = obj->getParent();
        FWObject *fw = parent;
        while (fw && Firewall::cast(fw)==NULL) fw = fw->getParent();
        assert(fw);
        QString obj_name = QString::fromUtf8(obj->getName().c_str());

        Resources* os_res = Resources::os_res[fw->getStr("host_OS")];
        string os_family = fw->getStr("host_OS");
        if (os_res!=NULL)
            os_family = os_res->getResourceStr("/FWBuilderResources/Target/family");

        std::auto_ptr<interfaceProperties> int_prop(
            interfacePropertiesObjectFactory::getInterfacePropertiesObject(
                os_family));
        if (int_prop->looksLikeVlanInterface(obj_name))
        {
            // even though we only call this function if the type of
            // this interface is 8021q, need to check its naming
            // schema as well. We can't automatically rename
            // interfaces that do not follow known naming convention.

            QString base_name;
            int vlan_id;
            int_prop->parseVlan(obj_name, &base_name, &vlan_id);
            if (base_name != "vlan")
            {
                QString new_name = QString("%1.%2")
                    .arg(QString::fromUtf8(
                             parent->getName().c_str()))
                    .arg(vlan_id);
                if (new_name != QString::fromUtf8(obj->getName().c_str()))
                {
                    FWCmdChange* cmd = new FWCmdChangeName(m_project, obj);
                    FWObject* new_state = cmd->getNewState();
                    new_state->setName(string(new_name.toUtf8()));
                    m_project->undoStack->push(cmd);
                }
            }
        }
    }
}

FWObject* ObjectManipulator::duplicateObject(FWObject *targetLib,
                                             FWObject *obj,
                                             const QString &name,
                                             bool)
{
    if (!isTreeReadWrite(this, targetLib)) return NULL;
    openLib(targetLib);
    QString newName;
    if (!name.isEmpty()) newName = name;
    else                 newName = QString::fromUtf8(obj->getName().c_str());
    return createObject(obj->getTypeName().c_str(), newName, obj);
}

void ObjectManipulator::moveObject(FWObject *targetLib, FWObject *obj)
{
    FWObject *cl=getCurrentLib();
    if (cl==targetLib) return;

    FWObject *grp = NULL;

    if (FWObjectDatabase::isA(targetLib)) grp = targetLib;
    else
    {
        grp = m_project->getFWTree()->getStandardSlotForObject(
            targetLib, obj->getTypeName().c_str());
    }
    if (grp==NULL) grp=targetLib;

    if (!grp->isReadOnly())
    {
        set<FWObject*> reference_holders;
        FWCmdMoveObject *cmd = new FWCmdMoveObject(m_project,
                                                   obj->getParent(),
                                                   grp,
                                                   obj, reference_holders,
                                                   "Move object");
        m_project->undoStack->push(cmd);
    }
    if (fwbdebug) qDebug("ObjectManipulator::moveObject  all done");
}

/*
 *  targetLibName is the name of the target library in Unicode
 */
void ObjectManipulator::moveObject(const QString &targetLibName,
                                   FWObject *obj)
{
    list<FWObject*> ll = m_project->db()->getByType( Library::TYPENAME );
    for (FWObject::iterator i=ll.begin(); i!=ll.end(); i++)
    {
        FWObject *lib=*i;
        if (targetLibName==QString::fromUtf8(lib->getName().c_str()))
        {
            if (fwbdebug)
                qDebug("ObjectManipulator::moveObject  found lib %s",
                       lib->getName().c_str() );

            moveObject(lib,obj);
        }
    }
}

FWObject*  ObjectManipulator::pasteTo(FWObject *target, FWObject *obj)
{
    map<int,int> map_ids;
    return actuallyPasteTo(target, obj, map_ids);
}

FWObject* ObjectManipulator::actuallyPasteTo(FWObject *target,
                                              FWObject *obj,
                                              std::map<int,int> &map_ids)
{
    //FWObject *res = NULL;

    FWObject *ta = prepareForInsertion(target, obj);
    if (ta == NULL) return NULL;

    if (fwbdebug)
        qDebug() << "ObjectManipulator::actuallyPasteTo"
                 << "target=" << target->getPath().c_str()
                 << "ta=" << ta->getPath().c_str();
    try
    {
/* clipboard holds a copy of the object */
        if (obj->getRoot() != ta->getRoot())
        {
            if (fwbdebug) qDebug("Copy object %s (%d) to a different object tree",
                                 obj->getName().c_str(), obj->getId());
            FWCmdAddObject *cmd = new FWCmdAddObject(m_project, target, NULL,
                QObject::tr("Add copy of %1 to %2")
                .arg(QString::fromUtf8(obj->getName().c_str()))
                .arg(QString::fromUtf8(ta->getName().c_str())));
            FWObject *new_state = cmd->getNewState();
            cmd->setNeedTreeReload(true);
            // recursivelyCopySubtree() needs access to the target tree root
            // when it copies subtree, so have to copy into the actual target
            // tree.
            FWObject *nobj = m_project->db()->recursivelyCopySubtree(target, obj, map_ids);
            target->remove(nobj, false);
            new_state->add(nobj);
            m_project->undoStack->push(cmd);
            return nobj;
        }

        Group *grp = Group::cast(ta);
        if (grp!=NULL && !FWBTree().isSystem(ta))
        {
            if (fwbdebug) qDebug("Copy object %s (%d) to a regular group",
                                 obj->getName().c_str(), obj->getId());
/* check for duplicates. We just won't add an object if it is already there */
            int cp_id = obj->getId();
            list<FWObject*>::iterator j;
            for (j=grp->begin(); j!=grp->end(); ++j)
            {
                FWObject *o1=*j;
                if(cp_id==o1->getId()) return o1;

                FWReference *ref;
                if( (ref=FWReference::cast(o1))!=NULL &&
                    cp_id==ref->getPointerId()) return o1;
            }
            FWCmdChange *cmd = new FWCmdChange(
                m_project, grp,
                QObject::tr("Add %1 to group %2")
                .arg(QString::fromUtf8(obj->getName().c_str()))
                .arg(QString::fromUtf8(grp->getName().c_str())));
            //cmd->setNeedTreeReload(false);
            FWObject *new_state = cmd->getNewState();
            new_state->addRef(obj);
            m_project->undoStack->push(cmd);
            return obj;

        } else
        {
/* add a copy of the object to system group , or
 * add ruleset object to a firewall.
 */
            if (fwbdebug)
                qDebug("Copy object %s (%d) to a system group, "
                       "a ruleset to a firewall or an address to an interface",
                       obj->getName().c_str(), obj->getId());

            FWObject *nobj = m_project->db()->create(obj->getTypeName());
            assert(nobj!=NULL);
            //nobj->ref();
            nobj->duplicate(obj, true);

            // If we paste interface, reset the type of the copy
            // See #299
            if (Interface::isA(obj) && Interface::isA(ta))
            {
                Interface *new_intf = Interface::cast(nobj);
                new_intf->getOptionsObject()->setStr("type", "ethernet");
                // see #391 : need to reset "mamagement" flag in the copy
                // to make sure we do not end up with two management interfaces
                new_intf->setManagement(false);
            }

            FWCmdChange *cmd = new FWCmdAddObject(m_project, ta, nobj,
                QObject::tr("Add copy of %1 to %2")
                .arg(QString::fromUtf8(obj->getName().c_str()))
                .arg(QString::fromUtf8(ta->getName().c_str())));
            FWObject *new_state = cmd->getNewState();

            // adding object to new_state is reduntant but
            // FWCmdAddObject supports this for consistency
            new_state->add(nobj);
            m_project->undoStack->push(cmd);

            return nobj;
        }
    }
    catch(FWException &ex)
    {
        QMessageBox::warning(
            this,"Firewall Builder",
            ex.toString().c_str(),
            "&Continue", QString::null,QString::null,
            0, 1 );
    }

    return NULL;
}

void ObjectManipulator::lockObject()
{
    if (fwbdebug)
        qDebug() << "ObjectManipulator::lockObject selected:"
                 << getCurrentObjectTree()->getNumSelected();

    if (getCurrentObjectTree()->getNumSelected()==0) return;

    try
    {
        FWObject *obj;

        vector<FWObject*> so = getCurrentObjectTree()->getSimplifiedSelection();
        for (vector<FWObject*>::iterator i=so.begin();  i!=so.end(); ++i)
        {
            obj= *i;
            FWObject *lib = obj->getLibrary();
            // these lbraries are locked anyway, do not let the user
            // lock objects inside because they won't be able to unlock them.
            if (lib->getId()!=FWObjectDatabase::STANDARD_LIB_ID &&
                lib->getId()!=FWObjectDatabase::TEMPLATE_LIB_ID)
            {
                FWCmdChange* cmd = new FWCmdChange(
                    m_project, obj,
                    QString("lock %1").arg(QString::fromUtf8(obj->getName().c_str())));
                FWObject* new_state = cmd->getNewState();
                new_state->setReadOnly(true);
                if (!cmd->getOldState()->cmp(new_state)) m_project->undoStack->push(cmd);
            }
        }
        // getCurrentObjectTree()->setLockFlags();

        // Arguably, locking an object should not change lastModified timestamp
        // because none of the attributes that affect generated policy change.
        //QCoreApplication::postEvent(
        //    mw, new dataModifiedEvent(m_project->getFileName(), 0));
    } catch (FWException &ex)
    {
        qDebug() << ex.toString().c_str();
    }
}

void ObjectManipulator::unlockObject()
{
    if (fwbdebug)
        qDebug() << "ObjectManipulator::unlockObject selected:"
                 << getCurrentObjectTree()->getNumSelected();

    if (getCurrentObjectTree()->getNumSelected()==0) return;

    try
    {
        FWObject *obj;

        vector<FWObject*> so = getCurrentObjectTree()->getSimplifiedSelection();
        for (vector<FWObject*>::iterator i=so.begin();  i!=so.end(); ++i)
        {
            obj= *i;
            FWObject *lib = obj->getLibrary();
            if (lib->getId()!=FWObjectDatabase::STANDARD_LIB_ID &&
                lib->getId()!=FWObjectDatabase::TEMPLATE_LIB_ID)
            {
                FWCmdChange* cmd = new FWCmdChange(
                    m_project, obj,
                    QString("lock %1").arg(QString::fromUtf8(obj->getName().c_str())));
                FWObject* new_state = cmd->getNewState();
                new_state->setReadOnly(false);
                if (!cmd->getOldState()->cmp(new_state)) m_project->undoStack->push(cmd);
            }
        }
        //getCurrentObjectTree()->setLockFlags();
    } catch (FWException &ex)
    {
        qDebug() << ex.toString().c_str();
    }
}

void ObjectManipulator::deleteObject(FWObject *obj, bool openobj)
{
    bool firstAction = true ;

    if (fwbdebug)
        qDebug() << "ObjectManipulator::delObj"
                 << "obj=" << obj
                 << "name=" << obj->getName().c_str()
                 << "openobj=" << openobj;

    FWObject *object_library = obj->getLibrary();
    FWObject *parent = obj->getParent();
    FWObject *deleted_objects_lib = m_project->db()->findInIndex(
        FWObjectDatabase::DELETED_OBJECTS_ID );

    if (deleted_objects_lib == NULL)
    {
        FWObject *dobj = m_project->db()->createLibrary();
        dobj->setId(FWObjectDatabase::DELETED_OBJECTS_ID);
        dobj->setName("Deleted Objects");
        dobj->setReadOnly(false);
        m_project->db()->add(dobj);
        deleted_objects_lib = dobj;
    }

    if (object_library->getId() == FWObjectDatabase::STANDARD_LIB_ID ||
        object_library->getId() == FWObjectDatabase::TEMPLATE_LIB_ID)
        return;

    if (obj->getId() == FWObjectDatabase::STANDARD_LIB_ID ||
        obj->getId() == FWObjectDatabase::DELETED_OBJECTS_ID) return;
    
    bool is_library = Library::isA(obj);
    bool is_firewall = Firewall::cast(obj) != NULL; // includes Cluster too
    bool is_deleted_object = (deleted_objects_lib!=NULL && obj->isChildOf(deleted_objects_lib));
    // ruleset_visible == true if 1) we delete firewall object and one of its
    // rulesets is visible in the project panel, or 2) we delete ruleset object
    // which is visible in the project panel
    bool ruleset_visible = (
        (is_firewall && m_project->getCurrentRuleSet()->isChildOf(obj)) ||
        (m_project->getCurrentRuleSet() == obj));

    mw->findObjectWidget->reset();

    QCoreApplication::postEvent(
        mw, new closeObjectEvent(m_project->getFileName(), obj->getId()));
 
    try
    {    
        if (fwbdebug)
            qDebug() << "ObjectManipulator::delObj"
                     << "is_library=" << is_library
                     << "is_firewall= " << is_firewall
                     << "ruleset_visible=" << ruleset_visible
                     << "is_deleted_object="<< is_deleted_object;
        if (is_deleted_object)
        {
            if (QMessageBox::question(
                    m_project, "Firewall Builder",
                    QObject::tr("You are trying to delete objects in "
                                "the Deleted Objects library. This can "
                                "not be undone. Do you want to continue ?"),
                    QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok)
                return;

            obj->getParent()->remove(obj);
            QString filename = m_project->getFileName();
            QCoreApplication::postEvent(mw, new reloadObjectTreeEvent(filename));
            return;
        }

        // /* remove from our internal tables before it is removed from the
        //  * object tree so we could use obj->getId()
        //  */
        // if (is_library && !is_deleted_object)
        // {
        //     int idx = getIdxForLib(obj);
        //     QTreeWidget *otv = idxToTrees[idx];
        //     assert(otv!=NULL);
        //     m_objectManipulator->widgetStack->removeWidget( otv );
        //     removeLib(idx);
        // } else
        //     removeObjectFromTreeView(obj);

        if (is_library && obj->isReadOnly()) obj->setReadOnly(false);

        if (is_library) parent = m_project->db()->getFirstByType(Library::TYPENAME);

        set<FWObject*> reference_holders;
        set<FWObject*> res;
        m_project->db()->findWhereObjectIsUsed(obj, m_project->db(), res);

        foreach(FWObject* o, res)
        {
            if (FWReference::cast(o))
            {
                FWObject *holder = o->getParent();
                qDebug() << holder->getName().c_str()
                         << "(" << holder->getTypeName().c_str() << ")";
                reference_holders.insert(holder);
            }
        }
        
        FWCmdMoveObject *cmd = new FWCmdMoveObject(
            m_project,
            obj->getParent(),
            deleted_objects_lib,
            obj,
            reference_holders,
            QString("Delete object %1").arg(QString::fromUtf8(obj->getName().c_str())));
        m_project->undoStack->push(cmd);

        if (ruleset_visible) m_project->closeRuleSetPanel();
    }
    catch (FWException &ex)
    {
        if (fwbdebug)
            qDebug("ObjectManipulator::delObj: catch:  restoreOverrideCursor");
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(
            this,"Firewall Builder",
            ex.toString().c_str(),
            "&Continue", QString::null,QString::null,
            0, 1 );
        throw(ex);
    }

    if (fwbdebug) qDebug("ObjectManipulator::delObj  done");

    firstAction = false ;
}

void ObjectManipulator::groupObjects()
{
    if (getCurrentObjectTree()->getNumSelected()==0) return;

    FWObject *co = getCurrentObjectTree()->getSelectedObjects().front();

    newGroupDialog ngd(this, m_project->db());

    if (ngd.exec()==QDialog::Accepted)
    {
        QString objName = ngd.m_dialog->obj_name->text();
        QString libName = ngd.m_dialog->libs->currentText();

        QString type = ObjectGroup::TYPENAME;
        if (Service::cast(co)!=NULL)  type=ServiceGroup::TYPENAME;
        if (Interval::cast(co)!=NULL) type=IntervalGroup::TYPENAME;

        FWObject *parent = NULL;
        FWObject *newgrp = NULL;

        list<FWObject*> ll = m_project->db()->getByType( Library::TYPENAME );
        for (FWObject::iterator i=ll.begin(); i!=ll.end(); i++)
        {
            FWObject *lib=*i;
            if (libName==QString::fromUtf8(lib->getName().c_str()))
            {
/* TODO: need to show a dialog and say that chosen library is
 * read-only.  this is not critical though since newGroupDialog fills
 * the pull-down only with names of read-write libraries
 */
                if (lib->isReadOnly()) return;
                parent = m_project->getFWTree()->getStandardSlotForObject(lib,type);
                if (parent==NULL)
                {
                    if (fwbdebug)
                        qDebug("ObjectManipulator::groupObjects(): could not find standard slot for object of type %s in library %s",
                               type.toAscii().constData(),lib->getName().c_str());
                    return;
                }
                newgrp = m_project->db()->create(type.toStdString());
                newgrp->setName(string(objName.toUtf8().constData()));
                break;
            }
        }
        if (newgrp==NULL) return;

        FWCmdAddObject *cmd = new FWCmdAddObject(
            m_project, parent, newgrp,
            QObject::tr("Create new group %1").arg(objName));
        FWObject *new_state = cmd->getNewState();
        new_state->add(newgrp);

        vector<FWObject*> so = getCurrentObjectTree()->getSimplifiedSelection();
        for (vector<FWObject*>::iterator i=so.begin();  i!=so.end(); ++i)
            newgrp->addRef(*i);

        m_project->undoStack->push(cmd);
    }
}

