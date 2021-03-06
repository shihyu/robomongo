#include "robomongo/gui/widgets/explorer/ExplorerCollectionTreeItem.h"

#include <QAction>
#include <QMenu>

#include "robomongo/gui/widgets/explorer/AddEditIndexDialog.h"
#include "robomongo/gui/widgets/explorer/ExplorerCollectionIndexesDir.h"
#include "robomongo/gui/widgets/explorer/ExplorerCollectionIndexItem.h"
#include "robomongo/gui/widgets/explorer/ExplorerDatabaseTreeItem.h"
#include "robomongo/gui/dialogs/CreateDatabaseDialog.h"
#include "robomongo/gui/dialogs/CopyCollectionDialog.h"
#include "robomongo/gui/dialogs/DocumentTextEditor.h"
#include "robomongo/gui/GuiRegistry.h"
#include "robomongo/gui/utils/DialogUtils.h"

#include "robomongo/core/settings/ConnectionSettings.h"
#include "robomongo/core/domain/MongoCollection.h"
#include "robomongo/core/domain/MongoServer.h"
#include "robomongo/core/domain/App.h"
#include "robomongo/core/utils/QtUtils.h"
#include "robomongo/core/utils/Logger.h"
#include "robomongo/core/AppRegistry.h"
#include "robomongo/core/EventBus.h"

namespace
{
    const char *tooltipTemplate =
        "%s "
        "<table>"
        "<tr><td>Count:</td> <td><b>&nbsp;&nbsp;%lld</b></td></tr>"
        "<tr><td>Size:</td><td><b>&nbsp;&nbsp;%s</b></td></tr>"
        "</table>"
        ;
}

namespace Robomongo
{
    R_REGISTER_EVENT(CollectionIndexesLoadingEvent)      

    ExplorerCollectionTreeItem::ExplorerCollectionTreeItem(
        QTreeWidgetItem *parent, ExplorerDatabaseTreeItem *databaseItem, MongoCollection *collection) 
        : BaseClass(parent), _collection(collection), _databaseItem(databaseItem)
    {
        QAction *addDocument = new QAction("Insert Document...", this);
        VERIFY(connect(addDocument, SIGNAL(triggered()), SLOT(ui_addDocument())));

        QAction *updateDocument = new QAction("Update Documents...", this);
        VERIFY(connect(updateDocument, SIGNAL(triggered()), SLOT(ui_updateDocument())));
        QAction *removeDocument = new QAction("Remove Documents...", this);
        VERIFY(connect(removeDocument, SIGNAL(triggered()), SLOT(ui_removeDocument())));

        QAction *removeAllDocuments = new QAction("Remove All Documents...", this);
        VERIFY(connect(removeAllDocuments, SIGNAL(triggered()), SLOT(ui_removeAllDocuments())));

        QAction *collectionStats = new QAction("Statistics", this);
        VERIFY(connect(collectionStats, SIGNAL(triggered()), SLOT(ui_collectionStatistics())));

        QAction *storageSize = new QAction("Storage Size", this);
        VERIFY(connect(storageSize, SIGNAL(triggered()), SLOT(ui_storageSize())));

        QAction *totalIndexSize = new QAction("Total Index Size", this);
        VERIFY(connect(totalIndexSize, SIGNAL(triggered()), SLOT(ui_totalIndexSize())));

        QAction *totalSize = new QAction("Total Size", this);
        VERIFY(connect(totalSize, SIGNAL(triggered()), SLOT(ui_totalSize())));
        QAction *shardVersion = new QAction("Shard Version", this);
        VERIFY(connect(shardVersion, SIGNAL(triggered()), SLOT(ui_shardVersion())));

        QAction *shardDistribution = new QAction("Shard Distribution", this);
        VERIFY(connect(shardDistribution, SIGNAL(triggered()), SLOT(ui_shardDistribution())));

        QAction *dropCollection = new QAction("Drop Collection...", this);
        VERIFY(connect(dropCollection, SIGNAL(triggered()), SLOT(ui_dropCollection())));

        QAction *renameCollection = new QAction("Rename Collection...", this);
        VERIFY(connect(renameCollection, SIGNAL(triggered()), SLOT(ui_renameCollection())));
        QAction *duplicateCollection = new QAction("Duplicate Collection...", this);
        VERIFY(connect(duplicateCollection, SIGNAL(triggered()), SLOT(ui_duplicateCollection())));

        // Disabling for 0.8.5 release as this is currently a broken misfeature (see discussion on issue #398)
        // QAction *copyCollectionToDiffrentServer = new QAction("Copy Collection to Database...", this);
        // VERIFY(connect(copyCollectionToDiffrentServer, SIGNAL(triggered()), SLOT(ui_copyToCollectionToDiffrentServer())));

        QAction *viewCollection = new QAction("View Documents", this);
        VERIFY(connect(viewCollection, SIGNAL(triggered()), SLOT(ui_viewCollection())));

        BaseClass::_contextMenu->addAction(viewCollection);
        BaseClass::_contextMenu->addSeparator();
        BaseClass::_contextMenu->addAction(addDocument);
        BaseClass::_contextMenu->addAction(updateDocument);
        BaseClass::_contextMenu->addAction(removeDocument);
        BaseClass::_contextMenu->addAction(removeAllDocuments);
        BaseClass::_contextMenu->addSeparator();
        BaseClass::_contextMenu->addAction(renameCollection);
        BaseClass::_contextMenu->addAction(duplicateCollection);
        // Disabling for 0.8.5 release as this is currently a broken misfeature (see discussion on issue #398)
        // BaseClass::_contextMenu->addAction(copyCollectionToDiffrentServer);
        BaseClass::_contextMenu->addAction(dropCollection);
        BaseClass::_contextMenu->addSeparator();
        BaseClass::_contextMenu->addAction(collectionStats);
        BaseClass::_contextMenu->addSeparator();
        BaseClass::_contextMenu->addAction(shardVersion);
        BaseClass::_contextMenu->addAction(shardDistribution);

        AppRegistry::instance().bus()->subscribe(_databaseItem, LoadCollectionIndexesResponse::Type, this);
        AppRegistry::instance().bus()->subscribe(_databaseItem, AddEditIndexResponse::Type, this);
        AppRegistry::instance().bus()->subscribe(_databaseItem, DropCollectionIndexResponse::Type, this);
        AppRegistry::instance().bus()->subscribe(this, CollectionIndexesLoadingEvent::Type, this);

        setText(0, QtUtils::toQString(_collection->name()));
        setIcon(0, GuiRegistry::instance().collectionIcon());

        _indexDir = new ExplorerCollectionIndexesDir(this);
        addChild(_indexDir);

        setExpanded(false);
        setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    }

    void ExplorerCollectionTreeItem::handle(LoadCollectionIndexesResponse *event)
    {
        if (event->isError()) {
            _indexDir->setText(0, "Indexes");
            _indexDir->setExpanded(false);
            QtUtils::clearChildItems(_indexDir);

            std::stringstream ss;
            ss << "Cannot load list of indexes.\n\nError:\n" << event->error().errorMessage();

            QMessageBox::information(NULL, "Error", QtUtils::toQString(ss.str()));
            return;
        }

        QtUtils::clearChildItems(_indexDir);
        const std::vector<IndexInfo> &indexes = event->indexes();

        // Do not expand, when we do not have functions
        if (indexes.size() == 0)
            _indexDir->setExpanded(false);

        for (std::vector<IndexInfo>::const_iterator it = indexes.begin(); it != indexes.end(); ++it) {
            _indexDir->addChild(new ExplorerCollectionIndexItem(_indexDir, *it));
        }
        _indexDir->setText(0, detail::buildName("Indexes", _indexDir->childCount()));
    }

    void ExplorerCollectionTreeItem::handle(AddEditIndexResponse *event)
    {
        bool const isAddIndex{ event->oldIndex_._name.empty() };
        QString const action{ isAddIndex ? "add" : "edit" };
        auto const index{ QString::fromStdString(
           isAddIndex ? event->newIndex_._name : event->oldIndex_._name
        )};
        if (event->isError()) {
            QString const header{ "Operation failed" };
            QString const msg{ "Failed to " + action + " index \"" + index + '\"'};
            auto const err{ QString::fromStdString(event->error().errorMessage()) };
            LOG_MSG((msg + ". " + err).toStdString(), mongo::logger::LogSeverity::Error());
            QMessageBox::critical(nullptr, "Error: " + header, msg + "\n\n" + err);
            return;
        }
        LOG_MSG(("Succeeded to " + action + " index \"" + index + '\"').toStdString(), 
            mongo::logger::LogSeverity::Info());
    }

    void ExplorerCollectionTreeItem::handle(DropCollectionIndexResponse *event)
    {
        if (event->isError()) {
            QString const header{"Operation failed"};
            QString const msg{ 
                "Failed to drop index \"" + QString::fromStdString(event->index()) + '\"' 
            };
            auto const err{ "Reason: " + QString::fromStdString(event->error().errorMessage()) };
            LOG_MSG((msg + ". " + err).toStdString(), mongo::logger::LogSeverity::Error());
            QMessageBox::critical(nullptr, "Error: " + header, msg + "\n\n" + err);
            return;
        }

        for (int i = 0; i < _indexDir->childCount(); ++i) {
            QTreeWidgetItem *item = _indexDir->child(i);
            if (item->text(0) == QString::fromStdString(event->index())) {
                removeChild(item);
                delete item;
                break;
            }
        }        

        LOG_MSG("Succeeded to drop index \"" + event->index() + '\"', 
            mongo::logger::LogSeverity::Info());

        _indexDir->setText(0, detail::buildName("Indexes", _indexDir->childCount()));
    }

    void ExplorerCollectionTreeItem::handle(CollectionIndexesLoadingEvent *event)
    {
        _indexDir->setText(0, detail::buildName("Indexes", -1));
    }

    void ExplorerCollectionTreeItem::expand()
    {
         AppRegistry::instance().bus()->publish(new CollectionIndexesLoadingEvent(this));
         if (_databaseItem) {
             _databaseItem->expandColection(this);
         }
    }

    void ExplorerCollectionTreeItem::dropIndex(const QTreeWidgetItem * const ind)
    {
        if (!_databaseItem)
            return;

        _databaseItem->dropIndexFromCollection(this, QtUtils::toStdString(ind->text(0)));
    }

    QString ExplorerCollectionTreeItem::buildToolTip(MongoCollection *collection)
    {
        // This function does not used now
        char buff[2048] = {0};
//        sprintf(buff,tooltipTemplate,collection->name().c_str(),collection->info().count(),collection->sizeString().c_str());
        return buff;
    }

    void ExplorerCollectionTreeItem::ui_addDocument()
    {
        MongoDatabase *database = _collection->database();
        MongoServer *server = database->server();
        ConnectionSettings *settings = server->connectionRecord();

        DocumentTextEditor editor(CollectionInfo(settings->getFullAddress(), database->name(), _collection->name()), "{\n    \n}");

        editor.setCursorPosition(1, 4);
        editor.setWindowTitle("Insert Document");
        int result = editor.exec();

        treeWidget()->activateWindow();

        if (result == QDialog::Accepted) {
            server->insertDocuments(editor.bsonObj(), MongoNamespace(database->name(), _collection->name()) );
        }
    }

    void ExplorerCollectionTreeItem::ui_removeDocument()
    {
        openCurrentCollectionShell(
            "remove({ '' : '' });"
            , false, CursorPosition(0, -10));
    }

    void ExplorerCollectionTreeItem::ui_removeAllDocuments()
    {
        MongoDatabase *database = _collection->database();
        // Ask user
        int answer = QMessageBox::question(treeWidget(),
            "Remove All Documents",
            QString("Remove all documents from <b>%1</b> collection?").arg(QtUtils::toQString(_collection->name())),
            QMessageBox::Yes, QMessageBox::No, QMessageBox::NoButton);

        if (answer == QMessageBox::Yes) {
            MongoServer *server = database->server();
            mongo::BSONObjBuilder builder;
            mongo::BSONObj bsonQuery = builder.obj();
            mongo::Query query(bsonQuery);
            server->removeDocuments(query, MongoNamespace(database->name(), _collection->name()), 
                                    RemoveDocumentCount::ALL);
        }
    }

    void ExplorerCollectionTreeItem::ui_updateDocument()
    {
        openCurrentCollectionShell(
            "update(\n"
            "    // query \n"
            "    {\n"
            "        \"key\" : \"value\"\n"
            "    },\n"
            "    \n"
            "    // update \n"
            "    {\n"
            "    },\n"
            "    \n"
            "    // options \n"
            "    {\n"
            "        \"multi\" : false,  // update only one document \n"
            "        \"upsert\" : false  // insert a new document, if no existing document match the query \n"
            "    }\n"
            ");", false);
    }

    void ExplorerCollectionTreeItem::ui_collectionStatistics()
    {
        openCurrentCollectionShell("stats()");
    }

    void ExplorerCollectionTreeItem::ui_dropCollection()
    {
        // Ask user
        int answer = utils::questionDialog(treeWidget(), "Drop", "collection", QtUtils::toQString(_collection->name()));

        if (answer == QMessageBox::Yes) {
            MongoDatabase *database = _collection->database();
            database->dropCollection(_collection->name());
        }
    }

    void ExplorerCollectionTreeItem::ui_duplicateCollection()
    {
        MongoDatabase *database = _collection->database();
        MongoServer *server = database->server();
        ConnectionSettings *settings = server->connectionRecord();

        CreateDatabaseDialog dlg(QtUtils::toQString(settings->getFullAddress()),
            QtUtils::toQString(database->name()),
            QtUtils::toQString(_collection->name()), treeWidget());
        dlg.setWindowTitle("Duplicate Collection");
        dlg.setOkButtonText("&Duplicate");
        dlg.setInputLabelText("New Collection Name:");
        dlg.setInputText(QtUtils::toQString(_collection->name() + "_copy"));
        int result = dlg.exec();

        if (result == QDialog::Accepted) {
            database->duplicateCollection(_collection->name(), QtUtils::toStdString(dlg.databaseName()));
        }
    }

    void ExplorerCollectionTreeItem::ui_copyToCollectionToDiffrentServer()
    {
        MongoDatabase *databaseFrom = _collection->database();
        MongoServer *server = databaseFrom->server();
        ConnectionSettings *settings = server->connectionRecord();

        CopyCollection dlg(QtUtils::toQString(settings->getFullAddress()), QtUtils::toQString(databaseFrom->name()), QtUtils::toQString(_collection->name()) );
        int result = dlg.exec();

        if (result == QDialog::Accepted) {
            MongoDatabase *databaseTo = dlg.selectedDatabase();
            databaseTo->copyCollection(server, databaseFrom->name(), _collection->name());
            databaseTo->loadCollections();
        }
    }

    void ExplorerCollectionTreeItem::ui_renameCollection()
    {
        MongoDatabase *database = _collection->database();
        MongoServer *server = database->server();
        ConnectionSettings *connSettings = server->connectionRecord();

        CreateDatabaseDialog dlg(QtUtils::toQString(connSettings->getFullAddress()),
            QtUtils::toQString(database->name()),
            QtUtils::toQString(_collection->name()), treeWidget());
        dlg.setWindowTitle("Rename Collection");
        dlg.setOkButtonText("&Rename");
        dlg.setInputLabelText("New Collection Name:");
        dlg.setInputText(QtUtils::toQString(_collection->name()));
        int result = dlg.exec();

        if (result == QDialog::Accepted) {
            database->renameCollection(_collection->name(), QtUtils::toStdString(dlg.databaseName()));
        }
    }

    void ExplorerCollectionTreeItem::ui_viewCollection()
    {
        CursorPosition cp(0, -2);
        openCurrentCollectionShell("find({})", true, cp);
    }

    void ExplorerCollectionTreeItem::ui_storageSize()
    {
        openCurrentCollectionShell("storageSize()");
    }

    void ExplorerCollectionTreeItem::ui_totalIndexSize()
    {
        openCurrentCollectionShell("totalIndexSize()");
    }

    void ExplorerCollectionTreeItem::ui_totalSize()
    {
        openCurrentCollectionShell("totalSize()");
    }

    void ExplorerCollectionTreeItem::ui_shardVersion()
    {
        openCurrentCollectionShell("getShardVersion()");
    }

    void ExplorerCollectionTreeItem::ui_shardDistribution()
    {
        openCurrentCollectionShell("getShardDistribution()");
    }

    void ExplorerCollectionTreeItem::openCurrentCollectionShell(const QString &script, bool execute, const CursorPosition &cursor)
    {
        QString query = detail::buildCollectionQuery(_collection->name(), script);
        AppRegistry::instance().app()->openShell(_collection->database(), query, execute, QtUtils::toQString(_collection->name()), cursor);
    }
}
