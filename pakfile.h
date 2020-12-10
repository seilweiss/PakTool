#ifndef PAKFILE_H
#define PAKFILE_H

#include <QString>
#include <QVector>

class PakFile
{
public:
    enum Endian
    {
        BIG_ENDIAN = 0,
        LITTLE_ENDIAN = 1
    };

    struct Resource
    {
        QString name;
        char* data;
        quint32 size;
        bool ownsData;
    };

    PakFile();

    static PakFile* open(QString& path);

    bool save();
    void deleteResource(int index);

    bool unsaved;
    QString path;
    Endian endian;
    char* data;
    quint32 sectorSize;
    quint32 sizeAlign;
    QVector<Resource> resources;
};

#endif // PAKFILE_H
