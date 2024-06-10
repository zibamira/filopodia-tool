#include "HxRetraceFilopodia.h"
#include "FilopodiaFunctions.h"
#include <hxfield/HxUniformScalarField3.h>
#include <mclib/McException.h>
#include <hxcore/HxObjectPool.h>
#include <hxcore/internal/HxWorkArea.h>
#include <hxspatialgraph/internal/SpatialGraphSmoothOperation.h>

#include "ConvertVectorAndMcDArray.h"

HX_INIT_CLASS(HxRetraceFilopodia,HxCompModule)


HxRetraceFilopodia::HxRetraceFilopodia()
    : HxCompModule(HxSpatialGraph::getClassTypeId())
    , portImageDir(this, tr("ImageDirectory"), tr("Image Directory"), HxPortFilename::LOAD_DIRECTORY)
    , portIntensityWeight(this, tr("IntensityWeight"), tr("Intensity weight"), 1)
    , portTopBrightness(this, tr("TopBrightness"), tr("Top brightness"), 1)
    , portSmooth(this, tr("ApplySmoothing"), tr("Apply smoothing"), 1)
    , portSurroundWidthInVoxels(this, tr("SurroundWidth"), tr("Surround width (vx)"), 1)
    , portAction(this, tr("action"), tr("Action"))
{
    portIntensityWeight.setValue(50.0f);
    portTopBrightness.setValue(50);
    portSmooth.setValue(0, true);
    portSurroundWidthInVoxels.setValue(10);
}


HxRetraceFilopodia::~HxRetraceFilopodia()
{
}

typedef QMap<int, QFileInfo> FileInfoMap;

FileInfoMap updateFileList(const QString imageDir, const TimeMinMax timeMinMax) {

    FileInfoMap fileMap;

    const QDir dir(imageDir);
    QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files);

    for (int f=0; f<fileInfoList.size(); ++f) {
        const int time = FilopodiaFunctions::getTimeFromFileNameNoGC(fileInfoList[f].fileName());

        if (fileMap.contains(time)) {
            throw McException(QString("Duplicate timestamp %1").arg(time));
        }

        fileMap.insert(time, fileInfoList[f]);
    }

    const int missing = (timeMinMax.maxT-timeMinMax.minT + 1) - fileMap.size();
    if (missing != 0) {
        throw McException(QString("Warning: Missing %1 image files").arg(missing));
    }

    return fileMap;
}


float getEdgeLength(const HxSpatialGraph* graph, const int edgeNum) {
    float length = 0.0f;
    const std::vector<McVec3f> points = graph->getEdgePoints(edgeNum);

    for (int p=1; p<points.size(); ++p) {
        const float segmentLength = (points[p] - points[p-1]).length();
        length += segmentLength;
    }

    return length;
}


void printDifferenceStats(const HxSpatialGraph* graph1, const HxSpatialGraph* graph2) {
    if (graph1->getNumEdges() != graph2->getNumEdges()) {
        throw McException(QString("Cannot compute difference statistics: graphs have different number of edges"));
    }

    float totalLength1 = 0.0f;
    float totalLength2 = 0.0f;
    float totalAbsoluteDifferencePerEdge = 0.0f;
    float totalSignedDifferencePerEdge = 0.0f;
    float totalRelativeDifferencePerEdge = 0.0f;
    float totalRelativeSignedDifferencePerEdge = 0.0f;

    for (int e=0; e<graph1->getNumEdges(); ++e) {
        const float len1 = getEdgeLength(graph1, e);
        const float len2 = getEdgeLength(graph2, e);

        totalLength1 += len1;
        totalLength2 += len2;
        totalAbsoluteDifferencePerEdge += fabs(len2-len1);
        totalSignedDifferencePerEdge += (len2-len1);
        totalRelativeDifferencePerEdge += fabs(len2-len1)/len1;
        totalRelativeSignedDifferencePerEdge += (len2-len1)/len1;
    }

    theMsg->printf(QString("Total length original:\t%1").arg(totalLength1));
    theMsg->printf(QString("Total length retrace:\t%1").arg(totalLength2));
    theMsg->printf(QString("Total length difference:\t%1 (%2 %)").arg(totalLength2 - totalLength1)
                   .arg((totalLength2-totalLength1)*100.0/totalLength1));
    theMsg->printf(QString("Averages over all edges:"));
    theMsg->printf(QString("Average absolute difference:\t%1").arg(totalAbsoluteDifferencePerEdge/graph1->getNumEdges()));
    theMsg->printf(QString("Average signed difference:\t\t%1").arg(totalSignedDifferencePerEdge/graph1->getNumEdges()));
    theMsg->printf(QString("Average unsigned relative difference:\t%1 %").arg(totalRelativeDifferencePerEdge*100.0f/graph1->getNumEdges()));
    theMsg->printf(QString("Average signed relative difference:\t%1 %").arg(totalRelativeSignedDifferencePerEdge*100.0f/graph1->getNumEdges()));
}



void HxRetraceFilopodia::compute() {

    if (!portAction.wasHit()) {
        return;
    }

    const HxSpatialGraph* inputGraph = hxconnection_cast<HxSpatialGraph> (portData);
    if (!inputGraph) {
        throw McException(QString("No input graph"));
    }

    McHandle<HxSpatialGraph> outputGraph = dynamic_cast<HxSpatialGraph*>(getResult());
    if (outputGraph) {
        outputGraph->copyFrom(inputGraph);
    }
    else {
        outputGraph = inputGraph->duplicate();
    }

    outputGraph->composeLabel(inputGraph->getLabel(), "retraced");

    TimeMinMax timeMinMax = FilopodiaFunctions::getTimeMinMaxFromGraphLabels(inputGraph);

    const int numSteps = timeMinMax.maxT - timeMinMax.minT + 1;
    theWorkArea->startWorking(QString("Retracing..."));

    const QString imageDir = portImageDir.getValue();

    FileInfoMap imageFiles = updateFileList(imageDir, timeMinMax);

    for (int t=timeMinMax.minT; t<=timeMinMax.maxT; ++t) {
        theWorkArea->setProgressInfo(QString("Processing time step %1/%2").arg(t-timeMinMax.minT+1).arg(numSteps));
        theWorkArea->setProgressValue((t-timeMinMax.minT+1)/float(numSteps));

        const SpatialGraphSelection timeSel = FilopodiaFunctions::getSelectionForTimeStep(outputGraph, t);
        if (timeSel.getNumSelectedEdges() == 0) {
            theMsg->printf(QString("No edges for timestep %1").arg(t));
            continue;
        }

        McHandle<HxUniformScalarField3> image = FilopodiaFunctions::loadUniformScalarField3(imageFiles[t].filePath());
        if (!image) {
            throw McException(QString("Could not read image from file %1").arg(imageFiles[t].filePath()));
        }

        if (image->primType() == McPrimType::MC_FLOAT ||
            image->primType() == McPrimType::MC_DOUBLE)
        {
            FilopodiaFunctions::convertToUnsignedChar(image);
        }

        theWorkArea->setProgressValue((t-timeMinMax.minT+1)/float(numSteps));

        SpatialGraphPoint dummyIntersectionPoint;
        int dummyIntersectionNode;

        SpatialGraphSelection::Iterator it(timeSel);
        for (int e=it.edges.nextSelected(); e!=-1; e=it.edges.nextSelected()) {
            const int v1 = outputGraph->getEdgeSource(e);
            const int v2 = outputGraph->getEdgeTarget(e);

            const std::vector<McVec3f> newPoints =
                    FilopodiaFunctions::trace(outputGraph,
                                              image,
                                              outputGraph->getVertexCoords(v1),
                                              outputGraph->getVertexCoords(v2),
                                              portSurroundWidthInVoxels.getValue(),
                                              portIntensityWeight.getValue(),
                                              portTopBrightness.getValue(),
                                              SpatialGraphSelection(outputGraph),
                                              dummyIntersectionPoint,
                                              dummyIntersectionNode);

            if (!newPoints.front().equalsRelative(outputGraph->getVertexCoords(v1))) {
                throw McException("First new point does not match existing vertex");
            }
            if (!newPoints.back().equalsRelative(outputGraph->getVertexCoords(v2))) {
                throw McException("Last new point does not match existing vertex");
            }
            outputGraph->setEdgePoints(e, newPoints);
        }

        theObjectPool->removeObject(image);
    }

    if (portSmooth.getValue(0)) {
        SpatialGraphSelection selectedElements(outputGraph);
        selectedElements.selectAllVerticesAndEdges();

        SmoothOperation smoothOp(outputGraph, selectedElements, SpatialGraphSelection(outputGraph), 3, true, true, true);
        smoothOp.exec();
    }

    printDifferenceStats(inputGraph, outputGraph);

    setResult(outputGraph);

    theWorkArea->stopWorking();
}

