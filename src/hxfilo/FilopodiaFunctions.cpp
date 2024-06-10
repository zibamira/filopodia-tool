#include "FilopodiaFunctions.h"
#include "FilopodiaOperationSet.h"
#include "HxFieldCorrelation.h"
#include "HxShortestPathToPointMap.h"
#include <hxneuroneditor/internal/HxNeuronEditorSubApp.h>
#include <hxspatialgraph/internal/HxSpatialGraphIO.h>
#include <hxspatialgraph/internal/SpatialGraphFunctions.h>
// #include <hxcontour2surface/McDijkstra.h>
#include <mclib/McException.h>
#include <mclib/internal/McRandom.h>
#include <hxfield/HxUniformScalarField3.h>
#include <hxfield/HxUniformLabelField3.h>
#include <hxfield/HxLoc3Uniform.h>
#include <hxfield/HxFieldEvaluator.h>
#include <hxcore/internal/HxReadAmiraMesh.h>
#include <hxcore/HxObjectPool.h>
#include <hxspreadsheet/internal/HxSpreadSheet.h>
#include <mclib/McVec2.h>
#include <mclib/McVec4.h>
#include <mclib/McMat4.h>
#include <mclib/McPlane.h>
#include <mclib/internal/McAuxGrid.h>
#include <QDebug>
#include <QDirIterator>
#include <QScopedPointer>
#include <QTime>

#include "ConvertVectorAndMcDArray.h"

struct GaussianParameters2D
{
    GaussianParameters2D()
        : sigma(0.0f, 0.0)
        , mean(0.0f, 0.0f)
        , height(0.0f)
    {
    }
    McVec2f sigma;
    McVec2f mean;
    float height;
};

void
printDims(const McDim3l& dims, const QString& info = QString())
{
    qDebug() << info << dims.nx << dims.ny << dims.nz;
}

void
printVec(const McVec3f& v, const QString& info = QString())
{
    qDebug() << info << v.x << v.y << v.z;
}

void
printBox(const McBox3f& box, const QString& info = QString())
{
    qDebug() << info;
    qDebug() << box.getMin()[0] << box.getMin()[1] << box.getMin()[2];
    qDebug() << box.getMax()[0] << box.getMax()[1] << box.getMax()[2];
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// --------------------------------------------------------------------------------------------------------------
// ----------- Functions for File Reading  -----------
QStringList
FilopodiaFunctions::getFileListFromDirectory(const QString& dirName, const QString type)
{
    QStringList files;

    const QString inputDir(dirName);
    QStringList nameFilters;
    if (type == "TIF")
    {
        nameFilters.append("*.tif");
        nameFilters.append("*.tiff");
    }
    else if (type == "AM")
    {
        nameFilters.append("*.am");
    }
    else if (type == "HOC")
    {
        nameFilters.append("*.hoc");
    }
    else
    {
        throw McException("Unknown data type to read");
    }

    QDirIterator it(inputDir, nameFilters, QDir::NoFilter, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        it.next();
        const QString filename(it.filePath());
        files.append(filename);
    }

    return files;
}

QString
FilopodiaFunctions::getFileNameFromPath(QString filePath)
{
    QFileInfo file(filePath);
    return file.fileName();
}

QString
FilopodiaFunctions::getOutputFileName(const QString& fileName,
                                      const QString& outputDir)
{
    QString name = FilopodiaFunctions::getFileNameFromPath(fileName);
    const QString path = outputDir + QDir::separator() + name;
    QString outputFile = QDir::cleanPath(path);
    outputFile.remove(".tif");
    outputFile.remove(".hoc");
    outputFile.remove(".am");
    outputFile += ".am";

    return outputFile;
}

HxUniformScalarField3*
FilopodiaFunctions::loadUniformScalarField3(const QString& fileName)
{
    if (!fileName.isEmpty())
    {
        std::vector<HxData*> result(0);
        auto tempResult = convertVector2McDArray(result);
        if (readAmiraMesh(fileName, tempResult))
        {
            HxData* data = tempResult[0];
            if (data->isOfType(HxUniformScalarField3::getClassTypeId()))
            {
                return dynamic_cast<HxUniformScalarField3*>(data);
            }
            else
            {
                theObjectPool->removeObject(data);
            }
        }
    }
    return 0;
}

HxUniformLabelField3*
FilopodiaFunctions::loadUniformLabelField3(const QString& fileName)
{
    if (!fileName.isEmpty())
    {
        std::vector<HxData*> result(0);
        // TEMP!
        auto tempResult = convertVector2McDArray(result);
        if (readAmiraMesh(fileName, tempResult))
        {
            HxData* data = tempResult[0];
            if (data->isOfType(HxUniformLabelField3::getClassTypeId()))
            {
                return dynamic_cast<HxUniformLabelField3*>(data);
            }
            else
            {
                theObjectPool->removeObject(data);
            }
        }
    }
    return 0;
}
HxSpatialGraph*
FilopodiaFunctions::readSpatialGraph(const QFileInfo& fileInfo)
{
    const QString path = fileInfo.absoluteFilePath();
    if (!fileInfo.exists())
    {
        throw McException(QString("File does not exist: %1").arg(path));
    }

    if (fileInfo.suffix() != QString("am"))
    {
        throw McException(QString("File is not a SpatialGraph file: %1").arg(path));
    }

    AmiraMesh::recordFilePosition = true;
    AmiraMesh* m = AmiraMesh::read(fileInfo.absoluteFilePath());
    if (!m)
    {
        throw McException(QString("Could not read .am file %1.").arg(path));
    }

    QString content;
    m->parameters.findString(QString("ContentType"), content);
    if (content != QString("HxSpatialGraph"))
    {
        delete m;
        throw McException(QString("Error: File is not of Content type HxSpatialGraph:").arg(path));
    }
    for (int dataNum = 0; dataNum < m->dataList.size(); ++dataNum)
    {
        m->dataList[dataNum]->readData();
    }
    AmiraMesh::recordFilePosition = false;

    HxSpatialGraph* graph = HxSpatialGraph::createInstance();
    if (!HxSpatialGraphIO::load(graph, m, qPrintable(path)))
    {
        delete m;
        throw McException(QString("Error reading file: %1").arg(path));
    }

    delete m;
    return graph;
}

void
FilopodiaFunctions::convertToUnsignedChar(HxUniformScalarField3* inputField)
{
    HxLattice3& lat = inputField->lattice();

    float minVal, maxVal;
    inputField->getRange(minVal, maxVal);

    bool doScale = false;
    float scaleFactor = 1.0f;

    if (minVal < 0.0f || maxVal > 255.0f)
    {
        doScale = true;
        if (maxVal > minVal)
        {
            scaleFactor = 255.0f / (maxVal - minVal);
        }
    }

    McHandle<HxLattice3> copy = lat.duplicate();
    lat.setPrimType(McPrimType::MC_UINT8);

    const McDim3l& dims = lat.getDims();
    float val;
    for (int z = 0; z < dims.nz; ++z)
    {
        for (int y = 0; y < dims.ny; ++y)
        {
            for (int x = 0; x < dims.nx; ++x)
            {
                copy->eval(x, y, z, &val);
                if (doScale)
                {
                    val = (val - minVal) * scaleFactor;
                }
                lat.set(x, y, z, &val);
            }
        }
    }
}

bool
getTimeFromRegExpNoGC(const QRegExp& rx, const QString& path, int& time)
{
    const int pos = rx.indexIn(path);
    if (pos == -1)
    {
        return false;
    }

    const QStringList capturedTexts = rx.capturedTexts();

    if (capturedTexts.size() == 2)
    {
        time = capturedTexts[1].toInt();
    }
    else
    {
        throw McException("Error extracting time from file name: error parsing path");
    }

    if (rx.indexIn(path, pos + rx.matchedLength()) != -1)
    {
        throw McException("Error extracting time from file name: multiple matches");
    }

    return true;
}

bool
getGrowthConeNumberAndTimeFromRegExp(const QRegExp& rx, const QString& path, int& gc, int& time)
{
    const int pos = rx.indexIn(path);
    if (pos == -1)
    {
        return false;
    }

    const QStringList capturedTexts = rx.capturedTexts();
    if (capturedTexts.size() == 3)
    {
        gc = capturedTexts[1].toInt();
        time = capturedTexts[2].toInt();
    }
    else if (capturedTexts.size() == 2)
    {
        gc = capturedTexts[1].toInt();
    }
    else
    {
        throw McException("Error extracting growth cone number and time from file name: error parsing path");
    }

    if (rx.indexIn(path, pos + rx.matchedLength()) != -1)
    {
        throw McException("Error extracting growth cone number and time from file name: multiple matches");
    }

    return true;
}

void
FilopodiaFunctions::getGrowthConeNumberAndTimeFromFileName(const QString& path, int& gc, int& time)
{
    // Standard format: "*_gc001_t000.am"
    QRegExp rx("_gc([0-9]{1,3})_t([0-9]{1,3})\\.am");
    if (getGrowthConeNumberAndTimeFromRegExp(rx, path, gc, time))
    {
        return;
    }

    QRegExp rx2("GrowthCone([0-9])_([0-9]{1,2})");
    if (getGrowthConeNumberAndTimeFromRegExp(rx2, path, gc, time))
    {
        theMsg->printf(QString("Warning: time and growth cone number in non-standard format: %1").arg(path));
        return;
    }

    QRegExp rx3("_([0-9]{1,2})_GrowthCone([0-9])");
    if (getGrowthConeNumberAndTimeFromRegExp(rx3, path, time, gc))
    {
        theMsg->printf(QString("Warning: time and growth cone number in non-standard format: %1").arg(path));
        return;
    }
    QRegExp rx4("_gc([0-9]{1,3})\\.am");
    if (getGrowthConeNumberAndTimeFromRegExp(rx4, path, gc, time))
    {
        return;
    }
    throw McException(QString("Error extracting growth cone number and time from file name: invalid file name format (%1)").arg(path));
}

int
FilopodiaFunctions::getGrowthConeNumberFromFileName(const QString& path)
{
    int gc, time;
    getGrowthConeNumberAndTimeFromFileName(path, gc, time);
    return gc;
}

int
FilopodiaFunctions::getTimeFromFileName(const QString& path)
{
    int gc, time;
    getGrowthConeNumberAndTimeFromFileName(path, gc, time);
    return time;
}

int
FilopodiaFunctions::getTimeFromFileNameNoGC(const QString& path)
{
    int time;
    // Standard format: "_t000.am"
    QRegExp rx("_t([0-9]{1,3})\\.am");
    if (getTimeFromRegExpNoGC(rx, path, time))
    {
        return time;
    }
    QRegExp rx2("_t([0-9]{1,3})");
    if (getTimeFromRegExpNoGC(rx2, path, time)) //inoperative
    {
        return time;
    }
    QRegExp rx3("_T([0-9]{1,3})\\.am");
    if (getTimeFromRegExpNoGC(rx3, path, time))
    {
        return time;
    }
    throw McException(QString("Error extracting time from file name: invalid file name format. Path: %1").arg(path));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// --------------------------------------------------------------------------------------------------------------
// ----------- Functions for labels -----------

bool
FilopodiaFunctions::isFilopodiaGraph(const HxSpatialGraph* graph)
{
    if (!graph)
    {
        return false;
    }
    const HierarchicalLabels* filopodiaLabels = graph->getLabelGroup(FilopodiaFunctions::getFilopodiaAttributeName());
    const HierarchicalLabels* timeLabels = graph->getLabelGroup(FilopodiaFunctions::getTimeStepAttributeName());
    HierarchicalLabels* nodeTypeLabels = graph->getLabelGroup(FilopodiaFunctions::getTypeLabelAttributeName());
    const HierarchicalLabels* manualGeometryLabels = graph->getLabelGroup(FilopodiaFunctions::getManualGeometryLabelAttributeName());
    HierarchicalLabels* locationLabels = graph->getLabelGroup(FilopodiaFunctions::getLocationAttributeName());
    const HierarchicalLabels* manualMatchLabels = graph->getLabelGroup(FilopodiaFunctions::getManualNodeMatchAttributeName());
    HierarchicalLabels* growthConeLabels = graph->getLabelGroup(FilopodiaFunctions::getGrowthConeAttributeName());

    if (!(filopodiaLabels &&
          timeLabels &&
          nodeTypeLabels &&
          manualGeometryLabels &&
          locationLabels &&
          manualMatchLabels &&
          growthConeLabels))
    {
        return false;
    }

    if (FilopodiaFunctions::getFilopodiaLabelId(graph, IGNORED) == -1 ||
        FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED) == -1 ||
        FilopodiaFunctions::getFilopodiaLabelId(graph, AXON) == -1)
    {
        return false;
    }

    if (FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC) == -1 ||
        FilopodiaFunctions::getManualGeometryLabelId(graph, MANUAL) == -1)
    {
        return false;
    }

    if (FilopodiaFunctions::getMatchLabelId(graph, IGNORED) == -1 ||
        FilopodiaFunctions::getMatchLabelId(graph, UNASSIGNED) == -1 ||
        FilopodiaFunctions::getMatchLabelId(graph, AXON) == -1)
    {
        return false;
    }

    if (nodeTypeLabels->hasLabel("STARTING_NODE"))
    {
        qDebug() << "\trename node type label \tSTARTING_NODE -> BASE_NODE";
        const int startId = nodeTypeLabels->getLabelIdFromName("STARTING_NODE");
        nodeTypeLabels->setLabelName(startId, "BASE_NODE");
    }
    if (nodeTypeLabels->hasLabel("ENDING_NODE"))
    {
        qDebug() << "\trename node type label \tENDING_NODE -> TIP_NODE";
        const int endId = nodeTypeLabels->getLabelIdFromName("ENDING_NODE");
        nodeTypeLabels->setLabelName(endId, "TIP_NODE");
    }

    if (FilopodiaFunctions::getTypeLabelId(graph, ROOT_NODE) == -1 ||
        FilopodiaFunctions::getTypeLabelId(graph, TIP_NODE) == -1 ||
        FilopodiaFunctions::getTypeLabelId(graph, BASE_NODE) == -1 ||
        FilopodiaFunctions::getTypeLabelId(graph, BRANCHING_NODE) == -1)
    {
        return false;
    }

    if (locationLabels->hasLabel("INSIDE"))
    {
        qDebug() << "\trename location label \tINSIDE -> GROWTHCONE";
        const int growthLabel = locationLabels->getLabelIdFromName("INSIDE");
        locationLabels->setLabelName(growthLabel, "GROWTHCONE");
    }
    if (locationLabels->hasLabel("OUTSIDE"))
    {
        qDebug() << "\trename location label \tOUTSIDE -> FILOPODIUM";
        const int filoLabel = locationLabels->getLabelIdFromName("OUTSIDE");
        locationLabels->setLabelName(filoLabel, "FILOPODIUM");
    }

    if (FilopodiaFunctions::getLocationLabelId(graph, GROWTHCONE) == -1 ||
        FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM) == -1)
    {
        return false;
    }

    for (int l=1; l<growthConeLabels->getNumLabels(); ++l)
    {
        QString oldName = QString("Object_%1").arg(l, 3, 10, QChar('0'));
        if (growthConeLabels->hasLabel(oldName))
        {
            QString newName = QString("GC_%1").arg(l, 3, 10, QChar('0'));
            const int obj = growthConeLabels->getLabelIdFromName(oldName);
            growthConeLabels->setLabelName(obj, newName);
            qDebug() << "\trename growthcone label" << oldName << "->" << newName;
        }
    }

    return true;
}

bool
FilopodiaFunctions::isRootNodeGraph(const HxSpatialGraph* graph)
{
    if (!graph)
    {
        return false;
    }
    const HierarchicalLabels* timeLabels = graph->getLabelGroup(FilopodiaFunctions::getTimeStepAttributeName());
    const HierarchicalLabels* nodeTypeLabels = graph->getLabelGroup(FilopodiaFunctions::getTypeLabelAttributeName());
    const HierarchicalLabels* growthConeLabels = graph->getLabelGroup(FilopodiaFunctions::getGrowthConeAttributeName());

    if (!(timeLabels &&
          nodeTypeLabels &&
          growthConeLabels))
    {
        return false;
    }

    if (FilopodiaFunctions::getTypeLabelId(graph, ROOT_NODE) == -1 ||
        FilopodiaFunctions::getTypeLabelId(graph, TIP_NODE) == -1 ||
        FilopodiaFunctions::getTypeLabelId(graph, BASE_NODE) == -1 ||
        FilopodiaFunctions::getTypeLabelId(graph, BRANCHING_NODE) == -1)
    {
        return false;
    }

    return true;
}

void
FilopodiaFunctions::addAllFilopodiaLabelGroups(HxSpatialGraph* graph, TimeMinMax timeMinMax)
{
    FilopodiaFunctions::addGrowthConeLabelAttribute(graph);
    FilopodiaFunctions::addTimeLabelAttribute(graph, timeMinMax);
    FilopodiaFunctions::addManualGeometryLabelAttribute(graph);
    FilopodiaFunctions::addTypeLabelAttribute(graph);
    FilopodiaFunctions::addManualNodeMatchLabelAttribute(graph);
    FilopodiaFunctions::addLocationLabelAttribute(graph);
    FilopodiaFunctions::addFilopodiaLabelAttribute(graph);
}

bool
FilopodiaFunctions::checkIfGraphHasAttribute(const HxSpatialGraph* graph, const char* attributeName)
{
    const HierarchicalLabels* label = graph->getLabelGroup(attributeName);

    if (!label)
    {
        return false;
    }
    else
    {
        return true;
    }

    return false;
}

// ----------- Functions for Time -----------
const char*
FilopodiaFunctions::getTimeStepAttributeName()
{
    return "TimeStep";
}

SbColor
FilopodiaFunctions::getColorForTime(const int t, const int minT, const int maxT)
{
    if (minT == maxT)
    {
        return SbColor(1.0f, 0.0f, 0.0f);
    }
    const float gb = float(t - minT) / float(maxT - minT);
    return SbColor(1.0f, 1.0f - gb, 1.0f - gb);
}

HierarchicalLabels*
FilopodiaFunctions::addTimeLabelAttribute(HxSpatialGraph* graph, const TimeMinMax timeMinMax)
{
    const char* labelAttName = getTimeStepAttributeName();
    EdgeVertexAttribute* vAtt = graph->findVertexAttribute(labelAttName);
    EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(labelAttName);
    if (!vAtt && !eAtt)
    {
        graph->addNewLabelGroup(labelAttName, true, true, false);

        for (int v = 0; v < graph->getNumVertices(); ++v)
        {
            FilopodiaFunctions::setTimeIdOfNode(graph, v, 1);
        }

        for (int e = 0; e < graph->getNumEdges(); ++e)
        {
            FilopodiaFunctions::setTimeIdOfEdge(graph, e, 1);
        }
    }
    else if (vAtt && !eAtt)
    {
        throw McException(("Time attribute already exists on vertices (not on edges)"));
    }
    else if (!vAtt && eAtt)
    {
        throw McException(("Time attribute already exists on edges (not on vertices)"));
    }

    HierarchicalLabels* labels = graph->getLabelGroup(labelAttName);

    for (int t = timeMinMax.minT; t <= timeMinMax.maxT; ++t)
    {
        const QString labelName = FilopodiaFunctions::getLabelNameFromTime(t);
        const SbColor color = getColorForTime(t, timeMinMax.minT, timeMinMax.maxT);
        if (!labels->hasLabel(McString(labelName)))
        {
            labels->addLabel(0, qPrintable(labelName), color);
        }
    }

    return labels;
}

QString
FilopodiaFunctions::getLabelNameFromTime(const int time)
{
    return QString("T_%1").arg(time, 3, 10, QChar('0'));
}

int
FilopodiaFunctions::getTimeFromLabelName(const McString& labelName)
{
    if (labelName.startsWith("T_") && labelName.length() == 5)
    {
        const McString digitStr = labelName.substr(2, 4);
        if (digitStr.isDigit())
        {
            bool ok;
            const int time = digitStr.toInt(ok);
            if (ok)
            {
                return time;
            }
        }
    }
    throw McException(QString("Invalid time label: %1").arg(QString::fromLatin1(labelName.getString())));
}

int
FilopodiaFunctions::getTimeIdFromTimeStep(const HxSpatialGraph* graph, const int timestep)
{
    const char* timeAttName = getTimeStepAttributeName();
    const HierarchicalLabels* labelGroup = graph->getLabelGroup(timeAttName);
    const QString timeLabelName = getLabelNameFromTime(timestep);
    return labelGroup->getLabelIdFromName(McString(timeLabelName));
}

int
FilopodiaFunctions::getTimeStepFromTimeId(const HxSpatialGraph* graph,
                                          const int timeId)
{
    const char* timeAttName = getTimeStepAttributeName();
    const HierarchicalLabels* labelGroup = graph->getLabelGroup(timeAttName);
    McString timeLabelName;
    if (!labelGroup->getLabelName(timeId, timeLabelName))
    {
        throw McException(QString("Invalid time label id: %1").arg(timeId));
    }
    return getTimeFromLabelName(timeLabelName);
}

TimeMinMax
FilopodiaFunctions::getTimeMinMaxFromGraphLabels(const HxSpatialGraph* graph)
{
    if (!graph)
    {
        throw McException(QString("FilopodiaFunctions::getTimeMinMax: No graph."));
    }

    const HierarchicalLabels* labels = graph->getLabelGroup(getTimeStepAttributeName());

    if (!labels)
    {
        throw McException(QString("FilopodiaFunctions::getTimeMinMax: No time step labels."));
    }

    TimeMinMax tMinMax;
    tMinMax.minT = std::numeric_limits<int>::max();
    tMinMax.maxT = std::numeric_limits<int>::min();

    const McDArray<int> ids = labels->getChildIds(0);
    for (int i = 0; i < ids.size(); ++i)
    {
        if (ids[i] == 0)
        {
            continue;
        }
        McString label;
        const bool ok = labels->getLabelName(ids[i], label);
        if (!ok)
        {
            throw McException("Error in getTimeMinMaxFromGraphLabels: cannot get label name.");
        }
        const int t = getTimeFromLabelName(label);
        if (t < tMinMax.minT)
        {
            tMinMax.minT = t;
        }
        if (t > tMinMax.maxT)
        {
            tMinMax.maxT = t;
        }
    }
    return tMinMax;
}

int
FilopodiaFunctions::getTimeIdOfNode(const HxSpatialGraph* graph, const int v)
{
    EdgeVertexAttribute* timeAtt = graph->findVertexAttribute(FilopodiaFunctions::getTimeStepAttributeName());
    if (!timeAtt)
    {
        throw McException(QString("No attribute found with name %1").arg(QString::fromLatin1(FilopodiaFunctions::getTimeStepAttributeName())));
    }
    return timeAtt->getIntDataAtIdx(v);
}

void
FilopodiaFunctions::setTimeIdOfNode(HxSpatialGraph* graph, const int v, const int timeId)
{
    EdgeVertexAttribute* att = graph->findVertexAttribute(getTimeStepAttributeName());
    if (!att)
    {
        throw McException("FilopodiaFunctions::setTimeIdOfNode: no time attribute available");
    }
    att->setIntDataAtIdx(v, timeId);
}

int
FilopodiaFunctions::getTimeOfNode(const HxSpatialGraph* graph, const int v)
{
    const int timeId = getTimeIdOfNode(graph, v);
    return getTimeStepFromTimeId(graph, timeId);
}

int
FilopodiaFunctions::getTimeOfEdge(const HxSpatialGraph* graph, const int e)
{
    const int timeId = getTimeIdOfEdge(graph, e);
    return getTimeStepFromTimeId(graph, timeId);
}

void
FilopodiaFunctions::setTimeIdOfEdge(HxSpatialGraph* graph, const int e, const int timeId)
{
    EdgeVertexAttribute* att = graph->findEdgeAttribute(getTimeStepAttributeName());
    if (!att)
    {
        throw McException("FilopodiaFunctions::setTimeIdOfEdge: no time attribute available");
    }
    att->setIntDataAtIdx(e, timeId);
}

int
FilopodiaFunctions::getTimeIdOfEdge(const HxSpatialGraph* graph, const int s)
{
    EdgeVertexAttribute* timeAtt = graph->findEdgeAttribute(FilopodiaFunctions::getTimeStepAttributeName());
    if (!timeAtt)
    {
        throw McException(QString("No attribute found with name %1").arg(QString::fromLatin1(FilopodiaFunctions::getTimeStepAttributeName())));
    }
    return timeAtt->getIntDataAtIdx(s);
}

SpatialGraphSelection
FilopodiaFunctions::getSelectionForTimeStep(const HxSpatialGraph* graph, const int timeStep)
{
    const int timeLabel = getTimeIdFromTimeStep(graph, timeStep);

    SpatialGraphSelection sel(graph);

    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        const int t = FilopodiaFunctions::getTimeIdOfNode(graph, v);
        if (t == timeLabel)
        {
            sel.selectVertex(v);
        }
    }

    for (int e = 0; e < graph->getNumEdges(); ++e)
    {
        const int t = FilopodiaFunctions::getTimeIdOfEdge(graph, e);
        if (t == timeLabel)
        {
            sel.selectEdge(e);
        }
    }

    return sel;
}

// ----------- Functions for Node Type -----------
const char*
FilopodiaFunctions::getTypeLabelAttributeName()
{
    return "NodeType";
}

void
FilopodiaFunctions::setTypeLabelsForAllNodes(HxSpatialGraph* graph, int rootNode)
{
    addTypeLabelAttribute(graph);

    bool foundRoot = false;

    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        if (v == rootNode)
        {
            setNodeType(graph, v, ROOT_NODE);
            foundRoot = true;
        }
        else if (SpatialGraphFunctions::isBranchingNode(graph, v))
        {
            setNodeType(graph, v, BRANCHING_NODE);
        }
        else if (SpatialGraphFunctions::isEndingNode(graph, v))
        {
            setNodeType(graph, v, TIP_NODE);
        }
        else if (graph->getIncidentEdges(v).size() == 0 && rootNode != -1)
        {
            throw McException(QString("Unexpected isolated node %1").arg(v));
        }
        else if (graph->getIncidentEdges(v).size() == 2)
        {
            setNodeType(graph, v, BASE_NODE);
        }
    }
    if (foundRoot == false && rootNode != -1)
    {
        throw McException(QString("Root was not found"));
    }
}

void
FilopodiaFunctions::addTypeLabelAttribute(HxSpatialGraph* graph)
{
    const char* labelAttName = getTypeLabelAttributeName();

    EdgeVertexAttribute* vAtt = graph->findVertexAttribute(labelAttName);

    const McString baseLabel = getTypeLabelName(BASE_NODE);
    const McString tipLabel = getTypeLabelName(TIP_NODE);
    const McString rootLabel = getTypeLabelName(ROOT_NODE);
    const McString branchLabel = getTypeLabelName(BRANCHING_NODE);

    HierarchicalLabels* labelGroup;

    if (vAtt)
    {
        labelGroup = graph->getLabelGroup(vAtt);
        if (!labelGroup->hasLabel(rootLabel))
        {
            labelGroup->addLabel(0, rootLabel, SbColor(0.0f, 0.0f, 0.0f));
        }
        if (!labelGroup->hasLabel(tipLabel))
        {
            labelGroup->addLabel(0, tipLabel, SbColor(255.0f, 165.0f, 0.0f));
        }
        if (!labelGroup->hasLabel(branchLabel))
        {
            labelGroup->addLabel(0, branchLabel, SbColor(1.0f, 1.0f, 0.0f));
        }
        if (!labelGroup->hasLabel(baseLabel))
        {
            labelGroup->addLabel(0, baseLabel, SbColor(0.0f, 1.0f, 0.0f));
        }
    }
    else
    {
        labelGroup = graph->addNewLabelGroup(labelAttName, false, true, false);
        labelGroup->addLabel(0, getTypeLabelName(TIP_NODE), SbColor(255.0f, 165.0f, 0.0f));
        labelGroup->addLabel(0, getTypeLabelName(BRANCHING_NODE), SbColor(1.0f, 1.0f, 0.0f));
        labelGroup->addLabel(0, getTypeLabelName(ROOT_NODE), SbColor(0.0f, 0.0f, 1.0f));
        labelGroup->addLabel(0, getTypeLabelName(BASE_NODE), SbColor(0.0f, 1.0f, 0.0f));

        vAtt = graph->findVertexAttribute(labelAttName);

        FilopodiaFunctions::setTypeLabelsForAllNodes(graph, -1);
    }
}

const char*
FilopodiaFunctions::getTypeLabelName(const FilopodiaNodeType nodeType)
{
    switch (nodeType)
    {
        case TIP_NODE:
            return "TIP_NODE";
        case BASE_NODE:
            return "BASE_NODE";
        case BRANCHING_NODE:
            return "BRANCHING_NODE";
        case ROOT_NODE:
            return "ROOT_NODE";
        default:
            throw McException(QString("FilopodiaFunctions::getTypeLabelName: Label not found"));
    }
}

int
FilopodiaFunctions::getTypeLabelId(const HxSpatialGraph* graph, const FilopodiaNodeType nodeType)
{
    if (!graph)
    {
        throw McException(QString("FilopodiaFunctions::getTypeLabelId: no graph"));
    }

    const HierarchicalLabels* labels = graph->getLabelGroup(getTypeLabelAttributeName());
    if (!labels)
    {
        throw McException(QString("FilopodiaFunctions::getTypeLabelId: graph has no node labels"));
    }

    return labels->getLabelIdFromName(McString(getTypeLabelName(nodeType)));
}

int
FilopodiaFunctions::getRootNodeFromTimeStep(const HxSpatialGraph* graph, const int time)
{
    int root = -1;
    SpatialGraphSelection timeSel = FilopodiaFunctions::getSelectionForTimeStep(graph, time);
    SpatialGraphSelection rootsOfTime = FilopodiaFunctions::getNodesOfTypeInSelection(graph, timeSel, ROOT_NODE);

    if (rootsOfTime.getNumSelectedVertices() == 1)
    {
        root = rootsOfTime.getSelectedVertex(0);
    }

    return root;
}

int
FilopodiaFunctions::getRootNodeFromTimeAndGC(const HxSpatialGraph* graph, const int time, const int gcId)
{
    SpatialGraphSelection timeSel = FilopodiaFunctions::getNodesOfTypeForTime(graph, ROOT_NODE, time);
    SpatialGraphSelection::Iterator it(timeSel);
    for (int v = it.vertices.nextSelected(); v != -1; v = it.vertices.nextSelected())
    {
        if (FilopodiaFunctions::getGcIdOfNode(graph, v) == gcId)
        {
            return v;
        }
    }

    return -1;
}

SpatialGraphSelection
FilopodiaFunctions::getNodesOfTypeForTime(const HxSpatialGraph* graph,
                                          const FilopodiaNodeType type,
                                          const int time)
{
    SpatialGraphSelection result(graph);
    SpatialGraphSelection timeSel = FilopodiaFunctions::getSelectionForTimeStep(graph, time);

    SpatialGraphSelection::Iterator it(timeSel);
    for (int v = it.vertices.nextSelected(); v != -1; v = it.vertices.nextSelected())
    {
        if (FilopodiaFunctions::hasNodeType(graph, type, v))
        {
            result.selectVertex(v);
        }
    }
    return result;
}

SpatialGraphSelection
FilopodiaFunctions::getNodesOfTypeInSelection(const HxSpatialGraph* graph,
                                              const SpatialGraphSelection& sel,
                                              const FilopodiaNodeType type)
{
    SpatialGraphSelection::Iterator it(sel);
    SpatialGraphSelection result(graph);
    for (int v = it.vertices.nextSelected(); v != -1; v = it.vertices.nextSelected())
    {
        if (FilopodiaFunctions::hasNodeType(graph, type, v))
        {
            result.selectVertex(v);
        }
    }

    return result;
}

SpatialGraphSelection
FilopodiaFunctions::getNodesOfType(const HxSpatialGraph* graph,
                                   const FilopodiaNodeType type)
{
    SpatialGraphSelection result(graph);
    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        if (FilopodiaFunctions::hasNodeType(graph, type, v))
        {
            result.selectVertex(v);
        }
    }

    return result;
}

int
FilopodiaFunctions::getTypeIdOfNode(const HxSpatialGraph* graph, const int v)
{
    EdgeVertexAttribute* vTypeAtt = graph->findVertexAttribute(FilopodiaFunctions::getTypeLabelAttributeName());
    if (!vTypeAtt)
    {
        throw McException(QString("No attribute found with name %1").arg(QString::fromLatin1(FilopodiaFunctions::getTypeLabelAttributeName())));
    }
    return vTypeAtt->getIntDataAtIdx(v);
}

bool
FilopodiaFunctions::hasNodeType(const HxSpatialGraph* graph, const FilopodiaNodeType nodeType, const int node)
{
    const EdgeVertexAttribute* typeAtt = graph->findVertexAttribute(getTypeLabelAttributeName());
    if (!typeAtt)
    {
        throw McException("FilopodiaFunctions::hasNodeType: no Node Type attribute available");
    }
    return typeAtt->getIntDataAtIdx(node) == getTypeLabelId(graph, nodeType);
}

void
FilopodiaFunctions::setNodeType(HxSpatialGraph* graph, const int node, const FilopodiaNodeType nodeType)
{
    EdgeVertexAttribute* att = graph->findVertexAttribute(getTypeLabelAttributeName());
    if (!att)
    {
        throw McException("FilopodiaFunctions::setNodeType: no Node Type attribute available");
    }
    att->setIntDataAtIdx(node, getTypeLabelId(graph, nodeType));
}

// ----------- Functions for Filopodia IDs -----------
const char*
FilopodiaFunctions::getFilopodiaAttributeName()
{
    return "Filopodia";
}

void
FilopodiaFunctions::addFilopodiaLabelAttribute(HxSpatialGraph* graph)
{
    const char* attName = getFilopodiaAttributeName();
    EdgeVertexAttribute* vAtt = graph->findVertexAttribute(getFilopodiaAttributeName());
    EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(getFilopodiaAttributeName());

    if (vAtt && eAtt)
    {
        HierarchicalLabels* labels = graph->getLabelGroup(attName);
        const QString unassigned = getFilopodiaLabelName(UNASSIGNED);
        const QString ignored = getFilopodiaLabelName(IGNORED);
        const QString axon = getFilopodiaLabelName(AXON);

        if (!labels->hasLabel(unassigned))
        {
            labels->addLabel(0, qPrintable(unassigned), SbColor(224.0f, 238.0f, 238.0f));
        }
        if (!labels->hasLabel(qPrintable(unassigned)))
        {
            labels->addLabel(0, qPrintable(ignored), SbColor(0.0f, 0.0f, 139.0f));
        }
        if (!labels->hasLabel(axon))
        {
            labels->addLabel(0, qPrintable(axon), SbColor(211.0f, 211.0f, 211.0f));
        }
    }
    else if (!vAtt && !eAtt)
    {
        addOrClearFilopodiaLabelAttribute(graph);
    }
    else
    {
        throw McException("Invalid existing Filopodia labels");
    }
}

void
FilopodiaFunctions::addOrClearFilopodiaLabelAttribute(HxSpatialGraph* graph)
{
    HierarchicalLabels* filopodiaLabels = graph->getLabelGroup(getFilopodiaAttributeName());
    if (filopodiaLabels)
    {
        filopodiaLabels->removeChildLabels();
    }
    else
    {
        filopodiaLabels = graph->addNewLabelGroup(getFilopodiaAttributeName(), true, true, false);
    }

    const int unassignedLabel = filopodiaLabels->addLabel(0, qPrintable(getFilopodiaLabelName(UNASSIGNED)), SbColor(224.0f, 238.0f, 238.0f));
    filopodiaLabels->addLabel(0, qPrintable(getFilopodiaLabelName(IGNORED)), SbColor(0.0f, 0.0f, 139.0f));
    filopodiaLabels->addLabel(0, qPrintable(getFilopodiaLabelName(AXON)), SbColor(211.0f, 211.0f, 211.0f));

    EdgeVertexAttribute* vAtt = graph->findVertexAttribute(getFilopodiaAttributeName());
    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        vAtt->setIntDataAtIdx(v, unassignedLabel);
    }

    EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(getFilopodiaAttributeName());
    for (int e = 0; e < graph->getNumEdges(); ++e)
    {
        eAtt->setIntDataAtIdx(e, unassignedLabel);
    }
}

int
FilopodiaFunctions::getFilopodiaLabelId(const HxSpatialGraph* graph, const MatchLabel filopodia)
{
    if (!graph)
    {
        throw McException(QString("FilopodiaFunctions::getFilopodiaLabelId: no graph"));
    }

    const HierarchicalLabels* labels = graph->getLabelGroup(getFilopodiaAttributeName());
    if (!labels)
    {
        throw McException(QString("FilopodiaFunctions::getFilopodiaLabelId: graph has no filopodia labels"));
    }

    return labels->getLabelIdFromName(McString(getFilopodiaLabelName(filopodia)));
}

QString
FilopodiaFunctions::getFilopodiaLabelName(const MatchLabel label)
{
    switch (label)
    {
        case UNASSIGNED:
            return QString("UNASSIGNED");
        case IGNORED:
            return QString("IGNORED");
        case AXON:
            return QString("AXON");
        default:
            throw McException(QString("FilopodiaFunctions::getNodeMatchLabelName: Label not found"));
    }
}

int
FilopodiaFunctions::getFilopodiaIdFromLabelName(const HxSpatialGraph* graph, const QString& label)
{
    const char* filoAttName = FilopodiaFunctions::getFilopodiaAttributeName();
    HierarchicalLabels* filoLabels = graph->getLabelGroup(filoAttName);
    if (!filoLabels)
    {
        throw McException(QString("No filopodia labels found (getFilopodiaIdFromLabelName)"));
    }

    const McString name(label);
    return filoLabels->getLabelIdFromName(name);
}

QString
FilopodiaFunctions::getFilopodiaLabelNameFromNumber(const int number)
{
    return QString("F_%1").arg(number - 3, 4, 10, QChar('0'));
}

QString
FilopodiaFunctions::getFilopodiaLabelNameFromID(const HxSpatialGraph* graph, const int ID)
{
    const char* filoAttName = getFilopodiaAttributeName();
    const HierarchicalLabels* filoLabelGroup = graph->getLabelGroup(filoAttName);
    McString labelName;
    filoLabelGroup->getLabelName(ID, labelName);
    return QString::fromLatin1(labelName.getString());
}

int
FilopodiaFunctions::getNumberOfFilopodia(const HxSpatialGraph* graph)
{
    const char* filopodiaAttName = FilopodiaFunctions::getFilopodiaAttributeName();
    HierarchicalLabels* fGroup = graph->getLabelGroup(filopodiaAttName);

    const int numFilopodia = fGroup->getNumLabels();

    return numFilopodia;
}

SpatialGraphSelection
FilopodiaFunctions::getFilopodiaSelectionOfNode(const HxSpatialGraph* graph,
                                                const int nodeId)
{
    SpatialGraphSelection filopodiaSel(graph);
    SpatialGraphSelection timeSel = graph->getConnectedComponent(nodeId);

    const int filoId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, nodeId);

    SpatialGraphSelection::Iterator it(timeSel);
    for (int v = it.vertices.nextSelected(); v != -1; v = it.vertices.nextSelected())
    {
        const int f = FilopodiaFunctions::getFilopodiaIdOfNode(graph, v);
        SpatialGraphSelection shortestPathToNode = graph->getShortestPath(v, nodeId);
        SpatialGraphSelection baseSelection = FilopodiaFunctions::getNodesOfTypeInSelection(graph, shortestPathToNode, BASE_NODE);

        if (f == filoId && baseSelection.getNumSelectedVertices() < 2)
        {
            filopodiaSel.selectVertex(v);
        }
    }

    for (int e = it.edges.nextSelected(); e != -1; e = it.edges.nextSelected())
    {
        const int f = FilopodiaFunctions::getFilopodiaIdOfEdge(graph, e);
        int connectedNode = graph->getEdgeSource(e);
        if (FilopodiaFunctions::getFilopodiaIdOfNode(graph, connectedNode) != f)
        {
            connectedNode = graph->getEdgeTarget(e);
        }

        SpatialGraphSelection shortestPathToNode = graph->getShortestPath(graph->getEdgeSource(e), nodeId);
        SpatialGraphSelection baseSelection = FilopodiaFunctions::getNodesOfTypeInSelection(graph, shortestPathToNode, BASE_NODE);

        if (f == filoId && baseSelection.getNumSelectedVertices() < 2)
        {
            filopodiaSel.selectEdge(e);
        }
    }

    return filopodiaSel;
}

SpatialGraphSelection
FilopodiaFunctions::getFilopodiaSelectionForAllTimeSteps(const HxSpatialGraph* graph,
                                                         const int filopodiaLabelId)
{
    SpatialGraphSelection sel(graph);
    sel.clear();

    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        const int f = FilopodiaFunctions::getFilopodiaIdOfNode(graph, v);
        if (f == filopodiaLabelId)
        {
            sel.selectVertex(v);
        }
    }

    for (int e = 0; e < graph->getNumEdges(); ++e)
    {
        const int f = FilopodiaFunctions::getFilopodiaIdOfEdge(graph, e);
        if (f == filopodiaLabelId)
        {
            sel.selectEdge(e);
        }
    }

    return sel;
}

int
FilopodiaFunctions::getNextAvailableFilopodiaId(const HxSpatialGraph* graph)
{
    // Returns label id of unused filopodia label.
    // Creates a new one if there is no unused label.
    const EdgeVertexAttribute* vAtt = graph->findVertexAttribute(getFilopodiaAttributeName());
    const EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(getFilopodiaAttributeName());
    if (!vAtt || !eAtt)
    {
        throw McException("FilopodiaFunctions::getNextAvailableFilopodiaId: no Filopodia attribute available");
    }

    HierarchicalLabels* labels = graph->getLabelGroup(vAtt);
    const int maxLabelId = labels->getMaxLabelId();

    McBitfield bits(maxLabelId + 1);
    bits.set(0); // Do not use root label
    bits.set(getMatchLabelId(graph, UNASSIGNED));
    bits.set(getMatchLabelId(graph, IGNORED));
    bits.set(getMatchLabelId(graph, AXON));
    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        bits.set(FilopodiaFunctions::getFilopodiaIdOfNode(graph, v));
    }
    for (int e = 0; e < graph->getNumEdges(); ++e)
    {
        bits.set(FilopodiaFunctions::getFilopodiaIdOfEdge(graph, e));
    }

    const mculong firstUnused = bits.firstUnsetBit();
    if (firstUnused == bits.nBits())
    {
        const QString name = FilopodiaFunctions::getFilopodiaLabelNameFromNumber(maxLabelId);
        const SbColor color(McRandom::rand(), McRandom::rand(), McRandom::rand());
        return labels->addLabel(0, qPrintable(name), color);
    }
    else
    {
        return int(firstUnused);
    }
}

SpatialGraphSelection
FilopodiaFunctions::getFilopodiaSelectionForTime(const HxSpatialGraph* graph, const int filoId, const int timeId)
{
    SpatialGraphSelection filoSel(graph);

    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        if (FilopodiaFunctions::getTimeIdOfNode(graph, v) == timeId && (FilopodiaFunctions::getFilopodiaIdOfNode(graph, v) == filoId))
        {
            filoSel.selectVertex(v);
        }
    }

    for (int e = 0; e < graph->getNumEdges(); ++e)
    {
        if (FilopodiaFunctions::getTimeIdOfEdge(graph, e) == timeId && (FilopodiaFunctions::getFilopodiaIdOfEdge(graph, e) == filoId))
        {
            filoSel.selectEdge(e);
        }
    }
    return filoSel;
}

int
FilopodiaFunctions::getFilopodiaIdOfNode(const HxSpatialGraph* graph, const int v)
{
    EdgeVertexAttribute* filoAtt = graph->findVertexAttribute(FilopodiaFunctions::getFilopodiaAttributeName());
    if (!filoAtt)
    {
        throw McException(QString("No attribute found with name %1").arg(QString::fromLatin1(FilopodiaFunctions::getFilopodiaAttributeName())));
    }
    return filoAtt->getIntDataAtIdx(v);
}

void
FilopodiaFunctions::setFilopodiaIdOfNode(HxSpatialGraph* graph, const int node, const int id)
{
    EdgeVertexAttribute* att = graph->findVertexAttribute(getFilopodiaAttributeName());
    if (!att)
    {
        throw McException("FilopodiaFunctions::setFilopodiaIdOfNode: no filopodia attribute available");
    }
    att->setIntDataAtIdx(node, id);
}

int
FilopodiaFunctions::getFilopodiaIdOfEdge(const HxSpatialGraph* graph, const int e)
{
    EdgeVertexAttribute* filoAtt = graph->findEdgeAttribute(FilopodiaFunctions::getFilopodiaAttributeName());
    if (!filoAtt)
    {
        throw McException(QString("No attribute found with name %1").arg(QString::fromLatin1(FilopodiaFunctions::getFilopodiaAttributeName())));
    }
    return filoAtt->getIntDataAtIdx(e);
}

void
FilopodiaFunctions::setFilopodiaIdOfEdge(HxSpatialGraph* graph, const int edge, const int id)
{
    EdgeVertexAttribute* att = graph->findEdgeAttribute(getFilopodiaAttributeName());
    if (!att)
    {
        throw McException("FilopodiaFunctions::setFilopodiaIdOfNode: no filopodia attribute available");
    }
    att->setIntDataAtIdx(edge, id);
}

bool
FilopodiaFunctions::hasFiloId(const HxSpatialGraph* graph, const int v, const int filoId)
{
    const EdgeVertexAttribute* filoAtt = graph->findVertexAttribute(getFilopodiaAttributeName());
    if (!filoAtt)
    {
        throw McException("FilopodiaFunctions::hasFiloId: no Node filopodia attribute available");
    }
    return filoAtt->getIntDataAtIdx(v) == filoId;
}

TimeMinMax
FilopodiaFunctions::getFilopodiumLifeTime(const HxSpatialGraph* graph,
                                          const int filopodiumId)
{
    // Returns the minimum and maximum time where a vertex/edge with filopodium Id is available
    const char* filoAtt = getFilopodiaAttributeName();
    const char* timeAtt = getTimeStepAttributeName();

    const EdgeVertexAttribute* filoVAtt = graph->findVertexAttribute(filoAtt);
    const EdgeVertexAttribute* filoEAtt = graph->findEdgeAttribute(filoAtt);
    const EdgeVertexAttribute* timeVAtt = graph->findVertexAttribute(timeAtt);
    const EdgeVertexAttribute* timeEAtt = graph->findEdgeAttribute(timeAtt);

    TimeMinMax lifeTime;
    lifeTime.minT = std::numeric_limits<int>::max();
    lifeTime.maxT = std::numeric_limits<int>::min();

    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        if (filoVAtt->getIntDataAtIdx(v) == filopodiumId)
        {
            const int timeId = timeVAtt->getIntDataAtIdx(v);
            const int time = getTimeStepFromTimeId(graph, timeId);
            if (time < lifeTime.minT)
                lifeTime.minT = time;
            if (time > lifeTime.maxT)
                lifeTime.maxT = time;
        }
    }

    for (int e = 0; e < graph->getNumEdges(); ++e)
    {
        if (filoEAtt->getIntDataAtIdx(e) == filopodiumId)
        {
            const int timeId = timeEAtt->getIntDataAtIdx(e);
            const int time = getTimeStepFromTimeId(graph, timeId);
            if (time < lifeTime.minT)
                lifeTime.minT = time;
            if (time > lifeTime.maxT)
                lifeTime.maxT = time;
        }
    }

    return lifeTime;
}

TimeMinMax
FilopodiaFunctions::getFilopodiumMatchLifeTime(const HxSpatialGraph* graph,
                                               const int matchId)
{
    const char* matchAtt = getManualNodeMatchAttributeName();
    const char* timeAtt = getTimeStepAttributeName();

    const EdgeVertexAttribute* matchVAtt = graph->findVertexAttribute(matchAtt);
    const EdgeVertexAttribute* timeVAtt = graph->findVertexAttribute(timeAtt);

    TimeMinMax lifeTime;
    lifeTime.minT = std::numeric_limits<int>::max();
    lifeTime.maxT = std::numeric_limits<int>::min();

    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        if (matchVAtt->getIntDataAtIdx(v) == matchId)
        {
            const int timeId = timeVAtt->getIntDataAtIdx(v);
            const int time = getTimeStepFromTimeId(graph, timeId);
            if (time < lifeTime.minT)
                lifeTime.minT = time;
            if (time > lifeTime.maxT)
                lifeTime.maxT = time;
        }
    }

    return lifeTime;
}

bool
FilopodiaFunctions::isRealFilopodium(const HxSpatialGraph* graph, const int filopodiumId)
{
    // Returns whether filopodiumId is a real filopodium, i.e. not unassigned/ignored
    return (filopodiumId != getFilopodiaLabelId(graph, UNASSIGNED)) &&
           (filopodiumId != getFilopodiaLabelId(graph, IGNORED)) &&
           (filopodiumId != getFilopodiaLabelId(graph, AXON)) &&
           (filopodiumId > 0) &&
           (filopodiumId <= graph->getLabelGroup(getFilopodiaAttributeName())->getMaxLabelId());
}

SpatialGraphSelection
FilopodiaFunctions::getNodesWithFiloIdFromSelection(const HxSpatialGraph* graph, SpatialGraphSelection sel, const int filoId)
{
    SpatialGraphSelection filoSel(graph);

    SpatialGraphSelection::Iterator it(sel);
    for (int v = it.vertices.nextSelected(); v != -1; v = it.vertices.nextSelected())
    {
        if (FilopodiaFunctions::hasFiloId(graph, v, filoId))
        {
            filoSel.selectVertex(v);
        }
    }

    return filoSel;
}

// ----------- Functions for Manual Node Match IDs -----------
int
FilopodiaFunctions::getMatchLabelId(const HxSpatialGraph* graph, const MatchLabel filopodia)
{
    if (!graph)
    {
        throw McException(QString("FilopodiaFunctions::getMatchLabelId: no graph"));
    }

    const HierarchicalLabels* labels = graph->getLabelGroup(getManualNodeMatchAttributeName());
    if (!labels)
    {
        throw McException(QString("FilopodiaFunctions::getMatchLabelId: graph has no match labels"));
    }

    return labels->getLabelIdFromName(McString(getMatchLabelName(filopodia)));
}

QString
FilopodiaFunctions::getLabelNameFromMatchId(const HxSpatialGraph* graph, const int matchId)
{
    const int unassignedLabel = FilopodiaFunctions::getMatchLabelId(graph, UNASSIGNED);
    const int ignoredLabel = FilopodiaFunctions::getMatchLabelId(graph, IGNORED);
    const int axonLabel = FilopodiaFunctions::getMatchLabelId(graph, AXON);
    if (matchId == unassignedLabel)
    {
        return getMatchLabelName(UNASSIGNED);
    }
    else if (matchId == ignoredLabel)
    {
        return getMatchLabelName(IGNORED);
    }
    else if (matchId == axonLabel)
    {
        return getMatchLabelName(AXON);
    }
    else
    {
        return QString("M_%1").arg(matchId - 3, 4, 10, QChar('0'));
    }
}

SpatialGraphSelection
FilopodiaFunctions::getNodesWithMatchId(const HxSpatialGraph* graph, const int matchId)
{
    const EdgeVertexAttribute* matchAtt = graph->findVertexAttribute(getManualNodeMatchAttributeName());
    if (!matchAtt)
    {
        throw McException("FilopodiaFunctions::getNodesWithMatchId: no manual node match attribute available");
    }

    SpatialGraphSelection baseSel = FilopodiaFunctions::getNodesOfType(graph, BASE_NODE);
    SpatialGraphSelection::Iterator it(baseSel);
    SpatialGraphSelection result(graph);
    for (int v = it.vertices.nextSelected(); v != -1; v = it.vertices.nextSelected())
    {
        const int id = FilopodiaFunctions::getMatchIdOfNode(graph, v);
        if (id == matchId)
        {
            result.selectVertex(v);
        }
    }

    return result;
}

QString
FilopodiaFunctions::getMatchLabelName(const MatchLabel label)
{
    switch (label)
    {
        case UNASSIGNED:
            return QString("UNASSIGNED");
        case IGNORED:
            return QString("IGNORED");
        case AXON:
            return QString("AXON");
        default:
            throw McException(QString("FilopodiaFunctions::getNodeMatchLabelName: Label not found"));
    }
}

const char*
FilopodiaFunctions::getManualNodeMatchAttributeName()
{
    return "ManualNodeMatch";
}

void
FilopodiaFunctions::addManualNodeMatchLabelAttribute(HxSpatialGraph* graph)
{
    const char* labelAttName = getManualNodeMatchAttributeName();
    EdgeVertexAttribute* matchAtt = graph->findVertexAttribute(labelAttName);

    const QString unassignedLabel = getMatchLabelName(UNASSIGNED);
    const QString ignoredLabel = getMatchLabelName(IGNORED);
    const QString axonLabel = getMatchLabelName(AXON);

    HierarchicalLabels* labels;
    if (matchAtt)
    {
        labels = graph->getLabelGroup(matchAtt);
        if (!labels->hasLabel(unassignedLabel))
        {
            labels->addLabel(0, qPrintable(unassignedLabel), SbColor(224.0f, 238.0f, 238.0f));
        }
        if (!labels->hasLabel(ignoredLabel))
        {
            labels->addLabel(0, qPrintable(ignoredLabel), SbColor(0.0f, 0.0f, 139.0f));
        }
        if (!labels->hasLabel(axonLabel))
        {
            labels->addLabel(0, qPrintable(axonLabel), SbColor(211.0f, 211.0f, 211.0f));
        }
    }
    else
    {
        labels = graph->addNewLabelGroup(labelAttName, false, true, false);
        const int unassigned = labels->addLabel(0, qPrintable(unassignedLabel), SbColor(224.0f, 238.0f, 238.0f));
        labels->addLabel(0, qPrintable(ignoredLabel), SbColor(0.0f, 0.0f, 139.0f));
        labels->addLabel(0, qPrintable(axonLabel), SbColor(211.0f, 211.0f, 211.0f));

        EdgeVertexAttribute* matchAtt = graph->findVertexAttribute(labelAttName);
        for (int v = 0; v < graph->getNumVertices(); ++v)
        {
            if (FilopodiaFunctions::getMatchIdOfNode(graph, v) == 0)
            {
                matchAtt->setIntDataAtIdx(v, unassigned);
            }
        }
    }
}

int
FilopodiaFunctions::getMatchIdOfNode(const HxSpatialGraph* graph, const int v)
{
    EdgeVertexAttribute* matchAtt = graph->findVertexAttribute(FilopodiaFunctions::getManualNodeMatchAttributeName());
    if (!matchAtt)
    {
        throw McException(QString("No attribute found with name %1").arg(QString::fromLatin1(FilopodiaFunctions::getManualNodeMatchAttributeName())));
    }
    return matchAtt->getIntDataAtIdx(v);
}

int
FilopodiaFunctions::getNextAvailableManualNodeMatchId(HxSpatialGraph* graph)
{
    // Returns label id of unused manual node match label.
    // Creates a new one if there is no unused label.
    const int unusedLabel = getUnusedManualNodeMatchId(graph);
    if (unusedLabel == -1)
    {
        const EdgeVertexAttribute* att = graph->findVertexAttribute(getManualNodeMatchAttributeName());
        HierarchicalLabels* labels = graph->getLabelGroup(att);
        const SbColor color(McRandom::rand(), McRandom::rand(), McRandom::rand());
        const QString name = getNewManualNodeMatchLabelName(graph);
        return labels->addLabel(0, qPrintable(name), color);
    }
    else
    {
        return unusedLabel;
    }
}

int
FilopodiaFunctions::getUnusedManualNodeMatchId(const HxSpatialGraph* graph)
{
    // Returns existing unused manual node match label (-1 if none exists)
    const EdgeVertexAttribute* vAtt = graph->findVertexAttribute(getManualNodeMatchAttributeName());
    if (!vAtt)
    {
        throw McException("FilopodiaFunctions::getNextAvailableManualNodeMatchId: no ManualNodeMatch attribute available");
    }

    const HierarchicalLabels* labels = graph->getLabelGroup(vAtt);
    const int maxLabelId = labels->getMaxLabelId();

    McBitfield bits(maxLabelId + 1);
    bits.set(0); // Do not use root label
    bits.set(getMatchLabelId(graph, UNASSIGNED));
    bits.set(getMatchLabelId(graph, IGNORED));
    bits.set(getMatchLabelId(graph, AXON));
    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        bits.set(FilopodiaFunctions::getMatchIdOfNode(graph, v));
    }

    const mculong firstUnused = bits.firstUnsetBit();
    if (firstUnused == bits.nBits())
    {
        return -1;
    }
    else
    {
        return int(firstUnused);
    }
}

QString
FilopodiaFunctions::getNewManualNodeMatchLabelName(const HxSpatialGraph* graph)
{
    // Returns a name for a non-existing manual node match label;
    const EdgeVertexAttribute* att = graph->findVertexAttribute(getManualNodeMatchAttributeName());
    if (!att)
    {
        throw McException("FilopodiaFunctions::getNewManualNodeMatchLabelName: no ManualNodeMatch attribute available");
    }

    const HierarchicalLabels* labels = graph->getLabelGroup(att);
    const int maxLabelId = labels->getMaxLabelId();
    int num = maxLabelId - 2;
    QString name = QString("M_%1").arg(num, 4, 10, QChar('0'));
    while (labels->hasLabel(McString(name)))
    {
        num += 1;
        name = QString("M_%1").arg(num, 4, 10, QChar('0'));
    }
    return name;
}

void
FilopodiaFunctions::setManualNodeMatchId(HxSpatialGraph* graph, const int node, const int id)
{
    EdgeVertexAttribute* att = graph->findVertexAttribute(getManualNodeMatchAttributeName());
    if (!att)
    {
        throw McException("FilopodiaFunctions::setManualNodeMatchId: no setManualNodeMatchId attribute available");
    }
    att->setIntDataAtIdx(node, id);
}

// ----------- Functions for Growth Cone -----------
// Growth Cone Label
HierarchicalLabels*
FilopodiaFunctions::addGrowthConeLabelAttribute(HxSpatialGraph* graph)
{
    HierarchicalLabels* growthConeLabels = graph->getLabelGroup(getGrowthConeAttributeName());
    if (!growthConeLabels)
    {
        growthConeLabels = graph->addNewLabelGroup(getGrowthConeAttributeName(), true, true, false);
    }

    return growthConeLabels;
}

const char*
FilopodiaFunctions::getGrowthConeAttributeName()
{
    return "GrowthCones";
}

bool
FilopodiaFunctions::hasGrowthConeLabels(const HxSpatialGraph* graph)
{
    const char* gcAttName = FilopodiaFunctions::getGrowthConeAttributeName();
    const HierarchicalLabels* gcLabels = graph->getLabelGroup(gcAttName);
    if (!gcLabels)
    {
        return false;
    }

    return graph->findEdgeAttribute(gcAttName) && graph->findVertexAttribute(gcAttName);
}

QString
FilopodiaFunctions::getLabelNameFromGrowthConeNumber(const int growthConeNumber)
{
    return QString("GC_%1").arg(growthConeNumber, 3, 10, QChar('0'));
}

McVec3f
FilopodiaFunctions::getGrowthConeCenter(const HxSpatialGraph* graph,
                                        const int growthCone,
                                        const int time,
                                        int& nodeId)
{
    const char* timeAttName = getTimeStepAttributeName();
    const EdgeVertexAttribute* timeAtt = graph->findVertexAttribute(timeAttName);
    if (!timeAtt)
    {
        throw McException(QString("No vertex attribute found with name %1").arg(QString::fromLatin1(timeAttName)));
    }

    const char* gcAttName = getGrowthConeAttributeName();
    const EdgeVertexAttribute* gcAtt = graph->findVertexAttribute(gcAttName);
    if (!gcAtt)
    {
        throw McException(QString("No vertex attribute found with name %1").arg(QString::fromLatin1(gcAttName)));
    }

    const QString timeLabelName = getLabelNameFromTime(time);
    const int timeId = graph->getLabelId(timeAtt, qPrintable(timeLabelName));

    const QString gcLabelName = getLabelNameFromGrowthConeNumber(growthCone);
    const int gcLabelId = graph->getLabelId(gcAtt, McString(gcLabelName));

    std::vector<int> verticesForTimeAndGrowthCone;
    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        if ((FilopodiaFunctions::getTimeIdOfNode(graph, v) == timeId) &&
            (FilopodiaFunctions::getGcIdOfNode(graph, v) == gcLabelId))
        {
            verticesForTimeAndGrowthCone.push_back(v);
        }
    }

    if (verticesForTimeAndGrowthCone.size() == 0)
    {
        throw McException(QString("No vertex for time %1 and growth cone %2 found").arg(time).arg(growthCone));
    }
    else if (verticesForTimeAndGrowthCone.size() >= 2)
    {
        throw McException(QString("Too many vertices (%1) for time %2 and growth cone %3 found")
                              .arg(verticesForTimeAndGrowthCone.size())
                              .arg(time)
                              .arg(growthCone));
    }

    nodeId = verticesForTimeAndGrowthCone[0];

    return graph->getVertexCoords(nodeId);
}

int
FilopodiaFunctions::getGrowthConeIdFromLabel(const HxSpatialGraph* graph, const QString& label)
{
    const char* gcAttName = FilopodiaFunctions::getGrowthConeAttributeName();
    HierarchicalLabels* gcLabels = graph->getLabelGroup(gcAttName);
    if (!gcLabels)
    {
        throw McException(QString("No growth cone labels found (getGrowthConeIdFromLabel)"));
    }

    const McString name(label);
    return gcLabels->getLabelIdFromName(name);
}

SpatialGraphSelection
FilopodiaFunctions::getNodesWithGcId(const HxSpatialGraph* graph, const int growthConeId)
{
    SpatialGraphSelection rootsFromGrowthCone(graph);

    const char* gcAttName = FilopodiaFunctions::getGrowthConeAttributeName();
    EdgeVertexAttribute* gcAtt = graph->findVertexAttribute(gcAttName);
    if (!gcAtt)
    {
        printf("no gcAtt");
    }

    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        const int gc = FilopodiaFunctions::getGcIdOfNode(graph, v);
        if (gc == growthConeId)
        {
            rootsFromGrowthCone.selectVertex(v);
        }
    }
    return rootsFromGrowthCone;
}

int
FilopodiaFunctions::getNumberOfGrowthCones(const HxSpatialGraph* graph)
{
    int numberOfGrowthCones;
    const char* gcAttName = FilopodiaFunctions::getGrowthConeAttributeName();
    HierarchicalLabels* gcLabels = graph->getLabelGroup(gcAttName);
    if (!gcLabels)
    {
        numberOfGrowthCones = 0;
    }
    else
    {
        numberOfGrowthCones = gcLabels->getNumLabels();
    }

    // Number of growth cone = numLabels -1 since we do not count 0
    return numberOfGrowthCones - 1;
}

QString
FilopodiaFunctions::getGrowthConeLabelFromNumber(const HxSpatialGraph* graph, const int childNum)
{
    const char* gcAttName = FilopodiaFunctions::getGrowthConeAttributeName();
    HierarchicalLabels* gcLabels = graph->getLabelGroup(gcAttName);
    if (!gcLabels)
    {
        throw McException(QString("No growth cone labels found (getGrowthConeLabelFromNumber)"));
    }

    if (childNum < 0)
    {
        throw McException(QString("Invalid index %1 (must be positive)").arg(childNum));
    }
    if (childNum >= gcLabels->getNumLabels())
    {
        throw McException(QString("Invalid index %1 (only %2 labels available)").arg(childNum).arg(gcLabels->getNumLabels()));
    }

    const int id = gcLabels->getChildId(0, childNum);
    McString name;
    gcLabels->getLabelName(id, name);
    return QString::fromLatin1(name.getString());
}

int
FilopodiaFunctions::getGrowthConeLabelIdFromNumber(const HxSpatialGraph* graph, const int childNum)
{
    const char* gcAttName = FilopodiaFunctions::getGrowthConeAttributeName();
    HierarchicalLabels* gcLabels = graph->getLabelGroup(gcAttName);
    if (!gcLabels)
    {
        throw McException(QString("No growth cone labels found (getGrowthConeLabelIdFromNumber)"));
    }

    return gcLabels->getChildId(0, childNum);
}

int
FilopodiaFunctions::getGcIdOfNode(const HxSpatialGraph* graph, const int v)
{
    EdgeVertexAttribute* gcAtt = graph->findVertexAttribute(FilopodiaFunctions::getGrowthConeAttributeName());
    if (!gcAtt)
    {
        throw McException(QString("No attribute found with name %1").arg(QString::fromLatin1(FilopodiaFunctions::getGrowthConeAttributeName())));
    }
    return gcAtt->getIntDataAtIdx(v);
}

void
FilopodiaFunctions::setGcIdOfNode(HxSpatialGraph* graph, const int node, const int id)
{
    EdgeVertexAttribute* att = graph->findVertexAttribute(getGrowthConeAttributeName());
    if (!att)
    {
        throw McException("setGcIdOfNode: no GrowthCone attribute available");
    }
    att->setIntDataAtIdx(node, id);
}

int
FilopodiaFunctions::getGcIdOfEdge(const HxSpatialGraph* graph, const int s)
{
    EdgeVertexAttribute* gcAtt = graph->findEdgeAttribute(FilopodiaFunctions::getGrowthConeAttributeName());
    if (!gcAtt)
    {
        throw McException(QString("No attribute found with name %1").arg(QString::fromLatin1(FilopodiaFunctions::getGrowthConeAttributeName())));
    }
    return gcAtt->getIntDataAtIdx(s);
}

void
FilopodiaFunctions::setGcIdOfEdge(HxSpatialGraph* graph, const int edge, const int id)
{
    EdgeVertexAttribute* att = graph->findEdgeAttribute(getGrowthConeAttributeName());
    if (!att)
    {
        throw McException("setGcIdOfNode: no GrowthCone attribute available");
    }
    att->setIntDataAtIdx(edge, id);
}

void
FilopodiaFunctions::transformGcLabelGroup(HxSpatialGraph* graph)
{
    const char* newName = "newGroup";
    HierarchicalLabels* gcLabelNew = graph->addNewLabelGroup(newName, true, true, false);
    EdgeVertexAttribute* vAtt = graph->findVertexAttribute(newName);
    EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(newName);

    if (!vAtt) throw McException("AddFiloEdgeAttribute: new node attribute not found");
    if (!eAtt) throw McException("AddFiloEdgeAttribute: new edge attribute not found");

    const HierarchicalLabels* gcLabel = graph->getLabelGroup(FilopodiaFunctions::getGrowthConeAttributeName());
    const int numLabels = gcLabel->getNumLabels();

    // Add and assign all labels
    for (int n=1; n<numLabels; ++n) {

        SpatialGraphSelection sel = FilopodiaFunctions::getNodesWithGcId(graph, n);

        const QString gcLabelName = FilopodiaFunctions::getLabelNameFromGrowthConeNumber(n);
        SbColor color;
        gcLabel->getLabelColor(n, color);
        const int gcId = gcLabelNew->addLabel(0, qPrintable(gcLabelName), color);

        SpatialGraphSelection::Iterator it(sel);
        for (int v = it.vertices.nextSelected(); v != -1; v = it.vertices.nextSelected())
        {
            vAtt->setIntDataAtIdx(v, gcId);
        }

        for (int e = it.edges.nextSelected(); e != -1; e = it.edges.nextSelected())
        {
            eAtt->setIntDataAtIdx(e, gcId);
        }
    }

    graph->deleteLabelGroup(FilopodiaFunctions::getGrowthConeAttributeName());
    graph->changeLabelGroupName(gcLabelNew, FilopodiaFunctions::getGrowthConeAttributeName());
}

// ----------- Functions for Location -----------
const char*
FilopodiaFunctions::getLocationAttributeName()
{
    return "Location";
}

void
FilopodiaFunctions::addLocationLabelAttribute(HxSpatialGraph* graph)
{
    const char* attName = getLocationAttributeName();

    EdgeVertexAttribute* vAtt = graph->findVertexAttribute(attName);
    EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(attName);

    const McString filo = getLocationLabelName(FILOPODIUM);
    const McString growth = getLocationLabelName(GROWTHCONE);

    HierarchicalLabels* labels;
    if (vAtt && eAtt)
    {
        labels = graph->getLabelGroup(attName);
        if (!labels->hasLabel(growth))
        {
            labels->addLabel(0, getLocationLabelName(GROWTHCONE), SbColor(224.0f, 238.0f, 238.0f));
        }
        if (!labels->hasLabel(filo))
        {
            labels->addLabel(0, getLocationLabelName(FILOPODIUM), SbColor(0.0f, 0.0f, 139.0f));
        }
    }
    else if (!vAtt && !eAtt)
    {
        HierarchicalLabels* labels = graph->addNewLabelGroup(attName, true, true, false);
        const int growthId = labels->addLabel(0, getLocationLabelName(GROWTHCONE), SbColor(224.0f, 238.0f, 238.0f));
        labels->addLabel(0, getLocationLabelName(FILOPODIUM), SbColor(0.0f, 0.0f, 139.0f));

        vAtt = graph->findVertexAttribute(attName);
        for (int v = 0; v < graph->getNumVertices(); ++v)
        {
            vAtt->setIntDataAtIdx(v, growthId);
        }

        eAtt = graph->findEdgeAttribute(attName);
        for (int e = 0; e < graph->getNumEdges(); ++e)
        {
            eAtt->setIntDataAtIdx(e, growthId);
        }
    }
    else
    {
        throw McException("Invalid existing Location attribute");
    }
}

int
FilopodiaFunctions::getLocationLabelId(const HxSpatialGraph* graph, const FilopodiaLocation location)
{
    if (!graph)
    {
        throw McException(QString("FilopodiaFunctions::getLocationLabelId: no graph"));
    }

    const HierarchicalLabels* labels = graph->getLabelGroup(getLocationAttributeName());
    if (!labels)
    {
        throw McException(QString("FilopodiaFunctions::getLocationLabelId: graph has no location labels"));
    }

    return labels->getLabelIdFromName(McString(getLocationLabelName(location)));
}

const char*
FilopodiaFunctions::getLocationLabelName(const FilopodiaLocation location)
{
    switch (location)
    {
        case GROWTHCONE:
            return "GROWTHCONE";
        case FILOPODIUM:
            return "FILOPODIUM";
        default:
            throw McException(QString("FilopodiaFunctions::getLocationLabelName: Label not found"));
    }
}

void
traverseLocation(const HxSpatialGraph* graph, const int nodeNum, const FilopodiaLocation location, SpatialGraphSelection& sel)
{
    if (!FilopodiaFunctions::nodeHasLocation(location, nodeNum, graph))
    {
        return;
    }

    sel.selectVertex(nodeNum);

    const IncidenceList edges = graph->getIncidentEdges(nodeNum);
    for (int e = 0; e < edges.size(); ++e)
    {
        const int edgeNum = edges[e];
        if (FilopodiaFunctions::edgeHasLocation(location, edgeNum, graph) && !sel.isSelectedEdge(edgeNum))
        {
            sel.selectEdge(edgeNum);
            if (graph->getEdgeSource(edgeNum) == nodeNum)
            {
                traverseLocation(graph, graph->getEdgeTarget(edgeNum), location, sel);
            }
            else
            {
                traverseLocation(graph, graph->getEdgeSource(edgeNum), location, sel);
            }
        }
    }
}

void
FilopodiaFunctions::getGrowthConeAndFilopodiumSelection(HxSpatialGraph* graph, const int time, SpatialGraphSelection& growthSel, SpatialGraphSelection& filoSel)
{
    const SpatialGraphSelection timeSel = getSelectionForTimeStep(graph, time);
    const int rootNode = getNodesOfTypeInSelection(graph, timeSel, ROOT_NODE).getSelectedVertex(0);

    const char* typeAttName = FilopodiaFunctions::getTypeLabelAttributeName();
    const EdgeVertexAttribute* typeAtt = graph->findVertexAttribute(typeAttName);
    if (!typeAtt)
    {
        throw McException(QString("No vertex attribute found with name %1").arg(QString::fromLatin1(typeAttName)));
    }

    // Edges and nodes are part of a filopodium if shortest path to root has base node selection
    for (int v = 0; v < timeSel.getNumSelectedVertices(); ++v)
    {
        const int currentVertex = timeSel.getSelectedVertex(v);
        const SpatialGraphSelection pathToRoot = graph->getShortestPath(currentVertex, rootNode);
        const SpatialGraphSelection baseNodes = FilopodiaFunctions::getNodesOfTypeInSelection(graph, pathToRoot, BASE_NODE);

        if (baseNodes.isEmpty())
        {
            growthSel.selectVertex(currentVertex);
        }
        else if (baseNodes.getNumSelectedVertices() == 1)
        {
            filoSel.selectVertex(currentVertex);
        }
        else
        {
            throw McException(QString("getGrowthConeAndFilopodiumSelection: Invalid Tree! Node %1 has more than 1 base on shortest path to root.").arg(currentVertex));
        }
    }

    for (int e = 0; e < timeSel.getNumSelectedEdges(); ++e)
    {
        const int currentEdge = timeSel.getSelectedEdge(e);
        int node = graph->getEdgeSource(currentEdge);
        if (FilopodiaFunctions::hasNodeType(graph, BASE_NODE, node))
        {
            node = graph->getEdgeTarget(currentEdge);
        }
        const SpatialGraphSelection pathToRoot = graph->getShortestPath(node, rootNode);
        const SpatialGraphSelection baseNodes = FilopodiaFunctions::getNodesOfTypeInSelection(graph, pathToRoot, BASE_NODE);

        if (baseNodes.isEmpty())
        {
            growthSel.selectEdge(currentEdge);
        }
        else if (baseNodes.getNumSelectedVertices() == 1)
        {
            filoSel.selectEdge(currentEdge);
        }
        else
        {
            throw McException(QString("getGrowthConeAndFilopodiumSelection: Invalid Tree! edge %1 has more than 1 base on shortest path to root.").arg(currentEdge));
        }
    }
}

bool
FilopodiaFunctions::checkIfNodeIsPartOfGrowthCone(const HxSpatialGraph* graph, const int node, const int rootNode)
{
    // Edges and nodes are part of a growth cone if shortest path to root has not base node selection
    SpatialGraphSelection shortestPath = graph->getShortestPath(node, rootNode);
    SpatialGraphSelection baseSelection = FilopodiaFunctions::getNodesOfTypeInSelection(graph, shortestPath, BASE_NODE);

    if (baseSelection.isEmpty())
    {
        return true;
    }
    else if (baseSelection.getNumSelectedVertices() == 1)
    {
        return false;
    }
    else
    {
        throw McException(QString("Invalid tree! Filopodium has more than 1 base."));
    }
}

bool
FilopodiaFunctions::checkIfEdgeIsPartOfGrowthCone(const HxSpatialGraph* graph, const int edge, const int rootNode)
{
    const int source = graph->getEdgeSource(edge);
    const int target = graph->getEdgeTarget(edge);

    SpatialGraphSelection sourcePath = graph->getShortestPath(source, rootNode);
    SpatialGraphSelection sourceBaseSelection = FilopodiaFunctions::getNodesOfTypeInSelection(graph, sourcePath, BASE_NODE);

    SpatialGraphSelection targetPath = graph->getShortestPath(target, rootNode);
    SpatialGraphSelection targetBaseSelection = FilopodiaFunctions::getNodesOfTypeInSelection(graph, targetPath, BASE_NODE);

    if (sourceBaseSelection.isEmpty() || targetBaseSelection.isEmpty())
    {
        return true;
    }
    else if (sourceBaseSelection.getNumSelectedVertices() == 1 && targetBaseSelection.getNumSelectedVertices() == 1)
    {
        return false;
    }
    else
    {
        throw McException(QString("Invalid tree! Filopodium has more than 1 base."));
    }
}

bool
FilopodiaFunctions::checkIfPointIsPartOfGrowthCone(const HxSpatialGraph* graph, const SpatialGraphPoint point, const int rootNode)
{
    const int edge = point.edgeNum;

    return (checkIfEdgeIsPartOfGrowthCone(graph, edge, rootNode));
}

int
FilopodiaFunctions::getLocationIdOfNode(const HxSpatialGraph* graph, const int v)
{
    EdgeVertexAttribute* locAtt = graph->findVertexAttribute(FilopodiaFunctions::getLocationAttributeName());
    if (!locAtt)
    {
        throw McException(QString("No attribute found with name %1").arg(QString::fromLatin1(FilopodiaFunctions::getLocationAttributeName())));
    }
    return locAtt->getIntDataAtIdx(v);
}

void
FilopodiaFunctions::setLocationIdOfNode(HxSpatialGraph* graph, const int node, const FilopodiaLocation loc)
{
    EdgeVertexAttribute* att = graph->findVertexAttribute(getLocationAttributeName());
    if (!att)
    {
        throw McException("setLocationIdOfNode: no Location attribute available");
    }
    att->setIntDataAtIdx(node, getLocationLabelId(graph, loc));
}

int
FilopodiaFunctions::getLocationIdOfEdge(const HxSpatialGraph* graph, const int e)
{
    EdgeVertexAttribute* locAtt = graph->findEdgeAttribute(FilopodiaFunctions::getLocationAttributeName());
    if (!locAtt)
    {
        throw McException(QString("No attribute found with name %1").arg(QString::fromLatin1(FilopodiaFunctions::getLocationAttributeName())));
    }
    return locAtt->getIntDataAtIdx(e);
}

void
FilopodiaFunctions::setLocationIdOfEdge(HxSpatialGraph* graph, const int edge, const FilopodiaLocation loc)
{
    EdgeVertexAttribute* att = graph->findEdgeAttribute(getLocationAttributeName());
    if (!att)
    {
        throw McException("setLocationIdOfEdge: no Location attribute available");
    }
    att->setIntDataAtIdx(edge, getLocationLabelId(graph, loc));
}

SpatialGraphSelection
FilopodiaFunctions::getFilopodiumSelectionOfTimeStep(const HxSpatialGraph* graph, const int timeStep)
{
    SpatialGraphSelection selectionForTime = FilopodiaFunctions::getSelectionForTimeStep(graph, timeStep);
    SpatialGraphSelection sel(graph);

    SpatialGraphSelection::Iterator it(selectionForTime);
    for (int v = it.vertices.nextSelected(); v != -1; v = it.vertices.nextSelected())
    {
        if (FilopodiaFunctions::nodeHasLocation(FILOPODIUM, v, graph))
        {
            sel.selectVertex(v);
        }
    }

    for (int e = it.edges.nextSelected(); e != -1; e = it.edges.nextSelected())
    {
        if (FilopodiaFunctions::edgeHasLocation(FILOPODIUM, e, graph))
        {
            sel.selectEdge(e);
        }
    }

    return sel;
}

SpatialGraphSelection
FilopodiaFunctions::getGrowthConeSelectionOfTimeStep(const HxSpatialGraph* graph, const int timeStep)
{
    SpatialGraphSelection selectionForTime = FilopodiaFunctions::getSelectionForTimeStep(graph, timeStep);
    SpatialGraphSelection sel(graph);

    SpatialGraphSelection::Iterator it(selectionForTime);
    for (int v = it.vertices.nextSelected(); v != -1; v = it.vertices.nextSelected())
    {
        if (FilopodiaFunctions::nodeHasLocation(GROWTHCONE, v, graph))
        {
            sel.selectVertex(v);
        }
    }

    for (int e = it.edges.nextSelected(); e != -1; e = it.edges.nextSelected())
    {
        if (FilopodiaFunctions::edgeHasLocation(GROWTHCONE, e, graph))
        {
            sel.selectEdge(e);
        }
    }

    return sel;
}

SpatialGraphSelection
FilopodiaFunctions::getFilopodiumSelectionFromNode(const HxSpatialGraph* graph, const int node)
{
    // Computes filopodium selection connected to a node.
    // Assumes that Location labels have been set correctly.

    EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(getLocationAttributeName());
    EdgeVertexAttribute* vAtt = graph->findVertexAttribute(getLocationAttributeName());
    if (!eAtt || !vAtt)
    {
        throw McException(QString("FilopodiaFunctions::getFilopodiumSelectionFromNode: no Location attribute"));
    }

    SpatialGraphSelection sel(graph);
    traverseLocation(graph, node, FILOPODIUM, sel);
    return sel;
}

SpatialGraphSelection
FilopodiaFunctions::getFilopodiumSelectionFromEdge(const HxSpatialGraph* graph, const int edge)
{
    EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(getLocationAttributeName());
    EdgeVertexAttribute* vAtt = graph->findVertexAttribute(getLocationAttributeName());
    if (!eAtt || !vAtt)
    {
        throw McException(QString("FilopodiaFunctions::getFilopodiumSelectionFromEdge: no Location attribute"));
    }

    SpatialGraphSelection sel(graph);
    if (FilopodiaFunctions::edgeHasLocation(FILOPODIUM, edge, graph))
    {
        sel.selectEdge(edge);
        traverseLocation(graph, graph->getEdgeSource(edge), FILOPODIUM, sel);
        traverseLocation(graph, graph->getEdgeTarget(edge), FILOPODIUM, sel);
    }
    return sel;
}

void
FilopodiaFunctions::labelGrowthConeAndFilopodiumSelection(HxSpatialGraph* graph, const int time)
{
    const SpatialGraphSelection timeSel = getSelectionForTimeStep(graph, time);
    const int rootNode = getNodesOfTypeInSelection(graph, timeSel, ROOT_NODE).getSelectedVertex(0);

    const char* locationAttName = FilopodiaFunctions::getLocationAttributeName();
    EdgeVertexAttribute* eLocAtt = graph->findEdgeAttribute(locationAttName);
    EdgeVertexAttribute* vLocAtt = graph->findVertexAttribute(locationAttName);
    if ((!vLocAtt) || (!eLocAtt))
    {
        throw McException(QString("No attribute found with name %1").arg(QString::fromLatin1(locationAttName)));
    }

    const int growthLabel = FilopodiaFunctions::getLocationLabelId(graph, GROWTHCONE);
    const int filoLabel = FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM);

    // Find all edges within the growth cone body
    SpatialGraphSelection growthSel(graph);
    SpatialGraphSelection filoSel(graph);
    getGrowthConeAndFilopodiumSelection(graph, time, growthSel, filoSel);

    for (int e = 0; e < timeSel.getNumSelectedEdges(); ++e)
    {
        const int currentEdge = timeSel.getSelectedEdge(e);

        if (growthSel.isSelectedEdge(currentEdge))
        {
            eLocAtt->setIntDataAtIdx(currentEdge, growthLabel);
        }
        else if (filoSel.isSelectedEdge(currentEdge))
        {
            eLocAtt->setIntDataAtIdx(currentEdge, filoLabel);
        }
        else
        {
            printf("Edge %i is neither part of growth cone nor filopodium\n", currentEdge);
        }
    }

    for (int v = 0; v < timeSel.getNumSelectedVertices(); ++v)
    {
        const int currentVertex = timeSel.getSelectedVertex(v);

        if (growthSel.isSelectedVertex(currentVertex))
        {
            vLocAtt->setIntDataAtIdx(currentVertex, growthLabel);
        }
        else if (filoSel.isSelectedVertex(currentVertex))
        {
            vLocAtt->setIntDataAtIdx(currentVertex, filoLabel);
        }
        else
        {
            printf("Vertex %i is neither part of growth cone nor filopodium\n", currentVertex);
        }
    }
    vLocAtt->setIntDataAtIdx(rootNode, growthLabel);
}

void
FilopodiaFunctions::assignGrowthConeAndFilopodiumLabels(HxSpatialGraph* graph)
{
    FilopodiaFunctions::addLocationLabelAttribute(graph);
    const TimeMinMax tMinMax = getTimeMinMaxFromGraphLabels(graph);

    for (int time = tMinMax.minT; time <= tMinMax.maxT; ++time)
    {
        const int rootNode = getRootNodeFromTimeStep(graph, time);
        if (rootNode != -1)
        {
            labelGrowthConeAndFilopodiumSelection(graph, time);
            graph->touch();
        }
    }
}

SpatialGraphSelection
FilopodiaFunctions::getLocationSelection(const HxSpatialGraph* graph, const FilopodiaLocation loc)
{
    SpatialGraphSelection sel(graph);
    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        if (FilopodiaFunctions::nodeHasLocation(loc, v, graph))
        {
            sel.selectVertex(v);
        }
    }
    for (int e = 0; e < graph->getNumEdges(); ++e)
    {
        if (FilopodiaFunctions::edgeHasLocation(loc, e, graph))
        {
            sel.selectEdge(e);
        }
    }

    return sel;
}

bool
FilopodiaFunctions::nodeHasLocation(const FilopodiaLocation loc, const int nodeId, const HxSpatialGraph* graph)
{
    const EdgeVertexAttribute* vAtt = graph->findVertexAttribute(getLocationAttributeName());
    if (!vAtt)
    {
        throw McException("FilopodiaFunctions::nodeHasLocation: no Location attribute available");
    }
    return vAtt->getIntDataAtIdx(nodeId) == getLocationLabelId(graph, loc);
}

bool
FilopodiaFunctions::edgeHasLocation(const FilopodiaLocation loc, const int edgeId, const HxSpatialGraph* graph)
{
    const EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(getLocationAttributeName());
    if (!eAtt)
    {
        throw McException("FilopodiaFunctions::nodeHasLocation: no Location attribute available");
    }
    return eAtt->getIntDataAtIdx(edgeId) == getLocationLabelId(graph, loc);
}

// ----------- Functions for ManualGeometry -----------
void
FilopodiaFunctions::addManualGeometryLabelAttribute(HxSpatialGraph* graph)
{
    const char* labelAttName = getManualGeometryLabelAttributeName();
    EdgeVertexAttribute* vAtt = graph->findVertexAttribute(labelAttName);
    EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(labelAttName);

    const McString autoLabel = getManualGeometryLabelName(AUTOMATIC);
    const McString manualLabel = getManualGeometryLabelName(MANUAL);

    HierarchicalLabels* labelGroup;
    if (vAtt && eAtt)
    {
        labelGroup = graph->getLabelGroup(vAtt);
        if (!labelGroup->hasLabel(autoLabel))
        {
            labelGroup->addLabel(0, autoLabel, SbColor(1.0f, 0.0f, 0.0f));
        }
        if (!labelGroup->hasLabel(manualLabel))
        {
            labelGroup->addLabel(0, manualLabel, SbColor(0.0f, 1.0f, 0.0f));
        }
    }
    else if (!vAtt && !eAtt)
    {
        labelGroup = graph->addNewLabelGroup(labelAttName, true, true, false);
        labelGroup->addLabel(0, autoLabel, SbColor(1.0f, 0.0f, 0.0f));
        labelGroup->addLabel(0, manualLabel, SbColor(0.0f, 1.0f, 0.0f));

        vAtt = graph->findVertexAttribute(labelAttName);
        eAtt = graph->findEdgeAttribute(labelAttName);

        const int manualLabelId = getManualGeometryLabelId(graph, AUTOMATIC);
        for (int v = 0; v < graph->getNumVertices(); ++v)
        {
            vAtt->setIntDataAtIdx(v, manualLabelId);
        }

        for (int e = 0; e < graph->getNumEdges(); ++e)
        {
            eAtt->setIntDataAtIdx(e, manualLabelId);
        }
    }
    else
    {
        throw McException("Invalid existing ManualGeometryLabels labels");
    }
}

const char*
FilopodiaFunctions::getManualGeometryLabelName(const FilopodiaManualGeometry ManualGeometry)
{
    switch (ManualGeometry)
    {
        case AUTOMATIC:
            return "AUTOMATIC";
        case MANUAL:
            return "MANUAL";
        default:
            throw McException(QString("FilopodiaFunctions::getManualGeometryLabelName: Label not found"));
    }
}

const char*
FilopodiaFunctions::getManualGeometryLabelAttributeName()
{
    return "ManualGeometry";
}

void
FilopodiaFunctions::setManualGeometryLabel(HxSpatialGraph* graph)
{
    addManualGeometryLabelAttribute(graph);

    const char* labelAttName = getManualGeometryLabelAttributeName();

    const int autoLabel = getManualGeometryLabelId(graph, AUTOMATIC);

    EdgeVertexAttribute* vAtt = graph->findVertexAttribute(labelAttName);
    EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(labelAttName);

    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        vAtt->setIntDataAtIdx(v, autoLabel);
    }

    for (int e = 0; e < graph->getNumEdges(); ++e)
    {
        eAtt->setIntDataAtIdx(e, autoLabel);
    }
}

int
FilopodiaFunctions::getManualGeometryLabelId(const HxSpatialGraph* graph, const FilopodiaManualGeometry ManualGeometry)
{
    if (!graph)
    {
        throw McException(QString("FilopodiaFunctions::getManualGeometryLabelId: no graph"));
    }

    const HierarchicalLabels* labels = graph->getLabelGroup(getManualGeometryLabelAttributeName());
    if (!labels)
    {
        throw McException(QString("FilopodiaFunctions::getManualGeometryLabelId: graph has no node labels"));
    }

    return labels->getLabelIdFromName(McString(getManualGeometryLabelName(ManualGeometry)));
}

int
FilopodiaFunctions::getGeometryIdOfNode(const HxSpatialGraph* graph, const int v)
{
    EdgeVertexAttribute* geoAtt = graph->findVertexAttribute(FilopodiaFunctions::getManualGeometryLabelAttributeName());
    if (!geoAtt)
    {
        throw McException(QString("No attribute found with name %1").arg(QString::fromLatin1(FilopodiaFunctions::getManualGeometryLabelAttributeName())));
    }
    return geoAtt->getIntDataAtIdx(v);
}

void
FilopodiaFunctions::setGeometryIdOfNode(HxSpatialGraph* graph, const int node, const int geometry)
{
    EdgeVertexAttribute* att = graph->findVertexAttribute(getManualGeometryLabelAttributeName());
    if (!att)
    {
        throw McException("FilopodiaFunctions::setGeometryLabel: no Manual Geometry attribute available");
    }
    att->setIntDataAtIdx(node, geometry);
}

int
FilopodiaFunctions::getGeometryIdOfEdge(const HxSpatialGraph* graph, const int e)
{
    EdgeVertexAttribute* geoAtt = graph->findEdgeAttribute(FilopodiaFunctions::getManualGeometryLabelAttributeName());
    if (!geoAtt)
    {
        throw McException(QString("No attribute found with name %1").arg(QString::fromLatin1(FilopodiaFunctions::getManualGeometryLabelAttributeName())));
    }
    return geoAtt->getIntDataAtIdx(e);
}

void
FilopodiaFunctions::setGeometryIdOfEdge(HxSpatialGraph* graph, const int edge, const int geometry)
{
    EdgeVertexAttribute* att = graph->findEdgeAttribute(getManualGeometryLabelAttributeName());
    if (!att)
    {
        throw McException("FilopodiaFunctions::setGeometryLabel: no Manual Geometry attribute available");
    }
    att->setIntDataAtIdx(edge, geometry);
}

// ----------- Functions for Bulbous label   -----------
const char*
FilopodiaFunctions::getBulbousAttributeName()
{
    return "Bulbous";
}

const char*
FilopodiaFunctions::getBulbousLabelName(const FilopodiaBulbous bulbous)
{
    switch (bulbous)
    {
        case BULBOUS:
            return "BULBOUS";
        case NONBULBOUS:
            return "NONBULBOUS";
        default:
            throw McException(QString("FilopodiaFunctions::getBulbousLabelName: Label not found"));
    }
}

void
FilopodiaFunctions::addBulbousLabel(HxSpatialGraph* graph)
{
    const char* attName = getBulbousAttributeName();

    EdgeVertexAttribute* vAtt = graph->findVertexAttribute(attName);
    EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(attName);

    const McString bulbous = getBulbousLabelName(BULBOUS);
    const McString nonbulbous = getBulbousLabelName(NONBULBOUS);

    HierarchicalLabels* labels;
    if (vAtt && eAtt)
    {
        labels = graph->getLabelGroup(attName);
        if (!labels->hasLabel(bulbous))
        {
            labels->addLabel(0, getBulbousLabelName(BULBOUS), SbColor(224.0f, 238.0f, 238.0f));
        }
        if (!labels->hasLabel(nonbulbous))
        {
            labels->addLabel(0, getBulbousLabelName(NONBULBOUS), SbColor(0.0f, 0.0f, 139.0f));
        }
    }
    else if (!vAtt && !eAtt)
    {
        HierarchicalLabels* labels = graph->addNewLabelGroup(attName, true, true, false);
        const int nonbulbousId = labels->addLabel(0, getBulbousLabelName(NONBULBOUS), SbColor(224.0f, 238.0f, 238.0f));
        labels->addLabel(0, getBulbousLabelName(BULBOUS), SbColor(0.0f, 0.0f, 139.0f));

        vAtt = graph->findVertexAttribute(attName);
        for (int v = 0; v < graph->getNumVertices(); ++v)
        {
            vAtt->setIntDataAtIdx(v, nonbulbousId);
        }

        eAtt = graph->findEdgeAttribute(attName);
        for (int e = 0; e < graph->getNumEdges(); ++e)
        {
            eAtt->setIntDataAtIdx(e, nonbulbousId);
        }
    }
    else
    {
        throw McException("Invalid existing bulbous attribute");
    }
}

int
FilopodiaFunctions::getBulbousIdOfNode(const HxSpatialGraph* graph, const int v)
{
    EdgeVertexAttribute* bulbAtt = graph->findVertexAttribute(FilopodiaFunctions::getBulbousAttributeName());
    if (!bulbAtt)
    {
        return 1;
    }
    return bulbAtt->getIntDataAtIdx(v);
}

int
FilopodiaFunctions::getBulbousIdOfEdge(const HxSpatialGraph* graph, const int e)
{
    EdgeVertexAttribute* bulbAtt = graph->findEdgeAttribute(FilopodiaFunctions::getBulbousAttributeName());
    if (!bulbAtt)
    {
        return 1;
    }
    return bulbAtt->getIntDataAtIdx(e);
}

void
FilopodiaFunctions::setBulbousIdOfNode(HxSpatialGraph* graph, const int node, const FilopodiaBulbous bulbous)
{
    EdgeVertexAttribute* att = graph->findVertexAttribute(getBulbousAttributeName());
    if (!att)
    {
        throw McException("FilopodiaFunctions::setBulbousIdOfNode: no bulbous label attribute available");
    }
    att->setIntDataAtIdx(node, getBulbousLabelId(graph, bulbous));
}

void
FilopodiaFunctions::setBulbousIdOfEdge(HxSpatialGraph* graph, const int edge, const FilopodiaBulbous bulbous)
{
    EdgeVertexAttribute* att = graph->findEdgeAttribute(getBulbousAttributeName());
    if (!att)
    {
        throw McException("FilopodiaFunctions::setBulbousIdOfNode: no bulbous label attribute available");
    }
    att->setIntDataAtIdx(edge, getBulbousLabelId(graph, bulbous));
}

int
FilopodiaFunctions::getBulbousLabelId(const HxSpatialGraph* graph, const FilopodiaBulbous bulbous)
{
    if (!graph)
    {
        throw McException(QString("FilopodiaFunctions::getBulbousLabelId: no graph"));
    }

    const HierarchicalLabels* labels = graph->getLabelGroup(getBulbousAttributeName());
    if (!labels)
    {
        throw McException(QString("FilopodiaFunctions::getBulbousLabelId: graph has no match labels"));
    }

    return labels->getLabelIdFromName(McString(getBulbousLabelName(bulbous)));
}

void
FilopodiaFunctions::extendBulbousLabel(HxSpatialGraph* graph)
{
    SpatialGraphSelection bulbousSel = FilopodiaFunctions::getBasesWithBulbousLabel(graph);
    std::vector<int> filoList = FilopodiaFunctions::getFiloIdsWithBulbousLabel(graph, bulbousSel);

    for (int i = 0; i < filoList.size(); ++i)
    {
        const int filoId = filoList[i];
        const QString filoName = FilopodiaFunctions::getFilopodiaLabelNameFromID(graph, filoId);
        const SpatialGraphSelection bulbousFilo = getNodesWithFiloIdFromSelection(graph, bulbousSel, filoId);

        int startBulbousTime = std::numeric_limits<int>::max();
        int endBulbousTime = std::numeric_limits<int>::min();

        SpatialGraphSelection::Iterator bulbousIt(bulbousFilo);
        for (int v = bulbousIt.vertices.nextSelected(); v != -1; v = bulbousIt.vertices.nextSelected())
        {
            const int timeId = FilopodiaFunctions::getTimeIdOfNode(graph, v);
            if (timeId < startBulbousTime)
                startBulbousTime = timeId;
            if (timeId > endBulbousTime)
                endBulbousTime = timeId;
        }

        if (startBulbousTime > endBulbousTime)
        {
            HxMessage::error(QString("Cannot extend bulbous label for filopodium %1.\nPlease assign the first and last timestep where the filopodium is bulbous.").arg(filoName), "ok");
            continue;
        }

        theMsg->printf("filopodium %s is bulbous from %i to %i.", qPrintable(filoName), getTimeStepFromTimeId(graph, startBulbousTime), getTimeStepFromTimeId(graph, endBulbousTime));

        SpatialGraphSelection filoSel = FilopodiaFunctions::getFilopodiaSelectionForAllTimeSteps(graph, filoId);
        SpatialGraphSelection::Iterator filoIt(filoSel);
        for (int v = filoIt.vertices.nextSelected(); v != -1; v = filoIt.vertices.nextSelected())
        {
            const int timeId = FilopodiaFunctions::getTimeIdOfNode(graph, v);
            if (timeId >= startBulbousTime && timeId <= endBulbousTime)
            {
                FilopodiaFunctions::setBulbousIdOfNode(graph, v, BULBOUS);
            }
        }

        for (int e = filoIt.edges.nextSelected(); e != -1; e = filoIt.edges.nextSelected())
        {
            const int timeId = FilopodiaFunctions::getTimeIdOfEdge(graph, e);
            if (timeId >= startBulbousTime && timeId <= endBulbousTime)
            {
                FilopodiaFunctions::setBulbousIdOfEdge(graph, e, BULBOUS);
            }
        }
    }
}

bool
FilopodiaFunctions::hasBulbousLabel(const HxSpatialGraph* graph, const FilopodiaBulbous bulbous, const int node)
{
    const EdgeVertexAttribute* bulbousAtt = graph->findVertexAttribute(getBulbousAttributeName());
    if (!bulbousAtt)
    {
        return false;
    }
    return bulbousAtt->getIntDataAtIdx(node) == getBulbousLabelId(graph, bulbous);
}

SpatialGraphSelection
FilopodiaFunctions::getBasesWithBulbousLabel(const HxSpatialGraph* graph)
{
    SpatialGraphSelection bulbousSel(graph);

    SpatialGraphSelection graphSel(graph);
    graphSel.selectAllVertices();
    SpatialGraphSelection baseSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, graphSel, BASE_NODE);

    SpatialGraphSelection::Iterator it(baseSel);
    for (int v = it.vertices.nextSelected(); v != -1; v = it.vertices.nextSelected())
    {
        if (hasBulbousLabel(graph, BULBOUS, v))
        {
            bulbousSel.selectVertex(v);
        }
    }

    return bulbousSel;
}

std::vector<int>
FilopodiaFunctions::getFiloIdsWithBulbousLabel(const HxSpatialGraph* graph, SpatialGraphSelection bulbousSel)
{
    std::vector<int> filoIds;

    SpatialGraphSelection::Iterator it(bulbousSel);
    for (int v = it.vertices.nextSelected(); v != -1; v = it.vertices.nextSelected())
    {
        const int filoId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, v);

        const auto ub = std::upper_bound(filoIds.begin(), filoIds.end(), filoId);

        if (ub == filoIds.end() || *ub != filoId)
        {
            filoIds.insert(ub, filoId);
        }
    }

    return filoIds;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Tracing
SpatialGraphPoint
FilopodiaFunctions::getEdgePointWithCoords(HxSpatialGraph* graph, McVec3f coords)
{
    SpatialGraphPoint sgPoint(-1, -1);
    float minDist = 0.001;

    for (int e = 0; e < graph->getNumEdges(); ++e)
    {
        for (int p = 0; p < graph->getNumEdgePoints(e); ++p)
        {
            const McVec3f pointCoords = graph->getEdgePoint(e, p);
            float dist = (pointCoords - coords).length();

            if (dist < minDist)
            {
                minDist = dist;
                sgPoint = SpatialGraphPoint(e, p);
            }
        }
    }
    return sgPoint;
}

int
FilopodiaFunctions::getNodeWithCoords(HxSpatialGraph* graph, McVec3f coords)
{
    int node = -1;
    float minDist = 0.001;

    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        const McVec3f vertexCoords = graph->getVertexCoords(v);

        float dist = (vertexCoords - coords).length();
        if (dist < minDist)
        {
            minDist = dist;
            node = v;
        }
    }

    return node;
}

float
FilopodiaFunctions::getEdgeLength(const HxSpatialGraph* graph, const int edge)
{
    // Edge length is the sum of all eucledian distances between the edge points
    float edgeLength = -1.0;

    const int source = graph->getEdgeSource(edge);

    float oldDistance = 0.0f;
    McVec3f oldCoords = graph->getVertexCoords(source);

    for (int p = 0; p < graph->getNumEdgePoints(edge); ++p)
    {
        const McVec3f newCoords = graph->getEdgePoint(edge, p);
        const float dst = (newCoords - oldCoords).length();

        oldCoords = newCoords;
        oldDistance = oldDistance + dst;
        edgeLength = oldDistance;
    }

    return edgeLength;
}

mcint64
getIndexFromXYZ(const McDim3l& location, const McDim3l& dims)
{
    return location[0] + location[1] * dims[0] + location[2] * dims[0] * dims[1];
}

inline uint
qHash(const McDim3l& key)
{
    return qHash((key.nx << 32) | (key.ny << 16) | key.nz);
}

unsigned char
FilopodiaFunctions::vectorToDirection(const McVec3i v)
{
    unsigned char d = 255;
    if ((v.k < -1) || (v.k > 1) || (v.j < -1) || (v.j > 1) || (v.i < -1) || (v.i > 1))
    {
        throw McException("Unexprected prior vector. \n");
    }
    else
    {
        d = 9 * (v.k + 1) + 3 * (v.j + 1) + (v.i + 1);
        return d;
    }
}

McVec3i
FilopodiaFunctions::directionToVector(const unsigned char d)
{
    McVec3i v;
    if ((d < 0) || (d > 26))
    {
        throw McException("Unexpected prior Id. \n");
    }
    else
    {
        v.k = int(d / 9);
        v.j = int((d % 9) / 3);
        v.i = int((d % 9) % 3);
        v = v - McVec3i(1, 1, 1);
    }
    return v;
}

std::vector<McVec3f>
FilopodiaFunctions::trace(const HxSpatialGraph* graph,
                          const HxUniformScalarField3* image,
                          const McVec3f& startPoint,
                          const McVec3f& targetPoint,
                          const int surround,
                          const float intensityWeight,
                          const int topBrightness,
                          const SpatialGraphSelection& edgesFromTime,
                          SpatialGraphPoint& intersectionPoint,
                          int& intersectionNode)
{
    if (!image)
    {
        throw McException("No image for tracing");
    }

    const McDim3l dims = image->lattice().getDims();
    HxUniformCoord3* coords = dynamic_cast<HxUniformCoord3*>(image->lattice().coords());
    QScopedPointer<HxLoc3Uniform> location(dynamic_cast<HxLoc3Uniform*>(coords->createLocation()));

    if (!location)
    {
        throw McException("FilopodiaFunctions::trace: Could not create location");
    }

    if (!location->set(startPoint))
    {
        throw McException("FilopodiaFunctions::trace: Invalid starting point");
    }

    const McDim3l startLoc = McDim3l(location->getIx(), location->getIy(), location->getIz());
    const McVec3f startU = McVec3f(location->getUx(), location->getUy(), location->getUz());
    const McDim3l startNearestVoxelCenter = getNearestVoxelCenterFromIndex(startLoc, startU, dims);

    if (!location->set(targetPoint))
    {
        throw McException("FilopodiaFunctions::trace: Invalid target point");
    }

    const McDim3l targetLoc = McDim3l(location->getIx(), location->getIy(), location->getIz());
    const McVec3f targetU = McVec3f(location->getUx(), location->getUy(), location->getUz());
    const McDim3l targetNearestVoxelCenter = getNearestVoxelCenterFromIndex(targetLoc, targetU, dims);

    McDim3l minVoxel;
    minVoxel.nx = MC_MIN2(startNearestVoxelCenter.nx, targetNearestVoxelCenter.nx);
    minVoxel.ny = MC_MIN2(startNearestVoxelCenter.ny, targetNearestVoxelCenter.ny);
    minVoxel.nz = MC_MIN2(startNearestVoxelCenter.nz, targetNearestVoxelCenter.nz);

    McDim3l maxVoxel;
    maxVoxel.nx = MC_MAX2(startNearestVoxelCenter.nx, targetNearestVoxelCenter.nx);
    maxVoxel.ny = MC_MAX2(startNearestVoxelCenter.ny, targetNearestVoxelCenter.ny);
    maxVoxel.nz = MC_MAX2(startNearestVoxelCenter.nz, targetNearestVoxelCenter.nz);

    McDim3l startVoxel;
    startVoxel.nx = MC_CLAMP(minVoxel.nx - surround, 0, dims.nx);
    startVoxel.ny = MC_CLAMP(minVoxel.ny - surround, 0, dims.ny);
    startVoxel.nz = MC_CLAMP(minVoxel.nz - surround, 0, dims.nz);

    McDim3l endVoxel;
    endVoxel.nx = MC_CLAMP(maxVoxel.nx + surround, 0, dims.nx - 1);
    endVoxel.ny = MC_CLAMP(maxVoxel.ny + surround, 0, dims.ny - 1);
    endVoxel.nz = MC_CLAMP(maxVoxel.nz + surround, 0, dims.nz - 1);

    McHandle<HxUniformLabelField3> dijkstraMap = HxUniformLabelField3::createInstance();
    McHandle<HxUniformScalarField3> distanceMap = HxUniformScalarField3::createInstance();

    HxShortestPathToPointMap::computeDijkstraMap(image, targetPoint, startVoxel, endVoxel, intensityWeight, topBrightness, dijkstraMap, distanceMap);

    return traceWithDijkstra(graph, dijkstraMap, startPoint, targetPoint, edgesFromTime, intersectionPoint, intersectionNode);
}

std::vector<McVec3f>
FilopodiaFunctions::traceWithDijkstra(const HxSpatialGraph* graph,
                                      const HxUniformScalarField3* priorMap,
                                      const McVec3f& startPoint,
                                      const McVec3f& rootPoint,
                                      const SpatialGraphSelection& edgesFromTime,
                                      SpatialGraphPoint& intersectionPoint,
                                      int& intersectionNode)
{
    if (!priorMap)
    {
        throw McException("No Dijkstra map for tracing.\nPlease check image directory.");
    }

    const McVec3f voxelSize = priorMap->getVoxelSize();
    const McBox3f bbox = priorMap->getBoundingBox();
    McDim3l dims = priorMap->lattice().getDims();

    HxUniformCoord3* coords = dynamic_cast<HxUniformCoord3*>(priorMap->lattice().coords());
    HxLoc3Uniform* location = dynamic_cast<HxLoc3Uniform*>(coords->createLocation());

    if (!location->set(startPoint))
    {
        throw McException("Tracing error: Invalid starting point");
    }

    const McDim3l startLoc = McDim3l(location->getIx(), location->getIy(), location->getIz());
    const McVec3f startU = McVec3f(location->getUx(), location->getUy(), location->getUz());
    const McDim3l startNearestVoxelCenter = getNearestVoxelCenterFromIndex(startLoc, startU, dims);
    const McVec3i startIdx(int(startNearestVoxelCenter.nx),
                           int(startNearestVoxelCenter.ny),
                           int(startNearestVoxelCenter.nz));

    if (!location->set(rootPoint))
    {
        throw McException("Tracing error: Invalid root point");
    }

    std::vector<McVec3f> tracedPoints;
    tracedPoints.push_back(startPoint);

    McVec3i currentIdx = startIdx;
    unsigned char prior;
    priorMap->lattice().evalNative(currentIdx[0], currentIdx[1], currentIdx[2], &prior);

    while (prior != FilopodiaFunctions::vectorToDirection(McVec3i(0, 0, 0)))
    {
        const float x = currentIdx[0] * voxelSize.x + bbox.getMin().x;
        const float y = currentIdx[1] * voxelSize.y + bbox.getMin().y;
        const float z = currentIdx[2] * voxelSize.z + bbox.getMin().z;
        const McVec3f edgePoint(x, y, z);
        tracedPoints.push_back(edgePoint);

        const McVec3i priorCoords = FilopodiaFunctions::directionToVector(prior);
        currentIdx = currentIdx + priorCoords;
        priorMap->lattice().evalNative(currentIdx[0], currentIdx[1], currentIdx[2], &prior);
    }

    tracedPoints.push_back(rootPoint);

    if (edgesFromTime.isEmpty()) // If selection of time is empty intersections/branches are not provided
    {
        return tracedPoints;
    }
    else
    {
        return getPointsOfNewBranch(graph, edgesFromTime, tracedPoints, voxelSize, intersectionPoint, intersectionNode);
    }
}

std::vector<McVec2f>
getRadialSamplePoints2DAroundOrigin(const int numAngles,
                                    const int numSamplesPerAngle,
                                    const float delta)
{
    std::vector<McVec2f> samplePoints;
    samplePoints.push_back(McVec2f(0.0f, 0.0f));

    for (int a = 0; a < numAngles; ++a)
    {
        const float angle = float(a) * 2.0f * M_PI / float(numAngles);
        const McVec2f dir(cos(angle), sin(angle));
        for (int r = 1; r <= numSamplesPerAngle; ++r)
        {
            const McVec2f gridPoint(r * delta * dir);
            samplePoints.push_back(McVec2f(gridPoint[0], gridPoint[1]));
        }
    }

    return samplePoints;
}

McVec3f
computePointNormal(const std::vector<McVec3f>& points,
                   const int atPoint)
{
    McVec3f normal;

    if (atPoint == 0)
    {
        normal = points[atPoint + 1] - points[atPoint];
    }
    else if (atPoint == points.size() - 1)
    {
        normal = points[atPoint] - points[atPoint - 1];
    }
    else
    {
        normal = points[atPoint + 1] - points[atPoint - 1];
    }
    normal.normalize();
    return normal;
}

std::vector<McVec3f>
getSamplePoints3D(const std::vector<McVec2f>& samplePoints2D,
                  const McMat4f& transformToPlaneCoords)
{
    std::vector<McVec3f> points;
    for (int p = 0; p < samplePoints2D.size(); ++p)
    {
        McVec4f pOut, pIn(samplePoints2D[p].x, samplePoints2D[p].y, 0.0f, 1.0f);
        transformToPlaneCoords.multVecMatrix(pIn, pOut);
        points.push_back(McVec3f(pOut.x, pOut.y, pOut.z));
    }
    return points;
}

void
getPlaneBasisVectors(const McVec3f& normal,
                     McVec3f& v1,
                     McVec3f& v2)
{
    const McPlane plane(normal);
    v1 = plane.project(McVec3f(0.0f, 0.0f, 1.0f)); // Project z-axis as first basis vector
    if (v1.length() < 0.0001f)
    {
        v1 = plane.project(McVec3f(1.0f, 0.0f, 0.0f));
    }
    v1.normalize();
    v2 = normal.cross(v1);
}

McMat4f
getTransformToPlaneCoords(const McVec3f& normal,
                          const McVec3f& translation)
{
    McVec3f v1, v2;
    getPlaneBasisVectors(normal, v1, v2);
    McMat4f mat(v1.x, v1.y, v1.z, 0.0f, v2.x, v2.y, v2.z, 0.0f, normal.x, normal.y, normal.z, 0.0f, translation.x, translation.y, translation.z, 1.0f);
    return mat;
}

std::vector<float>
getSampleValues(HxUniformScalarField3* image,
                const std::vector<McVec3f>& points)
{
    std::vector<float> values;

    HxFieldEvaluator::Method trilinear = HxFieldEvaluator::EVAL_STANDARD;
    HxFieldEvaluator* eval = HxFieldEvaluator::createEval(image, trilinear);
    HxLocation3* loc = eval->createLocation();

    float result;
    for (int p = 0; p < points.size(); ++p)
    {
        if (loc->move(points[p].x, points[p].y, points[p].z))
        {
            eval->eval(loc, &result);
            values.push_back(result);
        }
        else
        {
            values.push_back(0.0f);
        }
    }
    return values;
}

float
gaussian2D(const GaussianParameters2D& params, const McVec2f& pos)
{
    float xshift = pos.x - params.mean.x;
    float yshift = pos.y - params.mean.y;
    const float exponent = -((xshift * xshift) / (2.0 * params.sigma.x * params.sigma.x) + (yshift * yshift) / (2.0 * params.sigma.y * params.sigma.y));
    return params.height * exp(exponent);
}

GaussianParameters2D
estimateGaussianParams(const std::vector<McVec2f>& points, const std::vector<float>& values)
{
    if (points.size() != values.size())
    {
        throw McException("Invalid parameters for fitGaussian");
    }

    const int numPoints = points.size();

    if (numPoints == 0)
    {
        throw McException("fitGaussian: at least one point required");
    }

    GaussianParameters2D params;

    float valueSum = 0.0f;

    for (int p = 0; p < numPoints; ++p)
    {
        params.mean += values[p] * points[p];
        valueSum += values[p];
        if (values[p] < 0.0f)
        {
            throw McException("estimateGaussianParams: values must be positive");
        }
        if (values[p] > params.height)
        {
            params.height = values[p];
        }
    }

    if (fabs(valueSum) < 0.0001)
    {
        return GaussianParameters2D();
    }

    params.mean = params.mean / valueSum;

    McVec2f vdiff2sum(0.0f, 0.0f);
    for (int p = 0; p < numPoints; ++p)
    {
        const McVec2f diff = points[p] - params.mean;
        const McVec2f vdiff2 = values[p] * diff.compprod(diff);
        vdiff2sum += vdiff2;
    }
    params.sigma.x = sqrt(vdiff2sum.x / valueSum);
    params.sigma.y = sqrt(vdiff2sum.y / valueSum);

    return params;
}

float
computeGaussianError(const std::vector<McVec2f>& points,
                     const std::vector<float>& values,
                     const GaussianParameters2D& params)
{
    float rmsd = 0.0f;
    for (int p = 0; p < points.size(); ++p)
    {
        const float g = gaussian2D(params, points[p]);
        rmsd += (g - values[p]) * (g - values[p]);
    }
    rmsd = sqrt(rmsd);
    return rmsd;
}

float
getErrorOnDiagonal2(const std::vector<McVec2f>& pointsOnDiagonal)
{
    if (pointsOnDiagonal.size() <= 1)
    {
        throw McException("Error in getErrorOnDiagonal: not enough points");
    }

    double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumX2 = 0.0;

    for (int i = 0; i < pointsOnDiagonal.size(); i++)
    {
        sumX += pointsOnDiagonal[i].x;
        sumY += pointsOnDiagonal[i].y;
        sumXY += pointsOnDiagonal[i].x * pointsOnDiagonal[i].y;
        sumX2 += pointsOnDiagonal[i].x * pointsOnDiagonal[i].x;
    }

    const double xMean = sumX / pointsOnDiagonal.size();
    const double yMean = sumY / pointsOnDiagonal.size();

    const double denominator = sumX2 - sumX * xMean;
    if (fabs(denominator) < 1.e-7)
    {
        // Fail: it seems a vertical line
        throw McException("Error in getErrorOnDiagonal: points on vertical line");
    }

    const double slope = (sumXY - sumX * yMean) / denominator;
    const double yInt = yMean - slope * xMean;

    float error = 0.0f;
    for (int i = 0; i < pointsOnDiagonal.size(); i++)
    {
        const float diff = pointsOnDiagonal[i].y - (pointsOnDiagonal[i].x * slope + yInt);
        error += (diff * diff);
    }

    return error;
}

float
getErrorOnHorizontal2(const std::vector<McVec2f>& pointsOnHorizontal)
{
    if (pointsOnHorizontal.size() == 0)
    {
        throw McException("Error in getErrorOnHorizontal: no points");
    }

    float mean = 0.0f;

    for (int i = 0; i < pointsOnHorizontal.size(); ++i)
    {
        mean += pointsOnHorizontal[i].y;
    }

    mean /= pointsOnHorizontal.size();

    float error = 0.0f;
    for (int i = 0; i < pointsOnHorizontal.size(); ++i)
    {
        const float diff = (pointsOnHorizontal[i].y - mean);
        error += (diff * diff);
    }
    return error;
}

float
fitToPiecewiseLinearQuality(const std::vector<McVec2f>& pointsOnDiagonal,
                            const std::vector<McVec2f>& pointsOnHorizontal)
{
    const float errorOnDiagonal2 = getErrorOnDiagonal2(pointsOnDiagonal);
    const float errorOnHorizontal2 = getErrorOnHorizontal2(pointsOnHorizontal);
    const int numPoints = pointsOnDiagonal.size() + pointsOnHorizontal.size();
    const float totalError = (errorOnDiagonal2 + errorOnHorizontal2) / float(numPoints);
    return totalError;
}

int
findBasePointAlongErrorCurve(const std::vector<float>& x,
                             const std::vector<float>& y,
                             const float significantThreshold)
{
    float minError = std::numeric_limits<float>::max();
    int pointNum = -1;
    for (int splitIdx = 2; splitIdx <= x.size() - 2; ++splitIdx)
    {
        std::vector<McVec2f> pointsOnDiagonal;
        std::vector<McVec2f> pointsOnHorizontal;
        for (int i = 0; i < splitIdx; ++i)
        {
            pointsOnDiagonal.push_back(McVec2f(x[i], y[i]));
        }
        for (int i = splitIdx; i < x.size(); ++i)
        {
            pointsOnHorizontal.push_back(McVec2f(x[i], y[i]));
        }

        const float fitError = fitToPiecewiseLinearQuality(pointsOnDiagonal, pointsOnHorizontal);
        if (fitError < minError && fitError > significantThreshold)
        {
            minError = fitError;
            pointNum = splitIdx - 1;
        }
    }
    return pointNum;
}

int
FilopodiaFunctions::findBasePoint(const std::vector<McVec3f>& pathRootToEnd,
                                  HxUniformScalarField3* image,
                                  const float significantThreshold)
{
    const bool exportProfileSamples = false;

    const McVec3f voxelSize = image->getVoxelSize();
    const float sampleRadius = 10.0 * voxelSize[0];
    const int numDirections = 20;
    const int numRadii = 12;
    const float delta = sampleRadius / numRadii;
    const std::vector<McVec2f> samplePoints2D = getRadialSamplePoints2DAroundOrigin(numDirections, numRadii, delta);

    std::vector<float> lengths;
    lengths.push_back(0.0f);
    for (int p = 1; p < pathRootToEnd.size(); ++p)
    {
        const McVec3f p1 = pathRootToEnd[p - 1];
        const McVec3f p2 = pathRootToEnd[p];
        const float length = (p2 - p1).length();
        lengths.push_back(lengths.back() + length);
    }

    std::vector<float> errors;

    // Add copy of path as spatialgraph to objectpool. Add data as attributes.
    McHandle<HxSpatialGraph> gaussGraph;
    if (exportProfileSamples)
    {
        gaussGraph = HxSpatialGraph::createInstance();
        gaussGraph->setLabel(QString("GaussianProfilePath"));
        for (int i = 0; i < pathRootToEnd.size(); ++i)
        {
            gaussGraph->addVertex(pathRootToEnd[i]);
            if (i > 0)
            {
                gaussGraph->addEdge(i - 1, i);
            }
        }
        gaussGraph->addAttribute("Sigma1", HxSpatialGraph::VERTEX, McPrimType::MC_FLOAT, 1);
        gaussGraph->addAttribute("Sigma2", HxSpatialGraph::VERTEX, McPrimType::MC_FLOAT, 1);
        gaussGraph->addAttribute("Error", HxSpatialGraph::VERTEX, McPrimType::MC_FLOAT, 1);
        gaussGraph->addAttribute("CumulativeLength", HxSpatialGraph::VERTEX, McPrimType::MC_FLOAT, 1);
        theObjectPool->addObject(gaussGraph);
    }

    for (int p = 0; p < pathRootToEnd.size(); ++p)
    {
        const McVec3f pos = pathRootToEnd[p];
        const McVec3f normal = computePointNormal(pathRootToEnd, p);
        const McMat4f transformToPlaneCoords = getTransformToPlaneCoords(normal, pos);
        const std::vector<McVec3f> samplePoints3D = getSamplePoints3D(samplePoints2D, transformToPlaneCoords);
        const std::vector<float> values = getSampleValues(image, samplePoints3D);
        const GaussianParameters2D params = estimateGaussianParams(samplePoints2D, values);
        const float gaussianError = computeGaussianError(samplePoints2D, values, params);
        errors.push_back(gaussianError);

        if (exportProfileSamples)
        {
            McHandle<HxSpatialGraph> g = HxSpatialGraph::createInstance();
            g->setLabel(QString("Profile_Point_%1").arg(p));
            for (int i = 0; i < samplePoints3D.size(); ++i)
            {
                g->addVertex(samplePoints3D[i]);
            }
            McHandle<HxSpatialGraph> gxy = HxSpatialGraph::createInstance();
            gxy->setLabel(QString("Profile_Point_XY_%1").arg(p));
            for (int i = 0; i < samplePoints3D.size(); ++i)
            {
                gxy->addVertex(McVec3f(samplePoints2D[i][0], samplePoints2D[i][1], 0.0f));
            }

            EdgeVertexAttribute* att =
                dynamic_cast<EdgeVertexAttribute*>(g->addAttribute("Intensity", HxSpatialGraph::VERTEX, McPrimType::MC_FLOAT, 1));
            EdgeVertexAttribute* attXY =
                dynamic_cast<EdgeVertexAttribute*>(gxy->addAttribute("Intensity", HxSpatialGraph::VERTEX, McPrimType::MC_FLOAT, 1));

            for (int i = 0; i < samplePoints3D.size(); ++i)
            {
                att->setFloatDataAtIdx(i, values[i]);
                attXY->setFloatDataAtIdx(i, values[i]);
            }
            theObjectPool->addObject(g);
            theObjectPool->addObject(gxy);

            EdgeVertexAttribute* s1 = gaussGraph->findVertexAttribute("Sigma1");
            s1->setFloatDataAtIdx(p, params.sigma[0]);
            EdgeVertexAttribute* s2 = gaussGraph->findVertexAttribute("Sigma2");
            s2->setFloatDataAtIdx(p, params.sigma[1]);
            EdgeVertexAttribute* err = gaussGraph->findVertexAttribute("Error");
            err->setFloatDataAtIdx(p, gaussianError);
            qDebug() << "Error" << gaussianError;
            EdgeVertexAttribute* len = gaussGraph->findVertexAttribute("CumulativeLength");
            len->setFloatDataAtIdx(p, lengths[p]);
        }
    }

    const int basePoint = findBasePointAlongErrorCurve(lengths, errors, significantThreshold);
    return basePoint;
}

McDim3l
FilopodiaFunctions::getNearestVoxelCenterFromPointCoords(const McVec3f& point, const HxUniformScalarField3* image)
{
    HxUniformCoord3* coords = dynamic_cast<HxUniformCoord3*>(image->lattice().coords());
    HxLoc3Uniform* location = dynamic_cast<HxLoc3Uniform*>(coords->createLocation());

    if (!location->set(point))
    {
        delete location;
        throw McException("Error get nearest voxel center: Invalid root point");
    }

    const McDim3l dims = image->lattice().getDims();
    const McDim3l index = McDim3l(location->getIx(), location->getIy(), location->getIz());
    const McVec3f fraction = McVec3f(location->getUx(), location->getUy(), location->getUz());
    const McDim3l nearestVoxelCenter = FilopodiaFunctions::getNearestVoxelCenterFromIndex(index, fraction, dims);
    const McDim3l center(int(nearestVoxelCenter.nx),
                         int(nearestVoxelCenter.ny),
                         int(nearestVoxelCenter.nz));

    delete location;
    return center;
}

McDim3l
FilopodiaFunctions::getNearestVoxelCenterFromIndex(const McDim3l& ijk, const McVec3f& fraction, const McDim3l& dims)
{
    McDim3l result(ijk);
    for (int i = 0; i <= 2; ++i)
    {
        if (fraction[i] > 0.5f && ijk[i] < dims[i] - 1)
        {
            result[i] = ijk[i] + 1;
        }
    }
    return result;
}

SpatialGraphPoint
FilopodiaFunctions::intersectionOfTwoEdges(const HxSpatialGraph* graph, const McVec3f& point, const std::vector<int> edgesFromTime)
{
    for (int i = 0; i < edgesFromTime.size(); ++i)
    {
        const int e = edgesFromTime[i];

        std::vector<McVec3f> edgePoints = graph->getEdgePoints(e);
        for (int p = 1; p < edgePoints.size() - 1; ++p)
        {
            const McVec3f currentPoint = edgePoints[p];
            if ((currentPoint - point).length() < 0.001)
            {
                return SpatialGraphPoint(e, p);
            }
        }
    }

    return SpatialGraphPoint(-1, -1);
}

bool
pointInEllipsoid(const McVec3f& point,
                 const McVec3f& center,
                 const McVec3f& width)
{
    const float eps = 0.00001;
    if (width.x < eps || width.y < eps || width.z < eps)
    {
        throw McException(QString("pointInEllipsoid: invalid paramters (width must be larger than zero)"));
    }
    const McVec3f p = (point - center).compquot(width);
    return (p.length2() <= 1.0f);
}

std::vector<McVec3f>
FilopodiaFunctions::getPointsOfNewBranch(const HxSpatialGraph* graph,
                                         const SpatialGraphSelection& edgesToCheck,
                                         const std::vector<McVec3f>& tracedPoints,
                                         const McVec3f& voxelSize,
                                         SpatialGraphPoint& intersectionPoint,
                                         int& intersectionNode)
{
    // This method takes an array of points (usually from tracing)
    // and checks whether it coincides with an existing part (represented by
    // edgesToCheck) of the SpatialGraph.
    // The existing part is usually the subset of edges for a particular timestep.
    // The tracedPoints are assumed to be ordered: the first one is assumed
    // to become a tip, the last one is closest to the blob center.
    // The method returns a sequence of points that can be turned into an edge.
    // If no intersection is found, intersectionPoint is SpatialGraph(-1,-1).
    // If an intersection is found, intersectionPoint contains the point on the
    // graph that should be turned into a branching node.
    // If an intersection is found, the return value contains all traced points
    // up to and including the intersection point.

    const McBox3f bbox = graph->getBoundingBoxOfSelection(edgesToCheck);
    McAuxGrid<SpatialGraphPoint> auxGrid(bbox, voxelSize.z);

    // Fill the aux grid
    SpatialGraphSelection::Iterator it(edgesToCheck);
    for (int e = it.edges.nextSelected(); e != -1; e = it.edges.nextSelected())
    {
        const std::vector<McVec3f> points = graph->getEdgePoints(e);
        for (int p = 0; p < points.size(); ++p)
        {
            auxGrid.insert(points[p], SpatialGraphPoint(e, p));
        }
    }

    std::vector<McVec3f> result;
    intersectionPoint = SpatialGraphPoint(-1, -1);

    for (int p = 0; p < tracedPoints.size(); ++p)
    {
        const McVec3f tracedPoint = tracedPoints[p];
        std::vector<SpatialGraphPoint> nearPoints;
        auxGrid.searchWithRadius(tracedPoint, voxelSize.z, nearPoints);

        if (nearPoints.size() > 0)
        {
            // Find nearest point of all near points
            SpatialGraphPoint nearestPoint(-1, -1);
            float nearestDistance = std::numeric_limits<float>::max();
            McVec3f nearestCoords;
            for (int np = 0; np < nearPoints.size(); ++np)
            {
                const McVec3f nearPoint = graph->getEdgePoint(nearPoints[np].edgeNum, nearPoints[np].pointNum);
                const float d = (nearPoint - tracedPoint).length();
                if (d < nearestDistance)
                {
                    nearestDistance = d;
                    nearestPoint = nearPoints[np];
                    nearestCoords = nearPoint;
                }
            }

            if (pointInEllipsoid(tracedPoint, nearestCoords, voxelSize))
            {
                // Intersection found
                const int edgeNum = nearestPoint.edgeNum;
                const int pointNum = nearestPoint.pointNum;

                if (pointNum == 0)
                {
                    const int node = graph->getEdgeSource(nearestPoint.edgeNum);
                    if (FilopodiaFunctions::checkIfGraphHasAttribute(graph, FilopodiaFunctions::getTypeLabelAttributeName()))
                    {
                        if (FilopodiaFunctions::hasNodeType(graph, BASE_NODE, node))
                        {
                            if (graph->getNumEdgePoints(edgeNum) > 2)
                            {
                                intersectionPoint = SpatialGraphPoint(edgeNum, 1);
                                McVec3f pointCoords = graph->getEdgePoint(intersectionPoint.edgeNum, intersectionPoint.pointNum);
                                result.push_back(pointCoords);
                                qDebug() << "\tintersection found: intersection was base node -> choose next point as branch" << pointCoords.x << pointCoords.y << pointCoords.z;
                            }
                            else
                            {
                                intersectionNode = graph->getEdgeTarget(edgeNum);
                                result.push_back(graph->getVertexCoords(intersectionNode));
                                McVec3f coords = graph->getVertexCoords(intersectionNode);
                                qDebug() << "\tintersection found: intersection was base node -> choose next node as branch" << coords.x << coords.y << coords.z;
                            }
                        }
                        else
                        {
                            intersectionNode = node;
                            result.push_back(graph->getVertexCoords(intersectionNode));
                            qDebug() << "\tintersection found: intersection is source" << intersectionNode << graph->getVertexCoords(intersectionNode).x << graph->getVertexCoords(intersectionNode).y << graph->getVertexCoords(intersectionNode).z;
                        }
                    }
                    else
                    {
                        intersectionNode = node;
                        result.push_back(graph->getVertexCoords(intersectionNode));
                        qDebug() << "\tintersection found: intersection is source" << intersectionNode << graph->getVertexCoords(intersectionNode).x << graph->getVertexCoords(intersectionNode).y << graph->getVertexCoords(intersectionNode).z;
                    }
                }
                else if (pointNum == graph->getNumEdgePoints(nearestPoint.edgeNum) - 1)
                {
                    const int node = graph->getEdgeTarget(nearestPoint.edgeNum);
                    if (FilopodiaFunctions::checkIfGraphHasAttribute(graph, FilopodiaFunctions::getTypeLabelAttributeName()))
                    {
                        if (FilopodiaFunctions::hasNodeType(graph, BASE_NODE, node))
                        {
                            if (graph->getNumEdgePoints(edgeNum) > 2)
                            {
                                intersectionPoint = SpatialGraphPoint(edgeNum, pointNum - 1);
                                McVec3f pointCoords = graph->getEdgePoint(intersectionPoint.edgeNum, intersectionPoint.pointNum);
                                result.push_back(pointCoords);
                                qDebug() << "\tintersection found: intersection was base node -> choose prior point as branch" << pointCoords.x << pointCoords.y << pointCoords.z;
                            }
                            else
                            {
                                intersectionNode = graph->getEdgeSource(edgeNum);
                                result.push_back(graph->getVertexCoords(intersectionNode));
                                McVec3f coords = graph->getVertexCoords(intersectionNode);
                                qDebug() << "\tintersection found: intersection was base node -> choose next node as branch" << coords.x << coords.y << coords.z;
                            }
                        }
                        else
                        {
                            intersectionNode = node;
                            result.push_back(graph->getVertexCoords(intersectionNode));
                            qDebug() << "\tintersection found: intersection is source" << intersectionNode << graph->getVertexCoords(intersectionNode).x << graph->getVertexCoords(intersectionNode).y << graph->getVertexCoords(intersectionNode).z;
                        }
                    }
                    else
                    {
                        intersectionNode = node;
                        result.push_back(graph->getVertexCoords(intersectionNode));
                        qDebug() << "\tintersection found: intersection is source" << intersectionNode << graph->getVertexCoords(intersectionNode).x << graph->getVertexCoords(intersectionNode).y << graph->getVertexCoords(intersectionNode).z;
                    }
                }
                else
                {
                    intersectionPoint = nearestPoint;
                    result.push_back(nearestCoords);
                    qDebug() << "\tintersection found: intersection is point" << nearestCoords.x << nearestCoords.y << nearestCoords.z;
                }
                break;
            }
            else
            {
                // Near point, but no intersection
                result.push_back(tracedPoint);
            }
        }
        else
        {
            // No near points
            result.push_back(tracedPoint);
        }
    }

    // New branch must have at least 2 points
    if (result.size() < 2)
    {
        qDebug() << "\tnew branch has less than 2 edgepoints";
        result.clear();
    }

    return result;
}

Path
FilopodiaFunctions::getPathFromRootToTip(const HxSpatialGraph* graph,
                                         const int rootNode,
                                         const int tipNode)
{
    Path path;
    SpatialGraphSelection sel = graph->getShortestPath(rootNode, tipNode);
    int currentNode = rootNode;

    while (currentNode != tipNode)
    {
        path.nodeNums.push_back(currentNode);
        int nextSegment = getNextEdgeOnPath(sel, graph->getIncidentEdges(currentNode));
        path.segmentNums.push_back(nextSegment);
        const bool reversed = (graph->getEdgeTarget(nextSegment) == currentNode);
        path.segmentReversed.push_back(reversed);
        sel.deselectEdge(nextSegment);
        currentNode = reversed ? graph->getEdgeSource(nextSegment) : graph->getEdgeTarget(nextSegment);
    }
    path.nodeNums.push_back(tipNode);

    return path;
}

int
FilopodiaFunctions::getNextEdgeOnPath(const SpatialGraphSelection& sel,
                                      const IncidenceList& incidentEdges)
{
    for (int e = 0; e < incidentEdges.size(); ++e)
    {
        if (sel.isSelectedEdge(incidentEdges[e]))
        {
            return incidentEdges[e];
        }
    }
    throw McException("No next edge on path found");
}
SpatialGraphPoint
FilopodiaFunctions::getBasePointOnPath(const Path& path,
                                       const HxSpatialGraph* graph,
                                       HxUniformScalarField3* image,
                                       const float significantThreshold)
{
    std::vector<McVec3f> points;
    std::vector<SpatialGraphPoint> originalPoints;

    for (int e = 0; e < path.segmentNums.size(); ++e)
    {
        const int edgeNum = path.segmentNums[e];
        if (path.segmentReversed[e])
        {
            for (int p = graph->getNumEdgePoints(edgeNum) - 1; p >= 0; --p)
            {
                points.push_back(graph->getEdgePoint(edgeNum, p));
                originalPoints.push_back(SpatialGraphPoint(edgeNum, p));
            }
        }
        else
        {
            for (int p = 0; p < graph->getNumEdgePoints(edgeNum) - 1; ++p)
            {
                points.push_back(graph->getEdgePoint(edgeNum, p));
                originalPoints.push_back(SpatialGraphPoint(edgeNum, p));
            }
        }
    }

    const int basePoint = FilopodiaFunctions::findBasePoint(points, image, significantThreshold);

    if (basePoint >= 0)
    {
        return originalPoints[basePoint];
    }
    else
    {
        return SpatialGraphPoint(-1, -1);
    }
}

SpatialGraphSelection
FilopodiaFunctions::getSelectionTilBranchingNode(const HxSpatialGraph* graph, const int selectedNode, int& branchNode)
{
    SpatialGraphSelection selection(graph);
    const SpatialGraphSelection pathToRoot = graph->getShortestPath(selectedNode, branchNode);
    const SpatialGraphSelection branchNodes = FilopodiaFunctions::getNodesOfTypeInSelection(graph, pathToRoot, BRANCHING_NODE);
    const SpatialGraphSelection baseNodes = FilopodiaFunctions::getNodesOfTypeInSelection(graph, pathToRoot, BASE_NODE);

    if (baseNodes.getNumSelectedVertices() == 1)
    {
        const int baseNode = baseNodes.getSelectedVertex(0);
        IncidenceList incidentEdges = graph->getIncidentEdges(baseNode);

        if (incidentEdges.size() == 2)
        {
            if (branchNodes.getNumSelectedVertices() == 0)
            {
                selection.addSelection(pathToRoot);
                return selection;
            }
            else if (branchNodes.getNumSelectedVertices() > 0)
            {
                for (int i = 0; i < branchNodes.getNumSelectedVertices(); ++i)
                {
                    const int node = branchNodes.getSelectedVertex(i);
                    const SpatialGraphSelection pathToNode = graph->getShortestPath(selectedNode, node);
                    const SpatialGraphSelection branchNodesOnPath = FilopodiaFunctions::getNodesOfTypeInSelection(graph, pathToNode, BRANCHING_NODE);
                    if (branchNodesOnPath.getNumSelectedVertices() == 1)
                    {
                        branchNode = branchNodesOnPath.getSelectedVertex(0);
                        const SpatialGraphSelection pathToBranch = graph->getShortestPath(selectedNode, branchNode);
                        selection.addSelection(pathToBranch);
                        return selection;
                    }
                }
            }
            else
            {
                theMsg->printf("Could not select until branching node : Unexpected tree.");
                HxMessage::error(QString("Could not select until branching node: Unexpected tree."), "ok");
                return selection;
            }
        }
        else if (incidentEdges.size() > 2)
        {
            SpatialGraphSelection pathToBase = graph->getShortestPath(selectedNode, baseNode);
            selection.addSelection(pathToBase);
            return selection;
        }
        else
        {
            theMsg->printf("Could not select until branching node : Unexpected tree.");
            HxMessage::error(QString("Could not select until branching node: Unexpected tree."), "ok");
            return selection;
        }
    }

    return selection;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Semiautomatic Functions
HxUniformScalarField3*
FilopodiaFunctions::cropTip(const HxUniformScalarField3* image, const McDim3l tipNode, const McVec3i boundarySize)
{
    HxUniformScalarField3* cropped = image->duplicate();
    HxLattice3& croppedLat = cropped->lattice();

    McVec3i min, max;

    min[0] = tipNode[0] - boundarySize[0];
    min[1] = tipNode[1] - boundarySize[1];
    min[2] = tipNode[2] - boundarySize[2];

    max[0] = tipNode[0] + boundarySize[0];
    max[1] = tipNode[1] + boundarySize[1];
    max[2] = tipNode[2] + boundarySize[2];

    croppedLat.crop(&min[0], &max[0], "0.0");

    return cropped;
}

SpatialGraphSelection
FilopodiaFunctions::getSuccessorOfFilopodium(const HxSpatialGraph* graph, const int baseNode, SpatialGraphSelection& successorSel)
{
    const int filoId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, baseNode);
    const int currentTimeId = FilopodiaFunctions::getTimeIdOfNode(graph, baseNode);
    const int nextTimeId = currentTimeId + 1;

    SpatialGraphSelection filoSel = getFilopodiaSelectionForTime(graph, filoId, nextTimeId);
    SpatialGraphSelection baseSel = getNodesOfTypeInSelection(graph, filoSel, BASE_NODE);
    int nextBase = -1;
    if (baseSel.getNumSelectedVertices() == 1)
    {
        nextBase = baseSel.getSelectedVertex(0);
    }
    else
    {
        // Base node has no successor
        return successorSel;
    }

    const int nextRootNode = getRootNodeFromTimeStep(graph, FilopodiaFunctions::getTimeStepFromTimeId(graph, nextTimeId));
    int branch = nextRootNode;

    SpatialGraphSelection pathToBranch = getSelectionTilBranchingNode(graph, nextBase, branch);
    pathToBranch.deselectVertex(nextRootNode);

    successorSel.addSelection(filoSel);
    successorSel.addSelection(pathToBranch);

    return successorSel;
}

bool
FilopodiaFunctions::getNodePositionInNextTimeStep(const HxUniformScalarField3* templateImage,
                                                  HxUniformScalarField3* nextImage,
                                                  const McVec3f& searchCenter,
                                                  const McVec3i& searchRadius,
                                                  const McVec3f& templateCenter,
                                                  const McVec3i& templateRadius,
                                                  const float correlationThreshold,
                                                  const bool doGrayValueRangeCheck,
                                                  McVec3f& newNode,
                                                  float& correlation)
{
    HxLoc3Uniform* currentLocation = dynamic_cast<HxLoc3Uniform*>(templateImage->lattice().coords()->createLocation());
    if (!currentLocation->set(templateCenter))
    {
        qDebug() << "\t\tPosition error: Invalid starting point in current image";
        return false;
    }

    const McDim3l currentDims = templateImage->lattice().getDims();
    const McDim3l currentNodeLoc = McDim3l(currentLocation->getIx(), currentLocation->getIy(), currentLocation->getIz());
    const McVec3f currentNodeU = McVec3f(currentLocation->getUx(), currentLocation->getUy(), currentLocation->getUz());
    const McDim3l currentNearestVoxelCenter = FilopodiaFunctions::getNearestVoxelCenterFromIndex(currentNodeLoc, currentNodeU, currentDims);
    const float currentGrayValue = templateImage->evalReg(currentNearestVoxelCenter.nx, currentNearestVoxelCenter.ny, currentNearestVoxelCenter.nz);

    delete currentLocation;

    HxLoc3Uniform* nextLocation = dynamic_cast<HxLoc3Uniform*>(nextImage->lattice().coords()->createLocation());
    if (!nextLocation->set(searchCenter))
    {
        qDebug() << "\t\tPosition error: Invalid starting point in next image";
        return false;
    }

    const McDim3l nextDims = nextImage->lattice().getDims();
    const McVec3f nextVoxelSize = nextImage->getVoxelSize();
    const McDim3l nextNodeLoc = McDim3l(nextLocation->getIx(), nextLocation->getIy(), nextLocation->getIz());
    const McVec3f nextNodeU = McVec3f(nextLocation->getUx(), nextLocation->getUy(), nextLocation->getUz());
    const McDim3l nextNearestVoxelCenter = FilopodiaFunctions::getNearestVoxelCenterFromIndex(nextNodeLoc, nextNodeU, nextDims);

    delete nextLocation;

    McHandle<HxUniformScalarField3> currentTemplate =
        FilopodiaFunctions::cropTip(templateImage, currentNearestVoxelCenter, templateRadius);
    currentTemplate->setLabel("Template");

    const McBox3f nextBB = nextImage->getBoundingBox();

    McVec3i regionSize;
    McVec3i regionStart;

    for (int i = 0; i <= 2; ++i)
    {
        regionSize[i] = 2 * searchRadius[i] + 1;
        regionStart[i] = nextNearestVoxelCenter[i] - searchRadius[i];
        if (regionStart[i] < 0)
        {
            regionSize[i] += regionStart[i];
            regionStart[i] = 0;
        }
        if (regionStart[i] + regionSize[i] >= nextDims[i])
        {
            const int diff = abs(nextDims[i] - (regionStart[i] + regionSize[i]));
            regionSize[i] -= diff;
        }
    }

    McHandle<HxFieldCorrelation> computeCorrelation = HxFieldCorrelation::createInstance();
    computeCorrelation->portData.connect(nextImage);
    computeCorrelation->portTemplate.connect(currentTemplate);
    computeCorrelation->portStart.setValue(0, regionStart[0]);
    computeCorrelation->portStart.setValue(1, regionStart[1]);
    computeCorrelation->portStart.setValue(2, regionStart[2]);
    computeCorrelation->portSize.setValue(0, regionSize[0]);
    computeCorrelation->portSize.setValue(1, regionSize[1]);
    computeCorrelation->portSize.setValue(2, regionSize[2]);
    computeCorrelation->portAction.hit();
    computeCorrelation->fire();

    McHandle<HxUniformScalarField3> correlationField = dynamic_cast<HxUniformScalarField3*>(computeCorrelation->getResult());
    if (!correlationField)
    {
        throw McException("Could not create correlationfield");
    }
    correlationField->setLabel("Correlation");

    HxLattice3& corLat = correlationField->lattice();
    const McDim3l dims = corLat.getDims();

    float maxCorr = -1.0f;
    McDim3l maxCorrIdx;
    float value;
    McDim3l pos;
    McVec3f centerPos;
    centerPos[0] = currentNearestVoxelCenter.nx - regionStart.i;
    centerPos[1] = currentNearestVoxelCenter.ny - regionStart.j;
    centerPos[2] = currentNearestVoxelCenter.nz - regionStart.k;

    for (int x = 0; x < dims.nx; ++x)
    {
        for (int y = 0; y < dims.ny; ++y)
        {
            for (int z = 0; z < dims.nz; ++z)
            {
                corLat.eval(x, y, z, &value);

                pos[0] = x + regionStart[0];
                pos[1] = y + regionStart[1];
                pos[2] = z + regionStart[2];

                const float grayValue = nextImage->evalReg(pos[0], pos[1], pos[2]);
                if (!doGrayValueRangeCheck || (grayValue > 0.75 * currentGrayValue && grayValue < 1.25 * currentGrayValue))
                {
                    if (value > maxCorr)
                    {
                        maxCorr = value;
                        maxCorrIdx[0] = x;
                        maxCorrIdx[1] = y;
                        maxCorrIdx[2] = z;
                    }
                    else if (value == maxCorr)
                    {
                        const float distOld = (centerPos - McVec3f(maxCorrIdx.nx, maxCorrIdx.ny, maxCorrIdx.nz)).length();
                        const float distNew = (centerPos - McVec3f(x, y, z)).length();
                        if (distNew > distOld)
                        {
                            maxCorrIdx[0] = x;
                            maxCorrIdx[1] = y;
                            maxCorrIdx[2] = z;
                        }
                    }
                }
            }
        }
    }

    if (maxCorr > correlationThreshold)
    {
        correlation = maxCorr;
        newNode[0] = (maxCorrIdx[0] + regionStart[0]) * nextVoxelSize[0] + nextBB.getMin().x;
        newNode[1] = (maxCorrIdx[1] + regionStart[1]) * nextVoxelSize[1] + nextBB.getMin().y;
        newNode[2] = (maxCorrIdx[2] + regionStart[2]) * nextVoxelSize[2] + nextBB.getMin().z;

        return true;
    }
    correlation = maxCorr;
    return false;
}

QMap<int, QFileInfo>
FilopodiaFunctions::getFilePerTimestep(const QString imageDir, const TimeMinMax timeMinMax)
{
    QMap<int, QFileInfo> filePerTimeStep;
    const QDir dir(imageDir);
    const QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files);

    for (int f = 0; f < fileInfoList.size(); ++f)
    {
        const int time = FilopodiaFunctions::getTimeFromFileNameNoGC(fileInfoList[f].fileName());

        if (filePerTimeStep.contains(time))
        {
            throw McException(QString("Duplicate timestamp %1").arg(time));
        }

        filePerTimeStep.insert(time, fileInfoList[f]);
    }

    const int missing = (timeMinMax.maxT - timeMinMax.minT + 1) - filePerTimeStep.size();
    if (missing != 0)
    {
        throw McException(QString("Missing %1 image files").arg(missing));
    }

    return filePerTimeStep;
}

TimeMinMax
FilopodiaFunctions::getInfluenceArea(HxSpatialGraph* graph, const int node)
{
    const int nodeFiloId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, node);
    const int nodeTimeId = FilopodiaFunctions::getTimeIdOfNode(graph, node);

    const char* geoAttName = FilopodiaFunctions::getManualGeometryLabelAttributeName();
    EdgeVertexAttribute* vGeoAtt = graph->findVertexAttribute(geoAttName);
    const int manualLabelId = FilopodiaFunctions::getManualGeometryLabelId(graph, MANUAL);

    const TimeMinMax timeMinMax = FilopodiaFunctions::getTimeMinMaxFromGraphLabels(graph);
    TimeMinMax influenceArea = timeMinMax;

    int priorNodeTime = timeMinMax.minT;
    int successorNodeTime = timeMinMax.minT;

    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        const int vFiloId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, v);
        const int vGeoId = vGeoAtt->getIntDataAtIdx(v);

        if ((vFiloId == nodeFiloId) && (vGeoId == manualLabelId))
        {
            const int vTimeId = FilopodiaFunctions::getTimeIdOfNode(graph, v);

            if ((vTimeId < nodeTimeId) && (vTimeId > priorNodeTime))
            {
                priorNodeTime = vTimeId;
            }
            if ((vTimeId > nodeTimeId) && (vTimeId < successorNodeTime))
            {
                successorNodeTime = vTimeId;
            }
        }
    }

    const int minInfluence = nodeTimeId - floor((nodeTimeId - priorNodeTime) / 2);
    const int maxInfluence = nodeTimeId + floor((successorNodeTime - nodeTimeId) / 2);

    influenceArea.minT = minInfluence;
    influenceArea.maxT = maxInfluence;
    return influenceArea;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// --------------------------------------------------------------------------------------------------------------
// ----------- Prepare Operations -----------
PointToVertexOperation*
FilopodiaFunctions::convertPointToNode(HxSpatialGraph* graph,
                                       const SpatialGraphPoint& point)
{
    SpatialGraphSelection sel(graph);
    sel.selectPoint(point);

    PointToVertexOperation* convertOp = new PointToVertexOperation(graph, sel, SpatialGraphSelection(graph));
    convertOp->exec();

    return convertOp;
}

HxSpatialGraph*
FilopodiaFunctions::createFilopodiaBranch(const HxSpatialGraph* graph,
                                          const McVec3f newCoords,
                                          const int rootNode,
                                          const std::vector<McVec3f> points,
                                          SpatialGraphSelection& intersectionSel,
                                          HxUniformScalarField3* image)
{
    HxSpatialGraph* tmpGraph = HxSpatialGraph::createInstance();
    FilopodiaFunctions::addTypeLabelAttribute(tmpGraph);
    FilopodiaFunctions::addManualNodeMatchLabelAttribute(tmpGraph);

    int intersectionNode = -1;
    SpatialGraphPoint intersectionPoint = SpatialGraphPoint(-1, -1);

    if (intersectionSel.getNumSelectedVertices() + intersectionSel.getNumSelectedEdges() + intersectionSel.getNumSelectedPoints() != 1)
    {
        HxMessage::error(QString("Error: Cannot create filopodia branch.\nInvalid intersection sel.\n"), "ok");
        return tmpGraph;
    }
    if (intersectionSel.getNumSelectedVertices() == 1)
        intersectionNode = intersectionSel.getSelectedVertex(0);
    if (intersectionSel.getNumSelectedPoints() == 1)
        intersectionPoint = intersectionSel.getSelectedPoint(0);

    bool branchIsGC;
    int connectNode;

    if (intersectionNode != -1 && intersectionNode != rootNode)
    {
        connectNode = tmpGraph->addVertex(graph->getVertexCoords(intersectionNode));
        setNodeType(tmpGraph, connectNode, ROOT_NODE);
        branchIsGC = FilopodiaFunctions::checkIfNodeIsPartOfGrowthCone(graph, intersectionNode, rootNode);
    }
    else if (intersectionPoint != SpatialGraphPoint(-1, -1))
    {
        const int intersectionPointNum = intersectionPoint.pointNum;
        const int intersectionPointEdge = intersectionPoint.edgeNum;

        if (intersectionPointNum == 0)
        {
            const int intersection = graph->getEdgeSource(intersectionPointEdge);
            connectNode = tmpGraph->addVertex(graph->getVertexCoords(intersection));
            setNodeType(tmpGraph, connectNode, ROOT_NODE);
            intersectionSel.deselectPoint(intersectionPoint);
            intersectionSel.selectVertex(intersection);
            branchIsGC = FilopodiaFunctions::checkIfNodeIsPartOfGrowthCone(graph, intersection, rootNode);
        }
        else if (intersectionPointNum == graph->getNumEdgePoints(intersectionPointEdge) - 1)
        {
            const int intersection = graph->getEdgeTarget(intersectionPointEdge);
            connectNode = tmpGraph->addVertex(graph->getVertexCoords(intersection));
            setNodeType(tmpGraph, connectNode, ROOT_NODE);
            intersectionSel.deselectPoint(intersectionPoint);
            intersectionSel.selectVertex(intersection);
            branchIsGC = FilopodiaFunctions::checkIfNodeIsPartOfGrowthCone(graph, intersection, rootNode);
        }
        else
        {
            const McVec3f pointCoords = graph->getEdgePoint(intersectionPointEdge, intersectionPointNum);
            connectNode = tmpGraph->addVertex(pointCoords);
            setNodeType(tmpGraph, connectNode, ROOT_NODE);
            branchIsGC = FilopodiaFunctions::checkIfPointIsPartOfGrowthCone(graph, intersectionPoint, rootNode);
        }
    }
    else
    {
        connectNode = tmpGraph->addVertex(graph->getVertexCoords(rootNode));
        setNodeType(tmpGraph, connectNode, ROOT_NODE);
        branchIsGC = true;
    }

    SpatialGraphSelection connectSel(tmpGraph);
    connectSel.selectVertex(connectNode);

    // Add new tip
    const int newNode = tmpGraph->addVertex(newCoords);
    setNodeType(tmpGraph, newNode, TIP_NODE);

    // Add new edge between tip and intersection node
    int newEdge;
    McVec3f v1coords = tmpGraph->getVertexCoords(newNode);
    McVec3f v2coords = tmpGraph->getVertexCoords(connectNode);

    if (v1coords.equals(points.front()) && v2coords.equals(points.back()))
    {
        newEdge = tmpGraph->addEdge(newNode, connectNode, points);
    }
    else if (v1coords.equals(points.back()) && v2coords.equals(points.front()))
    {
        newEdge = tmpGraph->addEdge(connectNode, newNode, points);
    }
    else
    {
        qDebug() << "\t\tAddFilopodiaVertexAndConnectOperation: first and last edge point do not match vertices.";
        return tmpGraph;
    }

    // Find base on new edge
    if (branchIsGC)
    {
        int newBase = -1;

        std::vector<McVec3f> edgePoints(points);
        edgePoints.erase(edgePoints.begin());
        edgePoints.pop_back();

        const float significantThreshold = 0.0f; // Significant threshold is 0 because all added nodes (filopodia) need base node
        int basePointNum = FilopodiaFunctions::findBasePoint(edgePoints, image, significantThreshold);
        const SpatialGraphPoint basePoint(newEdge, basePointNum + 1);

        if (basePoint != SpatialGraphPoint(-1, -1))
        {
            if (basePoint.pointNum == 0)
            {
                newBase = tmpGraph->getEdgeSource(basePoint.edgeNum);
            }
            else if (basePoint.pointNum == tmpGraph->getNumEdgePoints(basePoint.edgeNum) - 1)
            {
                newBase = tmpGraph->getEdgeTarget(basePoint.edgeNum);
            }
            else
            {
                SpatialGraphSelection pointSel(tmpGraph);
                pointSel.selectPoint(basePoint);
                PointToVertexOperation* pointToVertexOp = new PointToVertexOperation(tmpGraph, pointSel, SpatialGraphSelection(tmpGraph));
                pointToVertexOp->exec();
                newBase = pointToVertexOp->getNewVertexNum();

                setNodeType(tmpGraph, newBase, BASE_NODE);
            }
        }
    }
    return tmpGraph;
}

HxSpatialGraph*
FilopodiaFunctions::createBranchToRoot(const HxSpatialGraph* graph,
                                       const McVec3f newCoords,
                                       const SpatialGraphSelection deleteSel,
                                       SpatialGraphSelection& intersectionSel,
                                       const HxUniformScalarField3* image,
                                       const int intensityWeight,
                                       const float topBrightness)
{
    // Create tmpGraph
    HxSpatialGraph* tmpGraph = HxSpatialGraph::createInstance();
    FilopodiaFunctions::addTypeLabelAttribute(tmpGraph);
    FilopodiaFunctions::addLocationLabelAttribute(tmpGraph);
    FilopodiaFunctions::addManualGeometryLabelAttribute(tmpGraph);
    FilopodiaFunctions::addManualNodeMatchLabelAttribute(tmpGraph);

    // Get old labels
    const SpatialGraphSelection baseSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, deleteSel, BASE_NODE);
    if (baseSel.getNumSelectedVertices() != 1)
    {
        qDebug() << "\t\tcreateBranchToRoot: cannot move base. more or less than 1 base selected.";
        tmpGraph->clear();
        return tmpGraph;
    }
    const int oldBase = baseSel.getSelectedVertex(0);
    const int timeId = FilopodiaFunctions::getTimeIdOfNode(graph, oldBase);
    const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, FilopodiaFunctions::getTimeStepFromTimeId(graph, timeId));

    // Add tip nodes
    SpatialGraphSelection newTipNodes(tmpGraph);
    SpatialGraphSelection tipSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, deleteSel, TIP_NODE);
    if (tipSel.getNumSelectedVertices() > 0)
    {
        for (int v = 0; v < tipSel.getNumSelectedVertices(); ++v)
        {
            const int oldTip = tipSel.getSelectedVertex(v);
            const int newTip = tmpGraph->addVertex(graph->getVertexCoords(oldTip));
            setNodeType(tmpGraph, newTip, TIP_NODE);
            setGeometryIdOfNode(tmpGraph, newTip, FilopodiaFunctions::getGeometryIdOfNode(graph, oldTip));
            newTipNodes.resize(tmpGraph->getNumVertices(), tmpGraph->getNumEdges());
            newTipNodes.selectVertex(newTip);
        }
    }
    else
    {
        qDebug() << "\t\tcreateBranchToRoot: cannot move base. Base has no tip node.";
        tmpGraph->clear();
        return tmpGraph;
    }

    // Add base node
    const int newBaseNode = tmpGraph->addVertex(newCoords);
    setNodeType(tmpGraph, newBaseNode, BASE_NODE);

    // Add new filopodium edges
    for (int v = 0; v < newTipNodes.getNumSelectedVertices(); ++v)
    {
        const int newTip = newTipNodes.getSelectedVertex(v);

        // Trace selected tip to new base node
        const SpatialGraphSelection filoSel = tmpGraph->getConnectedComponent(newBaseNode);
        SpatialGraphPoint intersectionPointFilo(-1, -1);
        int intersectionNodeFilo = -1;
        std::vector<McVec3f> pointsFilo = FilopodiaFunctions::trace(tmpGraph,
                                                                    image,
                                                                    tmpGraph->getVertexCoords(newTip),
                                                                    newCoords,
                                                                    10,
                                                                    intensityWeight,
                                                                    topBrightness,
                                                                    filoSel, // Only intersections within filopodium are allowed
                                                                    intersectionPointFilo,
                                                                    intersectionNodeFilo);

        int connectNodeFilo = newBaseNode;

        if (intersectionNodeFilo != -1)
        {
            connectNodeFilo = tmpGraph->addVertex(graph->getVertexCoords(intersectionNodeFilo));
            setNodeType(tmpGraph, connectNodeFilo, BRANCHING_NODE);
        }
        else if (intersectionPointFilo != SpatialGraphPoint(-1, -1))
        {
            SpatialGraphSelection pointSel(tmpGraph);
            pointSel.selectPoint(intersectionPointFilo);
            PointToVertexOperation* pointToVertexOp = new PointToVertexOperation(tmpGraph, pointSel, SpatialGraphSelection(tmpGraph));
            pointToVertexOp->exec();
            connectNodeFilo = pointToVertexOp->getNewVertexNum();
            setNodeType(tmpGraph, connectNodeFilo, BRANCHING_NODE);
        }
        else
        {
            IncidenceList incidentEdges = tmpGraph->getIncidentEdges(newBaseNode);
            if (incidentEdges.size() == 1)
            {
                qDebug() << "connection within filopodium is base node -> new branchs is neighboring edge point";
                int edge = incidentEdges[0];
                SpatialGraphPoint neighborPoint = SpatialGraphPoint(edge, tmpGraph->getNumEdgePoints(edge) - 2);
                SpatialGraphSelection pointSel(tmpGraph);
                pointSel.selectPoint(neighborPoint);
                PointToVertexOperation* pointToVertexOp = new PointToVertexOperation(tmpGraph, pointSel, SpatialGraphSelection(tmpGraph));
                pointToVertexOp->exec();
                connectNodeFilo = pointToVertexOp->getNewVertexNum();
                setNodeType(tmpGraph, connectNodeFilo, BRANCHING_NODE);
                pointsFilo.pop_back();
                pointsFilo.push_back(tmpGraph->getVertexCoords(connectNodeFilo));
            }
        }

        int filoEdge;
        McVec3f v1coords = tmpGraph->getVertexCoords(newTip);
        McVec3f v2coords = tmpGraph->getVertexCoords(connectNodeFilo);

        if (v1coords.equals(pointsFilo.front()) && v2coords.equals(pointsFilo.back()))
        {
            filoEdge = tmpGraph->addEdge(newTip, connectNodeFilo, pointsFilo);
        }
        else if (v1coords.equals(pointsFilo.back()) && v2coords.equals(pointsFilo.front()))
        {
            filoEdge = tmpGraph->addEdge(connectNodeFilo, newTip, pointsFilo);
        }
        else
        {
            qDebug() << "\t\tcreateBranchToRoot: first and last edge point do not match vertices.";
            tmpGraph->clear();
            return tmpGraph;
        }
        setLocationIdOfEdge(tmpGraph, filoEdge, FILOPODIUM);
    }

    // Add new growth cone edge
    const int currentTime = FilopodiaFunctions::getTimeStepFromTimeId(graph, timeId);
    SpatialGraphSelection selectionForTime = FilopodiaFunctions::getSelectionForTimeStep(graph, currentTime);
    SpatialGraphSelection filoSel = FilopodiaFunctions::getFilopodiumSelectionOfTimeStep(graph, currentTime);
    SpatialGraphSelection growthSel(selectionForTime);
    growthSel.subtractSelection(filoSel);
    growthSel.subtractSelection(deleteSel); // Only intersection within growthcone are allowed

    SpatialGraphPoint intersectionPointGC(-1, -1);
    int intersectionNodeGC = -1;
    int connectNodeGC = -1;

    // Trace new base to root
    const std::vector<McVec3f> pointsGC = FilopodiaFunctions::trace(graph,
                                                                 image,
                                                                 newCoords,
                                                                 graph->getVertexCoords(rootNode),
                                                                 10,
                                                                 intensityWeight,
                                                                 topBrightness,
                                                                 growthSel,
                                                                 intersectionPointGC,
                                                                 intersectionNodeGC);

    if (intersectionNodeGC > 0)
    {
        connectNodeGC = tmpGraph->addVertex(graph->getVertexCoords(intersectionNodeGC));
        setNodeType(tmpGraph, connectNodeGC, ROOT_NODE); // Intersection within growth cone is root of branch
        intersectionSel.selectVertex(intersectionNodeGC);
    }
    else if (intersectionPointGC != SpatialGraphPoint(-1, -1))
    {
        connectNodeGC = tmpGraph->addVertex(graph->getEdgePoint(intersectionPointGC.edgeNum, intersectionPointGC.pointNum));
        setNodeType(tmpGraph, connectNodeGC, ROOT_NODE); // Intersection within growth cone is root of branch
        intersectionSel.selectPoint(intersectionPointGC);
    }
    else
    {
        connectNodeGC = tmpGraph->addVertex(graph->getVertexCoords(rootNode));
        setNodeType(tmpGraph, connectNodeGC, ROOT_NODE); // Intersection within growth cone is root of branch
        intersectionSel.selectVertex(rootNode);
    }

    int gcEdge;
    McVec3f v1coords = tmpGraph->getVertexCoords(newBaseNode);
    McVec3f v2coords = tmpGraph->getVertexCoords(connectNodeGC);

    if (v1coords.equals(pointsGC.front()) && v2coords.equals(pointsGC.back()))
    {
        gcEdge = tmpGraph->addEdge(newBaseNode, connectNodeGC, pointsGC);
    }
    else if (v1coords.equals(pointsGC.back()) && v2coords.equals(pointsGC.front()))
    {
        gcEdge = tmpGraph->addEdge(connectNodeGC, newBaseNode, pointsGC);
    }
    else
    {
        qDebug() << "\t\tcreateBranchToRoot: first and last edge point do not match vertices.";
        tmpGraph->clear();
        return tmpGraph;
    }

    setLocationIdOfEdge(tmpGraph, gcEdge, GROWTHCONE);

    return tmpGraph;
}

HxSpatialGraph*
FilopodiaFunctions::createBranchToRootWithBase(const HxSpatialGraph* graph,
                                               const int tipNode,
                                               const McVec3f newCoords,
                                               const SpatialGraphSelection deleteSel,
                                               SpatialGraphSelection& intersectionSel,
                                               const HxUniformScalarField3* image,
                                               const int intensityWeight,
                                               const float topBrightness)
{
    McHandle<HxSpatialGraph> tmpGraph = HxSpatialGraph::createInstance();
    FilopodiaFunctions::addTypeLabelAttribute(tmpGraph);
    FilopodiaFunctions::addLocationLabelAttribute(tmpGraph);
    FilopodiaFunctions::addManualGeometryLabelAttribute(tmpGraph);
    FilopodiaFunctions::addManualNodeMatchLabelAttribute(tmpGraph);

    const int unassignedLabelId = FilopodiaFunctions::getMatchLabelId(graph, UNASSIGNED);
    int matchId = unassignedLabelId;

    SpatialGraphSelection filoSel = FilopodiaFunctions::getFilopodiaSelectionOfNode(graph, tipNode);
    SpatialGraphSelection oldTipSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, filoSel, TIP_NODE);
    SpatialGraphSelection oldBaseSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, filoSel, BASE_NODE);
    if (oldBaseSel.getNumSelectedVertices() == 1 && oldTipSel.getNumSelectedVertices() == 1)
    {
        const int oldBaseNode = oldBaseSel.getSelectedVertex(0);
        matchId = FilopodiaFunctions::getMatchIdOfNode(graph, oldBaseNode);
    }
    else
    {
        qDebug() << "\t\tMoveBaseOfTipOperation: cannot move edge. Tip has no base.";
        tmpGraph->clear();
        return tmpGraph;
    }

    // Add tip node
    const int newTip = tmpGraph->addVertex(graph->getVertexCoords(tipNode));
    setNodeType(tmpGraph, newTip, TIP_NODE);
    setGeometryIdOfNode(tmpGraph, newTip, FilopodiaFunctions::getGeometryIdOfNode(graph, tipNode));

    // Add base
    const int newBaseNode = tmpGraph->addVertex(newCoords);
    setNodeType(tmpGraph, newBaseNode, BASE_NODE);
    setManualNodeMatchId(tmpGraph, newBaseNode, matchId);

    // Trace new base to root
    const int currentTime = FilopodiaFunctions::getTimeOfNode(graph, tipNode);
    const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, currentTime);
    SpatialGraphSelection growthSel = FilopodiaFunctions::getGrowthConeSelectionOfTimeStep(graph, currentTime);
    growthSel.subtractSelection(deleteSel);

    SpatialGraphPoint intersectionPointGC(-1, -1);
    int intersectionNodeGC = -1;

    const std::vector<McVec3f> pointsGC = FilopodiaFunctions::trace(graph,
                                                                     image,
                                                                     newCoords,
                                                                     graph->getVertexCoords(rootNode),
                                                                     10,
                                                                     intensityWeight,
                                                                     topBrightness,
                                                                     growthSel,
                                                                     intersectionPointGC,
                                                                     intersectionNodeGC);

    int connectNodeGC = -1;

    if (intersectionNodeGC > 0)
    {
        connectNodeGC = tmpGraph->addVertex(graph->getVertexCoords(intersectionNodeGC));
        setNodeType(tmpGraph, connectNodeGC, ROOT_NODE);
        intersectionSel.selectVertex(intersectionNodeGC);
    }
    else if (intersectionPointGC != SpatialGraphPoint(-1, -1))
    {
        connectNodeGC = tmpGraph->addVertex(graph->getEdgePoint(intersectionPointGC.edgeNum, intersectionPointGC.pointNum));
        setNodeType(tmpGraph, connectNodeGC, ROOT_NODE);
        intersectionSel.selectPoint(intersectionPointGC);
    }
    else
    {
        connectNodeGC = tmpGraph->addVertex(graph->getVertexCoords(rootNode));
        setNodeType(tmpGraph, connectNodeGC, ROOT_NODE);
        intersectionSel.selectVertex(rootNode);
    }

    // add new gc edge
    int gcEdge = -1;
    McVec3f v1coords = tmpGraph->getVertexCoords(newBaseNode);
    McVec3f v2coords = tmpGraph->getVertexCoords(connectNodeGC);

    if (v1coords.equals(pointsGC.front()) && v2coords.equals(pointsGC.back()))
    {
        gcEdge = tmpGraph->addEdge(newBaseNode, connectNodeGC, pointsGC);
    }
    else if (v1coords.equals(pointsGC.back()) && v2coords.equals(pointsGC.front()))
    {
        gcEdge = tmpGraph->addEdge(connectNodeGC, newBaseNode, pointsGC);
    }
    else
    {
        qDebug() << "\t\tMoveBaseOfTipOperation: first and last edge point do not match vertices.";
        tmpGraph->clear();
        return tmpGraph;
    }
    setLocationIdOfEdge(tmpGraph, gcEdge, GROWTHCONE);

    // Trace selected tip to new base
    SpatialGraphPoint intersectionPointFilo(-1, -1);
    int intersectionNodeFilo = -1;
    const std::vector<McVec3f> pointsFilo = FilopodiaFunctions::trace(graph,
                                                                      image,
                                                                      tmpGraph->getVertexCoords(newTip),
                                                                      newCoords,
                                                                      10,
                                                                      intensityWeight,
                                                                      topBrightness,
                                                                      SpatialGraphSelection(graph), // Time selection is empty to
                                                                      intersectionPointFilo,     // Force connection with new branch node
                                                                      intersectionNodeFilo);

    // Add new filo edge
    int filoEdge = -1;
    McVec3f v3coords = tmpGraph->getVertexCoords(newBaseNode);
    McVec3f v4coords = tmpGraph->getVertexCoords(newTip);

    if (v3coords.equals(pointsFilo.front()) && v4coords.equals(pointsFilo.back()))
    {
        filoEdge = tmpGraph->addEdge(newBaseNode, newTip, pointsFilo);
    }
    else if (v3coords.equals(pointsFilo.back()) && v4coords.equals(pointsFilo.front()))
    {
        filoEdge = tmpGraph->addEdge(newTip, newBaseNode, pointsFilo);
    }
    else
    {
        qDebug() << "\t\tMoveBaseOfTipOperation: first and last edge point do not match vertices.";
        tmpGraph->clear();
        return tmpGraph;
    }
    setLocationIdOfEdge(tmpGraph, filoEdge, GROWTHCONE);

    return tmpGraph;
}

HxSpatialGraph*
FilopodiaFunctions::createBranchToPoint(const HxSpatialGraph* graph,
                                        const SpatialGraphSelection deleteSel,
                                        const SpatialGraphSelection& intersectionSel,
                                        HxUniformScalarField3* image,
                                        const int intensityWeight,
                                        const float topBrightness)
{
    HxSpatialGraph* tmpGraph = HxSpatialGraph::createInstance();
    FilopodiaFunctions::addTypeLabelAttribute(tmpGraph);
    FilopodiaFunctions::addLocationLabelAttribute(tmpGraph);
    FilopodiaFunctions::addManualGeometryLabelAttribute(tmpGraph);

    // Get labels of old tip
    SpatialGraphSelection tipSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, deleteSel, TIP_NODE);
    if (tipSel.getNumSelectedVertices() != 1)
    {
        return tmpGraph;
    }

    // Selection point
    if (intersectionSel.getNumSelectedPoints() != 1)
    {
        qDebug() << "CreateBranchToPoint: wrong selection.";
        return tmpGraph;
    }

    const int oldTip = tipSel.getSelectedVertex(0);
    const SpatialGraphPoint point = intersectionSel.getSelectedPoint(0);
    const int currentTime = FilopodiaFunctions::getTimeOfNode(graph, oldTip);
    const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, currentTime);

    // Add new branching node
    const int newBranch = tmpGraph->addVertex(graph->getEdgePoint(point.edgeNum, point.pointNum));
    setNodeType(tmpGraph, newBranch, ROOT_NODE);

    // Add tip
    const int newTip = tmpGraph->addVertex(graph->getVertexCoords(oldTip));
    setNodeType(tmpGraph, newTip, TIP_NODE);
    setGeometryIdOfNode(tmpGraph, newTip, FilopodiaFunctions::getGeometryIdOfNode(graph, oldTip));

    // Trace selected tip to edge point
    SpatialGraphPoint intersectionPoint(-1, -1);
    int intersectionNode = -1;
    const std::vector<McVec3f> points = FilopodiaFunctions::trace(graph,
                                                               image,
                                                               tmpGraph->getVertexCoords(newTip),
                                                               tmpGraph->getVertexCoords(newBranch),
                                                               10,
                                                               intensityWeight,
                                                               topBrightness,
                                                               SpatialGraphSelection(graph), // Time selection is empty to
                                                               intersectionPoint,            // Force connection with new branch node
                                                               intersectionNode);

    // Add new edge
    int newEdge = -1;
    McVec3f v1coords = tmpGraph->getVertexCoords(newTip);
    McVec3f v2coords = tmpGraph->getVertexCoords(newBranch);

    if (v1coords.equals(points.front()) && v2coords.equals(points.back()))
    {
        newEdge = tmpGraph->addEdge(newTip, newBranch, points);
    }
    else if (v1coords.equals(points.back()) && v2coords.equals(points.front()))
    {
        newEdge = tmpGraph->addEdge(newBranch, newTip, points);
    }
    else
    {
        qDebug() << "CreateBranchToPoint: first and last edge point do not match vertices.";
        tmpGraph->clear();
        return tmpGraph;
    }

    // Add base if branch is part of growth cone
    if (FilopodiaFunctions::checkIfPointIsPartOfGrowthCone(graph, point, rootNode))
    {
        int newBaseNode = -1;

        // Find base
        std::vector<McVec3f> edgePoints(points);
        edgePoints.erase(edgePoints.begin());
        edgePoints.pop_back();

        int basePointNum = FilopodiaFunctions::findBasePoint(edgePoints, image, 0); // significant threshold is 0 because all tips need base
        const SpatialGraphPoint basePoint(newEdge, basePointNum + 1);

        if (basePoint != SpatialGraphPoint(-1, -1))
        {
            if (basePoint.pointNum == 0)
            {
                newBaseNode = tmpGraph->getEdgeSource(basePoint.edgeNum);
            }
            else if (basePoint.pointNum == tmpGraph->getNumEdgePoints(basePoint.edgeNum) - 1)
            {
                newBaseNode = tmpGraph->getEdgeTarget(basePoint.edgeNum);
            }
            else
            {
                SpatialGraphSelection pointSel(tmpGraph);
                pointSel.selectPoint(basePoint);
                PointToVertexOperation* pointToVertexOp = new PointToVertexOperation(tmpGraph, pointSel, SpatialGraphSelection(tmpGraph));
                pointToVertexOp->exec();
                newBaseNode = pointToVertexOp->getNewVertexNum();

                setNodeType(tmpGraph, newBaseNode, BASE_NODE);
            }
        }
    }
    return tmpGraph;
}

HxSpatialGraph*
FilopodiaFunctions::createGraphForNextTimeStep(const HxSpatialGraph* graph,
                                               const int nextTime,
                                               const SpatialGraphSelection baseSel,
                                               const HxUniformScalarField3* currentImage,
                                               HxUniformScalarField3* nextImage,
                                               const HxUniformScalarField3* nextDijkstraMap,
                                               const McVec3i baseSearchRadius,
                                               const McVec3i baseTemplateRadius,
                                               const McVec3i tipSearchRadius,
                                               const McVec3i tipTemplateRadius,
                                               const float correlationThreshold,
                                               const float lengthThreshold,
                                               const float intensityWeight,
                                               const int topBrightness,
                                               McDArray<int>& previousBases)
{
    const int currentTime = nextTime - 1;
    HxSpatialGraph* tmpGraph = HxSpatialGraph::createInstance();
    FilopodiaFunctions::addTypeLabelAttribute(tmpGraph);
    FilopodiaFunctions::addLocationLabelAttribute(tmpGraph);
    FilopodiaFunctions::addFilopodiaLabelAttribute(tmpGraph);

    // Abuse this att on the temporary graph for storing matching vertices,
    // not manualMatchIds, as they may not exist yet on the current nodes (could be all unassigned).
    // In this case they need to be added on both the current and the next nodes.
    FilopodiaFunctions::addManualNodeMatchLabelAttribute(tmpGraph);

    const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, nextTime);
    McVec3f rootPos = graph->getVertexCoords(rootNode);
    const int tmpRoot = tmpGraph->addVertex(graph->getVertexCoords(rootNode));
    setNodeType(tmpGraph, tmpRoot, ROOT_NODE);
    setLocationIdOfNode(tmpGraph, tmpRoot, GROWTHCONE);

    float correlation;

    SpatialGraphSelection::Iterator it(baseSel);
    for (int s = it.vertices.nextSelected(); s != -1; s = it.vertices.nextSelected())
    {
        McVec3f nextBasePos;
        const int timeId = FilopodiaFunctions::getTimeIdOfNode(graph, s);
        if (timeId != FilopodiaFunctions::getTimeIdFromTimeStep(graph, currentTime))
        {
            HxMessage::error(QString("Warning: Selected base node %1 is not from current time. Skipping.\n").arg(s), "ok");
            continue;
        }

        // Get root node drifting
        const int currentRootNode = getRootNodeFromTimeStep(graph, currentTime);
        const McVec3f currentRootPos = graph->getVertexCoords(currentRootNode);
        const McVec3f drift = currentRootPos - rootPos;

        // Propagate base node
        const bool doGrayValueRangeCheck = true;

        if (getNodePositionInNextTimeStep(currentImage,
                                          nextImage,
                                          graph->getVertexCoords(s) - drift,
                                          baseSearchRadius,
                                          graph->getVertexCoords(s),
                                          baseTemplateRadius,
                                          correlationThreshold,
                                          doGrayValueRangeCheck,
                                          nextBasePos,
                                          correlation))
        {
            qDebug() << "\tFound base node with correlation:" << correlation;

            const int tmpBase = tmpGraph->addVertex(nextBasePos);
            setNodeType(tmpGraph, tmpBase, BASE_NODE);
            setLocationIdOfNode(tmpGraph, tmpBase, FILOPODIUM);
            setFilopodiaIdOfNode(tmpGraph, tmpBase, getFilopodiaIdOfNode(graph, s));
            setManualNodeMatchId(tmpGraph, tmpBase, getMatchIdOfNode(graph, s));

            // Propagate all tips of filopodium and trace them to base
            int numTipsFound = 0;
            const SpatialGraphSelection oldFiloSel = getFilopodiumSelectionFromNode(graph, s);
            const SpatialGraphSelection tipSel = getNodesOfTypeInSelection(graph, oldFiloSel, TIP_NODE);
            SpatialGraphSelection::Iterator tipIt(tipSel);
            for (int tip = tipIt.vertices.nextSelected(); tip != -1; tip = tipIt.vertices.nextSelected())
            {
                McVec3f nextTipPos;

                // Set correlation threshold
                // Reduce threshold for long filopodia since it is unlikely that they disappear
                float corrThresh = correlationThreshold;
                const float edgeLength = FilopodiaFunctions::getEdgeLength(graph, graph->getIncidentEdges(tip)[0]);
                if (edgeLength > 1.5f)
                    corrThresh = correlationThreshold - 0.2;

                if (getNodePositionInNextTimeStep(currentImage,
                                                  nextImage,
                                                  graph->getVertexCoords(tip) - drift,
                                                  tipSearchRadius,
                                                  graph->getVertexCoords(tip),
                                                  tipTemplateRadius,
                                                  corrThresh,
                                                  doGrayValueRangeCheck,
                                                  nextTipPos,
                                                  correlation))
                {
                    qDebug() << "\tFound tip node with correlation:" << correlation;

                    // Trace tip to base
                    const SpatialGraphSelection newFiloSel = FilopodiaFunctions::getFilopodiaSelectionOfNode(tmpGraph, tmpBase);
                    SpatialGraphPoint iPointFilo(-1, -1);
                    int iNodeFilo = -1;
                    const QTime traceStartTime = QTime::currentTime();
                    const std::vector<McVec3f> points = trace(tmpGraph,
                                                           nextImage,
                                                           nextTipPos,
                                                           nextBasePos,
                                                           10,
                                                           intensityWeight,
                                                           topBrightness,
                                                           newFiloSel,
                                                           iPointFilo,
                                                           iNodeFilo);

                    const QTime traceEndTime = QTime::currentTime();
                    qDebug() << "\tTrace time" << traceStartTime.msecsTo(traceEndTime) << "msec";

                    const int tmpTip = tmpGraph->addVertex(points.front());
                    setNodeType(tmpGraph, tmpTip, TIP_NODE);
                    setLocationIdOfNode(tmpGraph, tmpTip, FILOPODIUM);
                    setFilopodiaIdOfNode(tmpGraph, tmpTip, FilopodiaFunctions::getFilopodiaIdOfNode(graph, s));
                    setManualNodeMatchId(tmpGraph, tmpTip, FilopodiaFunctions::getMatchLabelId(graph, IGNORED));

                    if (iNodeFilo != -1)
                    {
                        qDebug() << "\tFound intersection node" << iNodeFilo;
                        SpatialGraphSelection connectSel(tmpGraph);
                        connectSel.selectVertex(tmpTip);
                        connectSel.selectVertex(iNodeFilo);
                        ConnectOperation connectOp(tmpGraph, connectSel, SpatialGraphSelection(tmpGraph), points);
                        connectOp.exec();

                        const int outEdge = connectOp.getNewEdgeNum();
                        setLocationIdOfEdge(tmpGraph, outEdge, FILOPODIUM);
                        setFilopodiaIdOfEdge(tmpGraph, outEdge, FilopodiaFunctions::getFilopodiaIdOfNode(graph, s));
                        const float edgeLength = FilopodiaFunctions::getEdgeLength(tmpGraph, outEdge);
                        if (edgeLength < lengthThreshold)
                        {
                            qDebug() << "\t\tedge with length" << edgeLength << "is shorter than" << lengthThreshold << "-> do not propagate tip";
                            theMsg->printf(QString("\tfilopodium %1 could not be propagated: filopodia length is too short.").arg(getFilopodiaLabelNameFromID(graph, getFilopodiaIdOfNode(graph, tip))));
                            tmpGraph->removeEdge(outEdge);
                            tmpGraph->removeVertex(tmpTip);
                            qDebug() << "\t\ttip" << tmpTip << "removed";
                            continue;
                        }
                        else
                        {
                            ++numTipsFound;
                        }
                    }
                    else if (iPointFilo != SpatialGraphPoint(-1, -1))
                    {
                        qDebug() << "\tFound intersection point" << iPointFilo.edgeNum << iPointFilo.pointNum;
                        SpatialGraphSelection convertSel(tmpGraph);
                        convertSel.selectPoint(iPointFilo);
                        PointToVertexOperation convertOp(tmpGraph, convertSel, SpatialGraphSelection(tmpGraph));
                        convertOp.exec();
                        const int outBranch = convertOp.getNewVertexNum();
                        setNodeType(tmpGraph, outBranch, BRANCHING_NODE);
                        setLocationIdOfNode(tmpGraph, outBranch, FILOPODIUM);
                        setFilopodiaIdOfNode(tmpGraph, outBranch, FilopodiaFunctions::getFilopodiaIdOfNode(graph, s));
                        setManualNodeMatchId(tmpGraph, outBranch, FilopodiaFunctions::getMatchLabelId(graph, IGNORED));

                        SpatialGraphSelection connectSel(tmpGraph);
                        connectSel.selectVertex(tmpTip);
                        connectSel.selectVertex(outBranch);
                        ConnectOperation connectOp(tmpGraph, connectSel, SpatialGraphSelection(tmpGraph), points);
                        connectOp.exec();

                        const int outEdge = connectOp.getNewEdgeNum();
                        setLocationIdOfEdge(tmpGraph, outEdge, FILOPODIUM);
                        setFilopodiaIdOfEdge(tmpGraph, outEdge, FilopodiaFunctions::getFilopodiaIdOfNode(graph, s));
                        const float edgeLength = FilopodiaFunctions::getEdgeLength(tmpGraph, outEdge);
                        if (edgeLength < lengthThreshold)
                        {
                            qDebug() << "\t\tedge with length" << edgeLength << "is shorter than" << lengthThreshold << "-> do not propagate tip";
                            theMsg->printf(QString("\tfilopodium %1 could not be propagated: filopodia length is too short.").arg(getFilopodiaLabelNameFromID(graph, getFilopodiaIdOfNode(graph, tip))));
                            tmpGraph->removeEdge(outEdge);
                            tmpGraph->removeVertex(tmpTip);
                            qDebug() << "\t\ttip" << tmpTip << "removed";
                            convertOp.undo();
                            continue;
                        }
                        else
                        {
                            ++numTipsFound;
                        }
                    }
                    else if (nextBasePos.equals(points.back()))
                    {
                        SpatialGraphSelection connectSel(tmpGraph);
                        connectSel.selectVertex(tmpTip);
                        connectSel.selectVertex(tmpBase);
                        ConnectOperation connectOp(tmpGraph, connectSel, SpatialGraphSelection(tmpGraph), points);
                        connectOp.exec();

                        const int outEdge = connectOp.getNewEdgeNum();
                        setLocationIdOfEdge(tmpGraph, outEdge, FILOPODIUM);
                        setFilopodiaIdOfEdge(tmpGraph, outEdge, FilopodiaFunctions::getFilopodiaIdOfNode(graph, s));
                        const float edgeLength = FilopodiaFunctions::getEdgeLength(tmpGraph, outEdge);
                        if (edgeLength < lengthThreshold)
                        {
                            qDebug() << "\t\tedge with length" << edgeLength << "is shorter than" << lengthThreshold << "-> do not propagate tip";
                            theMsg->printf(QString("\tfilopodium %1 could not be propagated: filopodia length is too short.").arg(getFilopodiaLabelNameFromID(graph, getFilopodiaIdOfNode(graph, tip))));
                            tmpGraph->removeEdge(outEdge);
                            tmpGraph->removeVertex(tmpTip);
                            qDebug() << "\t\ttip" << tmpTip << "removed";
                            continue;
                        }
                        else
                        {
                            ++numTipsFound;
                        }
                    }
                    else
                    {
                        qDebug() << "Trace tip to base does not terminate at node or point";
                        tmpGraph->clear();
                        return tmpGraph;
                    }
                }
                else
                {
                    qDebug() << "\tTip could not be propagated" << correlation;
                    theMsg->printf(QString("\tfilopodium %1 could not be propagated: correlation for tip is too small.").arg(getFilopodiaLabelNameFromID(graph, getFilopodiaIdOfNode(graph, tip))));
                }
            }

            // Trace base to root
            SpatialGraphSelection filoSel = tmpGraph->getConnectedComponent(tmpBase);
            filoSel.deselectVertex(tmpBase);
            if (filoSel.isEmpty())
            {
                qDebug() << "\t\tfilopodium has no tips -> do not propagate filopodium";
                tmpGraph->removeVertex(tmpBase);
                qDebug() << "\t\tbase" << tmpBase << "removed";
            }
            else
            {
                // Save prior base in current timestep
                previousBases.push_back(s);

                SpatialGraphSelection tmpSel = FilopodiaFunctions::getLocationSelection(tmpGraph, GROWTHCONE);
                SpatialGraphPoint iPointGC = SpatialGraphPoint(-1, -1);
                int iNodeGC = -1;
                const std::vector<McVec3f> points = traceWithDijkstra(tmpGraph,
                                                                   nextDijkstraMap,
                                                                   nextBasePos,
                                                                   rootPos,
                                                                   tmpSel,
                                                                   iPointGC,
                                                                   iNodeGC);

                if (iNodeGC != -1)
                {
                    SpatialGraphSelection connectSel(tmpGraph);
                    connectSel.selectVertex(tmpBase);
                    connectSel.selectVertex(iNodeGC);
                    ConnectOperation connectOp(tmpGraph, connectSel, SpatialGraphSelection(tmpGraph), points);
                    connectOp.exec();

                    const int gcEdge = connectOp.getNewEdgeNum();
                    setLocationIdOfEdge(tmpGraph, gcEdge, GROWTHCONE);
                }
                else if (iPointGC != SpatialGraphPoint(-1, -1))
                {
                    SpatialGraphSelection convertSel(tmpGraph);
                    convertSel.selectPoint(iPointGC);
                    PointToVertexOperation convertOp(tmpGraph, convertSel, SpatialGraphSelection(tmpGraph));
                    convertOp.exec();
                    const int gcBranch = convertOp.getNewVertexNum();
                    setNodeType(tmpGraph, gcBranch, BRANCHING_NODE);
                    setLocationIdOfNode(tmpGraph, gcBranch, GROWTHCONE);
                    setFilopodiaIdOfNode(tmpGraph, gcBranch, FilopodiaFunctions::getFilopodiaLabelId(graph, IGNORED));
                    setManualNodeMatchId(tmpGraph, gcBranch, FilopodiaFunctions::getMatchLabelId(graph, IGNORED));

                    SpatialGraphSelection connectSel(tmpGraph);
                    connectSel.selectVertex(tmpBase);
                    connectSel.selectVertex(gcBranch);
                    ConnectOperation connectOp(tmpGraph, connectSel, SpatialGraphSelection(tmpGraph), points);
                    connectOp.exec();

                    const int gcEdge = connectOp.getNewEdgeNum();
                    setLocationIdOfEdge(tmpGraph, gcEdge, GROWTHCONE);
                }
                else if (rootPos.equals(points.back()))
                {
                    SpatialGraphSelection connectSel(tmpGraph);
                    connectSel.selectVertex(tmpBase);
                    connectSel.selectVertex(tmpRoot);
                    ConnectOperation connectOp(tmpGraph, connectSel, SpatialGraphSelection(tmpGraph), points);
                    connectOp.exec();

                    const int gcEdge = connectOp.getNewEdgeNum();
                    setLocationIdOfEdge(tmpGraph, gcEdge, GROWTHCONE);
                }
                else
                {
                    qDebug() << "\tTrace base to root does not terminate at node or point";
                }
            }
        }
        else
        {
            qDebug() << "\tBase could not be propagated" << correlation;
            theMsg->printf(QString("\tfilopodium %1 could not be propagated: correlation for base is too small.").arg(getFilopodiaLabelNameFromID(graph, getFilopodiaIdOfNode(graph, s))));
        }
    }

    return tmpGraph;
}

AddFilopodiumOperationSet*
FilopodiaFunctions::addFilopodium(HxSpatialGraph* graph,
                                  const McVec3f newCoords,
                                  const int currentTime,
                                  HxUniformScalarField3* image,
                                  HxUniformScalarField3* priorMap,
                                  const bool addedIn2DViewer)
{
    AddFilopodiumOperationSet* op = 0;

    const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, currentTime);
    if (rootNode == -1)
    {
        HxMessage::error(QString("Warning: Filopodium cannot be added.\nTimestep %1 has no root node.\n").arg(currentTime), "ok");
        return op;
    }

    // trace new tip to root node
    const SpatialGraphSelection selectionForTime = FilopodiaFunctions::getSelectionForTimeStep(graph, currentTime);
    SpatialGraphPoint intersectionPoint(-1, -1);
    int intersectionNode = -1;
    const std::vector<McVec3f> points = FilopodiaFunctions::traceWithDijkstra(graph,
                                                                           priorMap,
                                                                           newCoords,
                                                                           graph->getVertexCoords(rootNode),
                                                                           selectionForTime,
                                                                           intersectionPoint,
                                                                           intersectionNode);
    if (points.size() == 0)
    {
        qDebug() << "\tFilopodium cannot be added: branch is too short.";
        return op;
    }

    SpatialGraphSelection intersectionSel(graph);
    if (intersectionNode == -1 && intersectionPoint == SpatialGraphPoint(-1, -1))
        intersectionSel.selectVertex(rootNode);
    else if (intersectionNode != -1)
        intersectionSel.selectVertex(intersectionNode);
    else if (intersectionPoint != SpatialGraphPoint(-1, -1))
        intersectionSel.selectPoint(intersectionPoint);
    else
    {
        qDebug() << "\tFilopodium cannot be added: invalid intersection";
        return op;
    }

    const HxSpatialGraph* tmpGraph = FilopodiaFunctions::createFilopodiaBranch(graph,
                                                                               newCoords,
                                                                               rootNode,
                                                                               points,
                                                                               intersectionSel,
                                                                               image);

    int filoRootNode = -1;

    std::vector<McVec3f> newNodes;
    std::vector<McDArray<int> > newNodeLabels;

    int filoId = -1;
    int locId = FilopodiaFunctions::getLocationLabelId(graph, GROWTHCONE);
    if (intersectionPoint != SpatialGraphPoint(-1, -1) && !checkIfPointIsPartOfGrowthCone(graph, intersectionPoint, rootNode))
    {
        filoId = FilopodiaFunctions::getFilopodiaIdOfEdge(graph, intersectionPoint.edgeNum);
        locId = FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM);
    }
    else
    {
        filoId = FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED);
    }

    for (int v = 0; v < tmpGraph->getNumVertices(); ++v)
    {
        McDArray<int> labels;
        labels.clear();
        labels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
        labels.push_back(FilopodiaFunctions::getTimeIdFromTimeStep(graph, currentTime));
        labels.push_back(-1);
        labels.push_back(FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM));
        labels.push_back(filoId);
        labels.push_back(-1);
        labels.push_back(-1);

        if (FilopodiaFunctions::hasNodeType(tmpGraph, ROOT_NODE, v))
        {
            filoRootNode = v;
        }
        else if (FilopodiaFunctions::hasNodeType(tmpGraph, TIP_NODE, v))
        {
            newNodes.push_back(tmpGraph->getVertexCoords(v));
            labels[2] = FilopodiaFunctions::getTypeIdOfNode(tmpGraph, v);
            labels[5] = FilopodiaFunctions::getFilopodiaLabelId(graph, IGNORED);
            labels[6] = FilopodiaFunctions::getManualGeometryLabelId(graph, MANUAL);

            newNodeLabels.push_back(labels);
        }
        else if (FilopodiaFunctions::hasNodeType(tmpGraph, BASE_NODE, v))
        {
            newNodes.push_back(tmpGraph->getVertexCoords(v));
            labels[2] = FilopodiaFunctions::getTypeIdOfNode(tmpGraph, v);
            labels[5] = FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED);
            labels[6] = FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC);

            newNodeLabels.push_back(labels);
        }
        else
        {
            HxMessage::error(QString("Error: Filopodia cannot be added.\nUnexpected node type.\n"), "ok");
            return op;
        }
    }

    McDArray<int> newEdgeSources;
    McDArray<int> newEdgeTargets;
    std::vector<std::vector<McVec3f> > newEdgePoints;
    std::vector<McDArray<int> > newEdgeLabels;

    for (int e = 0; e < tmpGraph->getNumEdges(); ++e)
    {
        McDArray<int> labels;
        labels.clear();
        labels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
        labels.push_back(FilopodiaFunctions::getTimeIdFromTimeStep(graph, currentTime));
        labels.push_back(-1);
        labels.push_back(filoId);
        labels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC));

        const int source = tmpGraph->getEdgeSource(e);
        const int target = tmpGraph->getEdgeTarget(e);

        if (source == filoRootNode)
        {
            newEdgeSources.push_back(-1);
        }
        else
        {
            if (filoRootNode == -1)
            {
                newEdgeSources.push_back(source);
            }
            else
            {
                newEdgeSources.push_back(source - 1); // Substract 1 since filo root has Id0 but is not added to newNodes.
            }                                      // -> node Ids and list position is not equal -> subtract -1
        }

        if (target == filoRootNode)
        {
            newEdgeTargets.push_back(-1);
        }
        else
        {
            if (filoRootNode == -1)
            {
                newEdgeTargets.push_back(target);
            }
            else
            {
                newEdgeTargets.push_back(target - 1); // Substract 1 since filo root has Id0 but is not added to newNodes.
            }                                      // -> node Ids and list position is not equal -> subtract -1
        }

        if (FilopodiaFunctions::checkIfEdgeIsPartOfGrowthCone(tmpGraph, e, filoRootNode))
        {
            labels[2] = FilopodiaFunctions::getLocationLabelId(graph, GROWTHCONE);
        }
        else
        {
            labels[2] = FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM);
        }
        newEdgeLabels.push_back(labels);

        newEdgePoints.push_back(tmpGraph->getEdgePoints(e));
    }

    McDArray<int> connectLabels;

    if (intersectionSel.getNumSelectedPoints() == 1)
    {
        connectLabels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
        connectLabels.push_back(FilopodiaFunctions::getTimeIdFromTimeStep(graph, currentTime));
        connectLabels.push_back(FilopodiaFunctions::getTypeLabelId(graph, BRANCHING_NODE));
        connectLabels.push_back(locId);
        connectLabels.push_back(filoId);
        connectLabels.push_back(FilopodiaFunctions::getMatchLabelId(graph, IGNORED));
        connectLabels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC));
    }

    op = new AddFilopodiumOperationSet(graph,
                                       intersectionSel,
                                       SpatialGraphSelection(graph),
                                       newNodes,
                                       newNodeLabels,
                                       newEdgeSources,
                                       newEdgeTargets,
                                       newEdgePoints,
                                       newEdgeLabels,
                                       connectLabels,
                                       addedIn2DViewer);

    return op;
}

ConnectOperationSet*
FilopodiaFunctions::connectNodes(HxSpatialGraph* graph,
                                 const SpatialGraphSelection selectedElements,
                                 HxUniformScalarField3* image,
                                 const float intensityWeight,
                                 const int topBrightness)
{
    ConnectOperationSet* op = 0;

    // check requirements
    if (selectedElements.getNumSelectedVertices() != 1 || selectedElements.getNumSelectedEdges() != 0 || selectedElements.getNumSelectedPoints() != 1)
    {
        qDebug() << "ConnectNodes: wrong selection";
        return op;
    }

    const int selectedNode = selectedElements.getSelectedVertex(0);
    const SpatialGraphPoint selectedPoint = selectedElements.getSelectedPoint(0);
    const int edgeId = selectedPoint.edgeNum;
    const int pointId = selectedPoint.pointNum;
    const int currentTime = FilopodiaFunctions::getTimeOfNode(graph, selectedNode);
    const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, currentTime);
    SpatialGraphSelection pathToRoot = graph->getShortestPath(selectedNode, rootNode);

    if (!FilopodiaFunctions::hasNodeType(graph, TIP_NODE, selectedNode))
    {
        qDebug() << "ConnectNodes: Selected node has to be tip.";
        return op;
    }
    else if (graph->getIncidentEdges(selectedNode).size() != 1)
    {
        qDebug() << "ConnectNodes: Selected node has more than one incident edges.";
        return op;
    }
    else if (!(FilopodiaFunctions::getTimeIdOfNode(graph, selectedNode) == FilopodiaFunctions::getTimeIdOfEdge(graph, edgeId)))
    {
        qDebug() << "ConnectNodes: Selected node and edge point should have same time Id.";
        return op;
    }
    else if ((pointId == 0) || (pointId == graph->getNumEdgePoints(edgeId) - 1))
    {
        qDebug() << "ConnectNodes: Selected edge point is node.";
        return op;
    }
    else if (pathToRoot.isSelectedEdge(edgeId))
    {
        qDebug() << "ConnectNodes: Selected edge point is part of shortest path to root from selected node.";
        return op;
    }

    // get selection to delete
    int oldBranch = rootNode;
    SpatialGraphSelection deleteSel = FilopodiaFunctions::getSelectionTilBranchingNode(graph, selectedNode, oldBranch);

    // create tmpGraph
    SpatialGraphSelection connectSel(graph);
    connectSel.selectPoint(selectedPoint);
    HxSpatialGraph* tmpGraph = createBranchToPoint(graph,
                                                   deleteSel,
                                                   connectSel,
                                                   image,
                                                   intensityWeight,
                                                   topBrightness);

    // get new nodes and node types
    int filoRootNode = -1;

    std::vector<McVec3f> newNodes;
    std::vector<McDArray<int> > newNodeLabels;

    for (int v = 0; v < tmpGraph->getNumVertices(); ++v)
    {
        McDArray<int> labels;
        labels.clear();
        labels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
        labels.push_back(FilopodiaFunctions::getTimeIdFromTimeStep(graph, currentTime));
        labels.push_back(-1);
        labels.push_back(FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM));
        labels.push_back(FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED));
        labels.push_back(-1);
        labels.push_back(-1);

        if (FilopodiaFunctions::hasNodeType(tmpGraph, ROOT_NODE, v))
        {
            filoRootNode = v;
        }
        else if (FilopodiaFunctions::hasNodeType(tmpGraph, TIP_NODE, v))
        {
            newNodes.push_back(tmpGraph->getVertexCoords(v));
            labels[2] = FilopodiaFunctions::getTypeIdOfNode(tmpGraph, v);
            labels[5] = FilopodiaFunctions::getFilopodiaLabelId(graph, IGNORED);
            labels[6] = FilopodiaFunctions::getGeometryIdOfNode(tmpGraph, v);

            newNodeLabels.push_back(labels);
        }
        else if (FilopodiaFunctions::hasNodeType(tmpGraph, BASE_NODE, v))
        {
            newNodes.push_back(tmpGraph->getVertexCoords(v));
            labels[2] = FilopodiaFunctions::getTypeIdOfNode(tmpGraph, v);
            labels[5] = FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED);
            labels[6] = FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC);

            newNodeLabels.push_back(labels);
        }
        else
        {
            HxMessage::error(QString("Error: Filopodia cannot be added.\nUnexpected node type.\n"), "ok");
        }
    }

    McDArray<int> newEdgeSources;
    McDArray<int> newEdgeTargets;
    std::vector<std::vector<McVec3f> > newEdgePoints;
    std::vector<McDArray<int> > newEdgeLabels;

    for (int e = 0; e < tmpGraph->getNumEdges(); ++e)
    {
        McDArray<int> labels;
        labels.clear();
        labels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
        labels.push_back(FilopodiaFunctions::getTimeIdFromTimeStep(graph, currentTime));
        labels.push_back(-1);
        labels.push_back(FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED));
        labels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC));

        const int source = tmpGraph->getEdgeSource(e);
        const int target = tmpGraph->getEdgeTarget(e);

        if (source == filoRootNode)
            newEdgeSources.push_back(-1);
        else
            newEdgeSources.push_back(source - 1); // Substract 1 since filo root has Id0 but is not added to newNodes.
                                               // -> node Ids and list position is not equal -> subtract -1

        if (target == filoRootNode)
            newEdgeTargets.push_back(-1);
        else
            newEdgeTargets.push_back(target - 1); // Substract 1 since filo root has Id0 but is not added to newNodes.
                                               // -> node Ids and list position is not equal -> subtract -1

        if (FilopodiaFunctions::checkIfEdgeIsPartOfGrowthCone(tmpGraph, e, filoRootNode))
        {
            labels[2] = FilopodiaFunctions::getLocationLabelId(graph, GROWTHCONE);
        }
        else
        {
            labels[2] = FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM);
        }
        newEdgeLabels.push_back(labels);

        newEdgePoints.push_back(tmpGraph->getEdgePoints(e));
    }

    McDArray<int> connectLabels;
    connectLabels.clear();
    connectLabels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
    connectLabels.push_back(FilopodiaFunctions::getTimeIdFromTimeStep(graph, currentTime));
    connectLabels.push_back(FilopodiaFunctions::getTypeLabelId(graph, BRANCHING_NODE));
    connectLabels.push_back(FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM));
    connectLabels.push_back(FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED));
    connectLabels.push_back(FilopodiaFunctions::getMatchLabelId(graph, IGNORED));
    connectLabels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, MANUAL));

    // connect
    op = new ConnectOperationSet(graph,
                                 connectSel,
                                 SpatialGraphSelection(graph),
                                 newNodes,
                                 newNodeLabels,
                                 newEdgeSources,
                                 newEdgeTargets,
                                 newEdgePoints,
                                 newEdgeLabels,
                                 connectLabels,
                                 deleteSel);

    return op;
}

MoveBaseOperationSet*
FilopodiaFunctions::moveBase(HxSpatialGraph* graph,
                             const int baseNode,
                             const McVec3f newCoords,
                             HxUniformScalarField3* image,
                             const float intensityWeight,
                             const int topBrightness)
{
    MoveBaseOperationSet* op = 0;

    const int filoId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, baseNode);
    const int matchId = FilopodiaFunctions::getMatchIdOfNode(graph, baseNode);
    const int timeId = FilopodiaFunctions::getTimeIdOfNode(graph, baseNode);
    const int currentTime = FilopodiaFunctions::getTimeOfNode(graph, baseNode);
    const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, currentTime);

    SpatialGraphSelection filoSel = FilopodiaFunctions::getFilopodiaSelectionOfNode(graph, baseNode);

    int branchNode = rootNode;
    SpatialGraphSelection deleteSel = FilopodiaFunctions::getSelectionTilBranchingNode(graph, baseNode, branchNode);
    deleteSel.addSelection(filoSel);
    deleteSel.deselectVertex(rootNode);

    SpatialGraphSelection intersectionSel(graph);
    const HxSpatialGraph* tmpGraph = FilopodiaFunctions::createBranchToRoot(graph,
                                                                            newCoords,
                                                                            deleteSel,
                                                                            intersectionSel,
                                                                            image,
                                                                            intensityWeight,
                                                                            topBrightness);

    EdgeVertexAttribute* typeAtt = tmpGraph->findVertexAttribute(FilopodiaFunctions::getTypeLabelAttributeName());
    if (!typeAtt)
    {
        qDebug() << "\t\tmoveBaseAndConnectOperation: no NodeTypeAttribute.";
        return op;
    }

    int filoRootNode = -1;

    std::vector<McVec3f> newNodes;
    std::vector<McDArray<int> > newNodeLabels;

    for (int v = 0; v < tmpGraph->getNumVertices(); ++v)
    {
        McDArray<int> labels;
        labels.clear();
        labels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
        labels.push_back(timeId);
        labels.push_back(-1);
        labels.push_back(FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM));
        labels.push_back(filoId);
        labels.push_back(-1);
        labels.push_back(-1);

        if (FilopodiaFunctions::hasNodeType(tmpGraph, ROOT_NODE, v))
        {
            filoRootNode = v;
        }
        else if (FilopodiaFunctions::hasNodeType(tmpGraph, TIP_NODE, v))
        {
            newNodes.push_back(tmpGraph->getVertexCoords(v));
            labels[2] = FilopodiaFunctions::getTypeIdOfNode(tmpGraph, v);
            labels[5] = FilopodiaFunctions::getFilopodiaLabelId(graph, IGNORED);
            labels[6] = FilopodiaFunctions::getGeometryIdOfNode(tmpGraph, v);

            newNodeLabels.push_back(labels);
        }
        else if (FilopodiaFunctions::hasNodeType(tmpGraph, BASE_NODE, v))
        {
            newNodes.push_back(tmpGraph->getVertexCoords(v));
            labels[2] = FilopodiaFunctions::getTypeIdOfNode(tmpGraph, v);
            labels[5] = matchId;
            labels[6] = FilopodiaFunctions::getManualGeometryLabelId(graph, MANUAL);

            newNodeLabels.push_back(labels);
        }
        else if (FilopodiaFunctions::hasNodeType(tmpGraph, BRANCHING_NODE, v))
        {
            newNodes.push_back(tmpGraph->getVertexCoords(v));
            labels[2] = FilopodiaFunctions::getTypeIdOfNode(tmpGraph, v);
            labels[5] = FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED);
            labels[6] = FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC);

            newNodeLabels.push_back(labels);
        }
        else
        {
            HxMessage::error(QString("Error: Base cannot be moved.\nUnexpected node type.\n"), "ok");
        }
    }

    McDArray<int> newEdgeSources;
    McDArray<int> newEdgeTargets;
    std::vector<std::vector<McVec3f> > newEdgePoints;
    std::vector<McDArray<int> > newEdgeLabels;

    for (int e = 0; e < tmpGraph->getNumEdges(); ++e)
    {
        McDArray<int> labels;
        labels.clear();
        labels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
        labels.push_back(timeId);
        labels.push_back(FilopodiaFunctions::getLocationIdOfEdge(tmpGraph, e));
        labels.push_back(FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED));
        labels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC));
        newEdgeLabels.push_back(labels);

        const int source = tmpGraph->getEdgeSource(e);
        const int target = tmpGraph->getEdgeTarget(e);

        if (source == filoRootNode)
            newEdgeSources.push_back(-1);
        else
            newEdgeSources.push_back(source); // This is correct since filo root has highest Id -> Id and list position is equal

        if (target == filoRootNode)
            newEdgeTargets.push_back(-1);
        else
            newEdgeTargets.push_back(target); // This is correct since filo root has highest Id -> Id and list position is equal

        newEdgePoints.push_back(tmpGraph->getEdgePoints(e));
    }

    McDArray<int> connectLabels;
    if (intersectionSel.getNumSelectedPoints() == 1)
    {
        connectLabels.clear();
        connectLabels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
        connectLabels.push_back(timeId);
        connectLabels.push_back(FilopodiaFunctions::getTypeLabelId(graph, BRANCHING_NODE));
        connectLabels.push_back(FilopodiaFunctions::getLocationLabelId(graph, GROWTHCONE));
        connectLabels.push_back(FilopodiaFunctions::getFilopodiaLabelId(graph, IGNORED));
        connectLabels.push_back(FilopodiaFunctions::getMatchLabelId(graph, IGNORED));
        connectLabels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC));
    }

    op = new MoveBaseOperationSet(graph,
                                  intersectionSel,
                                  SpatialGraphSelection(graph),
                                  newNodes,
                                  newNodeLabels,
                                  newEdgeSources,
                                  newEdgeTargets,
                                  newEdgePoints,
                                  newEdgeLabels,
                                  deleteSel,
                                  connectLabels);
    return op;
}

MoveBaseOperationSet*
FilopodiaFunctions::moveBaseOfTip(HxSpatialGraph* graph,
                                  const int tipNode,
                                  const McVec3f newCoords,
                                  HxUniformScalarField3* image,
                                  const float intensityWeight,
                                  const int topBrightness)
{
    MoveBaseOperationSet* op = 0;

    const int timeId = FilopodiaFunctions::getTimeIdOfNode(graph, tipNode);
    const int currentTime = FilopodiaFunctions::getTimeOfNode(graph, tipNode);
    const int filoId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, tipNode);

    const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, currentTime);
    int branchNode = rootNode;
    SpatialGraphSelection deleteSel = FilopodiaFunctions::getSelectionTilBranchingNode(graph, tipNode, branchNode);
    deleteSel.deselectVertex(rootNode);

    SpatialGraphSelection intersectionSel(graph);

    HxSpatialGraph* tmpGraph = FilopodiaFunctions::createBranchToRootWithBase(graph,
                                                                              tipNode,
                                                                              newCoords,
                                                                              deleteSel,
                                                                              intersectionSel,
                                                                              image,
                                                                              intensityWeight,
                                                                              topBrightness);

    int filoRootNode = -1;

    std::vector<McVec3f> newNodes;
    std::vector<McDArray<int> > newNodeLabels;

    for (int v = 0; v < tmpGraph->getNumVertices(); ++v)
    {
        McDArray<int> labels;
        labels.clear();
        labels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
        labels.push_back(timeId);
        labels.push_back(-1);
        labels.push_back(FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM));
        labels.push_back(filoId);
        labels.push_back(-1);
        labels.push_back(-1);

        if (FilopodiaFunctions::hasNodeType(tmpGraph, ROOT_NODE, v))
        {
            filoRootNode = v;
        }
        else if (FilopodiaFunctions::hasNodeType(tmpGraph, TIP_NODE, v))
        {
            newNodes.push_back(tmpGraph->getVertexCoords(v));
            labels[2] = FilopodiaFunctions::getTypeIdOfNode(tmpGraph, v);
            labels[5] = FilopodiaFunctions::getFilopodiaLabelId(graph, IGNORED);
            labels[6] = FilopodiaFunctions::getGeometryIdOfNode(tmpGraph, v);

            newNodeLabels.push_back(labels);
        }
        else if (FilopodiaFunctions::hasNodeType(tmpGraph, BASE_NODE, v))
        {
            newNodes.push_back(tmpGraph->getVertexCoords(v));
            labels[2] = FilopodiaFunctions::getTypeIdOfNode(tmpGraph, v);
            labels[5] = FilopodiaFunctions::getMatchIdOfNode(tmpGraph, v);
            labels[6] = FilopodiaFunctions::getManualGeometryLabelId(graph, MANUAL);

            newNodeLabels.push_back(labels);
        }
        else if (FilopodiaFunctions::hasNodeType(tmpGraph, BRANCHING_NODE, v))
        {
            newNodes.push_back(tmpGraph->getVertexCoords(v));
            labels[2] = FilopodiaFunctions::getTypeIdOfNode(tmpGraph, v);
            labels[5] = FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED);
            labels[6] = FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC);

            newNodeLabels.push_back(labels);
        }
        else
        {
            HxMessage::error(QString("tMoveBaseOfTipOperation: Base cannot be moved.\nUnexpected node type.\n"), "ok");
        }
    }

    McDArray<int> newEdgeSources;
    McDArray<int> newEdgeTargets;
    std::vector<std::vector<McVec3f> > newEdgePoints;
    std::vector<McDArray<int> > newEdgeLabels;

    for (int e = 0; e < tmpGraph->getNumEdges(); ++e)
    {
        McDArray<int> labels;
        labels.clear();
        labels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
        labels.push_back(timeId);
        labels.push_back(FilopodiaFunctions::getLocationIdOfEdge(tmpGraph, e));
        labels.push_back(FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED));
        labels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC));
        newEdgeLabels.push_back(labels);

        const int source = tmpGraph->getEdgeSource(e);
        const int target = tmpGraph->getEdgeTarget(e);

        if (source == filoRootNode)
            newEdgeSources.push_back(-1);
        else
            newEdgeSources.push_back(source); // This is correct since filo root has highest Id -> Id and list position is equal

        if (target == filoRootNode)
            newEdgeTargets.push_back(-1);
        else
            newEdgeTargets.push_back(target); // This is correct since filo root has highest Id -> Id and list position is equal

        newEdgePoints.push_back(tmpGraph->getEdgePoints(e));
    }

    McDArray<int> connectLabels;
    if (intersectionSel.getNumSelectedPoints() == 1)
    {
        connectLabels.clear();
        connectLabels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
        connectLabels.push_back(timeId);
        connectLabels.push_back(FilopodiaFunctions::getTypeLabelId(graph, BRANCHING_NODE));
        connectLabels.push_back(FilopodiaFunctions::getLocationLabelId(graph, GROWTHCONE));
        connectLabels.push_back(FilopodiaFunctions::getFilopodiaLabelId(graph, IGNORED));
        connectLabels.push_back(FilopodiaFunctions::getMatchLabelId(graph, IGNORED));
        connectLabels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC));
    }

    op = new MoveBaseOperationSet(graph,
                                  intersectionSel,
                                  SpatialGraphSelection(graph),
                                  newNodes,
                                  newNodeLabels,
                                  newEdgeSources,
                                  newEdgeTargets,
                                  newEdgePoints,
                                  newEdgeLabels,
                                  deleteSel,
                                  connectLabels);
    return op;
}

bool
checkIfAllBasesSelected(const HxSpatialGraph* graph, const SpatialGraphSelection baseSel, const int currentTime)
{
    SpatialGraphSelection selectionOfCurrentTime = FilopodiaFunctions::getSelectionForTimeStep(graph, currentTime);
    SpatialGraphSelection baseSelOfCurrentTime = FilopodiaFunctions::getNodesOfTypeInSelection(graph, selectionOfCurrentTime, BASE_NODE);
    baseSelOfCurrentTime.subtractSelection(baseSel);

    if (baseSelOfCurrentTime.isEmpty())
    {
        return true;
    }
    else
    {
        return false;
    }
}

MoveTipOperationSet*
FilopodiaFunctions::moveTip(HxSpatialGraph* graph,
                            const int oldTip,
                            const McVec3f newCoords,
                            HxUniformScalarField3* image,
                            int surroundWidthInVoxels,
                            float intensityWeight,
                            int topBrightness,
                            const bool addedIn2DViewer)
{
    MoveTipOperationSet* op = 0;

    const IncidenceList incidentEdges = graph->getIncidentEdges(oldTip);
    if (incidentEdges.size() != 1)
    {
        theMsg->printf("Could not move tip: Tip %i has more than one incident edge.", oldTip);
        HxMessage::error(QString("Could not move tip: Tip %1 has more than one incident edge.").arg(oldTip), "ok");
        return 0;
    }

    const int oldEdge = incidentEdges[0];

    // Get neighbor node (adjacent node different from selected tip)
    // New tip will be connected with this node
    int nextNode = -1;
    const int source = graph->getEdgeSource(oldEdge);
    const int target = graph->getEdgeTarget(oldEdge);
    if (source == oldTip)
        nextNode = target;
    else if (target == oldTip)
        nextNode = source;

    const int timeId = FilopodiaFunctions::getTimeIdOfNode(graph, oldTip);
    const int currentTime = FilopodiaFunctions::getTimeStepFromTimeId(graph, timeId);

    const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, currentTime);
    if (rootNode == -1)
    {
        HxMessage::error(QString("Warning: Tip cannot be moved.\nTimestep %1 has no root node.\n").arg(currentTime), "ok");
        return op;
    }

    // trace tip to neighbor node
    SpatialGraphPoint intersectionPoint(-1, -1);
    int intersectionNode = -1;

    const std::vector<McVec3f> points = FilopodiaFunctions::trace(graph,
                                                               image,
                                                               newCoords,
                                                               graph->getVertexCoords(nextNode),
                                                               surroundWidthInVoxels,
                                                               intensityWeight,
                                                               topBrightness,
                                                               SpatialGraphSelection(graph),
                                                               intersectionPoint,            // Force connection with nextNode
                                                               intersectionNode);

    SpatialGraphSelection intersectionSel(graph);
    intersectionSel.selectVertex(nextNode);

    const HxSpatialGraph* tmpGraph = FilopodiaFunctions::createFilopodiaBranch(graph,
                                                                               newCoords,
                                                                               rootNode,
                                                                               points,
                                                                               intersectionSel,
                                                                               image);

    int filoRootNode = -1;

    std::vector<McVec3f> newNodes;
    std::vector<McDArray<int> > newNodeLabels;

    for (int v = 0; v < tmpGraph->getNumVertices(); ++v)
    {
        McDArray<int> labels;
        labels.clear();
        labels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
        labels.push_back(FilopodiaFunctions::getTimeIdFromTimeStep(graph, currentTime));
        labels.push_back(-1);
        labels.push_back(FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM));
        labels.push_back(FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED));
        labels.push_back(-1);
        labels.push_back(-1);

        if (FilopodiaFunctions::hasNodeType(tmpGraph, ROOT_NODE, v))
        {
            filoRootNode = v;
        }
        else if (FilopodiaFunctions::hasNodeType(tmpGraph, TIP_NODE, v))
        {
            newNodes.push_back(tmpGraph->getVertexCoords(v));
            labels[2] = FilopodiaFunctions::getTypeIdOfNode(tmpGraph, v);
            labels[5] = FilopodiaFunctions::getMatchLabelId(graph, IGNORED);
            labels[6] = FilopodiaFunctions::getManualGeometryLabelId(graph, MANUAL);

            newNodeLabels.push_back(labels);
        }
        else if (FilopodiaFunctions::hasNodeType(tmpGraph, BASE_NODE, v))
        {
            newNodes.push_back(tmpGraph->getVertexCoords(v));
            labels[2] = FilopodiaFunctions::getTypeIdOfNode(tmpGraph, v);
            labels[5] = FilopodiaFunctions::getMatchLabelId(graph, UNASSIGNED);
            labels[6] = FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC);

            newNodeLabels.push_back(labels);
        }
        else
        {
            HxMessage::error(QString("Error: Filopodia cannot be added.\nUnexpected node type.\n"), "ok");
        }
    }

    McDArray<int> newEdgeSources;
    McDArray<int> newEdgeTargets;
    std::vector<std::vector<McVec3f> > newEdgePoints;
    std::vector<McDArray<int> > newEdgeLabels;

    for (int e = 0; e < tmpGraph->getNumEdges(); ++e)
    {
        McDArray<int> labels;
        labels.clear();
        labels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
        labels.push_back(FilopodiaFunctions::getTimeIdFromTimeStep(graph, currentTime));
        labels.push_back(-1);
        labels.push_back(FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED));
        labels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC));

        const int source = tmpGraph->getEdgeSource(e);
        const int target = tmpGraph->getEdgeTarget(e);

        if (source == filoRootNode)
        {
            newEdgeSources.push_back(-1);
        }
        else
        {
            if (filoRootNode == -1)
            {
                newEdgeSources.push_back(source);
            }
            else
            {
                newEdgeSources.push_back(source - 1); // Substract 1 since filo root has Id0 but is not added to newNodes.
            }                                      // -> node Ids and list position is not equal -> subtract -1
        }

        if (target == filoRootNode)
        {
            newEdgeTargets.push_back(-1);
        }
        else
        {
            if (filoRootNode == -1)
            {
                newEdgeTargets.push_back(target);
            }
            else
            {
                newEdgeTargets.push_back(target - 1); // Substract 1 since filo root has Id0 but is not added to newNodes.
            }                                      // -> node Ids and list position is not equal -> subtract -1
        }

        if (FilopodiaFunctions::checkIfEdgeIsPartOfGrowthCone(tmpGraph, e, filoRootNode))
        {
            labels[2] = FilopodiaFunctions::getLocationLabelId(graph, GROWTHCONE);
        }
        else
        {
            labels[2] = FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM);
        }
        newEdgeLabels.push_back(labels);

        newEdgePoints.push_back(tmpGraph->getEdgePoints(e));
    }

    SpatialGraphSelection selToDelete(graph);
    selToDelete.selectVertex(oldTip);
    selToDelete.selectEdge(oldEdge);

    op = new MoveTipOperationSet(graph,
                                 intersectionSel,
                                 SpatialGraphSelection(graph),
                                 newNodes,
                                 newNodeLabels,
                                 newEdgeSources,
                                 newEdgeTargets,
                                 newEdgePoints,
                                 newEdgeLabels,
                                 selToDelete,
                                 addedIn2DViewer);

    return op;
}

ReplaceFilopodiaEdgeOperationSet*
FilopodiaFunctions::replaceEdge(HxSpatialGraph* graph,
                                const int edge,
                                const McVec3f pickedPoint,
                                const HxUniformScalarField3* image,
                                const int surroundWidthInVoxels,
                                const float intensityWeight,
                                const int topBrightness)
{
    ReplaceFilopodiaEdgeOperationSet* op = 0;

    // Selection to delete contains only old edge
    SpatialGraphSelection selToDelete(graph);
    selToDelete.selectEdge(edge);

    // Get source and target of edge
    const int source = graph->getEdgeSource(edge);
    const int target = graph->getEdgeTarget(edge);
    SpatialGraphSelection connectSel(graph);
    connectSel.selectVertex(source);
    connectSel.selectVertex(target);

    // Trace from source to picked point
    SpatialGraphPoint intersectionPoint(-1, -1);
    int intersectionNode = -1;
    const std::vector<McVec3f> points1 = FilopodiaFunctions::trace(graph,
                                                                image,
                                                                graph->getVertexCoords(source),
                                                                pickedPoint,
                                                                surroundWidthInVoxels,
                                                                intensityWeight,
                                                                topBrightness,
                                                                SpatialGraphSelection(graph), // time selection is empty to...
                                                                intersectionPoint,            // force connection with source
                                                                intersectionNode);
    // Trace from picked point to target
    const std::vector<McVec3f> points2 = FilopodiaFunctions::trace(graph,
                                                                image,
                                                                pickedPoint,
                                                                graph->getVertexCoords(target),
                                                                surroundWidthInVoxels,
                                                                intensityWeight,
                                                                topBrightness,
                                                                SpatialGraphSelection(graph), // time selection is empty to...
                                                                intersectionPoint,            // force connection with target
                                                                intersectionNode);

    // Append points
    std::vector<McVec3f> newPoints;

    for (int i = 0; i < points1.size(); ++i)
        newPoints.push_back(points1[i]);

    for (int j = 0; j < points2.size(); ++j)
        newPoints.push_back(points2[j]);

    // Get labels of new edge
    const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, FilopodiaFunctions::getTimeOfEdge(graph, edge));
    McDArray<int> labels;
    labels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
    labels.push_back(FilopodiaFunctions::getTimeIdOfEdge(graph, edge));
    labels.push_back(FilopodiaFunctions::getLocationIdOfEdge(graph, edge));
    labels.push_back(FilopodiaFunctions::getFilopodiaIdOfEdge(graph, edge));
    labels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, MANUAL));

    // replace
    op = new ReplaceFilopodiaEdgeOperationSet(graph,
                                              connectSel,
                                              SpatialGraphSelection(graph),
                                              newPoints,
                                              labels,
                                              selToDelete);

    return op;
}

bool
itemsFromDifferentTimesSelected(const HxSpatialGraph* graph,
                                const SpatialGraphSelection& sel)
{
    const EdgeVertexAttribute* vAtt = graph->findVertexAttribute(FilopodiaFunctions::getTimeStepAttributeName());
    const EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(FilopodiaFunctions::getTimeStepAttributeName());
    if (!vAtt || !eAtt)
    {
        throw McException("FilopodiaFunctions::itemsFromDifferentTimesSelected: no Time attribute available");
    }

    int time = -1;
    SpatialGraphSelection::Iterator it(sel);

    for (int v = it.vertices.nextSelected(); v != -1; v = it.vertices.nextSelected())
    {
        const int vtime = FilopodiaFunctions::getTimeIdOfNode(graph, v);
        if (time == -1)
        {
            time = vtime;
        }
        else if (vtime != time)
        {
            return true;
        }
    }

    for (int e = it.edges.nextSelected(); e != -1; e = it.edges.nextSelected())
    {
        const int etime = FilopodiaFunctions::getTimeIdOfEdge(graph, e);
        if (time == -1)
        {
            time = etime;
        }
        else if (etime != time)
        {
            return true;
        }
    }

    return false;
}

ReplaceFilopodiaOperationSet*
FilopodiaFunctions::propagateFilopodia(HxSpatialGraph* graph,
                                       const SpatialGraphSelection& baseSel,
                                       const int currentTime,
                                       const HxUniformScalarField3* currentImage,
                                       HxUniformScalarField3* nextImage,
                                       const HxUniformScalarField3* nextDijkstraMap,
                                       const McVec3i baseSearchRadius,
                                       const McVec3i baseTemplateRadius,
                                       const McVec3i tipSearchRadius,
                                       const McVec3i tipTemplateRadius,
                                       const float correlationThreshold,
                                       const float lengthThreshold,
                                       const float intensityWeight,
                                       const int topBrightness)
{
    const int nextTime = currentTime + 1;
    theMsg->printf("Start propagation from time %i to %i", currentTime, nextTime);
    qDebug() << "Start propagation from time" << currentTime << "to" << nextTime;
    QTime startTime = QTime::currentTime();

    ReplaceFilopodiaOperationSet* op = 0;

    if (!baseSel.isEmpty() && itemsFromDifferentTimesSelected(graph, baseSel))
    {
        qDebug() << "Selection contains items from different time steps";
        return op;
    }

    // Save prior bases
    McDArray<int> previousBases;

    // Greate tmp graph for next time step
    HxSpatialGraph* tmpGraph = FilopodiaFunctions::createGraphForNextTimeStep(graph,
                                                                              nextTime,
                                                                              baseSel,
                                                                              currentImage,
                                                                              nextImage,
                                                                              nextDijkstraMap,
                                                                              baseSearchRadius,
                                                                              baseTemplateRadius,
                                                                              tipSearchRadius,
                                                                              tipTemplateRadius,
                                                                              correlationThreshold,
                                                                              lengthThreshold,
                                                                              intensityWeight,
                                                                              topBrightness,
                                                                              previousBases);

    SpatialGraphSelection tipsInFiloGraph = FilopodiaFunctions::getNodesOfType(tmpGraph, TIP_NODE);
    if (tipsInFiloGraph.getNumSelectedVertices() == 0)
    {
        qDebug() << "Filo Graph has no tips -> do not propagate";
        return op;
    }

    const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, nextTime);
    SpatialGraphSelection rootSel(graph);
    rootSel.selectVertex(rootNode);

    int filoRootNode = -1;

    // Get new nodes and node labels
    std::vector<McVec3f> newNodes;
    std::vector<McDArray<int> > newNodeLabels;

    for (int v = 0; v < tmpGraph->getNumVertices(); ++v)
    {
        McDArray<int> labels;
        labels.clear();
        labels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
        labels.push_back(FilopodiaFunctions::getTimeIdFromTimeStep(graph, nextTime));
        labels.push_back(-1);
        labels.push_back(-1);
        labels.push_back(-1);
        labels.push_back(-1);
        labels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC));

        if (FilopodiaFunctions::hasNodeType(tmpGraph, ROOT_NODE, v))
        {
            filoRootNode = v;
        }
        else if (FilopodiaFunctions::hasNodeType(tmpGraph, TIP_NODE, v))
        {
            newNodes.push_back(tmpGraph->getVertexCoords(v));
            labels[2] = FilopodiaFunctions::getTypeIdOfNode(tmpGraph, v);
            labels[3] = FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM);
            labels[4] = FilopodiaFunctions::getFilopodiaIdOfNode(tmpGraph, v);
            labels[5] = FilopodiaFunctions::getFilopodiaLabelId(graph, IGNORED);

            newNodeLabels.push_back(labels);
        }
        else if (FilopodiaFunctions::hasNodeType(tmpGraph, BASE_NODE, v))
        {
            newNodes.push_back(tmpGraph->getVertexCoords(v));
            labels[2] = FilopodiaFunctions::getTypeIdOfNode(tmpGraph, v);
            labels[3] = FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM);
            labels[4] = FilopodiaFunctions::getFilopodiaIdOfNode(tmpGraph, v);
            labels[5] = FilopodiaFunctions::getMatchIdOfNode(tmpGraph, v);

            newNodeLabels.push_back(labels);
        }
        else if (FilopodiaFunctions::hasNodeType(tmpGraph, BRANCHING_NODE, v))
        {
            newNodes.push_back(tmpGraph->getVertexCoords(v));
            labels[2] = FilopodiaFunctions::getTypeIdOfNode(tmpGraph, v);
            labels[4] = FilopodiaFunctions::getFilopodiaIdOfNode(tmpGraph, v);
            labels[5] = FilopodiaFunctions::getFilopodiaLabelId(graph, IGNORED);

            if (FilopodiaFunctions::checkIfNodeIsPartOfGrowthCone(tmpGraph, v, filoRootNode))
            {
                labels[3] = FilopodiaFunctions::getLocationLabelId(graph, GROWTHCONE);
            }
            else
            {
                labels[3] = FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM);
            }

            newNodeLabels.push_back(labels);
        }
        else
        {
            HxMessage::error(QString("Error: Filopodia cannot be added.\nUnexpected node type.\n"), "ok");
        }
    }

    // Get new edges and edge labels
    McDArray<int> newEdgeSources;
    McDArray<int> newEdgeTargets;
    std::vector<std::vector<McVec3f> > newEdgePoints;
    std::vector<McDArray<int> > newEdgeLabels;

    for (int e = 0; e < tmpGraph->getNumEdges(); ++e)
    {
        McDArray<int> labels;
        labels.clear();
        labels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
        labels.push_back(FilopodiaFunctions::getTimeIdFromTimeStep(graph, nextTime));
        labels.push_back(-1);
        labels.push_back(-1);
        labels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC));

        const int source = tmpGraph->getEdgeSource(e);
        const int target = tmpGraph->getEdgeTarget(e);

        if (source == filoRootNode)
        {
            newEdgeSources.push_back(-1);
        }
        else
        {
            if (filoRootNode == -1)
            {
                newEdgeSources.push_back(source);
            }
            else
            {
                newEdgeSources.push_back(source - 1); // Substract 1 since filo root has Id0 but is not added to newNodes.
            }                                      // -> node Ids and list position is not equal -> subtract -1
        }

        if (target == filoRootNode)
        {
            newEdgeTargets.push_back(-1);
        }
        else
        {
            if (filoRootNode == -1)
            {
                newEdgeTargets.push_back(target);
            }
            else
            {
                newEdgeTargets.push_back(target - 1); // Substract 1 since filo root has Id0 but is not added to newNodes.
            }                                      // -> node Ids and list position is not equal -> subtract -1
        }

        if (FilopodiaFunctions::checkIfEdgeIsPartOfGrowthCone(tmpGraph, e, filoRootNode))
        {
            labels[2] = FilopodiaFunctions::getLocationLabelId(graph, GROWTHCONE);
        }
        else
        {
            labels[2] = FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM);
        }

        labels[3] = FilopodiaFunctions::getFilopodiaIdOfEdge(tmpGraph, e);
        newEdgeLabels.push_back(labels);

        newEdgePoints.push_back(tmpGraph->getEdgePoints(e));
    }

    qDebug() << "tmpGraph:" << tmpGraph->getNumEdges() << tmpGraph->getNumVertices();
    qDebug() << "new nodes:" << newNodes.size() << "newEdges:" << newEdgeSources.size();

    // Get successor sel
    SpatialGraphSelection successorSel(graph);
    // Check if all base nodes of time are selected
    const bool allBaseSelected = checkIfAllBasesSelected(graph, baseSel, currentTime);
    if (allBaseSelected)
    {
        successorSel = getSelectionForTimeStep(graph, nextTime);
    }
    else
    {
        SpatialGraphSelection::Iterator it(baseSel);
        for (int s = it.vertices.nextSelected(); s != -1; s = it.vertices.nextSelected())
        {
            // Select components of successor if exists
            getSuccessorOfFilopodium(graph, s, successorSel);
        }
    }
    successorSel.deselectVertex(FilopodiaFunctions::getRootNodeFromTimeStep(graph, nextTime));

    op = new ReplaceFilopodiaOperationSet(graph,
                                          rootSel,
                                          SpatialGraphSelection(graph),
                                          newNodes,
                                          newNodeLabels,
                                          previousBases,
                                          newEdgeSources,
                                          newEdgeTargets,
                                          newEdgePoints,
                                          newEdgeLabels,
                                          successorSel);

    QTime endTime = QTime::currentTime();
    qDebug() << "\tPropagate time:" << startTime.msecsTo(endTime) << "msec";

    return op;
}

AddRootsOperationSet*
FilopodiaFunctions::propagateRootNodes(HxSpatialGraph* graph,
                                       const QMap<int, McHandle<HxUniformScalarField3> > images,
                                       const McVec3i rootNodeSearchRadius,
                                       const McVec3i rootNodeTemplateRadius)
{
    HxSpatialGraph* tmpGraph = graph->duplicate();

    const int timeMin = images.keys().first();
    const int timeMax = images.keys().last();

    std::vector<McVec3f> newNodes;
    std::vector<McDArray<int> > newLabels;
    const QTime propStartTime = QTime::currentTime();

    for (int t = timeMin; t < timeMax; ++t)
    {
        const int templateTime = MC_MAX2(0, t - 5);
        const int nextTime = t + 1;

        const HxUniformScalarField3* templateImage = images.value(templateTime);
        HxUniformScalarField3* nextImage = images.value(nextTime);

        SpatialGraphSelection templateRootNodes = FilopodiaFunctions::getNodesOfTypeForTime(tmpGraph, ROOT_NODE, templateTime);

        for (int v = 0; v < templateRootNodes.getNumSelectedVertices(); ++v)
        {
            const int templateNode = templateRootNodes.getSelectedVertex(v);
            const int gcId = FilopodiaFunctions::getGcIdOfNode(tmpGraph, templateNode);
            const int currentNode = FilopodiaFunctions::getRootNodeFromTimeAndGC(tmpGraph, t, gcId); // Node with same growth cone ID as templateNode

            if (currentNode == -1)
            {
                throw McException(QString("Error finding node in timestep %1 having growth cone number %2")
                                      .arg(t)
                                      .arg(gcId));
            }

            McVec3f nextRootNodePos;
            float correlation;

            if (!FilopodiaFunctions::getNodePositionInNextTimeStep(templateImage,
                                                                   nextImage,
                                                                   tmpGraph->getVertexCoords(currentNode),
                                                                   rootNodeSearchRadius,
                                                                   tmpGraph->getVertexCoords(templateNode),
                                                                   rootNodeTemplateRadius,
                                                                   0,
                                                                   false,
                                                                   nextRootNodePos,
                                                                   correlation))
            {
                qDebug() << "Warning: No new position found for timestep" << nextTime
                         << "and growth cone number" << gcId << "Using previous coordinates."
                         << "Correlation:" << correlation;
                nextRootNodePos = tmpGraph->getVertexCoords(currentNode);
            }

            const int newNode = tmpGraph->addVertex(nextRootNodePos);
            FilopodiaFunctions::setTimeIdOfNode(tmpGraph, newNode, FilopodiaFunctions::getTimeIdFromTimeStep(graph, nextTime));
            FilopodiaFunctions::setNodeType(tmpGraph, newNode, ROOT_NODE);
            FilopodiaFunctions::setGcIdOfNode(tmpGraph, newNode, gcId);

            McDArray<int> rootLabels;
            rootLabels.push_back(gcId);
            rootLabels.push_back(FilopodiaFunctions::getTimeIdFromTimeStep(graph, nextTime));
            rootLabels.push_back(FilopodiaFunctions::getTypeLabelId(graph, ROOT_NODE));

            newNodes.push_back(nextRootNodePos);
            newLabels.push_back(rootLabels);
        }
    }

    AddRootsOperationSet* op = new AddRootsOperationSet(graph,
                                                        SpatialGraphSelection(graph),
                                                        SpatialGraphSelection(graph),
                                                        newNodes,
                                                        newLabels);

    const QTime propEndTime = QTime::currentTime();
    qDebug() << "\tpropagate time" << propStartTime.msecsTo(propEndTime) / (60.0 * 1000.0) << "min";

    return op;
}

// -------------------------Other -------------------------------
McDim3l
getWindowRadius3DInVoxels(const McVec3f& voxelSize, const float radius)
{
    McDim3l boxRadius;
    for (int i = 0; i <= 2; ++i)
    {
        boxRadius[i] = int(radius / voxelSize[i] + 0.5f);
    }
    return boxRadius;
}

std::vector<double>
getSamples(const McDim3l& center,
           const McDim3l& windowRadius,
           const HxUniformScalarField3* image)
{
    HxLattice3& lat = image->lattice();
    const McDim3l dims = lat.getDims();

    std::vector<double> samples;
    float val;
    for (int z = center[2] - windowRadius[2]; z <= center[2] + windowRadius[2]; ++z)
    {
        for (int y = center[1] - windowRadius[1]; y <= center[1] + windowRadius[1]; ++y)
        {
            for (int x = center[0] - windowRadius[0]; x <= center[0] + windowRadius[0]; ++x)
            {
                if (z >= 0 && z < dims[2] &&
                    y >= 0 && y < dims[1] &&
                    x >= 0 && x < dims[0])
                {
                    lat.eval(x, y, z, &val);
                    samples.push_back(double(val));
                }
            }
        }
    }
    return samples;
}

// Two-pass variance computation, see
// https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance
double
getStandardDeviation(const std::vector<double>& samples)
{
    const mclong n = samples.size();
    if (n <= 1)
    {
        throw McException(QString("Error while computing variance: insufficient samples"));
    }

    double sum1 = 0.0;
    for (int i = 0; i < samples.size(); ++i)
    {
        sum1 += samples[i];
    }
    const double mean = sum1 / double(n);

    double sum2 = 0.0;
    for (int i = 0; i < samples.size(); ++i)
    {
        sum2 += (samples[i] - mean) * (samples[i] - mean);
    }

    return sqrt(sum2 / double(n - 1));
}

SpatialGraphPoint
FilopodiaFunctions::testVarianceAlongEdge(const HxSpatialGraph* graph,
                                          const int edgeNum,
                                          const HxUniformScalarField3* image)
{
    qDebug() << "Testing variance along edge" << edgeNum;

    if (!image)
    {
        throw McException("No image for tracing");
    }

    const McDim3l dims = image->lattice().getDims();
    const McVec3f voxelSize = image->getVoxelSize();

    HxLattice3& lat = image->lattice();
    HxUniformCoord3* coords = dynamic_cast<HxUniformCoord3*>(lat.coords());
    HxLoc3Uniform* location = dynamic_cast<HxLoc3Uniform*>(coords->createLocation());

    McHandle<HxSpreadSheet> sheet = HxSpreadSheet::createInstance();
    sheet->setLabel("Variances");
    sheet->setNumRows(graph->getNumEdgePoints(edgeNum));
    sheet->addColumn("Length", HxSpreadSheet::Column::FLOAT);
    const int lengthColNum = sheet->findColumn("Length", HxSpreadSheet::Column::FLOAT);

    float totalLength = 0.0f;
    sheet->column(lengthColNum)->setValue(0, totalLength);
    for (int p = 1; p < graph->getNumEdgePoints(edgeNum); ++p)
    {
        totalLength += (graph->getEdgePoint(edgeNum, p - 1) - graph->getEdgePoint(edgeNum, p)).length();
        sheet->column(lengthColNum)->setValue(p, totalLength);
    }

    for (int r = 1; r <= 20; ++r)
    {
        const float windowRadius = r * voxelSize[0];
        const McDim3l windowRadius3DInVoxels = getWindowRadius3DInVoxels(voxelSize, windowRadius);
        qDebug() << "Computing variance for radius" << windowRadius3DInVoxels.nx << windowRadius3DInVoxels.ny << windowRadius3DInVoxels.nz;

        std::vector<double> stdevs;

        for (int p = 0; p < graph->getNumEdgePoints(edgeNum); ++p)
        {
            const McVec3f pos = graph->getEdgePoint(edgeNum, p);
            if (!location->set(pos))
            {
                throw McException("Tracing error: Invalid starting point");
            }
            const McDim3l I = McDim3l(location->getIx(), location->getIy(), location->getIz());
            const McVec3f U = McVec3f(location->getUx(), location->getUy(), location->getUz());
            const McDim3l posIndex = getNearestVoxelCenterFromIndex(I, U, dims);

            const std::vector<double> samples = getSamples(posIndex, windowRadius3DInVoxels, image);

            double stdev;
            try
            {
                stdev = getStandardDeviation(samples);
            }
            catch (McException& e)
            {
                qDebug() << e.what();
                stdev = 0.0;
            }

            stdevs.push_back(stdev);
        }

        const QString columnName = QString("Radius_%1").arg(windowRadius);
        sheet->addColumn(qPrintable(columnName), HxSpreadSheet::Column::DOUBLE);
        const int colNum = sheet->findColumn(qPrintable(columnName), HxSpreadSheet::Column::DOUBLE);
        for (int v = 0; v < stdevs.size(); ++v)
        {
            sheet->column(colNum)->setValue(v, stdevs[v]);
        }
    }

    theObjectPool->addObject(sheet);
    return SpatialGraphPoint(-1, -1);
}

McVec3f
computePointNormal(const HxSpatialGraph* graph,
                   const SpatialGraphPoint& point)
{
    const std::vector<McVec3f> edgePoints = graph->getEdgePoints(point.edgeNum);
    return computePointNormal(edgePoints, point.pointNum);
}
