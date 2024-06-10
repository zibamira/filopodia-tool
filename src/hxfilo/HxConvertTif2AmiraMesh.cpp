#include "HxConvertTif2AmiraMesh.h"
#include "FilopodiaFunctions.h"
#include <QDirIterator>
#include <hxcore/HxResource.h>
#include <hxcore/HxObjectPool.h>
#include <hxcore/HxFileFormat.h>
#include <hxfield/HxField3.h>
#include <mclib/McException.h>
#include <QDebug>

HX_INIT_CLASS(HxConvertTif2AmiraMesh, HxCompModule);

HxConvertTif2AmiraMesh::HxConvertTif2AmiraMesh()
    : HxCompModule(HxSpatialGraph::getClassTypeId())
    , portInputDir(this, "portInputDirectory", tr("Input Directory"))
    , portOutputDir(this, "portOutputDirectory", tr("Output Directory"), HxPortFilename::SAVE_DIRECTORY)
    , portSize(this, "portVoxelSize", tr("Voxel Size"), 3)
    , adjustWindowSize(this, "adjustWindowSize", tr("Adjust Window Size"))
    , windowSize(this, "windowSize", tr("Window Size"), 2)
    , portDoIt(this, "action", tr("Action"))
{
    portData.hide();
    portInputDir.setValue("Please select");
    portOutputDir.setValue("Please select");
    portSize.setValue(0, 0.1);
    portSize.setValue(1, 0.1);
    portSize.setValue(2, 0.5);
    adjustWindowSize.setValue(false);
    windowSize.setMinMax(0, 255);
    windowSize.setValue(0, 0);
    windowSize.setValue(1, 255);
}

HxConvertTif2AmiraMesh::~HxConvertTif2AmiraMesh()
{
}

McHandle<HxUniformScalarField3>
loadSingleTifFile(QString fileName)
{
    McHandle<HxUniformScalarField3> imageField = 0;

    QStringList loadString;

    QString modeString("+mode");
    QString modeNumberString = QString("%1").arg(100);

    loadString.append(modeString);
    loadString.append(modeNumberString);
    loadString.append(fileName);

    HxFileFormat* format = HxResource::queryDataFormat(fileName);
    if (!format)
    {
        throw McException(QString("Unknown file format: %1").arg(fileName));
    }

    int numPoolObjectsBefore = theObjectPool->getNodeList().size();

    int res = format->load(loadString);
    if (!res)
    {
        throw McException(QString("Could not load file %1").arg(fileName));
    }

    int numPoolObjectsAfter = theObjectPool->getNodeList().size();

    for (int i = numPoolObjectsAfter - 1; i >= numPoolObjectsBefore; --i)
    {
        HxObject* obj = theObjectPool->getNodeList()[i];
        if (obj->isOfType(HxUniformScalarField3::getClassTypeId()))
        {
            imageField = dynamic_cast<HxUniformScalarField3*>(obj);
        }
        theObjectPool->removeObject(obj);
    }

    if (!imageField)
    {
        throw McException(QString("Loaded data is not a scalar image"));
    }

    return imageField;
}

McHandle<HxUniformScalarField3>
setVoxelSize(McHandle<HxUniformScalarField3> image, const McVec3f voxelSize)
{
    McBox3f bbox = image->getBoundingBox();
    const McDim3l& dims = image->lattice().getDims();
    const McVec3f pmin(0, 0, 0);
    int dim3[3] = { int(dims.nx), int(dims.ny), int(dims.nz) };

    bbox.setVoxelSizeAndMin(pmin, voxelSize, dim3);
    image->lattice().setBoundingBox(bbox);

    return image;
}

void
HxConvertTif2AmiraMesh::getWindowMinMax(HxUniformScalarField3* image, float& dataWindowMin, float& dataWindowMax)
{
    image->getRange(dataWindowMin, dataWindowMax, HxUniformScalarField3::DATA_WINDOW);
}

bool
HxConvertTif2AmiraMesh::isInputDirectoryValid()
{
    const QString inputDir(portInputDir.getFilename());
    QStringList files = FilopodiaFunctions::getFileListFromDirectory(inputDir, "TIF");

    if (!portInputDir.getFilename().isEmpty() && (portInputDir.getFilename() != "Please select") && QDir(inputDir).exists() && files.size() != 0)
        return true;

    else
        return false;
}

McHandle<HxUniformScalarField3>
HxConvertTif2AmiraMesh::getSingleFile()
{
    const QString inputDir(portInputDir.getFilename());
    QStringList files = FilopodiaFunctions::getFileListFromDirectory(inputDir, "TIF");
    McHandle<HxUniformScalarField3> image = loadSingleTifFile(files[0]);

    return image;
}

void
HxConvertTif2AmiraMesh::setWindowMinMax(HxUniformScalarField3* image, float& dataWindowMin, float& dataWindowMax)
{
    image->setDataWindow(dataWindowMin, dataWindowMax);
}

void
HxConvertTif2AmiraMesh::compute()
{
    if (!portDoIt.wasHit())
        return;

    if (adjustWindowSize.getValue() && windowMax <= windowMin)
        throw McException(QString("Max window has to be greater than min!"));

    if (portInputDir.getFilename().isEmpty() || portInputDir.getFilename() == "Please select")
    {
        throw McException(QString("Specify input directory!"));
    }

    if (portOutputDir.getFilename().isEmpty() || portOutputDir.getFilename() == "Please select")
    {
        throw McException(QString("Specify output directory!"));
    }

    const QString inputDir(portInputDir.getFilename());
    const QString outputDir(portOutputDir.getFilename());

    if (!QDir(inputDir).exists())
    {
        throw McException(QString("Input directory does not exist. \nPlease select valid input directory containing tif files."));
    }

    if (!QDir(outputDir).exists())
    {
        throw McException(QString("Output directory does not exist. \nPlease select valid output directory."));
    }

    McVec3f voxelSize;
    voxelSize[0] = portSize.getValue(0);
    voxelSize[1] = portSize.getValue(1);
    voxelSize[2] = portSize.getValue(2);

    QStringList files = FilopodiaFunctions::getFileListFromDirectory(inputDir, "TIF");
    QStringList filesInOutputDir = FilopodiaFunctions::getFileListFromDirectory(outputDir, "AM");

    if (files.size() == 0)
    {
        throw McException(QString("No tif files in directory!"));
    }
    if (filesInOutputDir.size() > 0)
    {
        const int answer = HxMessage::warning("Output directory is not empty. Files may be overwritten. \nContinue?", "Continue", "Cancel");
        if (answer == 1)
        {
            return;
        }
    }

    for (int f = 0; f < files.size(); ++f)
    {
        QString fileName = files[f];
        McHandle<HxUniformScalarField3> image = loadSingleTifFile(fileName);
        image = setVoxelSize(image, voxelSize);
        if (adjustWindowSize.getValue())
            setWindowMinMax(image, windowMin, windowMax);
        QString outputFile = FilopodiaFunctions::getOutputFileName(fileName, outputDir);
        image->writeAmiraMeshBinary(qPrintable(outputFile));
        theMsg->printf(QString("Saving %1").arg(outputFile));
    }
}

void
HxConvertTif2AmiraMesh::update()
{
    McHandle<HxUniformScalarField3> firstImage;

    if (adjustWindowSize.isNew())
    {
        if (adjustWindowSize.getValue())
        {
            windowSize.setVisible(true);

            if (isInputDirectoryValid())
            {
                firstImage = getSingleFile();
                getWindowMinMax(firstImage, windowMin, windowMax);

                windowSize.setValue(0, windowMin);
                windowSize.setValue(1, windowMax);
            }

            else
            {
                windowSize.setValue(0, 0);
                windowSize.setValue(1, 255);
            }
        }

        else
        {
            windowSize.setVisible(false);
        }
    }

    if (windowSize.isNew())
    {
        windowMin = windowSize.getValue(0);
        windowMax = windowSize.getValue(1);
    }

    portInputDir.setMode(HxPortFilename::LOAD_DIRECTORY);
}
