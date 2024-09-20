#include "QGCTileCacheDatabase.h"
#include "QGCCacheTile.h"
#include "QGCCachedTileSet.h"
#include "QGCMapUrlEngine.h"
#include "QGCLoggingCategory.h"

#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QSettings>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
// #include <QtSql/QSqlQueryModel>

QGC_LOGGING_CATEGORY(QGCTileCacheDBLog, "qgc.qtlocationplugin.qgctilecachedb")

/*===========================================================================*/

SetTilesTableModel::SetTilesTableModel(QObject *parent, std::shared_ptr<QSqlDatabase> db)
    : QObject(parent)
    , _db(db)
{
    // qCDebug(QGCTileCacheDBLog) << Q_FUNC_INFO << this;

    // setTable("setTiles");
}

SetTilesTableModel::~SetTilesTableModel()
{
    // qCDebug(QGCTileCacheDBLog) << Q_FUNC_INFO << this;
}

bool SetTilesTableModel::create() const
{
    QSqlQuery query(*_db);

    (void) query.prepare(
        "CREATE TABLE IF NOT EXISTS SetTiles ("
        "setID INTEGER NOT NULL, "
        "tileID INTEGER NOT NULL, "
        "PRIMARY KEY (setID, tileID)"
        ")"
    );

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool SetTilesTableModel::selectFromSetID(quint64 setID) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("SELECT * FROM SetTiles WHERE setID = ?");
    query.addBindValue(setID);

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << "No rows found for setID" << setID;
        return false;
    }

    return true;
}

bool SetTilesTableModel::insertSetTiles(quint64 setID, quint64 tileID) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("INSERT INTO SetTiles(tileID, setID) VALUES(?, ?)"); // INSERT OR IGNORE?
    query.addBindValue(tileID);
    query.addBindValue(setID);

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool SetTilesTableModel::deleteTileSet(quint64 setID) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("DELETE FROM SetTiles WHERE setID = ?");
    query.addBindValue(setID);

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool SetTilesTableModel::drop() const
{
    QSqlQuery query(*_db);
    (void) query.prepare("DROP TABLE IF EXISTS SetTiles");

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return query.exec();
}

/*===========================================================================*/

TileSetsTableModel::TileSetsTableModel(QObject *parent, std::shared_ptr<QSqlDatabase> db)
    : QObject(parent)
    , _db(db)
{
    // qCDebug(QGCTileCacheDBLog) << Q_FUNC_INFO << this;

    // setTable("TileSets");
}

TileSetsTableModel::~TileSetsTableModel()
{
    // qCDebug(QGCTileCacheDBLog) << Q_FUNC_INFO << this;
}

bool TileSetsTableModel::create() const
{
    QSqlQuery query(*_db);
    (void) query.prepare(
        "CREATE TABLE IF NOT EXISTS TileSets ("
        "setID INTEGER PRIMARY KEY NOT NULL, "
        "name TEXT NOT NULL UNIQUE, "
        "typeStr TEXT, "
        "topleftLat REAL DEFAULT 0.0, "
        "topleftLon REAL DEFAULT 0.0, "
        "bottomRightLat REAL DEFAULT 0.0, "
        "bottomRightLon REAL DEFAULT 0.0, "
        "minZoom INTEGER DEFAULT 3, "
        "maxZoom INTEGER DEFAULT 3, "
        "type INTEGER DEFAULT -1, "
        "numTiles INTEGER DEFAULT 0, "
        "defaultSet INTEGER DEFAULT 0, "
        "date INTEGER DEFAULT 0)"
    );

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool TileSetsTableModel::insertTileSet(const QGCCachedTileSet &tileSet) const
{
    QSqlQuery query(*_db);
    (void) query.prepare(
        "INSERT INTO TileSets"
        "(name, typeStr, topleftLat, topleftLon, bottomRightLat, bottomRightLon, minZoom, maxZoom, type, numTiles, date) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
    );
    query.addBindValue(tileSet.name());
    query.addBindValue(tileSet.mapTypeStr());
    query.addBindValue(tileSet.topleftLat());
    query.addBindValue(tileSet.topleftLon());
    query.addBindValue(tileSet.bottomRightLat());
    query.addBindValue(tileSet.bottomRightLon());
    query.addBindValue(tileSet.minZoom());
    query.addBindValue(tileSet.maxZoom());
    query.addBindValue(UrlFactory::getQtMapIdFromProviderType(tileSet.type()));
    query.addBindValue(tileSet.totalTileCount());
    query.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool TileSetsTableModel::getTileSets(QList<QGCCachedTileSet*> &tileSets) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("SELECT * FROM TileSets ORDER BY defaultSet DESC, name ASC");

    if (!query.exec()) {
        qCDebug(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    while (query.next()) {
        const QString name = query.value("name").toString();
        QGCCachedTileSet* const set = new QGCCachedTileSet(name);
        set->setId(query.value("setID").toULongLong());
        set->setMapTypeStr(query.value("typeStr").toString());
        set->setTopleftLat(query.value("topleftLat").toDouble());
        set->setTopleftLon(query.value("topleftLon").toDouble());
        set->setBottomRightLat(query.value("bottomRightLat").toDouble());
        set->setBottomRightLon(query.value("bottomRightLon").toDouble());
        set->setMinZoom(query.value("minZoom").toInt());
        set->setMaxZoom(query.value("maxZoom").toInt());
        set->setType(UrlFactory::getProviderTypeFromQtMapId(query.value("type").toInt()));
        set->setTotalTileCount(query.value("numTiles").toUInt());
        set->setDefaultSet(query.value("defaultSet").toInt() != 0);
        set->setCreationDate(QDateTime::fromSecsSinceEpoch(query.value("date").toUInt()));
        (void) tileSets.append(set);
    }

    return true;
}

bool TileSetsTableModel::setName(quint64 setID, const QString &newName) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("UPDATE TileSets SET name = ? WHERE setID = ?");
    query.addBindValue(newName);
    query.addBindValue(setID);

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool TileSetsTableModel::setNumTiles(quint64 setID, quint64 numTiles) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("UPDATE TileSets SET numTiles = ? WHERE setID = ?");
    query.addBindValue(numTiles);
    query.addBindValue(setID);

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool TileSetsTableModel::getTileSetID(quint64 &setID, const QString &name) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("SELECT setID FROM TileSets WHERE name = ?");
    query.addBindValue(name);

    if (query.exec() && query.next()) {
        setID = query.value(0).toULongLong();
        return true;
    }

    qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << "TileSet not found for name" << name;
    return false;
}

bool TileSetsTableModel::getDefaultTileSet(quint64 &setID)
{
    if (_defaultSet != UINT64_MAX) {
        setID = _defaultSet;
        return true;
    }

    QSqlQuery query(*_db);

    (void) query.prepare("SELECT setID FROM TileSets WHERE defaultSet = 1");

    if (!query.exec() || !query.next()) {
        setID = 1; // Default fallback if no set found.
        return false;
    }

    _defaultSet = query.value(0).toULongLong();
    setID = _defaultSet;

    return true;
}

bool TileSetsTableModel::deleteTileSet(quint64 setID) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("DELETE FROM TileSets WHERE setID = ?");
    query.addBindValue(setID);

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool TileSetsTableModel::drop() const
{
    QSqlQuery query(*_db);

    (void) query.prepare("DROP TABLE IF EXISTS TileSets");

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool TileSetsTableModel::createDefaultTileSet() const
{
    static const QString kDefaultSet = QStringLiteral("Default Tile Set");

    QSqlQuery query(*_db);

    (void) query.prepare("SELECT name FROM TileSets WHERE name = ?");
    query.addBindValue(kDefaultSet);

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    if (query.next()) {
        return true;
    }

    (void) query.prepare("INSERT INTO TileSets(name, defaultSet, date) VALUES(?, ?, ?)");
    query.addBindValue(kDefaultSet);
    query.addBindValue(1);
    query.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

/*===========================================================================*/

QByteArray TilesTableModel::_bingNoTileImage;

TilesTableModel::TilesTableModel(QObject *parent, std::shared_ptr<QSqlDatabase> db)
    : QObject(parent)
    , _db(db)
{
    // qCDebug(QGCTileCacheDBLog) << Q_FUNC_INFO << this;

    // setTable("Tiles");
}

TilesTableModel::~TilesTableModel()
{
    // qCDebug(QGCTileCacheDBLog) << Q_FUNC_INFO << this;
}

bool TilesTableModel::create() const
{
    QSqlQuery query(*_db);
    (void) query.prepare(
        "CREATE TABLE IF NOT EXISTS Tiles ("
        "tileID INTEGER PRIMARY KEY NOT NULL, "
        "hash TEXT NOT NULL UNIQUE, "
        "format TEXT NOT NULL, "
        "tile BLOB, "
        "size INTEGER, "
        "type INTEGER, "
        "date INTEGER DEFAULT 0)"
    );

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    (void) query.prepare(
        "CREATE INDEX IF NOT EXISTS hash ON Tiles ("
        "hash, size, type)"
    );

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool TilesTableModel::getTile(QGCCacheTile *tile, const QString &hash) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("SELECT tile, format, type FROM Tiles WHERE hash = ?");
    query.addBindValue(hash);

    if (!query.exec() || !query.next()) {
        qCDebug(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();;
        return false;
    }

    const QByteArray array = query.value(0).toByteArray();
    const QString format = query.value(1).toString();
    const QString type = query.value(2).toString();
    tile = new QGCCacheTile(hash, array, format, type);

    return true;
}

bool TilesTableModel::getTileID(quint64 &tileID, const QString &hash) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("SELECT tileID FROM Tiles WHERE hash = ?");
    query.addBindValue(hash);

    if (!query.exec() || !query.next()) {
        qCDebug(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();;
        return false;
    }

    tileID = query.value(0).toULongLong();

    return true;
}

bool TilesTableModel::selectFromSetID(quint64 tileID) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("SELECT * FROM Tiles WHERE tileID = ?");
    query.addBindValue(tileID);

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool TilesTableModel::insertTile(quint64 &tileID, const QGCCacheTile &tile) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("INSERT INTO Tiles(hash, format, tile, size, type, date) VALUES(?, ?, ?, ?, ?, ?)");
    query.addBindValue(tile.hash());
    query.addBindValue(tile.format());
    query.addBindValue(tile.img());
    query.addBindValue(tile.img().size());
    query.addBindValue(tile.type());
    query.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    tileID = query.lastInsertId().toULongLong();

    return true;
}

bool TilesTableModel::deleteTileSet(quint64 setID) const
{
    QSqlQuery query(*_db);

    (void) query.prepare(
        "DELETE FROM Tiles WHERE tileID IN "
        "(SELECT A.tileID FROM SetTiles A "
        "JOIN SetTiles B ON A.tileID = B.tileID WHERE B.setID = ? "
        "GROUP BY A.tileID HAVING COUNT(A.tileID) = 1)"
    );
    query.addBindValue(setID);

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool TilesTableModel::getSetTotal(quint64 &count, quint64 setID) const
{
    QSqlQuery query(*_db);

    (void) query.prepare(
        "SELECT COUNT(size) FROM Tiles A "
        "INNER JOIN SetTiles B on A.tileID = B.tileID "
        "WHERE B.setID = ?"
    );
    query.addBindValue(setID);

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << "Failed to execute COUNT query:" << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << "No result from COUNT query.";
        return false;
    }

    count = query.value(0).toULongLong();

    return true;
}

bool TilesTableModel::updateSetTotals(QGCCachedTileSet &set, quint64 setID) const
{
    QSqlQuery query(*_db);

    (void) query.prepare(
        "SELECT COUNT(size), SUM(size) FROM Tiles A "
        "INNER JOIN SetTiles B on A.tileID = B.tileID WHERE B.setID = ?"
    );
    query.addBindValue(setID);

    if (!query.exec() || !query.next()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << "query failed";
        return false;
    }

    set.setSavedTileCount(query.value(0).toUInt());
    set.setSavedTileSize(query.value(1).toULongLong());
    qCDebug(QGCTileCacheDBLog) << "Set" << set.id() << "Totals:" << set.savedTileCount() << set.savedTileSize() << "Expected:" << set.totalTileCount() << set.totalTilesSize();

    quint64 avg = UrlFactory::averageSizeForType(set.type());
    if (set.totalTileCount() <= set.savedTileCount()) {
        set.setTotalTileSize(set.savedTileSize());
    } else {
        if ((set.savedTileCount() > 10) && (set.savedTileSize() > 0)) {
            avg = (set.savedTileSize() / set.savedTileCount());
        }
        set.setTotalTileSize(avg * set.totalTileCount());
    }

    quint32 ucount = 0;
    quint64 usize = 0;
    (void) query.prepare(
        "SELECT COUNT(size), SUM(size) FROM Tiles "
        "WHERE tileID IN (SELECT A.tileID FROM SetTiles A "
        "JOIN SetTiles B on A.tileID = B.tileID WHERE B.setID = ? "
        "GROUP by A.tileID HAVING COUNT(A.tileID) = 1)"
    );
    query.addBindValue(setID);

    if (query.exec() && query.next()) {
        ucount = query.value(0).toUInt();
        usize = query.value(1).toULongLong();
    }

    quint32 expectedUcount = set.totalTileCount() - set.savedTileCount();
    if (ucount == 0) {
        usize = expectedUcount * avg;
    } else {
        expectedUcount = ucount;
    }
    set.setUniqueTileCount(expectedUcount);
    set.setUniqueTileSize(usize);

    return true;
}

bool TilesTableModel::updateTotals(quint32 &totalCount, quint64 &totalSize, quint32 &defaultCount, quint64 &defaultSize, quint64 defaultTileSetID) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("SELECT COUNT(size), SUM(size) FROM Tiles");

    if (!query.exec() || !query.next()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    totalCount = query.value(0).toUInt();
    totalSize = query.value(1).toULongLong();

    (void) query.prepare(
        "SELECT COUNT(size), SUM(size) FROM Tiles WHERE tileID IN "
        "(SELECT A.tileID FROM SetTiles A "
        "JOIN SetTiles B on A.tileID = B.tileID WHERE B.setID = ? "
        "GROUP by A.tileID HAVING COUNT(A.tileID) = 1)"
    );
    query.addBindValue(defaultTileSetID);

    if (!query.exec() || !query.next()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    defaultCount = query.value(0).toUInt();
    defaultSize = query.value(1).toULongLong();

    return true;
}

bool TilesTableModel::prune(quint64 defaultTileSetID, qint64 amount) const
{
    QSqlQuery query(*_db);

    (void) query.prepare(
        "SELECT tileID, size, hash FROM Tiles WHERE tileID IN "
        "(SELECT A.tileID FROM SetTiles A JOIN SetTiles B on A.tileID = B.tileID "
        "WHERE B.setID = ? GROUP by A.tileID HAVING COUNT(A.tileID) = 1) "
        "ORDER BY DATE ASC LIMIT 128"
    );
    query.addBindValue(defaultTileSetID);

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    QQueue<quint64> tilelist;
    while (query.next() && (amount >= 0)) {
        tilelist.enqueue(query.value(0).toULongLong());
        amount -= query.value(1).toULongLong();
    }

    while (!tilelist.isEmpty()) {
        const quint64 tileID = tilelist.dequeue();

        (void) query.prepare("DELETE FROM Tiles WHERE tileID = ?");
        query.addBindValue(tileID);

        if (!query.exec()) {
            qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
            return false;
        }
    }

    return true;
}

bool TilesTableModel::_initDataFromResources()
{
    if (_bingNoTileImage.isEmpty()) {
        QFile file("://res/BingNoTileBytes.dat");
        if (file.open(QFile::ReadOnly)) {
            _bingNoTileImage = file.readAll();
            file.close();
        }

        if (_bingNoTileImage.isEmpty()) {
            qCWarning(QGCTileCacheDBLog) << "Unable to read BingNoTileBytes";
            return false;
        }
    }

    return true;
}

bool TilesTableModel::deleteBingNoTileImageTiles() const
{
    static const QString alreadyDoneKey = QStringLiteral("_deleteBingNoTileTilesDone");

    QSettings settings;
    if (settings.value(alreadyDoneKey, false).toBool()) {
        return true;
    }
    settings.setValue(alreadyDoneKey, true);

    if (!_initDataFromResources()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << "Error getting BingNoTileBytes.dat";
        return false;
    }

    QSqlQuery query(*_db);

    (void) query.prepare("SELECT tileID, tile FROM Tiles WHERE LENGTH(tile) = ?");
    query.addBindValue(_bingNoTileImage.length());

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    QList<quint64> idsToDelete;
    while (query.next()) {
        if (query.value(1).toByteArray() == _bingNoTileImage) {
            (void) idsToDelete.append(query.value(0).toULongLong());
        }
    }

    for (const quint64 tileId: idsToDelete) {
        query.prepare("DELETE FROM Tiles WHERE tileID = ?");
        query.addBindValue(tileId);

        if (!query.exec()) {
            qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
            return false;
        }
    }

    return true;
}

bool TilesTableModel::drop() const
{
    QSqlQuery query(*_db);

    (void) query.prepare("DROP TABLE IF EXISTS Tiles");

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

/*===========================================================================*/

TilesDownloadTableModel::TilesDownloadTableModel(QObject *parent, std::shared_ptr<QSqlDatabase> db)
    : QObject(parent)
    , _db(db)
{
    // qCDebug(QGCTileCacheDBLog) << Q_FUNC_INFO << this;

    // setTable("TilesDownload");
}

TilesDownloadTableModel::~TilesDownloadTableModel()
{
    // qCDebug(QGCTileCacheDBLog) << Q_FUNC_INFO << this;
}

bool TilesDownloadTableModel::create() const
{
    QSqlQuery query(*_db);

    (void) query.prepare(
        "CREATE TABLE IF NOT EXISTS TilesDownload ("
        "setID INTEGER PRIMARY KEY NOT NULL, "
        "hash TEXT NOT NULL UNIQUE, "
        "type INTEGER, "
        "x INTEGER, "
        "y INTEGER, "
        "z INTEGER, "
        "state INTEGER DEFAULT 0)"
    );

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool TilesDownloadTableModel::insertTilesDownload(QGCCachedTileSet* tileSet) const
{
    QSqlQuery query(*_db);

    const quint64 setID = query.lastInsertId().toULongLong();
    tileSet->setId(setID);

    if (!_db->transaction()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << "Transaction start failed";
        return false;
    }

    for (int z = tileSet->minZoom(); z <= tileSet->maxZoom(); z++) {
        const QGCTileSet set = UrlFactory::getTileCount(
            z,
            tileSet->topleftLon(),
            tileSet->topleftLat(),
            tileSet->bottomRightLon(),
            tileSet->bottomRightLat(),
            tileSet->type()
        );
        const QString type = tileSet->type();

        for (int x = set.tileX0; x <= set.tileX1; x++) {
            for (int y = set.tileY0; y <= set.tileY1; y++) {
                const QString hash = UrlFactory::getTileHash(type, x, y, z);
                const quint64 tileID = 0; // TODO: _findTile(hash);
                // _tilesTable->getTileID(tileID, hash);
                if (!tileID) {
                    (void) query.prepare(QStringLiteral(
                        "INSERT OR IGNORE INTO TilesDownload(setID, hash, type, x, y, z, state) "
                        "VALUES(?, ?, ?, ?, ? ,? ,?)"
                    ));
                    query.addBindValue(setID);
                    query.addBindValue(hash);
                    query.addBindValue(UrlFactory::getQtMapIdFromProviderType(type));
                    query.addBindValue(x);
                    query.addBindValue(y);
                    query.addBindValue(z);
                    query.addBindValue(0);

                    if (!query.exec()) {
                        qCWarning(QGCTileCacheDBLog) << "Map Cache SQL error (add tile into TilesDownload):"
                                                     << query.lastError().text();
                        return false;
                    }
                } else {
                    const QString s = QStringLiteral(
                        "INSERT OR IGNORE INTO SetTiles(tileID, setID) VALUES(%1, %2)"
                    ).arg(tileID).arg(setID);

                    if (!query.exec(s)) {
                        qCWarning(QGCTileCacheDBLog) << "Map Cache SQL error (add tile into SetTiles):"
                                                     << query.lastError().text();
                    }

                    qCDebug(QGCTileCacheDBLog) << Q_FUNC_INFO << "Already Cached HASH:" << hash;
                }
            }
        }
    }

    if (!_db->commit()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << "Transaction commit failed";
        return false;
    }

    return true;
}

bool TilesDownloadTableModel::setState(quint64 setID, const QString &hash, int state) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("UPDATE TilesDownload SET state = ? WHERE setID = ? AND hash = ?");
    query.addBindValue(state);
    query.addBindValue(setID);
    query.addBindValue(hash);

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool TilesDownloadTableModel::setState(quint64 setID, int state) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("UPDATE TilesDownload SET state = ? WHERE setID = ?");
    query.addBindValue(state);
    query.addBindValue(setID);

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool TilesDownloadTableModel::deleteTileSet(quint64 setID) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("DELETE FROM TilesDownload WHERE setID = ?");
    query.addBindValue(setID);

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool TilesDownloadTableModel::getTileDownloadList(QQueue<QGCTile*> &tiles, quint64 setID, int count) const
{
    QSqlQuery query(*_db);

    (void) query.prepare("SELECT hash, type, x, y, z FROM TilesDownload WHERE setID = ? AND state = 0 LIMIT ?");
    query.addBindValue(setID);
    query.addBindValue(count);

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    while (query.next()) {
        QGCTile* const tile = new QGCTile();
        // tile->setTileSet(task->setID());
        tile->setHash(query.value("hash").toString());
        tile->setType(UrlFactory::getProviderTypeFromQtMapId(query.value("type").toInt()));
        tile->setX(query.value("x").toInt());
        tile->setY(query.value("y").toInt());
        tile->setZ(query.value("z").toInt());
        tiles.enqueue(tile);
    }

    for (qsizetype i = 0; i < tiles.size(); i++) {
        (void) query.prepare("UPDATE TilesDownload SET state = ? WHERE setID = ? AND hash = ?");
        query.addBindValue(static_cast<int>(QGCTile::StateDownloading));
        query.addBindValue(setID);
        query.addBindValue(tiles[i]->hash());

        if (!query.exec()) {
            qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
            return false;
        }
    }

    return true;
}

bool TilesDownloadTableModel::updateTilesDownloadSet(QGCTile::TileState state, quint64 setID, const QString &hash) const
{
    QSqlQuery query(*_db);

    if (state == QGCTile::StateComplete) {
        (void) query.prepare("DELETE FROM TilesDownload WHERE setID = ? AND hash = ?");
        query.addBindValue(setID);
        query.addBindValue(hash);
    } else if (hash == "*") {
        (void) query.prepare("UPDATE TilesDownload SET state = ? WHERE setID = ?");
        query.addBindValue(state);
        query.addBindValue(setID);
    } else {
        (void) query.prepare("UPDATE TilesDownload SET state = ? WHERE setID = ? AND hash = ?");
        query.addBindValue(state);
        query.addBindValue(setID);
        query.addBindValue(hash);
    }

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

bool TilesDownloadTableModel::drop() const
{
    QSqlQuery query(*_db);

    (void) query.prepare("DROP TABLE IF EXISTS TilesDownload");

    if (!query.exec()) {
        qCWarning(QGCTileCacheDBLog) << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    return true;
}

/*===========================================================================*/
