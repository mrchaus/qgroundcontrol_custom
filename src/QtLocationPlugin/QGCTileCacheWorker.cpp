/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


/**
 * @file
 *   @brief Map Tile Cache Worker Thread
 *
 *   @author Gus Grubba <gus@auterion.com>
 *
 */

#include "QGCTileCacheWorker.h"
#include "QGCTileCacheDatabase.h"
#include "QGCCachedTileSet.h"
#include "QGCMapTasks.h"
#include "QGCMapUrlEngine.h"
#include "QGCLoggingCategory.h"

#include <QtCore/QDateTime>
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QSettings>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

QByteArray QGCCacheWorker::_bingNoTileImage;

QGC_LOGGING_CATEGORY(QGCTileCacheWorkerLog, "qgc.qtlocationplugin.qgctilecacheworker")

QGCCacheWorker::QGCCacheWorker(QObject *parent)
    : QThread(parent)
    , _db(std::make_shared<QSqlDatabase>(QSqlDatabase::addDatabase("QSQLITE", kSession)))
    , _setTilesTable(new SetTilesTableModel(this, _db))
    , _tileSetsTable(new TileSetsTableModel(this, _db))
    , _tilesTable(new TilesTableModel(this, _db))
    , _tilesDownloadTable(new TilesDownloadTableModel(this, _db))
{
    // qCDebug(QGCTileCacheWorkerLog) << Q_FUNC_INFO << this;

    _db->setDatabaseName(_databasePath);
    _db->setConnectOptions(QStringLiteral("QSQLITE_ENABLE_SHARED_CACHE"));
}

QGCCacheWorker::~QGCCacheWorker()
{
    stop();

    // qCDebug(QGCTileCacheWorkerLog) << Q_FUNC_INFO << this;
}

void QGCCacheWorker::stop()
{
    // TODO: Send Stop Task Instead?
    QMutexLocker lock(&_taskQueueMutex);
    qDeleteAll(_taskQueue);
    lock.unlock();

    if (isRunning()) {
        _waitc.wakeAll();
    }
}

bool QGCCacheWorker::enqueueTask(QGCMapTask *task)
{
    if (!_valid && (task->type() != QGCMapTask::taskInit)) {
        task->setError(tr("Database Not Initialized"));
        task->deleteLater();
        return false;
    }

    // TODO: Prepend Stop Task Instead?
    QMutexLocker lock(&_taskQueueMutex);
    _taskQueue.enqueue(task);
    lock.unlock();

    if (isRunning()) {
        _waitc.wakeAll();
    } else {
        start(QThread::HighPriority);
    }

    return true;
}

void QGCCacheWorker::run()
{
    if (!_valid && !_failed) {
        if (!_init()) {
            qCWarning(QGCTileCacheWorkerLog) << Q_FUNC_INFO << "Failed To Init Database";
            return;
        }
    }

    if (_valid) {
        if (_connectDB()) {
            _deleteBingNoTileTiles();
        }
    }

    QMutexLocker lock(&_taskQueueMutex);
    while (true) {
        if (!_taskQueue.isEmpty()) {
            QGCMapTask* const task = _taskQueue.dequeue();
            lock.unlock();
            _runTask(task);
            lock.relock();
            task->deleteLater();

            const qsizetype count = _taskQueue.count();
            if (count > 100) {
                _updateTimeout = kLongTimeout;
            } else if (count < 25) {
                _updateTimeout = kShortTimeout;
            }

            if ((count == 0) || _updateTimer.hasExpired(_updateTimeout)) {
                if (_valid) {
                    lock.unlock();
                    _updateTotals();
                    lock.relock();
                }
            }
        } else {
            (void) _waitc.wait(lock.mutex(), 5000);
            if (_taskQueue.isEmpty()) {
                break;
            }
        }
    }
    lock.unlock();

    for (const QString &connection: QSqlDatabase::connectionNames()) {
        QSqlDatabase db = QSqlDatabase::database(connection, false);
        if (db.isOpen()) {
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);
    }
}

void QGCCacheWorker::_runTask(QGCMapTask *task)
{
    switch (task->type()) {
    case QGCMapTask::taskInit:
        break;
    case QGCMapTask::taskCacheTile:
        _saveTile(task);
        break;
    case QGCMapTask::taskFetchTile:
        _getTile(task);
        break;
    case QGCMapTask::taskFetchTileSets:
        _getTileSets(task);
        break;
    case QGCMapTask::taskCreateTileSet:
        _createTileSet(task);
        break;
    case QGCMapTask::taskGetTileDownloadList:
        _getTileDownloadList(task);
        break;
    case QGCMapTask::taskUpdateTileDownloadState:
        _updateTileDownloadState(task);
        break;
    case QGCMapTask::taskDeleteTileSet:
        _deleteTileSet(task);
        break;
    case QGCMapTask::taskRenameTileSet:
        _renameTileSet(task);
        break;
    case QGCMapTask::taskPruneCache:
        _pruneCache(task);
        break;
    case QGCMapTask::taskReset:
        _resetCacheDatabase(task);
        break;
    case QGCMapTask::taskExport:
        _exportSets(task);
        break;
    case QGCMapTask::taskImport:
        _importSets(task);
        break;
    default:
        qCWarning(QGCTileCacheWorkerLog) << Q_FUNC_INFO << "given unhandled task type" << task->type();
        break;
    }
}

void QGCCacheWorker::_deleteBingNoTileTiles()
{
    (void) _tilesTable->deleteBingNoTileImageTiles();
}

bool QGCCacheWorker::_findTileSetID(const QString &name, quint64 &setID)
{
    return _tileSetsTable->getTileSetID(setID, name);
}

quint64 QGCCacheWorker::_getDefaultTileSet()
{
    quint64 setID;
    (void) _tileSetsTable->getDefaultTileSet(setID);
    return setID;
}

void QGCCacheWorker::_saveTile(QGCMapTask *mtask)
{
    if (!_testTask(mtask)) {
        return;
    }

    if (!_valid) {
        qCWarning(QGCTileCacheWorkerLog) << "Map Cache SQL error (saveTile() open db): Not Valid";
        return;
    }

    QGCSaveTileTask* const task = static_cast<QGCSaveTileTask*>(mtask);

    quint64 tileID = UINT64_MAX;
    if (!_tilesTable->insertTile(tileID, *task->tile())) {
        qCWarning(QGCTileCacheWorkerLog) << "Map Cache SQL error (insert into Tiles)";
        return;
    }

    const quint64 setID = task->tile()->tileSet() == UINT64_MAX ? _getDefaultTileSet() : task->tile()->tileSet();
    if (!_setTilesTable->insertSetTiles(setID, tileID)) {
        qCWarning(QGCTileCacheWorkerLog) << "Map Cache SQL error (add tile into SetTiles)";
    }
}

void QGCCacheWorker::_getTile(QGCMapTask *mtask)
{
    if (!_testTask(mtask)) {
        return;
    }

    QGCFetchTileTask* const task = static_cast<QGCFetchTileTask*>(mtask);

    QGCCacheTile *tile = nullptr;
    if (!_tilesTable->getTile(tile, task->hash())) {
        qCDebug(QGCTileCacheWorkerLog) << Q_FUNC_INFO << "(NOT in DB) HASH:" << task->hash();
        task->setError(tr("Tile not in cache database"));
        return;
    }

    task->setTileFetched(tile);
    qCDebug(QGCTileCacheWorkerLog) << Q_FUNC_INFO << "(Found in DB) HASH:" << task->hash();
}

void QGCCacheWorker::_getTileSets(QGCMapTask *mtask)
{
    if (!_testTask(mtask)) {
        return;
    }

    QGCFetchTileSetTask* const task = static_cast<QGCFetchTileSetTask*>(mtask);

    QList<QGCCachedTileSet*> tileSets;
    if (!_tileSetsTable->getTileSets(tileSets)) {
        qCDebug(QGCTileCacheWorkerLog) << Q_FUNC_INFO << "No tile set in database";
        task->setError(tr("No tile set in database"));
        return;
    }

    for (QGCCachedTileSet* tileSet : tileSets) {
        _updateSetTotals(tileSet);
        tileSet->moveToThread(QCoreApplication::instance()->thread());
        task->setTileSetFetched(tileSet);
    }
}

void QGCCacheWorker::_updateSetTotals(QGCCachedTileSet *set)
{
    if (set->defaultSet()) {
        _updateTotals();
        set->setSavedTileCount(_totalCount);
        set->setSavedTileSize(_totalSize);
        set->setTotalTileCount(_defaultCount);
        set->setTotalTileSize(_defaultSize);
        return;
    }

    (void) _tilesTable->updateSetTotals(*set, set->id());
}

void QGCCacheWorker::_updateTotals()
{
    (void) _tilesTable->updateTotals(_totalCount, _totalSize, _defaultCount, _defaultSize, _getDefaultTileSet());

    emit updateTotals(_totalCount, _totalSize, _defaultCount, _defaultSize);
    if (!_updateTimer.isValid()) {
        _updateTimer.start();
    } else {
        (void) _updateTimer.restart();
    }
}

quint64 QGCCacheWorker::_findTile(const QString &hash)
{
    quint64 tileID = 0;
    (void) _tilesTable->getTileID(tileID, hash);
    return tileID;
}

void QGCCacheWorker::_createTileSet(QGCMapTask *mtask)
{
    QGCCreateTileSetTask* const task = static_cast<QGCCreateTileSetTask*>(mtask);

    if (!_valid) {
        task->setError(tr("Error saving tile set"));
        return;
    }

    QGCCachedTileSet* const tileSet = task->tileSet();

    if (!_tileSetsTable->insertTileSet(*tileSet)) {
        task->setError(tr("Error saving tile set"));
        return;
    }

    if (!_tilesDownloadTable->insertTilesDownload(tileSet)) {
        mtask->setError(tr("Error creating tile set download list"));
        return;
    }

    _updateSetTotals(tileSet);
    task->setTileSetSaved();
}

void QGCCacheWorker::_getTileDownloadList(QGCMapTask *mtask)
{
    if (!_testTask(mtask)) {
        return;
    }

    QGCGetTileDownloadListTask* const task = static_cast<QGCGetTileDownloadListTask*>(mtask);

    QQueue<QGCTile*> tiles;

    (void) _tilesDownloadTable->getTileDownloadList(tiles, task->setID(), task->count());

    task->setTileListFetched(tiles);
}

void QGCCacheWorker::_updateTileDownloadState(QGCMapTask *mtask)
{
    if (!_testTask(mtask)) {
        return;
    }

    QGCUpdateTileDownloadStateTask* const task = static_cast<QGCUpdateTileDownloadStateTask*>(mtask);

    (void) _tilesDownloadTable->updateTilesDownloadSet(task->state(), task->setID(), task->hash());
}

void QGCCacheWorker::_pruneCache(QGCMapTask *mtask)
{
    if (!_testTask(mtask)) {
        return;
    }

    QGCPruneCacheTask* const task = static_cast<QGCPruneCacheTask*>(mtask);

    if (!_tilesTable->prune(_getDefaultTileSet(), static_cast<qint64>(task->amount()))) {
        return;
    }

    task->setPruned();
}

void QGCCacheWorker::_deleteTileSet(QGCMapTask *mtask)
{
    if (!_testTask(mtask)) {
        return;
    }

    QGCDeleteTileSetTask* const task = static_cast<QGCDeleteTileSetTask*>(mtask);
    _deleteTileSet(task->setID());
    task->setTileSetDeleted();
}

void QGCCacheWorker::_deleteTileSet(quint64 id)
{
    (void) _tilesTable->deleteTileSet(id);

    _updateTotals();
}

void QGCCacheWorker::_renameTileSet(QGCMapTask *mtask)
{
    if (!_testTask(mtask)) {
        return;
    }

    QGCRenameTileSetTask* const task = static_cast<QGCRenameTileSetTask*>(mtask);

    if (!_tileSetsTable->setName(task->setID(), task->newName())) {
        task->setError(tr("Error renaming tile set"));
    }
}


void QGCCacheWorker::_resetCacheDatabase(QGCMapTask *mtask)
{
    if (!_testTask(mtask)) {
        return;
    }

    QGCResetTask* const task = static_cast<QGCResetTask*>(mtask);

    (void) _tilesTable->drop();
    (void) _tileSetsTable->drop();
    (void) _setTilesTable->drop();
    (void) _tilesDownloadTable->drop();

    _valid = _createDB(*_db, true);

    task->setResetCompleted();
}

void QGCCacheWorker::_importSets(QGCMapTask *mtask)
{
    if (!_testTask(mtask)) {
        return;
    }

    QGCImportTileTask* const task = static_cast<QGCImportTileTask*>(mtask);

    QSqlDatabase db = QSqlDatabase::database(kSession);

    if (task->replace()) {
        db.close();
        QSqlDatabase::removeDatabase(kSession);
        QFile file(_databasePath);
        (void) file.remove();
        (void) QFile::copy(task->path(), _databasePath);
        task->setProgress(25);
        if (!_init()) {
            qCWarning(QGCTileCacheWorkerLog) << Q_FUNC_INFO << "Failed To Init Database";
        }
        if (_valid) {
            task->setProgress(50);
            (void) _connectDB();
        }
        task->setProgress(100);
        task->setImportCompleted();
        return;
    }

    QSqlDatabase dbImport = QSqlDatabase::addDatabase("QSQLITE", QStringLiteral("QGeoTileImportSession"));
    dbImport.setDatabaseName(task->path());
    dbImport.setConnectOptions(QStringLiteral("QSQLITE_ENABLE_SHARED_CACHE"));
    if (dbImport.open()) {
        quint64 tileCount = 0;
        QSqlQuery query(dbImport);
        query.prepare("SELECT COUNT(tileID) FROM Tiles");
        if (query.exec() && query.next()) {
            tileCount = query.value(0).toULongLong();
        }

        if (tileCount) {
            query.prepare("SELECT * FROM TileSets ORDER BY defaultSet DESC, name ASC");
            if (query.exec()) {
                quint64 currentCount = 0;
                int lastProgress = -1;
                while (query.next()) {
                    QString name = query.value("name").toString();
                    const quint64 setID = query.value("setID").toULongLong();
                    const QString mapType = query.value("typeStr").toString();
                    const double topleftLat = query.value("topleftLat").toDouble();
                    const double topleftLon = query.value("topleftLon").toDouble();
                    const double bottomRightLat = query.value("bottomRightLat").toDouble();
                    const double bottomRightLon = query.value("bottomRightLon").toDouble();
                    const int minZoom = query.value("minZoom").toInt();
                    const int maxZoom = query.value("maxZoom").toInt();
                    const int type = query.value("type").toInt();
                    const quint32 numTiles = query.value("numTiles").toUInt();
                    const int defaultSet = query.value("defaultSet").toInt();
                    quint64 insertSetID = _getDefaultTileSet();
                    if (defaultSet == 0) {
                        if (_findTileSetID(name, insertSetID)) {
                            int testCount = 0;
                            while (true) {
                                const QString testName = QString::asprintf("%s %02d", name.toLatin1().data(), ++testCount);
                                if (!_findTileSetID(testName, insertSetID) || (testCount > 99)) {
                                    name = testName;
                                    break;
                                }
                            }
                        }

                        // _tileSetsTable->insertTileSet();

                        QSqlQuery cQuery(db);
                        (void) cQuery.prepare("INSERT INTO TileSets("
                            "name, typeStr, topleftLat, topleftLon, bottomRightLat, bottomRightLon, minZoom, maxZoom, type, numTiles, defaultSet, date"
                            ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
                        );
                        cQuery.addBindValue(name);
                        cQuery.addBindValue(mapType);
                        cQuery.addBindValue(topleftLat);
                        cQuery.addBindValue(topleftLon);
                        cQuery.addBindValue(bottomRightLat);
                        cQuery.addBindValue(bottomRightLon);
                        cQuery.addBindValue(minZoom);
                        cQuery.addBindValue(maxZoom);
                        cQuery.addBindValue(type);
                        cQuery.addBindValue(numTiles);
                        cQuery.addBindValue(defaultSet);
                        cQuery.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());
                        if (!cQuery.exec()) {
                            task->setError(tr("Error adding imported tile set to database"));
                            break;
                        } else {
                            insertSetID = cQuery.lastInsertId().toULongLong();
                        }
                    }

                    QSqlQuery cQuery(db);
                    QSqlQuery subQuery(dbImport);
                    subQuery.prepare("SELECT * FROM Tiles WHERE tileID IN (SELECT A.tileID FROM SetTiles A JOIN SetTiles B ON A.tileID = B.tileID WHERE B.setID = ? GROUP BY A.tileID HAVING COUNT(A.tileID) = 1)");
                    subQuery.addBindValue(setID);
                    if (subQuery.exec()) {
                        quint64 tilesFound = 0;
                        quint64 tilesSaved = 0;
                        (void) db.transaction();
                        while (subQuery.next()) {
                            tilesFound++;
                            const QString hash = subQuery.value("hash").toString();
                            const QString format = subQuery.value("format").toString();
                            const QByteArray img = subQuery.value("tile").toByteArray();
                            const int type = subQuery.value("type").toInt();
                            (void) cQuery.prepare("INSERT INTO Tiles(hash, format, tile, size, type, date) VALUES(?, ?, ?, ?, ?, ?)");
                            cQuery.addBindValue(hash);
                            cQuery.addBindValue(format);
                            cQuery.addBindValue(img);
                            cQuery.addBindValue(img.size());
                            cQuery.addBindValue(type);
                            cQuery.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());
                            if (cQuery.exec()) {
                                tilesSaved++;
                                const quint64 importTileID = cQuery.lastInsertId().toULongLong();
                                cQuery.prepare("INSERT INTO SetTiles(tileID, setID) VALUES(?, ?)");
                                cQuery.addBindValue(importTileID);
                                cQuery.addBindValue(insertSetID);
                                (void) cQuery.exec();
                                currentCount++;
                                if (tileCount) {
                                    const int progress = static_cast<int>((static_cast<double>(currentCount) / static_cast<double>(tileCount)) * 100.0);
                                    if (lastProgress != progress) {
                                        lastProgress = progress;
                                        task->setProgress(progress);
                                    }
                                }
                            }
                        }

                        (void) db.commit();

                        quint64 count;
                        if (!_tilesTable->getSetTotal(count, insertSetID)) {
                            break;
                        }

                        if (!_tileSetsTable->setNumTiles(insertSetID, count)) {
                            break;
                        }

                        const qint64 uniqueTiles = tilesFound - tilesSaved;
                        if (static_cast<quint64>(uniqueTiles) < tileCount) {
                            tileCount -= uniqueTiles;
                        } else {
                            tileCount = 0;
                        }

                        if (!tilesSaved && !defaultSet) {
                            _deleteTileSet(insertSetID);
                            qCDebug(QGCTileCacheWorkerLog) << "No unique tiles in" << name << "Removing it.";
                        }
                    }
                }
            } else {
                task->setError(tr("No tile set in database"));
            }
        }

        if (!tileCount) {
            task->setError(tr("No unique tiles in imported database"));
        }
    } else {
        task->setError(tr("Error opening import database"));
    }

    dbImport.close();
    QSqlDatabase::removeDatabase(dbImport.connectionName());

    task->setImportCompleted();
}

void QGCCacheWorker::_exportSets(QGCMapTask *mtask)
{
    if (!_testTask(mtask)) {
        return;
    }

    QGCExportTileTask* const task = static_cast<QGCExportTileTask*>(mtask);

    QSqlDatabase db = QSqlDatabase::database(kSession);

    QFile file(task->path());
    (void) file.remove();

    QSqlDatabase dbExport = QSqlDatabase::addDatabase("QSQLITE", QStringLiteral("QGeoTileExportSession"));
    dbExport.setDatabaseName(task->path());
    dbExport.setConnectOptions(QStringLiteral("QSQLITE_ENABLE_SHARED_CACHE"));
    if (dbExport.open()) {
        if (_createDB(dbExport, false)) {
            quint64 tileCount = 0;
            quint64 currentCount = 0;
            for (qsizetype i = 0; i < task->sets().count(); i++) {
                const QGCCachedTileSet* const set = task->sets().at(i);
                if (set->defaultSet()) {
                    tileCount += set->totalTileCount();
                } else {
                    tileCount += set->uniqueTileCount();
                }
            }

            if (tileCount == 0) {
                tileCount = 1;
            }

            for (qsizetype i = 0; i < task->sets().count(); i++) {
                const QGCCachedTileSet* const set = task->sets().at(i);

                QSqlQuery exportQuery(dbExport);
                (void) exportQuery.prepare("INSERT INTO TileSets("
                    "name, typeStr, topleftLat, topleftLon, bottomRightLat, bottomRightLon, minZoom, maxZoom, type, numTiles, defaultSet, date"
                    ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
                );
                exportQuery.addBindValue(set->name());
                exportQuery.addBindValue(set->mapTypeStr());
                exportQuery.addBindValue(set->topleftLat());
                exportQuery.addBindValue(set->topleftLon());
                exportQuery.addBindValue(set->bottomRightLat());
                exportQuery.addBindValue(set->bottomRightLon());
                exportQuery.addBindValue(set->minZoom());
                exportQuery.addBindValue(set->maxZoom());
                exportQuery.addBindValue(UrlFactory::getQtMapIdFromProviderType(set->type()));
                exportQuery.addBindValue(set->totalTileCount());
                exportQuery.addBindValue(set->defaultSet());
                exportQuery.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());
                if (!exportQuery.exec()) {
                    task->setError(tr("Error adding tile set to exported database"));
                    break;
                }

                const quint64 exportSetID = exportQuery.lastInsertId().toULongLong();

                QSqlQuery query(db);
                query.prepare("SELECT * FROM SetTiles WHERE setID = ?");
                query.addBindValue(set->id());
                if (query.exec()) { // _setTilesTable->selectFromSetID(set->id())
                    dbExport.transaction();
                    while (query.next()) {
                        const quint64 tileID = query.value("tileID").toULongLong();
                        QSqlQuery subQuery(db);
                        subQuery.prepare("SELECT * FROM Tiles WHERE tileID = ?");
                        subQuery.addBindValue(tileID);
                        if (subQuery.exec() && subQuery.next()) {
                            const QString hash = subQuery.value("hash").toString();
                            const QString format = subQuery.value("format").toString();
                            const QByteArray img = subQuery.value("tile").toByteArray();
                            const int type = subQuery.value("type").toInt();

                            (void) exportQuery.prepare("INSERT INTO Tiles(hash, format, tile, size, type, date) VALUES(?, ?, ?, ?, ?, ?)");
                            exportQuery.addBindValue(hash);
                            exportQuery.addBindValue(format);
                            exportQuery.addBindValue(img);
                            exportQuery.addBindValue(img.size());
                            exportQuery.addBindValue(type);
                            exportQuery.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());
                            if (exportQuery.exec()) { // _tileTable->insertTile(QGCCacheTile(hash, img, format, type))
                                const quint64 exportTileID = exportQuery.lastInsertId().toULongLong();
                                // (void) _setTilesTable->insertSetTiles(exportSetID, exportTileID);
                                exportQuery.prepare("INSERT INTO SetTiles(tileID, setID) VALUES(?, ?)");
                                exportQuery.addBindValue(exportTileID);
                                exportQuery.addBindValue(exportSetID);
                                (void) exportQuery.exec();
                                currentCount++;
                                task->setProgress(static_cast<int>(static_cast<double>(currentCount) / static_cast<double>(tileCount) * 100.0));
                            }
                        }
                    }
                    (void) dbExport.commit();
                }
            }
        } else {
            task->setError(tr("Error creating export database"));
        }
    } else {
        qCCritical(QGCTileCacheWorkerLog) << "Map Cache SQL error (create export database):" << dbExport.lastError();
        task->setError(tr("Error opening export database"));
    }

    dbExport.close();
    QSqlDatabase::removeDatabase(dbExport.connectionName());

    task->setExportCompleted();
}

bool QGCCacheWorker::_testTask(QGCMapTask *mtask)
{
    if (!_valid) {
        mtask->setError(tr("No Cache Database"));
        return false;
    }

    return true;
}

bool QGCCacheWorker::_init()
{
    _failed = false;

    if (_databasePath.isEmpty()) {
        qCCritical(QGCTileCacheWorkerLog) << "Could not find suitable cache directory.";
        _failed = true;
        return _failed;
    }

    qCDebug(QGCTileCacheWorkerLog) << "Mapping cache directory:" << _databasePath;
    if (!_connectDB()) {
        qCCritical(QGCTileCacheWorkerLog) << "Map Cache SQL error (init() open db)";
        _failed = true;
        return _failed;
    }

    _valid = _createDB(*_db);
    if (!_valid) {
        _failed = true;
    }

    _db->close();
    return !_failed;
}

bool QGCCacheWorker::_connectDB()
{
    if (!_db->isOpen()) {
        _valid = _db->open();
    } else {
        _valid = true;
    }

    return _valid;
}

bool QGCCacheWorker::_createDB(QSqlDatabase &db, bool createDefault)
{
    bool res = false;

    if (!_tilesTable->create()) {
        qCWarning(QGCTileCacheWorkerLog) << "Map Cache SQL error (create Tiles db)";
    } else {
        if (!_tileSetsTable->create()) {
            qCWarning(QGCTileCacheWorkerLog) << "Map Cache SQL error (create TileSets db)";
        } else if (!_setTilesTable->create()) {
            qCWarning(QGCTileCacheWorkerLog) << "Map Cache SQL error (create SetTiles db)";
        } else if (!_tilesDownloadTable->create()) {
            qCWarning(QGCTileCacheWorkerLog) << "Map Cache SQL error (create TilesDownload db)";
        } else {
            res = true;
        }
    }

    if (res && createDefault) {
        res = _tileSetsTable->createDefaultTileSet();
    }

    if (!res) {
        QFile file(_databasePath);
        (void) file.remove();
    }

    return res;
}
