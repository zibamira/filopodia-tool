#include "HxConvertHoc2AmiraMesh.h"
#include "FilopodiaFunctions.h"
#include <hxcore/internal/HxWorkArea.h>
#include <hxspatialgraph/internal/HxSpatialGraphIO.h>
#include <hxspatialgraph/internal/readHOC.h>
#include <mclib/McException.h>


HX_INIT_CLASS(HxConvertHoc2AmiraMesh, HxCompModule);

HxConvertHoc2AmiraMesh::HxConvertHoc2AmiraMesh () :
    HxCompModule(HxSpatialGraph::getClassTypeId()),
    portInputDir(this, "portinputdirectory", tr("Input Directory")),
    portOutputDir(this, "portOutputDirectory", tr("Output Directory"), HxPortFilename::SAVE_DIRECTORY),
    portDoIt(this, "action", tr("Action"))
{
    portData.hide();
    portInputDir.setValue("Please select");
    portOutputDir.setValue("Please select");
}

HxConvertHoc2AmiraMesh::~HxConvertHoc2AmiraMesh ()
{
}

void HxConvertHoc2AmiraMesh::compute() {

    if (!portDoIt.wasHit()) return;

    if (portInputDir.getFilename().isEmpty() || portInputDir.getFilename() == "Please select") {
        throw McException(QString("Specify input directory!"));
    }

    if (portOutputDir.getFilename().isEmpty() || portOutputDir.getFilename() == "Please select") {
        throw McException(QString("Specify output directory!"));
    }

    const QString inputDir(portInputDir.getFilename());
    const QString outputDir(portOutputDir.getFilename());

    if (!QDir(inputDir).exists()) {
        throw McException(QString("Input directory does not exist. \nPlease select valid input directory containing hoc files."));
    }

    if (!QDir(outputDir).exists()) {
        throw McException(QString("Output directory does not exist. \nPlease select valid output directory."));
    }

    QStringList files = FilopodiaFunctions::getFileListFromDirectory(inputDir, "HOC");
    QStringList filesInOutputDir = FilopodiaFunctions::getFileListFromDirectory(outputDir, "AM");

    if (files.size() == 0) {
        throw McException(QString("No hoc files in directory!"));
    }
    if (filesInOutputDir.size() > 0) {
        const int answer = HxMessage::warning("Output directory is not empty. Files may be overwritten. \nContinue?", "Continue", "Cancel");
        if (answer == 1) {
            return;
        }
    }

    for (int f=0; f<files.size(); ++f) {

        QString fileName = files[f];
        McHandle<HxSpatialGraph> spatialGraph = loadHOCToHxSpatialGraph(qPrintable(fileName))                                                                                                                           ;
        QString outputFile = FilopodiaFunctions::getOutputFileName(fileName, outputDir);
        spatialGraph->saveAmiraMeshBinary(qPrintable(outputFile));
        theMsg->printf(QString("Saving %1").arg(outputFile));
    }
}

void HxConvertHoc2AmiraMesh::update() {
    portInputDir.setMode(HxPortFilename::LOAD_DIRECTORY);
}
