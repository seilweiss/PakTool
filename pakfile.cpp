#include "pakfile.h"

#include <QFile>
#include <QtEndian>

struct PakHeader
{
    quint32 magic;
    quint32 endian;
    quint32 dataOffset;
    quint32 pakSize;
    quint32 nameOffset;
    quint32 resCount;
};

struct PakResource
{
    quint32 nameOffset;
    quint32 dataOffset;
    quint32 dataSize;
};

#define align(val, alignment) (((val) + (alignment) - 1) & -(alignment))

PakFile::PakFile()
{
    endian = PAKFILE_BIG_ENDIAN;
    unsaved = true;
    data = nullptr;
    sectorSize = 2048;
    sizeAlign = 64;
}

PakFile* PakFile::open(QString &path)
{
    QFile file(path);

    if (!file.open(QFile::ReadOnly))
    {
        return nullptr;
    }

    char* pakData = new char[file.size()];

    if (file.read(pakData, file.size()) != file.size())
    {
        delete[] pakData;
        file.close();
        return nullptr;
    }

    file.close();

    PakHeader* pakHeader = reinterpret_cast<PakHeader*>(pakData);
    PakResource* pakResources = reinterpret_cast<PakResource*>(pakHeader + 1);

    if (pakHeader->magic != 'pack' && pakHeader->magic != 'kcap')
    {
        delete[] pakData;
        return nullptr;
    }

    PakFile* pakFile = new PakFile;
    pakFile->data = pakData;
    pakFile->path = path;
    pakFile->unsaved = false;

    if (pakHeader->endian == 0)
    {
        pakFile->endian = PAKFILE_BIG_ENDIAN;
        qFromBigEndian<quint32>(pakHeader, 6, pakHeader);
        qFromBigEndian<quint32>(pakResources, 3 * pakHeader->resCount, pakResources);
    }
    else
    {
        pakFile->endian = PAKFILE_LITTLE_ENDIAN;
        qFromLittleEndian<quint32>(pakHeader, 6, pakHeader);
        qFromLittleEndian<quint32>(pakResources, 3 * pakHeader->resCount, pakResources);
    }

    for (quint32 i = 0; i < pakHeader->resCount; i++)
    {
        PakResource* pakResource = &pakResources[i];
        Resource resource;

        resource.name = pakData + pakHeader->nameOffset + pakResource->nameOffset;
        resource.data = pakData + pakResource->dataOffset;
        resource.size = pakResource->dataSize;
        resource.ownsData = false;

        pakFile->resources.append(resource);
    }

    return pakFile;
}

bool PakFile::save()
{
    QFile file(path);

    if (!file.open(QFile::WriteOnly))
    {
        return false;
    }

    quint32 nameOffset = sizeof(PakHeader) + sizeof(PakResource) * resources.count();
    quint32 dataOffset = nameOffset;
    quint32 pakSize = 0;
    quint32 resCount = resources.count();

    for (quint32 i = 0; i < resCount; i++)
    {
        dataOffset += resources[i].name.length() + 1;
    }

    pakSize = dataOffset;

    for (quint32 i = 0; i < resCount; i++)
    {
        pakSize = align(pakSize, sectorSize);
        pakSize += resources[i].size;
    }

    pakSize = align(pakSize, sizeAlign);

    char* pakData = new char[pakSize];

    PakHeader* pakHeader = reinterpret_cast<PakHeader*>(pakData);
    PakResource* pakResources = reinterpret_cast<PakResource*>(pakHeader + 1);

    pakHeader->magic = 'pack';
    pakHeader->endian = endian;
    pakHeader->dataOffset = dataOffset;
    pakHeader->pakSize = pakSize;
    pakHeader->nameOffset = nameOffset;
    pakHeader->resCount = resCount;

    nameOffset = 0;

    for (quint32 i = 0; i < resCount; i++)
    {
        dataOffset = (dataOffset + sectorSize - 1) & -sectorSize;

        PakResource* pakResource = &pakResources[i];
        pakResource->nameOffset = nameOffset;
        pakResource->dataOffset = dataOffset;
        pakResource->dataSize = resources[i].size;

        quint32 nameLength = resources[i].name.length() + 1;

        memcpy(pakData + dataOffset, resources[i].data, resources[i].size);
        memcpy(pakData + pakHeader->nameOffset + nameOffset, qPrintable(resources[i].name), nameLength);

        nameOffset += nameLength;
        dataOffset += resources[i].size;
    }

    if (endian == PAKFILE_BIG_ENDIAN)
    {
        qToBigEndian<quint32>(pakHeader, 6, pakHeader);
        qToBigEndian<quint32>(pakResources, 3 * resCount, pakResources);
    }
    else
    {
        qToLittleEndian<quint32>(pakHeader, 6, pakHeader);
        qToLittleEndian<quint32>(pakResources, 3 * resCount, pakResources);
    }

    if (file.write(pakData, pakSize) != pakSize)
    {
        file.close();
        delete[] pakData;
        return false;
    }

    file.close();

    /*
    if (data)
    {
        delete data;
    }

    data = pakData;

    for (quint32 i = 0; i < resCount; i++)
    {
        if (resources[i].ownsData)
        {
            delete resources[i].data;
        }

        resources[i].data = pakData + pakResources[i].dataOffset;
        resources[i].ownsData = false;
    }
    */

    delete[] pakData;

    unsaved = false;
    return true;
}

void PakFile::deleteResource(int index)
{
    if (resources[index].ownsData)
    {
        delete resources[index].data;
    }

    resources.remove(index);
}
