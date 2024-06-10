#include "HxAdjustWindowLevelOfAmiraMesh.h"
#include "FilopodiaFunctions.h"
#include <mclib/McException.h>
#include <hxcore/HxResource.h>
#include <hxcore/HxFileFormat.h>
#include <hxcore/HxObjectPool.h>


HX_INIT_CLASS(HxAdjustWindowLevelOfAmiraMesh, HxCompModule);

HxAdjustWindowLevelOfAmiraMesh::HxAdjustWindowLevelOfAmiraMesh()
    : HxCompModule(HxSpatialGraph::getClassTypeId())
    , portInputDir(this, "portInputDirectory", tr("Input Directory"))
    , portOutputDir(this, "portOutputDirectory", tr("Output Directory"), HxPortFilename::SAVE_DIRECTORY)
    , windowSize(this, "windowSize", tr("Window Size"), 2)
    , portDoIt(this, "action", tr("Action"))
{
    portData.hide();
    portInputDir.setValue("Please select");
    portOutputDir.setValue("Please select");
    windowSize.setMinMax(0, 255);
    windowSize.setValue(0, 0);
    windowSize.setValue(1, 255);
}

HxAdjustWindowLevelOfAmiraMesh::~HxAdjustWindowLevelOfAmiraMesh(){}

McHandle<HxUniformScalarField3>
loadSingleAmFile(QString fileName)
{
    McHandle<HxUniformScalarField3> imageField = 0;

    QStringList loadString;
    loadString.append(fileName);

    HxFileFormat* format = HxResource::queryDataFormat(fileName);
    if (!format)
        throw McException(QString("Unknown file format: %1").arg(fileName));

    int numPoolObjectsBefore = theObjectPool->getNodeList().size();

    int res = format->load(loadString);
    if (!res)
        throw McException(QString("Could not load file %1").arg(fileName));

    int numPoolObjectsAfter = theObjectPool->getNodeList().size();

    for (int i = numPoolObjectsAfter - 1; i >= numPoolObjectsBefore; --i)
    {
        HxObject* obj = theObjectPool->getNodeList()[i];
        if (obj->isOfType(HxUniformScalarField3::getClassTypeId()))
            imageField = dynamic_cast<HxUniformScalarField3*>(obj);
        theObjectPool->removeObject(obj);
    }

    if (!imageField)
        throw McException(QString("Loaded data is not a scalar image"));

    return imageField;
}

McHandle<HxUniformScalarField3>
HxAdjustWindowLevelOfAmiraMesh::getSingleFile()
{
    const QString inputDir(portInputDir.getFilename());
    QStringList files = FilopodiaFunctions::getFileListFromDirectory(inputDir, "AM");
    McHandle<HxUniformScalarField3> image = loadSingleAmFile(files[0]);

    return image;
}

bool
HxAdjustWindowLevelOfAmiraMesh::isInputDirectoryValid()
{
    const QString inputDir(portInputDir.getFilename());
    QStringList files = FilopodiaFunctions::getFileListFromDirectory(inputDir, "AM");

    if (!portInputDir.getFilename().isEmpty() && (portInputDir.getFilename() != "Please select") && QDir(inputDir).exists() && files.size() != 0)
        return true;

    else
        return false;
}

void
HxAdjustWindowLevelOfAmiraMesh::getWindowMinMax(HxUniformScalarField3* image, float& dataWindowMin, float& dataWindowMax)
{
    image->getRange(dataWindowMin, dataWindowMax, HxUniformScalarField3::DATA_WINDOW);
}

void
HxAdjustWindowLevelOfAmiraMesh::setWindowMinMax(HxUniformScalarField3* image, float& dataWindowMin, float& dataWindowMax)
{
    image->setDataWindow(dataWindowMin, dataWindowMax);
}

void HxAdjustWindowLevelOfAmiraMesh::compute(){
    if (!portDoIt.wasHit())
        return;

    if (windowMax <= windowMin)
        throw McException(QString("Max window has to be greater than min!"));

    if (portInputDir.getFilename().isEmpty() || portInputDir.getFilename() == "Please select")
        throw McException(QString("Specify input directory!"));

    if (portOutputDir.getFilename().isEmpty() || portOutputDir.getFilename() == "Please select")
        throw McException(QString("Specify output directory!"));

    const QString inputDir(portInputDir.getFilename());
    const QString outputDir(portOutputDir.getFilename());

    if (!QDir(inputDir).exists())
        throw McException(QString("Input directory does not exist. \nPlease select valid input directory containing tif files."));

    if (!QDir(outputDir).exists())
        throw McException(QString("Output directory does not exist. \nPlease select valid output directory."));

    QStringList files = FilopodiaFunctions::getFileListFromDirectory(inputDir, "AM");
    QStringList filesInOutputDir = FilopodiaFunctions::getFileListFromDirectory(outputDir, "AM");

    if (files.size() == 0)
        throw McException(QString("No .am files in directory!"));

    if (filesInOutputDir.size() > 0)
    {
        const int answer = HxMessage::warning("Output directory is not empty. Files may be overwritten. \nContinue?", "Continue", "Cancel");
        if (answer == 1) return;
    }

    for (int f = 0; f < files.size(); ++f)
    {
        QString fileName = files[f];
        McHandle<HxUniformScalarField3> image = loadSingleAmFile(fileName);
        setWindowMinMax(image, windowMin, windowMax);
        QString outputFile = FilopodiaFunctions::getOutputFileName(fileName, outputDir);
        image->writeAmiraMeshBinary(qPrintable(outputFile));
        theMsg->printf(QString("Saving %1").arg(outputFile));
    }
}

void HxAdjustWindowLevelOfAmiraMesh::update(){

    McHandle<HxUniformScalarField3> firstImage;

    if (windowSize.isNew())
    {
        windowMin = windowSize.getValue(0);
        windowMax = windowSize.getValue(1);
    }

    if (portInputDir.isNew())
    {
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


    portInputDir.setMode(HxPortFilename::LOAD_DIRECTORY);
}
