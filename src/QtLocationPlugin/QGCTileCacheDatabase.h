#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QQueue>
// #include <QtSql/QSqlTableModel>

#include "QGCTile.h"

class QGCCacheTile;
class QGCCachedTileSet;
class QSqlDatabase;

Q_DECLARE_LOGGING_CATEGORY(QGCTileCacheDBLog)

/*===========================================================================*/

class SetTilesTableModel: public QObject
{
    Q_OBJECT

public:
    explicit SetTilesTableModel(QObject *parent = nullptr, std::shared_ptr<QSqlDatabase> db = nullptr);
    ~SetTilesTableModel();

    bool create() const;
    bool selectFromSetID(quint64 setID) const;
    bool insertSetTiles(quint64 setID, quint64 tileID) const;
    bool deleteTileSet(quint64 setID) const;
    bool drop() const;

private:
    std::shared_ptr<QSqlDatabase> _db = nullptr;
};

/*===========================================================================*/

class TileSetsTableModel: public QObject
{
    Q_OBJECT

public:
    explicit TileSetsTableModel(QObject *parent = nullptr, std::shared_ptr<QSqlDatabase> db = nullptr);
    ~TileSetsTableModel();

    bool create() const;
    bool insertTileSet(const QGCCachedTileSet &tileSet) const;
    bool getTileSets(QList<QGCCachedTileSet*> &tileSets) const;
    bool setName(quint64 setID, const QString &newName) const;
    bool setNumTiles(quint64 setID, quint64 numTiles) const;
    bool getTileSetID(quint64 &setID, const QString &name) const;
    bool getDefaultTileSet(quint64 &setID);
    bool deleteTileSet(quint64 setID) const;
    bool drop() const;
    bool createDefaultTileSet() const;

private:
    std::shared_ptr<QSqlDatabase> _db = nullptr;
    quint64 _defaultSet = UINT64_MAX;
};

/*===========================================================================*/

class TilesTableModel: public QObject
{
    Q_OBJECT

public:
    explicit TilesTableModel(QObject *parent = nullptr, std::shared_ptr<QSqlDatabase> db = nullptr);
    ~TilesTableModel();

    bool create() const;
    bool getTile(QGCCacheTile *tile, const QString &hash) const;
    bool getTileID(quint64 &tileID, const QString &hash) const;
    bool selectFromSetID(quint64 tileID) const;
    bool insertTile(quint64 &tileID, const QGCCacheTile &tile) const;
    bool deleteTileSet(quint64 setID) const;
    bool getSetTotal(quint64 &count, quint64 setID) const;
    bool updateSetTotals(QGCCachedTileSet &set, quint64 setID) const;
    bool updateTotals(quint32 &totalCount, quint64 &totalSize, quint32 &defaultCount, quint64 &defaultSize, quint64 defaultTileSetID) const;
    bool prune(quint64 defaultTileSetID, qint64 amount) const;
    bool deleteBingNoTileImageTiles() const;
    bool drop() const;

private:
    static bool _initDataFromResources();

    std::shared_ptr<QSqlDatabase> _db = nullptr;
    static QByteArray _bingNoTileImage;
};

/*===========================================================================*/

class TilesDownloadTableModel: public QObject
{
    Q_OBJECT

public:
    explicit TilesDownloadTableModel(QObject *parent = nullptr, std::shared_ptr<QSqlDatabase> db = nullptr);
    ~TilesDownloadTableModel();

    bool create() const;
    bool insertTilesDownload(QGCCachedTileSet* tileSet) const;
    bool setState(quint64 setID, const QString &hash, int state) const;
    bool setState(quint64 setID, int state) const;
    bool deleteTileSet(quint64 setID) const;
    bool getTileDownloadList(QQueue<QGCTile*> &tiles, quint64 setID, int count) const;
    bool updateTilesDownloadSet(QGCTile::TileState state, quint64 setID, const QString &hash) const;
    bool drop() const;

private:
    std::shared_ptr<QSqlDatabase> _db = nullptr;
};

/*===========================================================================*/
