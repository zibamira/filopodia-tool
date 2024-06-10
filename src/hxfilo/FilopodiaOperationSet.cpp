#include "FilopodiaOperationSet.h"
#include "FilopodiaFunctions.h"
#include "HxFilopodiaTrack.h"
#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <hxspatialgraph/internal/SpatialGraphFunctions.h>
#include <hxspatialgraph/internal/SpatialGraphOperationSet.h>
#include <hxspatialgraph/internal/SpatialGraphSmoothOperation.h>
#include <hxspatialgraph/internal/SpatialGraphOperation.h>
#include <hxspatialgraph/internal/OperationLogEntry.h>
#include "QxVariantConversions.h"
#include <hxcore/HxMessage.h>
#include <mclib/internal/McAssert.h>
#include <mclib/McException.h>
#include <mclib/internal/McRandom.h>
#include <limits>
#include <QVariantMap>
#include <QDebug>

#include "ConvertVectorAndMcDArray.h"

////////////////////////////////////////////////////////////////////////////////////////////////
AddAndAssignLabelOperationSet::AddAndAssignLabelOperationSet(HxSpatialGraph* sg,
                                                             SpatialGraphSelection sel,
                                                             SpatialGraphSelection vis,
                                                             const QString& attName,
                                                             const int newLabelId)
    : OperationSet(sg, sel, vis)
    , mAttName(attName)
    , mNewLabelId(newLabelId)
{
    mcenter("AddAndAssignLabelOperationSet::AddAndAssignLabelOperationSet");
}

AddAndAssignLabelOperationSet::AddAndAssignLabelOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry)
    : OperationSet(sg, logEntry.m_highlightedSelection, logEntry.m_visibleSelection)
    , mAttName(logEntry.m_customData["attributeName"].toString())
    , mNewLabelId(logEntry.m_customData["newLabelId"].toInt())
{
    if (logEntry.m_customData.find("attributeName") == logEntry.m_customData.end())
    {
        throw McException(QString("Invalid AddAndAssignLabelOperationSet log entry: no \"attributeName\" field."));
    }
    if (logEntry.m_customData.find("newLabelId") == logEntry.m_customData.end())
    {
        throw McException(QString("Invalid AddAndAssignLabelOperationSet log entry: no \"newLabelId\" field."));
    }

    mAttName = logEntry.m_customData["attributeName"].toString();
    if (mAttName.isEmpty())
    {
        throw McException(QString("Invalid AddAndAssignLabelOperationSet log entry: no valid \"attributeName\" value."));
    }

    bool ok;
    mNewLabelId = logEntry.m_customData["newLabelId"].toInt(&ok);
    if (!ok)
    {
        throw McException(QString("Invalid AddAndAssignLabelOperationSet log entry: no valid \"newLabelId\" value."));
    }
}

OperationLogEntry
AddAndAssignLabelOperationSet::getLogEntry() const
{
    OperationLogEntry entry;
    entry.m_operationName = QString("AddAndAssignLabelOperationSet");
    entry.m_highlightedSelection = mSelection;
    entry.m_visibleSelection = mVisibleSelection;
    entry.m_customData["attributeName"] = mAttName;
    entry.m_customData["newLabelId"] = mNewLabelId;
    return entry;
}

void
AddAndAssignLabelOperationSet::exec()
{
    // check edges with all points selected
    const int numEdgesWithAllPointsSelected = mSelection.getNumEdgesWithAllPointsSelected();

    for (int e = 0; e < numEdgesWithAllPointsSelected; ++e)
    {
        const int edgeWithAllPointsSelected = mSelection.getSelectedEdgeWithAllPointsSelected(e);

        for (int p = 0; p < graph->getNumEdgePoints(edgeWithAllPointsSelected); ++p)
        {
            mSelection.selectPoint(SpatialGraphPoint(edgeWithAllPointsSelected, p));
        }
    }

    mRememberOldSelection = mSelection;
    const QByteArray ba = mAttName.toLatin1();
    const char* attName = ba.constData();

    if (mSelection.getNumSelectedVertices() == 0 && mSelection.getNumSelectedEdges() == 0 && mSelection.getNumSelectedPoints() == 0)
    {
        throw McException(QString("Could not assign label: \nNothing selected."));
    }

    HierarchicalLabels* labels = graph->getLabelGroup(attName);
    if (!labels)
    {
        throw McException(QString("Could not assign label: Label group %1 does not exist.").arg(mAttName));
    }
    if (!labels->isValidId(mNewLabelId))
    {
        const QString newLabelName = FilopodiaFunctions::getNewManualNodeMatchLabelName(graph);
        const SbColor color(McRandom::rand(), McRandom::rand(), McRandom::rand());

        AddLabelOperation* addLabelOp = new AddLabelOperation(graph,
                                                              SpatialGraphSelection(graph),
                                                              SpatialGraphSelection(graph),
                                                              mAttName,
                                                              newLabelName,
                                                              color);
        addLabelOp->exec();
        operations.push_back(addLabelOp);
        mNewLabelId = addLabelOp->getNewLabelId();
    }

    // Change selected vertex label:
    if (mSelection.getNumSelectedVertices() > 0)
    {
        mRememberOldVertexLabelId.resize(mSelection.getNumSelectedVertices());
        EdgeVertexAttribute* vAtt = graph->findVertexAttribute(attName);

        for (int i = 0; i < mSelection.getNumSelectedVertices(); ++i)
        {
            const int selNode = mSelection.getSelectedVertex(i);
            const int oldLabelId = vAtt->getIntDataAtIdx(selNode);
            mRememberOldVertexLabelId[i] = oldLabelId;
        }
    }

    // Change selected edge label:
    if (mSelection.getNumSelectedEdges() > 0)
    {
        mRememberOldEdgeLabelId.resize(mSelection.getNumSelectedEdges());
        EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(attName);

        for (int i = 0; i < mSelection.getNumSelectedEdges(); ++i)
        {
            const int selEdge = mSelection.getSelectedEdge(i);
            const int oldLabelId = eAtt->getIntDataAtIdx(selEdge);
            mRememberOldEdgeLabelId[i] = oldLabelId;
        }
    }

    // Change selected edge point label:
    if (mSelection.getNumSelectedPoints() > 0)
    {
        mRememberOldPointLabelId.resize(mSelection.getNumSelectedPoints());
        PointAttribute* pAtt = graph->findEdgePointAttribute(attName);

        for (int i = 0; i < mSelection.getNumSelectedPoints(); ++i)
        {
            const SpatialGraphPoint selPoint = mSelection.getSelectedPoint(i);
            const int oldLabelId = pAtt->getIntDataAtPoint(selPoint.edgeNum, selPoint.pointNum);
            mRememberOldPointLabelId[i] = oldLabelId;
        }
    }

    AssignLabelOperation* assignLabelOp = new AssignLabelOperation(graph, mSelection, mVisibleSelection, mAttName, mNewLabelId);

    assignLabelOp->exec();
    operations.push_back(assignLabelOp);
}

void
AddAndAssignLabelOperationSet::undo()
{
    mSelection = mRememberOldSelection;
    const QByteArray ba = mAttName.toLatin1();
    const char* attName = ba.constData();

    // Change selected vertex label:
    if (mSelection.getNumSelectedVertices() > 0)
    {
        EdgeVertexAttribute* vAtt = graph->findVertexAttribute(attName);
        if (!vAtt)
        {
            throw McException(QString("Could not undo label assignment: \nNo node attribute named %1 found.").arg(mAttName));
        }

        for (int i = 0; i < mSelection.getNumSelectedVertices(); ++i)
        {
            const int selNode = mSelection.getSelectedVertex(i);
            vAtt->setIntDataAtIdx(selNode, mRememberOldVertexLabelId[i]);
        }
    }

    // Change selected edge label:
    if (mSelection.getNumSelectedEdges() > 0)
    {
        EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(attName);
        if (!eAtt)
        {
            throw McException(QString("Could not undo label assignment: \nNo segment attribute named %1 found.").arg(mAttName));
        }

        for (int i = 0; i < mSelection.getNumSelectedEdges(); ++i)
        {
            const int selEdge = mSelection.getSelectedEdge(i);
            eAtt->setIntDataAtIdx(selEdge, mRememberOldEdgeLabelId[i]);
        }
    }

    // Change selected edge point label:
    if (mSelection.getNumSelectedPoints() > 0)
    {
        PointAttribute* pAtt = graph->findEdgePointAttribute(attName);
        if (!pAtt)
        {
            throw McException(QString("Could not undo label assignment: \nNo point attribute named %1 found.").arg(mAttName));
        }

        for (int i = 0; i < mSelection.getNumSelectedPoints(); ++i)
        {
            const SpatialGraphPoint selPoint = mSelection.getSelectedPoint(i);
            pAtt->setIntDataAtPoint(mRememberOldPointLabelId[i], selPoint.edgeNum, selPoint.pointNum);
        }
    }
}

SpatialGraphSelection
AddAndAssignLabelOperationSet::getSelectionAfterOperation(const SpatialGraphSelection& sel) const
{
    mcassert(graph->getNumVertices() == sel.getNumVertices() && graph->getNumEdges() == sel.getNumEdges());
    return sel;
}

//////////////////////////////////////////////////////////////////////////////////////////////
ConvertFilopodiaPointOperationSet::ConvertFilopodiaPointOperationSet(HxSpatialGraph* sg,
                                                                     const SpatialGraphSelection& selectedElements,
                                                                     const SpatialGraphSelection& visibleElements,
                                                                     const McDArray<int> labels)
    : OperationSet(sg, selectedElements, visibleElements)
    , mPoint(SpatialGraphPoint(-1, -1))
    , mNewVertexNum(-1)
    , mLabels(labels)
{
    mcenter("ConvertFilopodiaPointOperationSet::ConvertFilopodiaPointOperationSet");
}

ConvertFilopodiaPointOperationSet::ConvertFilopodiaPointOperationSet(HxSpatialGraph* sg,
                                                                     const OperationLogEntry& logEntry)
    : OperationSet(sg, logEntry.m_highlightedSelection, logEntry.m_visibleSelection)
    , mNewVertexNum(-1)
{
    if (logEntry.m_customData.contains("labels"))
    {
        mLabels = qxvariantconversions::variantToIntArray(logEntry.m_customData["labels"]);
    }
    else
    {
        throw McException(QString("Invalid ConvertFilopodiaPointOperationSet log entry: no \"labels\" field."));
    }
}

ConvertFilopodiaPointOperationSet::~ConvertFilopodiaPointOperationSet()
{
    mcenter("ConvertFilopodiaPointOperationSet::~ConvertFilopodiaPointOperationSet");
}

OperationLogEntry
ConvertFilopodiaPointOperationSet::getLogEntry() const
{
    QVariantMap customData;
    customData["labels"] = qxvariantconversions::intArrayToVariant(mLabels);

    OperationLogEntry entry;
    entry.m_operationName = QString("ConvertFilopodiaPointOperationSet");
    entry.m_highlightedSelection = mSelection;
    entry.m_visibleSelection = mVisibleSelection;
    entry.m_customData = customData;

    return entry;
}

void
ConvertFilopodiaPointOperationSet::exec()
{
    if (!FilopodiaFunctions::isFilopodiaGraph(graph))
    {
        throw McException("ConvertFilopodiaPointOperationSet: graph is not filopodia graph.");
    }

    if (mLabels.size() != 7)
    {
        throw McException("ConvertFilopodiaPointOperationSet: wrong number of labels. 7 labels are required.");
    }

    if (mSelection.getNumSelectedEdges() != 0 || mSelection.getNumSelectedVertices() != 0 || mSelection.getNumSelectedPoints() != 1)
    {
        throw McException("ConvertFilopodiaPointOperationSet: wrong selection.");
    }

    if (mSelection.getNumSelectedPoints() == 1)
    {
        SpatialGraphPoint point = mSelection.getSelectedPoint(0);
        if (point.pointNum == 0 || point.pointNum == graph->getNumEdgePoints(point.edgeNum) - 1)
        {
            throw McException("ConvertFilopodiaPointOperationSet: selected point is not convertable");
        }
    }

    mPoint = mSelection.getSelectedPoint(0);

    PointToVertexOperation* convertOp = new PointToVertexOperation(graph, mSelection, SpatialGraphSelection(graph));
    convertOp->exec();
    operations.push_back(convertOp);
    mNewVertexNum = convertOp->getNewVertexNum();

    SpatialGraphSelection sel(graph);
    sel.selectVertex(mNewVertexNum);

    AssignLabelOperation* gcOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getGrowthConeAttributeName()), mLabels[0]);
    gcOp->exec();
    operations.push_back(gcOp);

    AssignLabelOperation* timeOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getTimeStepAttributeName()), mLabels[1]);
    timeOp->exec();
    operations.push_back(timeOp);

    AssignLabelOperation* typeOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getTypeLabelAttributeName()), mLabels[2]);
    typeOp->exec();
    operations.push_back(typeOp);

    AssignLabelOperation* locOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getLocationAttributeName()), mLabels[3]);
    locOp->exec();
    operations.push_back(locOp);

    AssignLabelOperation* filoOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getFilopodiaAttributeName()), mLabels[4]);
    filoOp->exec();
    operations.push_back(filoOp);

    AssignLabelOperation* matchOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getManualNodeMatchAttributeName()), mLabels[5]);
    matchOp->exec();
    operations.push_back(matchOp);

    AssignLabelOperation* geoOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getManualGeometryLabelAttributeName()), mLabels[6]);
    geoOp->exec();
    operations.push_back(geoOp);
}

int
ConvertFilopodiaPointOperationSet::getNewVertexNum() const
{
    return mNewVertexNum;
}

//////////////////////////////////////////////////////////////////////////////////////////////
AddFilopodiaVertexOperationSet::AddFilopodiaVertexOperationSet(HxSpatialGraph* sg,
                                                               const McVec3f& coords,
                                                               const SpatialGraphSelection& selectedElements,
                                                               const SpatialGraphSelection& visibleElements,
                                                               const McDArray<int> labels)
    : OperationSet(sg, selectedElements, visibleElements)
    , mCoords(coords)
    , mNewVertexNum(-1)
    , mLabels(labels)
{
    mcenter("AddFilopodiaVertexOperationSet::AddFilopodiaVertexOperationSet");
}

AddFilopodiaVertexOperationSet::AddFilopodiaVertexOperationSet(HxSpatialGraph* sg,
                                                               const OperationLogEntry& logEntry)
    : OperationSet(sg, logEntry.m_highlightedSelection, logEntry.m_visibleSelection)
    , mNewVertexNum(-1)
{
    if (logEntry.m_customData.contains("newPos"))
    {
        mCoords = qxvariantconversions::variantToPoint(logEntry.m_customData["newPos"]);
    }
    else
    {
        throw McException(QString("Invalid AddFilopodiaVertexOperationSet log entry: no \"newPos\" field."));
    }

    if (logEntry.m_customData.contains("labels"))
    {
        mLabels = qxvariantconversions::variantToIntArray(logEntry.m_customData["labels"]);
    }
    else
    {
        throw McException(QString("Invalid AddFilopodiaVertexOperationSet log entry: no \"labels\" field."));
    }
}

AddFilopodiaVertexOperationSet::~AddFilopodiaVertexOperationSet()
{
    mcenter("AddFilopodiaVertexOperationSet::~AddFilopodiaVertexOperationSet");
}

OperationLogEntry
AddFilopodiaVertexOperationSet::getLogEntry() const
{
    QVariantMap customData;
    customData["newPos"] = qxvariantconversions::pointToVariant(mCoords);
    customData["labels"] = qxvariantconversions::intArrayToVariant(mLabels);

    OperationLogEntry entry;
    entry.m_operationName = QString("AddFilopodiaVertexOperationSet");
    entry.m_highlightedSelection = mSelection;
    entry.m_visibleSelection = mVisibleSelection;
    entry.m_customData = customData;

    return entry;
}

void
AddFilopodiaVertexOperationSet::exec()
{
    if (!FilopodiaFunctions::isFilopodiaGraph(graph) && !FilopodiaFunctions::isRootNodeGraph(graph))
    {
        throw McException("AddFilopodiaVertexOperationSet: graph is not filopodia nor rootnode graph");
    }
    if (FilopodiaFunctions::isFilopodiaGraph(graph) && mLabels.size() != 7)
    {
        throw McException("AddFilopodiaVertexOperationSet: wrong number of labels");
    }
    if (!FilopodiaFunctions::isFilopodiaGraph(graph) && FilopodiaFunctions::isRootNodeGraph(graph) && mLabels.size() != 3)
    {
        throw McException("AddFilopodiaVertexOperationSet: wrong number of labels");
    }

    AddVertexOperation* addVertexOp = new AddVertexOperation(graph, mCoords, mSelection, mVisibleSelection);
    addVertexOp->exec();
    operations.push_back(addVertexOp);
    mNewVertexNum = addVertexOp->getNewVertexNum();

    SpatialGraphSelection sel(graph);
    sel.selectVertex(mNewVertexNum);

    AssignLabelOperation* gcOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getGrowthConeAttributeName()), mLabels[0]);
    gcOp->exec();
    operations.push_back(gcOp);

    AssignLabelOperation* timeOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getTimeStepAttributeName()), mLabels[1]);
    timeOp->exec();
    operations.push_back(timeOp);

    AssignLabelOperation* typeOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getTypeLabelAttributeName()), mLabels[2]);
    typeOp->exec();
    operations.push_back(typeOp);

    if (FilopodiaFunctions::isFilopodiaGraph(graph))
    {
        AssignLabelOperation* locOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getLocationAttributeName()), mLabels[3]);
        locOp->exec();
        operations.push_back(locOp);

        AssignLabelOperation* filoOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getFilopodiaAttributeName()), mLabels[4]);
        filoOp->exec();
        operations.push_back(filoOp);

        AssignLabelOperation* matchOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getManualNodeMatchAttributeName()), mLabels[5]);
        matchOp->exec();
        operations.push_back(matchOp);

        AssignLabelOperation* geoOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getManualGeometryLabelAttributeName()), mLabels[6]);
        geoOp->exec();
        operations.push_back(geoOp);
    }
}

int
AddFilopodiaVertexOperationSet::getNewVertexNum() const
{
    return mNewVertexNum;
}

//////////////////////////////////////////////////////////////////////////////////////////////
AddFilopodiaEdgeOperationSet::AddFilopodiaEdgeOperationSet(HxSpatialGraph* sg,
                                                           const SpatialGraphSelection& selectedElements,
                                                           const SpatialGraphSelection& visibleElements,
                                                           const std::vector<McVec3f>& points,
                                                           const McDArray<int> labels)
    : OperationSet(sg, selectedElements, visibleElements)
    , mPoints(points)
    , mNewEdgeNum(-1)
    , mLabels(labels)
{
    mcenter("AddFilopodiaEdgeOperationSet::AddFilopodiaEdgeOperationSet");
}

AddFilopodiaEdgeOperationSet::AddFilopodiaEdgeOperationSet(HxSpatialGraph* sg,
                                                           const OperationLogEntry& logEntry)
    : OperationSet(sg, logEntry.m_highlightedSelection, logEntry.m_visibleSelection)
    , mNewEdgeNum(-1)
{
    if (logEntry.m_customData.contains("points"))
    {
        mPoints = qxvariantconversions::variantToPointArray(logEntry.m_customData["points"]);
    }
    else
    {
        throw McException(QString("Invalid AddFilopodiaEdgeOperationSet log entry: no \"points\" field."));
    }

    if (logEntry.m_customData.contains("labels"))
    {
        mLabels = qxvariantconversions::variantToIntArray(logEntry.m_customData["labels"]);
    }
    else
    {
        throw McException(QString("Invalid AddFilopodiaEdgeOperationSet log entry: no \"labels\" field."));
    }
}

AddFilopodiaEdgeOperationSet::~AddFilopodiaEdgeOperationSet()
{
    mcenter("AddFilopodiaEdgeOperationSet::~AddFilopodiaEdgeOperationSet");
}

OperationLogEntry
AddFilopodiaEdgeOperationSet::getLogEntry() const
{
    QVariantMap customData;
    customData["labels"] = qxvariantconversions::intArrayToVariant(mLabels);
    customData["points"] = qxvariantconversions::pointArrayToVariant(mPoints);

    OperationLogEntry entry;
    entry.m_operationName = QString("AddFilopodiaEdgeOperationSet");
    entry.m_highlightedSelection = mSelection;
    entry.m_visibleSelection = mVisibleSelection;
    entry.m_customData = customData;

    return entry;
}

void
AddFilopodiaEdgeOperationSet::exec()
{
    if (!FilopodiaFunctions::isFilopodiaGraph(graph) && !FilopodiaFunctions::isRootNodeGraph(graph))
    {
        throw McException("AddFiloEdgeAttribute: graph is not filopodia nor rootnode graph");
    }

    if (!FilopodiaFunctions::hasGrowthConeLabels(graph)){
        qDebug() << "AddFiloEdgeAttribute: no edge attribute named GrowthCones -> transform labelgroup";
        FilopodiaFunctions::transformGcLabelGroup(graph);
    }

    if (mLabels.size() != 5)
    {
        throw McException("AddFiloEdgeAttribute: wrong number of labels");
    }

    ConnectOperation* addEdgeOp = new ConnectOperation(graph, mSelection, mVisibleSelection, mPoints);
    addEdgeOp->exec();
    operations.push_back(addEdgeOp);
    mNewEdgeNum = addEdgeOp->getNewEdgeNum();

    SpatialGraphSelection sel(graph);
    sel.selectEdge(mNewEdgeNum);

    // Assign required labels
    AssignLabelOperation* gcOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getGrowthConeAttributeName()), mLabels[0]);
    gcOp->exec();
    operations.push_back(gcOp);

    AssignLabelOperation* timeOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getTimeStepAttributeName()), mLabels[1]);
    timeOp->exec();
    operations.push_back(timeOp);

    AssignLabelOperation* locOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getLocationAttributeName()), mLabels[2]);
    locOp->exec();
    operations.push_back(locOp);

    AssignLabelOperation* filoOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getFilopodiaAttributeName()), mLabels[3]);
    filoOp->exec();
    operations.push_back(filoOp);

    AssignLabelOperation* geoOp = new AssignLabelOperation(graph, sel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getManualGeometryLabelAttributeName()), mLabels[4]);
    geoOp->exec();
    operations.push_back(geoOp);

    // Smooth new edge
    SmoothOperation* smoothOp = new SmoothOperation(graph, sel, SpatialGraphSelection(graph));
    smoothOp->exec();
    operations.push_back(smoothOp);

}

int
AddFilopodiaEdgeOperationSet::getNewEdgeNum() const
{
    return mNewEdgeNum;
}

//////////////////////////////////////////////////////////////////////////////////////////////
ReplaceFilopodiaEdgeOperationSet::ReplaceFilopodiaEdgeOperationSet(HxSpatialGraph* sg,
                                                                   const SpatialGraphSelection& selectedElements,
                                                                   const SpatialGraphSelection& visibleElements,
                                                                   const std::vector<McVec3f>& points,
                                                                   const McDArray<int>& labels,
                                                                   const SpatialGraphSelection& selToDelete)
    : OperationSet(sg, selectedElements, visibleElements)
    , mNewEdgeNum(-1)
    , mPoints(points)
    , mSelToDelete(selToDelete)
    , mLabels(labels)
    , mTimeId(-1)
{
    mcenter("ReplaceFilopodiaEdgeOperationSet::ReplaceFilopodiaEdgeOperationSet");
}

ReplaceFilopodiaEdgeOperationSet::ReplaceFilopodiaEdgeOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry)
    : OperationSet(sg, logEntry.m_highlightedSelection, logEntry.m_visibleSelection)
    , mNewEdgeNum(-1)
{
    if (logEntry.m_customData.contains("points"))
    {
        mPoints = qxvariantconversions::variantToPointArray(logEntry.m_customData["points"]);
    }
    else
    {
        throw McException(QString("Invalid ReplaceFilopodiaEdgeOperationSet log entry: no \"points\" field."));
    }

    if (logEntry.m_customData.contains("labels"))
    {
        mLabels = qxvariantconversions::variantToIntArray(logEntry.m_customData["labels"]);
    }
    else
    {
        throw McException(QString("Invalid ReplaceFilopodiaEdgeOperationSet log entry: no \"labels\" field."));
    }

    if (logEntry.m_customData.contains("selectionToDelete"))
    {
        mSelToDelete.deserialize(logEntry.m_customData["selectionToDelete"].toByteArray());
    }
    else
    {
        throw McException(QString("Invalid ReplaceFilopodiaEdgeOperationSet log entry: no \"selectionToDelete\" field."));
    }

    mTimeId = logEntry.m_customData["timeId"].toInt();
}

ReplaceFilopodiaEdgeOperationSet::~ReplaceFilopodiaEdgeOperationSet()
{
    mcenter("ReplaceFilopodiaEdgeOperationSet::~ReplaceFilopodiaEdgeOperationSet");
}

OperationLogEntry
ReplaceFilopodiaEdgeOperationSet::getLogEntry() const
{
    QVariantMap customData;

    customData["points"] = qxvariantconversions::pointArrayToVariant(mPoints);
    customData["labels"] = qxvariantconversions::intArrayToVariant(mLabels);
    customData["selectionToDelete"] = mSelToDelete.serialize();
    customData["timeId"] = mTimeId;

    OperationLogEntry entry;
    entry.m_operationName = QString("ReplaceFilopodiaEdgeOperationSet");
    entry.m_highlightedSelection = mSelection;
    entry.m_visibleSelection = mVisibleSelection;
    entry.m_customData = customData;

    return entry;
}

void
ReplaceFilopodiaEdgeOperationSet::exec()
{
    if (!FilopodiaFunctions::isFilopodiaGraph(graph))
    {
        throw McException("ReplaceFilopodiaEdgeOperationSet: graph is not filopodia graph.");
    }

    if (!(mSelToDelete.getNumSelectedVertices() == 0 && mSelToDelete.getNumSelectedPoints() == 0 && mSelToDelete.getNumSelectedEdges() == 1))
    {
        throw McException(QString("ReplaceFilopodiaEdgeOperationSet: Invalid selection to delete."));
    }

    if (!(mSelection.getNumSelectedVertices() == 2 && mSelection.getNumSelectedPoints() == 0 && mSelection.getNumSelectedEdges() == 0))
    {
        throw McException(QString("ReplaceFilopodiaEdgeOperationSet: Invalid selection."));
    }

    mTimeId = FilopodiaFunctions::getTimeIdOfEdge(graph, mSelToDelete.getSelectedEdge(0));

    SpatialGraphSelection selToDelete(mSelToDelete);

    // Add new edge
    AddFilopodiaEdgeOperationSet* addEdgeOp = new AddFilopodiaEdgeOperationSet(graph, mSelection, SpatialGraphSelection(graph), mPoints, mLabels);
    addEdgeOp->exec();
    operations.push_back(addEdgeOp);
    mNewEdgeNum = addEdgeOp->getNewEdgeNum();
    selToDelete = addEdgeOp->getSelectionAfterOperation(selToDelete);

    // Delete old edge
    DeleteOperation* deleteOp = new DeleteOperation(graph, selToDelete, SpatialGraphSelection(graph));
    deleteOp->exec();
    operations.push_back(deleteOp);
}

int
ReplaceFilopodiaEdgeOperationSet::getNewEdgeNum() const
{
    return mNewEdgeNum;
}

//////////////////////////////////////////////////////////////////////////////////////////////
AddRootsOperationSet::AddRootsOperationSet(HxSpatialGraph* sg,
                                           const SpatialGraphSelection& selectedElements,
                                           const SpatialGraphSelection& visibleElements,
                                           const std::vector<McVec3f>& newNodes,
                                           const std::vector<McDArray<int> >& newNodeLabels)
    : OperationSet(sg, selectedElements, visibleElements)
    , mNewNodes(newNodes)
    , mNewNodeLabels(newNodeLabels)
{
    mcenter("AddRootsOperationSet::AddRootsOperationSet");
}

AddRootsOperationSet::AddRootsOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry)
    : OperationSet(sg, logEntry.m_highlightedSelection, logEntry.m_visibleSelection)
{
    if (logEntry.m_customData.contains("newNodes"))
    {
        mNewNodes = qxvariantconversions::variantToPointArray(logEntry.m_customData["newNodes"]);
    }
    else
    {
        throw McException(QString("Invalid AddRootsOperationSet log entry: no \"newNodes\" field."));
    }

    if (logEntry.m_customData.contains("newNodeLabels"))
    {
        const QVariantList nodeLabelList = logEntry.m_customData["newNodeLabels"].toList();
        for (int i = 0; i < nodeLabelList.size(); ++i)
        {
            mNewNodeLabels.push_back(qxvariantconversions::variantToIntArray(nodeLabelList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid AddRootsOperationSet log entry: no \"newNodeLabels\" field."));
    }
}

AddRootsOperationSet::~AddRootsOperationSet()
{
    mcenter("AddRootsOperationSet::~AddRootsOperationSet");
}

OperationLogEntry
AddRootsOperationSet::getLogEntry() const
{
    QVariantMap customData;
    customData["newNodes"] = qxvariantconversions::pointArrayToVariant(mNewNodes);

    QVariantList nodeLabels;
    for (int n = 0; n < mNewNodeLabels.size(); ++n)
    {
        nodeLabels.push_back(qxvariantconversions::intArrayToVariant(mNewNodeLabels[n]));
    }
    customData["newNodeLabels"] = nodeLabels;

    OperationLogEntry entry;
    entry.m_operationName = QString("AddRootsOperationSet");
    entry.m_highlightedSelection = mSelection;
    entry.m_visibleSelection = mVisibleSelection;
    entry.m_customData = customData;

    return entry;
}

void
AddRootsOperationSet::exec()
{
    if (!mSelection.isEmpty())
    {
        throw McException("AddRootsOperationSet: Invalid selection.");
    }
    if (!FilopodiaFunctions::isRootNodeGraph(graph))
    {
        throw McException("AddRootsOperationSet: graph is not root node graph.");
    }
    if (mNewNodeLabels.size() != mNewNodes.size())
    {
        throw McException("AddRootsOperationSet: invalid number of node labels");
    }

    for (int v = 0; v < mNewNodes.size(); ++v)
    {
        AddFilopodiaVertexOperationSet* addFiloVertexOp = new AddFilopodiaVertexOperationSet(graph, mNewNodes[v], SpatialGraphSelection(graph), SpatialGraphSelection(graph), mNewNodeLabels[v]);
        addFiloVertexOp->exec();
        operations.push_back(addFiloVertexOp);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////
DeleteFilopodiaOperationSet::DeleteFilopodiaOperationSet(HxSpatialGraph* sg,
                                                         const SpatialGraphSelection& selectedElements,
                                                         const SpatialGraphSelection& visibleElements)
    : OperationSet(sg, selectedElements, visibleElements)
    , mTimeId(-1)
    , mOldGeo(-1)
{
    mcenter("DeleteFilopodiaOperationSet::DeleteFilopodiaOperationSet");
}

DeleteFilopodiaOperationSet::DeleteFilopodiaOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry)
    : OperationSet(sg, logEntry.m_highlightedSelection, logEntry.m_visibleSelection)
{
    mTimeId = logEntry.m_customData["timeId"].toInt();
}

DeleteFilopodiaOperationSet::~DeleteFilopodiaOperationSet()
{
    mcenter("DeleteFilopodiaOperationSet::~DeleteFilopodiaOperationSet");
}

OperationLogEntry
DeleteFilopodiaOperationSet::getLogEntry() const
{
    QVariantMap customData;
    customData["timeId"] = mTimeId;
    customData["geoId"] = mOldGeo;

    OperationLogEntry entry;
    entry.m_operationName = QString("DeleteFilopodiaOperationSet");
    entry.m_highlightedSelection = mSelection;
    entry.m_visibleSelection = mVisibleSelection;
    entry.m_customData = customData;

    return entry;
}

void
DeleteFilopodiaOperationSet::exec()
{
    if (!FilopodiaFunctions::isFilopodiaGraph(graph))
    {
        throw McException("DeleteFilopodiaOperationSet: graph is not filopodia graph.");
    }

    SpatialGraphSelection oldTips = FilopodiaFunctions::getNodesOfTypeInSelection(graph, mSelection, TIP_NODE);
    mOldGeo = FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC);
    if (oldTips.getNumSelectedVertices() == 1)
    {
        const int oldNode = oldTips.getSelectedVertex(0);
        mOldGeo = FilopodiaFunctions::getGeometryIdOfNode(graph, oldNode);
        if (mOldGeo != FilopodiaFunctions::getManualGeometryLabelId(graph, MANUAL))
        {
            mOldGeo = FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC);
        }
    }

    mTimeId = FilopodiaFunctions::getTimeIdOfNode(graph, mSelection.getSelectedVertex(0));

    // Delete selected nodes and edges
    DeleteOperation* deleteEdgesOp = new DeleteOperation(graph, mSelection, SpatialGraphSelection(graph));
    deleteEdgesOp->exec();
    operations.push_back(deleteEdgesOp);
    SpatialGraphSelection branchNodeSel = deleteEdgesOp->getSelectionAfterOperation(mSelection);

    // Convert branching nodes
    if (branchNodeSel.getNumSelectedVertices() > 0)
    {
        MultipleVerticesToPointsOperationSet* v2pOperation = new MultipleVerticesToPointsOperationSet(graph, branchNodeSel, SpatialGraphSelection(graph));
        v2pOperation->exec();
        operations.push_back(v2pOperation);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////
MergeFilopodiaOperationSet::MergeFilopodiaOperationSet(HxSpatialGraph* sg,
                                                       const SpatialGraphSelection& selectedElements,
                                                       const SpatialGraphSelection& visibleElements,
                                                       const std::vector<McVec3f>& newNodes,
                                                       const std::vector<McDArray<int> >& newNodeLabels,
                                                       const McDArray<int>& newEdgeSources,
                                                       const McDArray<int>& newEdgeTargets,
                                                       const std::vector<std::vector<McVec3f> >& newEdgePoints,
                                                       const std::vector<McDArray<int> >& newEdgeLabels,
                                                       const McDArray<int> connectLabels)
    : OperationSet(sg, selectedElements, visibleElements)
    , mNewNodes(newNodes)
    , mNewNodeLabels(newNodeLabels)
    , mNewEdgeSources(newEdgeSources)
    , mNewEdgeTargets(newEdgeTargets)
    , mNewEdgePoints(newEdgePoints)
    , mNewEdgeLabels(newEdgeLabels)
    , mConnectLabels(connectLabels)
    , mNewNodesAndEdges(graph)
    , mNewConnection(-1)
{
    mcenter("MergeFilopodiaOperationSet::MergeFilopodiaOperationSet");
}

MergeFilopodiaOperationSet::MergeFilopodiaOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry)
    : OperationSet(sg, logEntry.m_highlightedSelection, logEntry.m_visibleSelection)
{
    if (logEntry.m_customData.contains("newNodes"))
    {
        mNewNodes = qxvariantconversions::variantToPointArray(logEntry.m_customData["newNodes"]);
    }
    else
    {
        throw McException(QString("Invalid MergeFilopodiaOperationSet log entry: no \"newNodes\" field."));
    }

    if (logEntry.m_customData.contains("newNodeLabels"))
    {
        const QVariantList nodeLabelList = logEntry.m_customData["newNodeLabels"].toList();
        for (int i = 0; i < nodeLabelList.size(); ++i)
        {
            mNewNodeLabels.push_back(qxvariantconversions::variantToIntArray(nodeLabelList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid MergeFilopodiaOperationSet log entry: no \"newNodeLabels\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeSources"))
    {
        mNewEdgeSources = qxvariantconversions::variantToIntArray(logEntry.m_customData["newEdgeSources"]);
    }
    else
    {
        throw McException(QString("Invalid MergeFilopodiaOperationSet log entry: no \"newEdgeSources\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeTargets"))
    {
        mNewEdgeTargets = qxvariantconversions::variantToIntArray(logEntry.m_customData["newEdgeTargets"]);
    }
    else
    {
        throw McException(QString("Invalid MergeFilopodiaOperationSet log entry: no \"newEdgeTargets\" field."));
    }

    if (logEntry.m_customData.contains("newEdgePoints"))
    {
        const QVariantList pointsList = logEntry.m_customData["newEdgePoints"].toList();
        for (int i = 0; i < pointsList.size(); ++i)
        {
            mNewEdgePoints.push_back(qxvariantconversions::variantToPointArray(pointsList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid MergeFilopodiaOperationSet log entry: no \"newEdgePoints\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeLabels"))
    {
        const QVariantList edgeLabelList = logEntry.m_customData["newEdgeLabels"].toList();
        for (int i = 0; i < edgeLabelList.size(); ++i)
        {
            mNewEdgeLabels.push_back(qxvariantconversions::variantToIntArray(edgeLabelList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid MergeFilopodiaOperationSet log entry: no \"newEdgeLabels\" field."));
    }

    if (logEntry.m_customData.contains("connectLabels"))
    {
        mConnectLabels = qxvariantconversions::variantToIntArray(logEntry.m_customData["connectLabels"]);
    }
    else
    {
        mConnectLabels = McDArray<int>();
    }
}

MergeFilopodiaOperationSet::~MergeFilopodiaOperationSet()
{
    mcenter("MergeFilopodiaOperationSet::~MergeFilopodiaOperationSet");
}

OperationLogEntry
MergeFilopodiaOperationSet::getLogEntry() const
{
    QVariantMap customData;
    customData["newNodes"] = qxvariantconversions::pointArrayToVariant(mNewNodes);

    QVariantList nodeLabels;
    for (int n = 0; n < mNewNodeLabels.size(); ++n)
    {
        nodeLabels.push_back(qxvariantconversions::intArrayToVariant(mNewNodeLabels[n]));
    }
    customData["newNodeLabels"] = nodeLabels;

    customData["newEdgeSources"] = qxvariantconversions::intArrayToVariant(mNewEdgeSources);
    customData["newEdgeTargets"] = qxvariantconversions::intArrayToVariant(mNewEdgeTargets);

    QVariantList points;
    for (int pts = 0; pts < mNewEdgePoints.size(); ++pts)
    {
        points.push_back(qxvariantconversions::pointArrayToVariant(mNewEdgePoints[pts]));
    }
    customData["newEdgePoints"] = points;

    QVariantList edgeLabels;
    for (int e = 0; e < mNewEdgeLabels.size(); ++e)
    {
        edgeLabels.push_back(qxvariantconversions::intArrayToVariant(mNewEdgeLabels[e]));
    }
    customData["newEdgeLabels"] = edgeLabels;

    if (mConnectLabels.size() > 0)
    {
        customData["connectLabels"] = qxvariantconversions::intArrayToVariant(mConnectLabels);
    }

    OperationLogEntry entry;
    entry.m_operationName = QString("MergeFilopodiaOperationSet");
    entry.m_highlightedSelection = mSelection;
    entry.m_visibleSelection = mVisibleSelection;
    entry.m_customData = customData;

    return entry;
}

void
MergeFilopodiaOperationSet::exec()
{
    // Check requirements
    if (!FilopodiaFunctions::isFilopodiaGraph(graph))
    {
        throw McException("MergeFilopodiaOperationSet: graph is not filopodia graph.");
    }
    if (mNewNodeLabels.size() != mNewNodes.size())
    {
        throw McException("MergeFilopodiaOperationSet: invalid number of node types");
    }
    if (mNewEdgeTargets.size() != mNewEdgeSources.size())
    {
        throw McException("MergeFilopodiaOperationSet: invalid number of edge targets");
    }
    if (mNewEdgePoints.size() != mNewEdgeSources.size())
    {
        throw McException("MergeFilopodiaOperationSet: invalid number of edge point arrays");
    }
    if (mNewEdgePoints.size() != mNewEdgeLabels.size())
    {
        throw McException("MergeFilopodiaOperationSet: invalid number of edge labels");
    }
    for (int e = 0; e < mNewEdgeSources.size(); ++e)
    {
        if (mNewEdgeSources[e] == mNewEdgeTargets[e])
        {
            throw McException("MergeFilopodiaOperationSet: edge source and edge target must differ");
        }
    }

    // Get connection with filopodia graph and convert point if necessary
    if (mSelection.getNumSelectedVertices() == 0 && mSelection.getNumSelectedEdges() == 0 && (mSelection.getNumSelectedPoints() == 1))
    {
        if (mConnectLabels.size() == 0)
        {
            throw McException("MergeFilopodiaOperationSet: labels for connection point needed.");
        }
        else if (mConnectLabels.size() != 7)
        {
            throw McException("MergeFilopodiaOperationSet: wrong number of labels. 7 labels are required.");
        }

        ConvertFilopodiaPointOperationSet* convertOp = new ConvertFilopodiaPointOperationSet(graph, mSelection, mVisibleSelection, mConnectLabels);
        operations.push_back(convertOp);
        convertOp->exec();
        mNewConnection = convertOp->getNewVertexNum();
    }
    else if (mSelection.getNumSelectedVertices() == 1 && mSelection.getNumSelectedEdges() == 0 && (mSelection.getNumSelectedPoints() == 0))
    {
        mNewConnection = mSelection.getSelectedVertex(0);
    }
    else
    {
        throw McException(QString("MergeFilopodiaOperationSet: Invalid selection."));
    }

    std::vector<int> nodeIds;
    for (int v = 0; v < mNewNodes.size(); ++v)
    {
        if (mNewNodes[v].equals(graph->getVertexCoords(mNewConnection)))
        {
            nodeIds.push_back(-1);
            continue;
        }

        AddFilopodiaVertexOperationSet* addVertexOp = new AddFilopodiaVertexOperationSet(graph, mNewNodes[v], SpatialGraphSelection(graph), SpatialGraphSelection(graph), mNewNodeLabels[v]);
        addVertexOp->exec();
        operations.push_back(addVertexOp);
        const int newVertexNum = addVertexOp->getNewVertexNum();
        nodeIds.push_back(newVertexNum);
        mNewNodesAndEdges.resize(graph->getNumVertices(), graph->getNumEdges());
        mNewNodesAndEdges.selectVertex(newVertexNum);
    }

    for (int e = 0; e < mNewEdgeSources.size(); ++e)
    {
        int source, target;

        if (mNewEdgeSources[e] == -1)
            source = mNewConnection;
        else
            source = nodeIds[mNewEdgeSources[e]];

        if (mNewEdgeTargets[e] == -1)
            target = mNewConnection;
        else
            target = nodeIds[mNewEdgeTargets[e]];

        SpatialGraphSelection sel(graph);
        sel.selectVertex(source);
        sel.selectVertex(target);

        AddFilopodiaEdgeOperationSet* addEdgeOp = new AddFilopodiaEdgeOperationSet(graph, sel, SpatialGraphSelection(graph), mNewEdgePoints[e], mNewEdgeLabels[e]);
        addEdgeOp->exec();
        operations.push_back(addEdgeOp);

        mNewNodesAndEdges.resize(graph->getNumVertices(), graph->getNumEdges());
        mNewNodesAndEdges.selectEdge(addEdgeOp->getNewEdgeNum());
    }
}

SpatialGraphSelection
MergeFilopodiaOperationSet::getNewNodesAndEdges() const
{
    return mNewNodesAndEdges;
}

int
MergeFilopodiaOperationSet::getConnectNode() const
{
    return mNewConnection;
}

SpatialGraphSelection
MergeFilopodiaOperationSet::getSelAfterOperation(SpatialGraphSelection sel) const
{
    if (mSelection.getNumVertices() != sel.getNumVertices() ||
        mSelection.getNumEdges() != sel.getNumEdges())
    {
        const QString msg = QApplication::translate("MergeFilopodiaOperationSet",
                                                    "Error in MergeFilopodiaOperationSet::getSelAfterOperation: \
                                     selection has %1 vertices and %2 edges (Expected: %3 vertices and %4 edges")
                                .arg(sel.getNumVertices())
                                .arg(sel.getNumEdges())
                                .arg(mSelection.getNumVertices())
                                .arg(mSelection.getNumEdges());
        throw McException(msg);
    }

    SpatialGraphSelection newSel(graph);
    newSel.addSelection(sel);
    newSel.deselectAllEdges();
    newSel.deselectAllPoints();

    if (mSelection.getNumSelectedPoints() == 0)
    {
        // No selected point -> no pointToVertex Operation -> selection does not change
        newSel.addSelection(sel);
    }
    else if (mSelection.getNumSelectedPoints() == 1)
    {
        // Selected point -> pointToVertex Operation -> selection change as in pointToVertexOperation
        const SpatialGraphPoint selectedPoint = mSelection.getSelectedPoint(0);
        const int oldEdge = selectedPoint.edgeNum;

        SpatialGraphSelection edgeSel(graph);
        edgeSel.selectEdge(oldEdge);

        for (int selE = 0; selE < sel.getNumSelectedEdges(); ++selE)
        {
            const int selEdge = sel.getSelectedEdge(selE);
            if (!edgeSel.isSelectedEdge(selEdge))
            {
                int smaller = 0;
                if (oldEdge < selEdge)
                {
                    smaller++;
                }
                newSel.selectEdge(selEdge - smaller);
            }
        }

        for (int selP = 0; selP < sel.getNumSelectedPoints(); ++selP)
        {
            const SpatialGraphPoint selPoint = sel.getSelectedPoint(selP);
            const int pointEdge = selPoint.edgeNum;
            if (pointEdge != oldEdge)
            {
                int smaller = 0;
                if (oldEdge < pointEdge)
                {
                    smaller++;
                }
                SpatialGraphPoint newPoint = SpatialGraphPoint(pointEdge - smaller, selPoint.pointNum);
                newSel.selectPoint(newPoint);
            }
        }
    }
    else
    {
        throw McException("MergeFilopodiaOp::getSelAfterOp: wrong number of selected points.");
    }

    return newSel;
}

//////////////////////////////////////////////////////////////////////////////////////////////
AddFilopodiumOperationSet::AddFilopodiumOperationSet(HxSpatialGraph* sg,
                                                     const SpatialGraphSelection& selectedElements,
                                                     const SpatialGraphSelection& visibleElements,
                                                     const std::vector<McVec3f>& newNodes,
                                                     const std::vector<McDArray<int> >& newNodeLabels,
                                                     const McDArray<int>& newEdgeSources,
                                                     const McDArray<int>& newEdgeTargets,
                                                     const std::vector<std::vector<McVec3f> >& newEdgePoints,
                                                     const std::vector<McDArray<int> >& newEdgeLabels,
                                                     const McDArray<int> connectLabels,
                                                     const bool addedIn2DViewer)
    : OperationSet(sg, selectedElements, visibleElements)
    , mNewNodes(newNodes)
    , mNewNodeLabels(newNodeLabels)
    , mNewEdgeSources(newEdgeSources)
    , mNewEdgeTargets(newEdgeTargets)
    , mNewEdgePoints(newEdgePoints)
    , mNewEdgeLabels(newEdgeLabels)
    , mConnectLabels(connectLabels)
    , mAddedIn2DViewer(addedIn2DViewer)
    , mTimeId(-1)
    , mNewNodesAndEdges(graph)
    , mNewConnection(-1)
{
    mcenter("AddFilopodiumOperationSet::AddFilopodiumOperationSet");
}

AddFilopodiumOperationSet::AddFilopodiumOperationSet(HxSpatialGraph* sg,
                                                     const OperationLogEntry& logEntry)
    : OperationSet(sg, logEntry.m_highlightedSelection, logEntry.m_visibleSelection)
{
    if (logEntry.m_customData.contains("newNodes"))
    {
        mNewNodes = qxvariantconversions::variantToPointArray(logEntry.m_customData["newNodes"]);
    }
    else
    {
        throw McException(QString("Invalid AddFilopodiumOperationSet log entry: no \"newNodes\" field."));
    }

    if (logEntry.m_customData.contains("newNodeLabels"))
    {
        const QVariantList nodeLabelList = logEntry.m_customData["newNodeLabels"].toList();
        for (int i = 0; i < nodeLabelList.size(); ++i)
        {
            mNewNodeLabels.push_back(qxvariantconversions::variantToIntArray(nodeLabelList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid AddFilopodiumOperationSet log entry: no \"newNodeLabels\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeSources"))
    {
        mNewEdgeSources = qxvariantconversions::variantToIntArray(logEntry.m_customData["newEdgeSources"]);
    }
    else
    {
        throw McException(QString("Invalid AddFilopodiumOperationSet log entry: no \"newEdgeSources\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeTargets"))
    {
        mNewEdgeTargets = qxvariantconversions::variantToIntArray(logEntry.m_customData["newEdgeTargets"]);
    }
    else
    {
        throw McException(QString("Invalid AddFilopodiumOperationSet log entry: no \"newEdgeTargets\" field."));
    }

    if (logEntry.m_customData.contains("newEdgePoints"))
    {
        const QVariantList pointsList = logEntry.m_customData["newEdgePoints"].toList();
        for (int i = 0; i < pointsList.size(); ++i)
        {
            mNewEdgePoints.push_back(qxvariantconversions::variantToPointArray(pointsList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid AddFilopodiumOperationSet log entry: no \"newEdgePoints\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeLabels"))
    {
        const QVariantList edgeLabelList = logEntry.m_customData["newEdgeLabels"].toList();
        for (int i = 0; i < edgeLabelList.size(); ++i)
        {
            mNewEdgeLabels.push_back(qxvariantconversions::variantToIntArray(edgeLabelList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid AddFilopodiumOperationSet log entry: no \"newEdgeLabels\" field."));
    }

    if (logEntry.m_customData.contains("connectLabels"))
    {
        mConnectLabels = qxvariantconversions::variantToIntArray(logEntry.m_customData["connectLabels"]);
    }
    else
    {
        mConnectLabels = McDArray<int>();
    }

    if (logEntry.m_customData.contains("addedIn2DViewer"))
    {
        mAddedIn2DViewer = logEntry.m_customData["addedIn2DViewer"].toBool();
    }

    mTimeId = logEntry.m_customData["timeId"].toInt();
}

AddFilopodiumOperationSet::~AddFilopodiumOperationSet()
{
    mcenter("AddFilopodiumOperationSet::~AddFilopodiumOperationSet");
}

OperationLogEntry
AddFilopodiumOperationSet::getLogEntry() const
{
    QVariantMap customData;
    customData["newNodes"] = qxvariantconversions::pointArrayToVariant(mNewNodes);

    QVariantList nodeLabels;
    for (int n = 0; n < mNewNodeLabels.size(); ++n)
    {
        nodeLabels.push_back(qxvariantconversions::intArrayToVariant(mNewNodeLabels[n]));
    }
    customData["newNodeLabels"] = nodeLabels;

    customData["newEdgeSources"] = qxvariantconversions::intArrayToVariant(mNewEdgeSources);
    customData["newEdgeTargets"] = qxvariantconversions::intArrayToVariant(mNewEdgeTargets);

    QVariantList points;
    for (int pts = 0; pts < mNewEdgePoints.size(); ++pts)
    {
        points.push_back(qxvariantconversions::pointArrayToVariant(mNewEdgePoints[pts]));
    }
    customData["newEdgePoints"] = points;

    QVariantList edgeLabels;
    for (int e = 0; e < mNewEdgeLabels.size(); ++e)
    {
        edgeLabels.push_back(qxvariantconversions::intArrayToVariant(mNewEdgeLabels[e]));
    }
    customData["newEdgeLabels"] = edgeLabels;

    if (mConnectLabels.size() > 0)
    {
        customData["connectLabels"] = qxvariantconversions::intArrayToVariant(mConnectLabels);
    }

    customData["addedIn2DViewer"] = mAddedIn2DViewer;
    customData["timeId"] = mTimeId;

    OperationLogEntry entry;
    entry.m_operationName = QString("AddFilopodiumOperationSet");
    entry.m_highlightedSelection = mSelection;
    entry.m_visibleSelection = mVisibleSelection;
    entry.m_customData = customData;

    return entry;
}

void
AddFilopodiumOperationSet::exec()
{
    // This operation only calls MergeFilopodiaOperationSet() but is needed for Logging

    // Check requirements
    if (!FilopodiaFunctions::isFilopodiaGraph(graph))
    {
        throw McException("AddFilopodiumOperationSet: graph is not filopodia graph.");
    }
    if (mSelection.getNumSelectedEdges() + mSelection.getNumSelectedVertices() + mSelection.getNumSelectedPoints() != 1)
    {
        throw McException("AddFilopodiumOperationSet: invalid selection");
    }
    if (mSelection.getNumSelectedEdges() != 0 || (mSelection.getNumSelectedVertices() != 1 && mSelection.getNumSelectedPoints() != 1))
    {
        throw McException("AddFilopodiumOperationSet: invalid selection");
    }
    if (mNewNodeLabels.size() != mNewNodes.size())
    {
        throw McException("AddFilopodiumOperationSet: invalid number of node types");
    }
    if (mNewEdgeTargets.size() != mNewEdgeSources.size())
    {
        throw McException("AddFilopodiumOperationSet: invalid number of edge targets");
    }
    if (mNewEdgePoints.size() != mNewEdgeSources.size())
    {
        throw McException("AddFilopodiumOperationSet: invalid number of edge point arrays");
    }
    if (mNewEdgePoints.size() != mNewEdgeLabels.size())
    {
        throw McException("AddFilopodiumOperationSet: invalid number of edge labels");
    }
    for (int e = 0; e < mNewEdgeSources.size(); ++e)
    {
        if (mNewEdgeSources[e] == mNewEdgeTargets[e])
        {
            throw McException("AddFilopodiumOperationSet: edge source and edge target must differ");
        }
    }

    if (mSelection.getNumSelectedVertices() == 1)
    {
        mTimeId = FilopodiaFunctions::getTimeIdOfNode(graph, mSelection.getSelectedVertex(0));
    }
    else if (mSelection.getNumSelectedPoints() == 1)
    {
        mTimeId = FilopodiaFunctions::getTimeIdOfEdge(graph, mSelection.getSelectedPoint(0).edgeNum);
    }

    MergeFilopodiaOperationSet* mergeFiloOp = new MergeFilopodiaOperationSet(graph,
                                                                             mSelection,
                                                                             mVisibleSelection,
                                                                             mNewNodes,
                                                                             mNewNodeLabels,
                                                                             mNewEdgeSources,
                                                                             mNewEdgeTargets,
                                                                             mNewEdgePoints,
                                                                             mNewEdgeLabels,
                                                                             mConnectLabels);
    mergeFiloOp->exec();
    operations.push_back(mergeFiloOp);

    mNewNodesAndEdges = mergeFiloOp->getNewNodesAndEdges();
    mNewConnection = mergeFiloOp->getConnectNode();
}

SpatialGraphSelection
AddFilopodiumOperationSet::getNewNodesAndEdges() const
{
    return mNewNodesAndEdges;
}

int
AddFilopodiumOperationSet::getConnectNode() const
{
    return mNewConnection;
}

//////////////////////////////////////////////////////////////////////////////////////////////
MoveTipOperationSet::MoveTipOperationSet(HxSpatialGraph* sg,
                                         const SpatialGraphSelection& selectedElements,
                                         const SpatialGraphSelection& visibleElements,
                                         const std::vector<McVec3f>& newNodes,
                                         const std::vector<McDArray<int> >& newNodeLabels,
                                         const McDArray<int>& newEdgeSources,
                                         const McDArray<int>& newEdgeTargets,
                                         const std::vector<std::vector<McVec3f> >& newEdgePoints,
                                         const std::vector<McDArray<int> >& newEdgeLabels,
                                         const SpatialGraphSelection& selToDelete,
                                         const bool addedIn2DViewer)
    : OperationSet(sg, selectedElements, visibleElements)
    , mNewNodes(newNodes)
    , mNewNodeLabels(newNodeLabels)
    , mNewEdgeSources(newEdgeSources)
    , mNewEdgeTargets(newEdgeTargets)
    , mNewEdgePoints(newEdgePoints)
    , mNewEdgeLabels(newEdgeLabels)
    , mSelToDelete(selToDelete)
    , mAddedIn2DViewer(addedIn2DViewer)
    , mNewNodesAndEdges(graph)
    , mTimeId(-1)
    , mOldPos()
    , mOldGeo(-1)
    , mNewPos()
{
    mcenter("MoveTipOperationSet::MoveTipOperationSet");
}

MoveTipOperationSet::MoveTipOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry)
    : OperationSet(sg, logEntry.m_highlightedSelection, logEntry.m_visibleSelection)
{
    if (logEntry.m_customData.contains("newNodes"))
    {
        mNewNodes = qxvariantconversions::variantToPointArray(logEntry.m_customData["newNodes"]);
    }
    else
    {
        throw McException(QString("Invalid MoveTipOperationSet log entry: no \"newNodes\" field."));
    }

    if (logEntry.m_customData.contains("newNodeLabels"))
    {
        const QVariantList nodeLabelList = logEntry.m_customData["newNodeLabels"].toList();
        for (int i = 0; i < nodeLabelList.size(); ++i)
        {
            mNewNodeLabels.push_back(qxvariantconversions::variantToIntArray(nodeLabelList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid MoveTipOperationSet log entry: no \"newNodeLabels\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeSources"))
    {
        mNewEdgeSources = qxvariantconversions::variantToIntArray(logEntry.m_customData["newEdgeSources"]);
    }
    else
    {
        throw McException(QString("Invalid MoveTipOperationSet log entry: no \"newEdgeSources\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeTargets"))
    {
        mNewEdgeTargets = qxvariantconversions::variantToIntArray(logEntry.m_customData["newEdgeTargets"]);
    }
    else
    {
        throw McException(QString("Invalid MoveTipOperationSet log entry: no \"newEdgeTargets\" field."));
    }

    if (logEntry.m_customData.contains("newEdgePoints"))
    {
        const QVariantList pointsList = logEntry.m_customData["newEdgePoints"].toList();
        for (int i = 0; i < pointsList.size(); ++i)
        {
            mNewEdgePoints.push_back(qxvariantconversions::variantToPointArray(pointsList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid MoveTipOperationSet log entry: no \"newEdgePoints\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeLabels"))
    {
        const QVariantList edgeLabelList = logEntry.m_customData["newEdgeLabels"].toList();
        for (int i = 0; i < edgeLabelList.size(); ++i)
        {
            mNewEdgeLabels.push_back(qxvariantconversions::variantToIntArray(edgeLabelList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid MoveTipOperationSet log entry: no \"newEdgeLabels\" field."));
    }

    if (logEntry.m_customData.contains("selectionToDelete"))
    {
        mSelToDelete.deserialize(logEntry.m_customData["selectionToDelete"].toByteArray());
    }
    else
    {
        throw McException(QString("Invalid MoveTipOperationSet log entry: no \"selectionToDelete\" field."));
    }

    if (logEntry.m_customData.contains("addedIn2DViewer"))
    {
        mAddedIn2DViewer = logEntry.m_customData["addedIn2DViewer"].toBool();
    }

    mTimeId = logEntry.m_customData["timeId"].toInt();
}

MoveTipOperationSet::~MoveTipOperationSet()
{
    mcenter("MoveTipOperationSet::~MoveTipOperationSet");
}

OperationLogEntry
MoveTipOperationSet::getLogEntry() const
{
    QVariantMap customData;
    customData["newNodes"] = qxvariantconversions::pointArrayToVariant(mNewNodes);

    QVariantList nodeLabels;
    for (int n = 0; n < mNewNodeLabels.size(); ++n)
    {
        nodeLabels.push_back(qxvariantconversions::intArrayToVariant(mNewNodeLabels[n]));
    }
    customData["newNodeLabels"] = nodeLabels;

    customData["newEdgeSources"] = qxvariantconversions::intArrayToVariant(mNewEdgeSources);
    customData["newEdgeTargets"] = qxvariantconversions::intArrayToVariant(mNewEdgeTargets);

    QVariantList points;
    for (int pts = 0; pts < mNewEdgePoints.size(); ++pts)
    {
        points.push_back(qxvariantconversions::pointArrayToVariant(mNewEdgePoints[pts]));
    }
    customData["newEdgePoints"] = points;

    QVariantList edgeLabels;
    for (int e = 0; e < mNewEdgeLabels.size(); ++e)
    {
        edgeLabels.push_back(qxvariantconversions::intArrayToVariant(mNewEdgeLabels[e]));
    }
    customData["newEdgeLabels"] = edgeLabels;

    customData["selectionToDelete"] = mSelToDelete.serialize();
    customData["addedIn2DViewer"] = mAddedIn2DViewer;
    customData["oldPos"] = qxvariantconversions::pointToVariant(mOldPos);
    customData["geoId"] = mOldGeo;
    customData["newPos"] = qxvariantconversions::pointToVariant(mNewPos);
    customData["timeId"] = mTimeId;

    OperationLogEntry entry;
    entry.m_operationName = QString("MoveTipOperationSet");
    entry.m_highlightedSelection = mSelection;
    entry.m_visibleSelection = mVisibleSelection;
    entry.m_customData = customData;

    return entry;
}

void
MoveTipOperationSet::exec()
{
    // Check requirements
    if (!FilopodiaFunctions::isFilopodiaGraph(graph))
    {
        throw McException("MoveTipOperationSet: graph is not filopodia graph.");
    }
    if (mSelection.getNumSelectedEdges() + mSelection.getNumSelectedVertices() + mSelection.getNumSelectedPoints() != 1)
    {
        throw McException("MoveTipOperationSet: invalid selection");
    }
    if (mSelection.getNumSelectedEdges() != 0 || (mSelection.getNumSelectedVertices() != 1 && mSelection.getNumSelectedPoints() != 1))
    {
        throw McException("MoveTipOperationSet: invalid selection");
    }
    if (mSelection.getNumSelectedPoints() == 1)
    {
        SpatialGraphPoint point = mSelection.getSelectedPoint(0);
        if (point.pointNum == 0 || point.pointNum == graph->getNumEdgePoints(point.edgeNum) - 1)
        {
            throw McException("MoveTipOperationSet: selected point is not convertable");
        }
    }
    if (mNewNodeLabels.size() != mNewNodes.size())
    {
        throw McException("MoveTipOperationSet: invalid number of node types");
    }
    if (mNewEdgeTargets.size() != mNewEdgeSources.size())
    {
        throw McException("MoveTipOperationSet: invalid number of edge targets");
    }
    if (mNewEdgePoints.size() != mNewEdgeSources.size())
    {
        throw McException("MoveTipOperationSet: invalid number of edge point arrays");
    }
    if (mNewEdgePoints.size() != mNewEdgeLabels.size())
    {
        throw McException("MoveTipOperationSet: invalid number of edge labels");
    }
    for (int e = 0; e < mNewEdgeSources.size(); ++e)
    {
        if (mNewEdgeSources[e] == mNewEdgeTargets[e])
        {
            throw McException("MoveTipOperationSet: edge source and edge target must differ");
        }
    }

    if (mSelection.getNumSelectedVertices() == 1)
    {
        mTimeId = FilopodiaFunctions::getTimeIdOfNode(graph, mSelection.getSelectedVertex(0));
    }
    else if (mSelection.getNumSelectedPoints() == 1)
    {
        mTimeId = FilopodiaFunctions::getTimeIdOfEdge(graph, mSelection.getSelectedPoint(0).edgeNum);
    }

    // Get infos for log entry
    SpatialGraphSelection oldTipSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, mSelToDelete, TIP_NODE);
    if (oldTipSel.getNumSelectedVertices() == 1)
    {
        const int oldTip = oldTipSel.getSelectedVertex(0);
        mOldPos = graph->getVertexCoords(oldTip);
        mOldGeo = FilopodiaFunctions::getGcIdOfNode(graph, oldTip);
    }

    // Merge
    MergeFilopodiaOperationSet* mergeFiloOp = new MergeFilopodiaOperationSet(graph,
                                                                             mSelection,
                                                                             mVisibleSelection,
                                                                             mNewNodes,
                                                                             mNewNodeLabels,
                                                                             mNewEdgeSources,
                                                                             mNewEdgeTargets,
                                                                             mNewEdgePoints,
                                                                             mNewEdgeLabels);
    mergeFiloOp->exec();
    operations.push_back(mergeFiloOp);
    SpatialGraphSelection selToDelete = mergeFiloOp->getSelAfterOperation(mSelToDelete);
    mNewNodesAndEdges = mergeFiloOp->getNewNodesAndEdges();

    SpatialGraphSelection newTipSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, mNewNodesAndEdges, TIP_NODE);
    if (newTipSel.getNumSelectedVertices() == 1)
    {
        const int newTip = newTipSel.getSelectedVertex(0);
        mNewPos = graph->getVertexCoords(newTip);
    }
    else
    {
        qDebug() << "MoveTipOperationSet: new nodes and edges does not contain tip" << newTipSel.getNumSelectedVertices();
    }

    // Delete
    DeleteFilopodiaOperationSet* deleteOp = new DeleteFilopodiaOperationSet(graph, selToDelete, mergeFiloOp->getVisibleSelectionAfterOperation());
    deleteOp->exec();
    operations.push_back(deleteOp);
    mNewNodesAndEdges = deleteOp->getSelectionAfterOperation(mNewNodesAndEdges);
}

SpatialGraphSelection
MoveTipOperationSet::getNewNodesAndEdges() const
{
    return mNewNodesAndEdges;
}

//////////////////////////////////////////////////////////////////////////////////////////////
MoveTipAlongEdgeOperationSet::MoveTipAlongEdgeOperationSet(HxSpatialGraph* sg,
                                                           const SpatialGraphSelection& selectedElements,
                                                           const SpatialGraphSelection& visibleElements,
                                                           const McDArray<int> labels)
    : OperationSet(sg, selectedElements, visibleElements)
    , mLabels(labels)
    , mTimeId(-1)
    , mOldPos()
    , mOldGeo(-1)
    , mNewVertexNum(-1)
    , mNewPos()

{
    mcenter("MoveTipAlongEdgeOperationSet::MoveTipAlongEdgeOperationSet");
}

MoveTipAlongEdgeOperationSet::MoveTipAlongEdgeOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry)
    : OperationSet(sg, logEntry.m_highlightedSelection, logEntry.m_visibleSelection)
{
    if (logEntry.m_customData.contains("newNodeLabels"))
    {
        mLabels = qxvariantconversions::variantToIntArray(logEntry.m_customData["newNodeLabels"]);
    }
    else
    {
        throw McException(QString("Invalid MoveTipAlongEdgeOperationSet log entry: no \"newNodeLabels\" field."));
    }

    mTimeId = logEntry.m_customData["timeId"].toInt();
}

MoveTipAlongEdgeOperationSet::~MoveTipAlongEdgeOperationSet()
{
    mcenter("MoveTipAlongEdgeOperationSet::~MoveTipAlongEdgeOperationSet");
}

OperationLogEntry
MoveTipAlongEdgeOperationSet::getLogEntry() const
{
    QVariantMap customData;

    customData["newNodeLabels"] = qxvariantconversions::intArrayToVariant(mLabels);
    customData["timeId"] = mTimeId;
    customData["oldPos"] = qxvariantconversions::pointToVariant(mOldPos);
    customData["newPos"] = qxvariantconversions::pointToVariant(mNewPos);
    customData["geoId"] = mOldGeo;

    OperationLogEntry entry;
    entry.m_customData = customData;
    entry.m_operationName = QString("MoveTipAlongEdgeOperationSet");
    entry.m_highlightedSelection = mSelection;
    entry.m_visibleSelection = mVisibleSelection;
    return entry;
}

void
MoveTipAlongEdgeOperationSet::exec()
{
    if (!FilopodiaFunctions::isFilopodiaGraph(graph))
    {
        throw McException("MoveTipAlongEdgeOperationSet: graph is not filopodia graph.");
    }
    if (!(mSelection.getNumSelectedVertices() == 1 && mSelection.getNumSelectedPoints() == 1))
    {
        throw McException("MoveTipAlongEdgeOperationSet: wrong selection.");
    }

    SpatialGraphSelection pointSel(graph);
    pointSel.selectPoint(mSelection.getSelectedPoint(0));

    SpatialGraphSelection deleteSel(graph);
    deleteSel.copyVertexSelection(mSelection);

    // Get infos for log entry
    mTimeId = FilopodiaFunctions::getTimeIdOfNode(graph, mSelection.getSelectedVertex(0));
    mOldPos = graph->getVertexCoords(mSelection.getSelectedVertex(0));
    mOldGeo = FilopodiaFunctions::getGeometryIdOfNode(graph, mSelection.getSelectedVertex(0));
    mNewPos = graph->getEdgePoint(mSelection.getSelectedPoint(0).edgeNum, mSelection.getSelectedPoint(0).pointNum);

    // Convert selected edge point to node
    ConvertFilopodiaPointOperationSet* convertOp = new ConvertFilopodiaPointOperationSet(graph, pointSel, SpatialGraphSelection(graph), mLabels);
    convertOp->exec();
    operations.push_back(convertOp);
    deleteSel = convertOp->getSelectionAfterOperation(deleteSel);
    mNewVertexNum = convertOp->getNewVertexNum();
    SpatialGraphSelection newNodeSel(graph);
    newNodeSel.selectVertex(mNewVertexNum);

    // Delete excess segment (old tip, incident edge)
    const int oldTip = deleteSel.getSelectedVertex(0);
    const IncidenceList incidentList = graph->getIncidentEdges(oldTip);
    if (incidentList.size() == 1)
    {
        deleteSel.selectEdge(incidentList[0]);
    }
    else
    {
        throw McException("MoveTipAlongEdgeOperationSet: inconsistent graph. \nSelected tip has to have exactly one incident edge.");
    }

    DeleteOperation* deleteOp = new DeleteOperation(graph, deleteSel, SpatialGraphSelection(graph));
    deleteOp->exec();
    operations.push_back(deleteOp);
    newNodeSel = deleteOp->getSelectionAfterOperation(newNodeSel);
    mNewVertexNum = newNodeSel.getSelectedVertex(0);
}

int
MoveTipAlongEdgeOperationSet::getNewVertexNum() const
{
    return mNewVertexNum;
}

//////////////////////////////////////////////////////////////////////////////////////////////
MoveBaseOperationSet::MoveBaseOperationSet(HxSpatialGraph* sg,
                                           const SpatialGraphSelection& selectedElements,
                                           const SpatialGraphSelection& visibleElements,
                                           const std::vector<McVec3f>& newNodes,
                                           const std::vector<McDArray<int> >& newNodeLabels,
                                           const McDArray<int>& newEdgeSources,
                                           const McDArray<int>& newEdgeTargets,
                                           const std::vector<std::vector<McVec3f> >& newEdgePoints,
                                           const std::vector<McDArray<int> >& newEdgeLabels,
                                           const SpatialGraphSelection& deleteSel,
                                           const McDArray<int>& connectLabels)
    : OperationSet(sg, selectedElements, visibleElements)
    , mNewNodes(newNodes)
    , mNewNodeLabels(newNodeLabels)
    , mNewEdgeSources(newEdgeSources)
    , mNewEdgeTargets(newEdgeTargets)
    , mNewEdgePoints(newEdgePoints)
    , mNewEdgeLabels(newEdgeLabels)
    , mSelToDelete(deleteSel)
    , mConnectLabels(connectLabels)
    , mTimeId(-1)
    , mOldPos()
    , mOldGeo(-1)
    , mNewNodesAndEdges(graph)
    , mNewConnection(-1)
    , mNewPos()
{
    mcenter("MoveBaseOperationSet::MoveBaseOperationSet");
}

MoveBaseOperationSet::MoveBaseOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry)
    : OperationSet(sg, logEntry.m_highlightedSelection, logEntry.m_visibleSelection)
{
    if (logEntry.m_customData.contains("newNodes"))
    {
        mNewNodes = qxvariantconversions::variantToPointArray(logEntry.m_customData["newNodes"]);
    }
    else
    {
        throw McException(QString("Invalid MoveBaseOperationSet log entry: no \"newNodes\" field."));
    }

    if (logEntry.m_customData.contains("newNodeLabels"))
    {
        const QVariantList nodeLabelList = logEntry.m_customData["newNodeLabels"].toList();
        for (int i = 0; i < nodeLabelList.size(); ++i)
        {
            mNewNodeLabels.push_back(qxvariantconversions::variantToIntArray(nodeLabelList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid MoveBaseOperationSet log entry: no \"newNodeLabels\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeSources"))
    {
        mNewEdgeSources = qxvariantconversions::variantToIntArray(logEntry.m_customData["newEdgeSources"]);
    }
    else
    {
        throw McException(QString("Invalid MoveBaseOperationSet log entry: no \"newEdgeSources\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeTargets"))
    {
        mNewEdgeTargets = qxvariantconversions::variantToIntArray(logEntry.m_customData["newEdgeTargets"]);
    }
    else
    {
        throw McException(QString("Invalid MoveBaseOperationSet log entry: no \"newEdgeTargets\" field."));
    }

    if (logEntry.m_customData.contains("newEdgePoints"))
    {
        const QVariantList pointsList = logEntry.m_customData["newEdgePoints"].toList();
        for (int i = 0; i < pointsList.size(); ++i)
        {
            mNewEdgePoints.push_back(qxvariantconversions::variantToPointArray(pointsList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid MoveBaseOperationSet log entry: no \"newEdgePoints\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeLabels"))
    {
        const QVariantList edgeLabelList = logEntry.m_customData["newEdgeLabels"].toList();
        for (int i = 0; i < edgeLabelList.size(); ++i)
        {
            mNewEdgeLabels.push_back(qxvariantconversions::variantToIntArray(edgeLabelList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid MoveBaseOperationSet log entry: no \"newEdgeLabels\" field."));
    }

    if (logEntry.m_customData.contains("selectionToDelete"))
    {
        mSelToDelete.deserialize(logEntry.m_customData["selectionToDelete"].toByteArray());
    }
    else
    {
        throw McException(QString("Invalid MoveBaseOperationSet log entry: no \"selectionToDelete\" field."));
    }

    if (logEntry.m_customData.contains("connectLabels"))
    {
        mConnectLabels = qxvariantconversions::variantToIntArray(logEntry.m_customData["connectLabels"]);
    }
    else
    {
        mConnectLabels = McDArray<int>();
    }

    mTimeId = logEntry.m_customData["timeId"].toInt();
}

MoveBaseOperationSet::~MoveBaseOperationSet()
{
    mcenter("MoveBaseOperationSet::~MoveBaseOperationSet");
}

OperationLogEntry
MoveBaseOperationSet::getLogEntry() const
{
    QVariantMap customData;
    customData["newNodes"] = qxvariantconversions::pointArrayToVariant(mNewNodes);

    QVariantList nodeLabels;
    for (int n = 0; n < mNewNodeLabels.size(); ++n)
    {
        nodeLabels.push_back(qxvariantconversions::intArrayToVariant(mNewNodeLabels[n]));
    }
    customData["newNodeLabels"] = nodeLabels;

    customData["newEdgeSources"] = qxvariantconversions::intArrayToVariant(mNewEdgeSources);
    customData["newEdgeTargets"] = qxvariantconversions::intArrayToVariant(mNewEdgeTargets);

    QVariantList points;
    for (int pts = 0; pts < mNewEdgePoints.size(); ++pts)
    {
        points.push_back(qxvariantconversions::pointArrayToVariant(mNewEdgePoints[pts]));
    }
    customData["newEdgePoints"] = points;

    QVariantList edgeLabels;
    for (int e = 0; e < mNewEdgeLabels.size(); ++e)
    {
        edgeLabels.push_back(qxvariantconversions::intArrayToVariant(mNewEdgeLabels[e]));
    }
    customData["newEdgeLabels"] = edgeLabels;

    customData["selectionToDelete"] = mSelToDelete.serialize();

    if (mConnectLabels.size() > 0)
    {
        customData["connectLabels"] = qxvariantconversions::intArrayToVariant(mConnectLabels);
    }

    customData["oldPos"] = qxvariantconversions::pointToVariant(mOldPos);
    customData["geoId"] = mOldGeo;
    customData["newPos"] = qxvariantconversions::pointToVariant(mNewPos);
    customData["timeId"] = mTimeId;

    OperationLogEntry entry;
    entry.m_operationName = QString("MoveBaseOperationSet");
    entry.m_highlightedSelection = mSelection;
    entry.m_visibleSelection = mVisibleSelection;
    entry.m_customData = customData;

    return entry;
}

void
MoveBaseOperationSet::exec()
{
    // Check requirements
    if (!FilopodiaFunctions::isFilopodiaGraph(graph))
    {
        throw McException("MoveBaseOperationSet: graph is not filopodia graph.");
    }
    if (mSelection.getNumSelectedEdges() + mSelection.getNumSelectedVertices() + mSelection.getNumSelectedPoints() != 1)
    {
        throw McException("MoveBaseOperationSet: invalid selection");
    }
    if (mSelection.getNumSelectedEdges() != 0 || (mSelection.getNumSelectedVertices() != 1 && mSelection.getNumSelectedPoints() != 1))
    {
        throw McException("MoveBaseOperationSet: invalid selection");
    }
    if (mSelection.getNumSelectedPoints() == 1)
    {
        SpatialGraphPoint point = mSelection.getSelectedPoint(0);
        if (point.pointNum == 0 || point.pointNum == graph->getNumEdgePoints(point.edgeNum) - 1)
        {
            throw McException("MoveBaseOperationSet: selected point is not convertable");
        }
    }
    if (mNewNodeLabels.size() != mNewNodes.size())
    {
        throw McException("MoveBaseOperationSet: invalid number of node types");
    }
    if (mNewEdgeTargets.size() != mNewEdgeSources.size())
    {
        throw McException("MoveBaseOperationSet: invalid number of edge targets");
    }
    if (mNewEdgePoints.size() != mNewEdgeSources.size())
    {
        throw McException("MoveBaseOperationSet: invalid number of edge point arrays");
    }
    if (mNewEdgePoints.size() != mNewEdgeLabels.size())
    {
        throw McException("MoveBaseOperationSet: invalid number of edge labels");
    }
    for (int e = 0; e < mNewEdgeSources.size(); ++e)
    {
        if (mNewEdgeSources[e] == mNewEdgeTargets[e])
        {
            throw McException("MoveBaseOperationSet: edge source and edge target must differ");
        }
    }

    // Get infos for log entry
    if (mSelection.getNumSelectedVertices() == 1)
    {
        mTimeId = FilopodiaFunctions::getTimeIdOfNode(graph, mSelection.getSelectedVertex(0));
    }
    else if (mSelection.getNumSelectedPoints() == 1)
    {
        mTimeId = FilopodiaFunctions::getTimeIdOfEdge(graph, mSelection.getSelectedPoint(0).edgeNum);
    }

    SpatialGraphSelection oldBaseSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, mSelToDelete, BASE_NODE);
    if (oldBaseSel.getNumSelectedVertices() == 1)
    {
        const int oldBase = oldBaseSel.getSelectedVertex(0);
        mOldPos = graph->getVertexCoords(oldBase);
        mOldGeo = FilopodiaFunctions::getGcIdOfNode(graph, oldBase);
    }

    MergeFilopodiaOperationSet* mergeFiloOp = new MergeFilopodiaOperationSet(graph,
                                                                             mSelection,
                                                                             mVisibleSelection,
                                                                             mNewNodes,
                                                                             mNewNodeLabels,
                                                                             mNewEdgeSources,
                                                                             mNewEdgeTargets,
                                                                             mNewEdgePoints,
                                                                             mNewEdgeLabels,
                                                                             mConnectLabels);
    mergeFiloOp->exec();
    operations.push_back(mergeFiloOp);
    SpatialGraphSelection deleteSel = mergeFiloOp->getSelAfterOperation(mSelToDelete);

    mNewNodesAndEdges = mergeFiloOp->getNewNodesAndEdges();
    SpatialGraphSelection newBaseSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, mNewNodesAndEdges, BASE_NODE);
    if (newBaseSel.getNumSelectedVertices() == 1)
    {
        const int newBase = newBaseSel.getSelectedVertex(0);
        mNewPos = graph->getVertexCoords(newBase);
    }
    else
    {
        qDebug() << "MoveBaseOperationSet: new edges and nodes does not contain new base" << newBaseSel.getNumSelectedVertices();
    }

    mNewConnection = mergeFiloOp->getConnectNode();
    SpatialGraphSelection newNodeSel(graph);
    newNodeSel.selectVertex(mNewConnection);

    DeleteFilopodiaOperationSet* deleteOp = new DeleteFilopodiaOperationSet(graph, deleteSel, mergeFiloOp->getVisibleSelectionAfterOperation());
    deleteOp->exec();
    operations.push_back(deleteOp);
    mNewNodesAndEdges = deleteOp->getSelectionAfterOperation(mNewNodesAndEdges);

    newNodeSel = deleteOp->getSelectionAfterOperation(newNodeSel);
    mNewConnection = newNodeSel.getSelectedVertex(0);
}

SpatialGraphSelection
MoveBaseOperationSet::getNewNodesAndEdges() const
{
    return mNewNodesAndEdges;
}

int
MoveBaseOperationSet::getConnectNode() const
{
    return mNewConnection;
}

//////////////////////////////////////////////////////////////////////////////////////////////
MoveBaseAlongEdgeOperationSet::MoveBaseAlongEdgeOperationSet(HxSpatialGraph* sg,
                                                             const SpatialGraphSelection& selectedElements,
                                                             const SpatialGraphSelection& visibleElements,
                                                             McDArray<int> labels)
    : OperationSet(sg, selectedElements, visibleElements)
    , mLabels(labels)
    , mTimeId(-1)
    , mOldPos()
    , mOldGeo(-1)
    , mNewBase(-1)
    , mNewPos()
{
    mcenter("MoveBaseAlongEdgeOperationSet::MoveBaseAlongEdgeOperationSet");
}

MoveBaseAlongEdgeOperationSet::MoveBaseAlongEdgeOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry)
    : OperationSet(sg, logEntry.m_highlightedSelection, logEntry.m_visibleSelection)
{
    if (logEntry.m_customData.contains("newNodeLabels"))
    {
        mLabels = qxvariantconversions::variantToIntArray(logEntry.m_customData["newNodeLabels"]);
    }
    else
    {
        throw McException(QString("Invalid MoveBaseAlongEdgeOperationSet log entry: no \"newNodeLabels\" field."));
    }

    mTimeId = logEntry.m_customData["timeId"].toInt();
}

MoveBaseAlongEdgeOperationSet::~MoveBaseAlongEdgeOperationSet()
{
    mcenter("MoveBaseAlongEdgeOperationSet::~MoveBaseAlongEdgeOperationSet");
}

OperationLogEntry
MoveBaseAlongEdgeOperationSet::getLogEntry() const
{
    QVariantMap customData;

    customData["newNodeLabels"] = qxvariantconversions::intArrayToVariant(mLabels);
    customData["oldPos"] = qxvariantconversions::pointToVariant(mOldPos);
    customData["newPos"] = qxvariantconversions::pointToVariant(mNewPos);
    customData["timeId"] = mTimeId;
    customData["geoId"] = mOldGeo;

    OperationLogEntry entry;
    entry.m_operationName = QString("MoveBaseAlongEdgeOperationSet");
    entry.m_highlightedSelection = mSelection;
    entry.m_visibleSelection = mVisibleSelection;
    entry.m_customData = customData;

    return entry;
}

void
MoveBaseAlongEdgeOperationSet::exec()
{
    if (!FilopodiaFunctions::isFilopodiaGraph(graph))
    {
        throw McException("MoveBaseAlongEdgeOperationSet: graph is not filopodia graph.");
    }
    if (!(mSelection.getNumSelectedEdges() == 0 && mSelection.getNumSelectedPoints() == 1 && (mSelection.getNumSelectedVertices() == 1 || mSelection.getNumSelectedVertices() == 0)))
    {
        throw McException("MoveBaseAlongEdgeOperationSet: wrong selection.");
    }

    mTimeId = FilopodiaFunctions::getTimeIdOfEdge(graph, mSelection.getSelectedPoint(0).edgeNum);
    SpatialGraphSelection pointSel(graph);
    pointSel.selectPoint(mSelection.getSelectedPoint(0));

    SpatialGraphSelection oldBaseSel(graph);
    if (mSelection.getNumSelectedVertices() == 1)
    {
        const int oldBase = mSelection.getSelectedVertex(0);
        oldBaseSel.selectVertex(oldBase);
    }

    // Convert selected edge point to node
    ConvertFilopodiaPointOperationSet* convertOp = new ConvertFilopodiaPointOperationSet(graph, pointSel, SpatialGraphSelection(graph), mLabels);
    convertOp->exec();
    operations.push_back(convertOp);
    oldBaseSel = convertOp->getSelectionAfterOperation(oldBaseSel);
    mNewBase = convertOp->getNewVertexNum();
    mNewPos = graph->getVertexCoords(mNewBase);

    if (mSelection.getNumSelectedVertices() == 1)
    {
        SpatialGraphSelection newNodeSel(graph);
        newNodeSel.selectVertex(mNewBase);

        const int oldNode = mSelection.getSelectedVertex(0);
        mOldPos = graph->getVertexCoords(oldNode);
        mOldGeo = FilopodiaFunctions::getGeometryIdOfNode(graph, oldNode);
        if (mOldGeo != FilopodiaFunctions::getManualGeometryLabelId(graph, MANUAL))
        {
            mOldGeo = FilopodiaFunctions::getManualGeometryLabelId(graph, AUTOMATIC);
        }

        // Convert selected base to edge point
        VertexToPointOperation* v2pOperation = new VertexToPointOperation(graph, oldBaseSel, SpatialGraphSelection(graph));
        v2pOperation->exec();
        operations.push_back(v2pOperation);

        newNodeSel = v2pOperation->getSelectionAfterOperation(newNodeSel);
        mNewBase = newNodeSel.getSelectedVertex(0);
    }
}

int
MoveBaseAlongEdgeOperationSet::getNewBase() const
{
    return mNewBase;
}

//////////////////////////////////////////////////////////////////////////////////////////////
ConnectOperationSet::ConnectOperationSet(HxSpatialGraph* sg,
                                         const SpatialGraphSelection& selectedElements,
                                         const SpatialGraphSelection& visibleElements,
                                         const std::vector<McVec3f>& newNodes,
                                         const std::vector<McDArray<int> >& newNodeLabels,
                                         const McDArray<int>& newEdgeSources,
                                         const McDArray<int>& newEdgeTargets,
                                         const std::vector<std::vector<McVec3f> >& newEdgePoints,
                                         const std::vector<McDArray<int> >& newEdgeLabels,
                                         const McDArray<int>& connectLabels,
                                         const SpatialGraphSelection& deleteSel)
    : OperationSet(sg, selectedElements, visibleElements)
    , mSelToDelete(deleteSel)
    , mNewNodes(newNodes)
    , mNewNodeLabels(newNodeLabels)
    , mNewEdgeSources(newEdgeSources)
    , mNewEdgeTargets(newEdgeTargets)
    , mNewEdgePoints(newEdgePoints)
    , mNewEdgeLabels(newEdgeLabels)
    , mConnectLabels(connectLabels)
    , mTimeId(-1)
{
    mcenter("ConnectOperationSet::ConnectOperationSet");
}

ConnectOperationSet::ConnectOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry)
    : OperationSet(sg, logEntry.m_highlightedSelection, logEntry.m_visibleSelection)
{
    if (logEntry.m_customData.contains("newNodes"))
    {
        mNewNodes = qxvariantconversions::variantToPointArray(logEntry.m_customData["newNodes"]);
    }
    else
    {
        throw McException(QString("Invalid ConnectOperationSet log entry: no \"newNodes\" field."));
    }

    if (logEntry.m_customData.contains("newNodeLabels"))
    {
        const QVariantList nodeLabelList = logEntry.m_customData["newNodeLabels"].toList();
        for (int i = 0; i < nodeLabelList.size(); ++i)
        {
            mNewNodeLabels.push_back(qxvariantconversions::variantToIntArray(nodeLabelList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid ConnectOperationSet log entry: no \"newNodeLabels\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeSources"))
    {
        mNewEdgeSources = qxvariantconversions::variantToIntArray(logEntry.m_customData["newEdgeSources"]);
    }
    else
    {
        throw McException(QString("Invalid ConnectOperationSet log entry: no \"newEdgeSources\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeTargets"))
    {
        mNewEdgeTargets = qxvariantconversions::variantToIntArray(logEntry.m_customData["newEdgeTargets"]);
    }
    else
    {
        throw McException(QString("Invalid ConnectOperationSet log entry: no \"newEdgeTargets\" field."));
    }

    if (logEntry.m_customData.contains("newEdgePoints"))
    {
        const QVariantList pointsList = logEntry.m_customData["newEdgePoints"].toList();
        for (int i = 0; i < pointsList.size(); ++i)
        {
            mNewEdgePoints.push_back(qxvariantconversions::variantToPointArray(pointsList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid ConnectOperationSet log entry: no \"newEdgePoints\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeLabels"))
    {
        const QVariantList edgeLabelList = logEntry.m_customData["newEdgeLabels"].toList();
        for (int i = 0; i < edgeLabelList.size(); ++i)
        {
            mNewEdgeLabels.push_back(qxvariantconversions::variantToIntArray(edgeLabelList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid ConnectOperationSet log entry: no \"newEdgeLabels\" field."));
    }

    if (logEntry.m_customData.contains("selectionToDelete"))
    {
        mSelToDelete.deserialize(logEntry.m_customData["selectionToDelete"].toByteArray());
    }
    else
    {
        throw McException(QString("Invalid ConnectOperationSet log entry: no \"selectionToDelete\" field."));
    }

    mTimeId = logEntry.m_customData["timeId"].toInt();
}

ConnectOperationSet::~ConnectOperationSet()
{
    mcenter("ConnectOperationSet::~ConnectOperationSet");
}

OperationLogEntry
ConnectOperationSet::getLogEntry() const
{
    QVariantMap customData;
    customData["newNodes"] = qxvariantconversions::pointArrayToVariant(mNewNodes);

    QVariantList nodeLabels;
    for (int n = 0; n < mNewNodeLabels.size(); ++n)
    {
        nodeLabels.push_back(qxvariantconversions::intArrayToVariant(mNewNodeLabels[n]));
    }
    customData["newNodeLabels"] = nodeLabels;

    customData["newEdgeSources"] = qxvariantconversions::intArrayToVariant(mNewEdgeSources);
    customData["newEdgeTargets"] = qxvariantconversions::intArrayToVariant(mNewEdgeTargets);

    QVariantList points;
    for (int pts = 0; pts < mNewEdgePoints.size(); ++pts)
    {
        points.push_back(qxvariantconversions::pointArrayToVariant(mNewEdgePoints[pts]));
    }
    customData["newEdgePoints"] = points;

    QVariantList edgeLabels;
    for (int e = 0; e < mNewEdgeLabels.size(); ++e)
    {
        edgeLabels.push_back(qxvariantconversions::intArrayToVariant(mNewEdgeLabels[e]));
    }
    customData["newEdgeLabels"] = edgeLabels;

    customData["selectionToDelete"] = mSelToDelete.serialize();
    customData["timeId"] = mTimeId;

    OperationLogEntry entry;
    entry.m_operationName = QString("ConnectOperationSet");
    entry.m_highlightedSelection = mSelection;
    entry.m_visibleSelection = mVisibleSelection;
    entry.m_customData = customData;

    return entry;
}

void
ConnectOperationSet::exec()
{
    // Check requirements
    if (!FilopodiaFunctions::isFilopodiaGraph(graph))
    {
        throw McException("ConnectOperationSet: graph is not filopodia graph.");
    }
    if (mSelection.getNumSelectedEdges() + mSelection.getNumSelectedVertices() + mSelection.getNumSelectedPoints() != 1)
    {
        throw McException("ConnectOperationSet: invalid selection");
    }
    if (mSelection.getNumSelectedEdges() != 0 || mSelection.getNumSelectedVertices() != 0 || mSelection.getNumSelectedPoints() != 1)
    {
        throw McException("ConnectOperationSet: invalid selection");
    }
    if (mNewNodeLabels.size() != mNewNodes.size())
    {
        throw McException("ConnectOperationSet: invalid number of node types");
    }
    if (mNewEdgeTargets.size() != mNewEdgeSources.size())
    {
        throw McException("ConnectOperationSet: invalid number of edge targets");
    }
    if (mNewEdgePoints.size() != mNewEdgeSources.size())
    {
        throw McException("ConnectOperationSet: invalid number of edge point arrays");
    }
    if (mNewEdgePoints.size() != mNewEdgeLabels.size())
    {
        throw McException("ConnectOperationSet: invalid number of edge labels");
    }
    for (int e = 0; e < mNewEdgeSources.size(); ++e)
    {
        if (mNewEdgeSources[e] == mNewEdgeTargets[e])
        {
            throw McException("ConnectOperationSet: edge source and edge target must differ");
        }
    }

    mTimeId = FilopodiaFunctions::getTimeIdOfEdge(graph, mSelection.getSelectedPoint(0).edgeNum);

    MergeFilopodiaOperationSet* mergeFiloOp = new MergeFilopodiaOperationSet(graph,
                                                                             mSelection,
                                                                             mVisibleSelection,
                                                                             mNewNodes,
                                                                             mNewNodeLabels,
                                                                             mNewEdgeSources,
                                                                             mNewEdgeTargets,
                                                                             mNewEdgePoints,
                                                                             mNewEdgeLabels,
                                                                             mConnectLabels);
    mergeFiloOp->exec();
    operations.push_back(mergeFiloOp);
    SpatialGraphSelection deleteSel = mergeFiloOp->getSelAfterOperation(mSelToDelete);

    const int connectNode = mergeFiloOp->getConnectNode();
    SpatialGraphSelection connectSel(graph);
    connectSel.selectVertex(connectNode);

    DeleteFilopodiaOperationSet* deleteOp = new DeleteFilopodiaOperationSet(graph, deleteSel, mergeFiloOp->getVisibleSelectionAfterOperation());
    deleteOp->exec();
    operations.push_back(deleteOp);
}

//////////////////////////////////////////////////////////////////////////////////////////////
ReplaceFilopodiaOperationSet::ReplaceFilopodiaOperationSet(HxSpatialGraph* sg,
                                                           const SpatialGraphSelection& selectedElements,
                                                           const SpatialGraphSelection& visibleElements,
                                                           const std::vector<McVec3f>& newNodes,
                                                           const std::vector<McDArray<int> >& newNodeLabels,
                                                           const McDArray<int>& matchedPreviousNodes,
                                                           const McDArray<int>& newEdgeSources,
                                                           const McDArray<int>& newEdgeTargets,
                                                           const std::vector<std::vector<McVec3f> >& newEdgePoints,
                                                           const std::vector<McDArray<int> >& newEdgeLabels,
                                                           const SpatialGraphSelection& successorSel)
    : OperationSet(sg, selectedElements, visibleElements)
    , mNewNodes(newNodes)
    , mNewNodeLabels(newNodeLabels)
    , mMatchedPreviousNodes(matchedPreviousNodes)
    , mNewEdgeSources(newEdgeSources)
    , mNewEdgeTargets(newEdgeTargets)
    , mNewEdgePoints(newEdgePoints)
    , mNewEdgeLabels(newEdgeLabels)
    , mSuccessorSel(successorSel)
    , mTimeId(-1)
    , mNewNodesAndEdges(graph)
{
    mcenter("ReplaceFilopodiaOperationSet::ReplaceFilopodiaOperationSet");
}

ReplaceFilopodiaOperationSet::ReplaceFilopodiaOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry)
    : OperationSet(sg, logEntry.m_highlightedSelection, logEntry.m_visibleSelection)
{
    if (logEntry.m_customData.contains("newNodes"))
    {
        mNewNodes = qxvariantconversions::variantToPointArray(logEntry.m_customData["newNodes"]);
    }
    else
    {
        throw McException(QString("Invalid ReplaceFilopodiaOperationSet log entry: no \"newNodes\" field."));
    }

    if (logEntry.m_customData.contains("newNodeLabels"))
    {
        const QVariantList nodeLabelList = logEntry.m_customData["newNodeLabels"].toList();
        for (int i = 0; i < nodeLabelList.size(); ++i)
        {
            mNewNodeLabels.push_back(qxvariantconversions::variantToIntArray(nodeLabelList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid ReplaceFilopodiaOperationSet log entry: no \"newNodeLabels\" field."));
    }

    if (logEntry.m_customData.contains("matchedPreviousNodes"))
    {
        mMatchedPreviousNodes = qxvariantconversions::variantToIntArray(logEntry.m_customData["matchedPreviousNodes"]);
    }
    else
    {
        throw McException(QString("Invalid ReplaceFilopodiaOperationSet log entry: no \"matchedPreviousNodes\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeSources"))
    {
        mNewEdgeSources = qxvariantconversions::variantToIntArray(logEntry.m_customData["newEdgeSources"]);
    }
    else
    {
        throw McException(QString("Invalid ReplaceFilopodiaOperationSet log entry: no \"newEdgeSources\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeTargets"))
    {
        mNewEdgeTargets = qxvariantconversions::variantToIntArray(logEntry.m_customData["newEdgeTargets"]);
    }
    else
    {
        throw McException(QString("Invalid ReplaceFilopodiaOperationSet log entry: no \"newEdgeTargets\" field."));
    }

    if (logEntry.m_customData.contains("newEdgePoints"))
    {
        const QVariantList pointsList = logEntry.m_customData["newEdgePoints"].toList();
        for (int i = 0; i < pointsList.size(); ++i)
        {
            mNewEdgePoints.push_back(qxvariantconversions::variantToPointArray(pointsList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid ReplaceFilopodiaOperationSet log entry: no \"newEdgePoints\" field."));
    }

    if (logEntry.m_customData.contains("newEdgeLabels"))
    {
        const QVariantList edgeLabelList = logEntry.m_customData["newEdgeLabels"].toList();
        for (int i = 0; i < edgeLabelList.size(); ++i)
        {
            mNewEdgeLabels.push_back(qxvariantconversions::variantToIntArray(edgeLabelList[i]));
        }
    }
    else
    {
        throw McException(QString("Invalid ReplaceFilopodiaOperationSet log entry: no \"newEdgeLabels\" field."));
    }

    if (logEntry.m_customData.contains("selectionToDelete"))
    {
        mSuccessorSel.deserialize(logEntry.m_customData["selectionToDelete"].toByteArray());
    }
    else
    {
        throw McException(QString("Invalid ReplaceFilopodiaOperationSet log entry: no \"selectionToDelete\" field."));
    }

    mTimeId = logEntry.m_customData["timeId"].toInt();
}

ReplaceFilopodiaOperationSet::~ReplaceFilopodiaOperationSet()
{
    mcenter("ReplaceFilopodiaOperationSet::~ReplaceFilopodiaOperationSet");
}

OperationLogEntry
ReplaceFilopodiaOperationSet::getLogEntry() const
{
    QVariantMap customData;
    customData["newNodes"] = qxvariantconversions::pointArrayToVariant(mNewNodes);

    QVariantList nodeLabels;
    for (int n = 0; n < mNewNodeLabels.size(); ++n)
    {
        nodeLabels.push_back(qxvariantconversions::intArrayToVariant(mNewNodeLabels[n]));
    }
    customData["newNodeLabels"] = nodeLabels;

    customData["matchedPreviousNodes"] = qxvariantconversions::intArrayToVariant(mMatchedPreviousNodes);
    customData["newEdgeSources"] = qxvariantconversions::intArrayToVariant(mNewEdgeSources);
    customData["newEdgeTargets"] = qxvariantconversions::intArrayToVariant(mNewEdgeTargets);

    QVariantList points;
    for (int pts = 0; pts < mNewEdgePoints.size(); ++pts)
    {
        points.push_back(qxvariantconversions::pointArrayToVariant(mNewEdgePoints[pts]));
    }
    customData["newEdgePoints"] = points;

    QVariantList edgeLabels;
    for (int e = 0; e < mNewEdgeLabels.size(); ++e)
    {
        edgeLabels.push_back(qxvariantconversions::intArrayToVariant(mNewEdgeLabels[e]));
    }
    customData["newEdgeLabels"] = edgeLabels;

    customData["selectionToDelete"] = mSuccessorSel.serialize();
    customData["timeId"] = mTimeId;

    OperationLogEntry entry;
    entry.m_operationName = QString("ReplaceFilopodiaOperationSet");
    entry.m_highlightedSelection = mSelection;
    entry.m_visibleSelection = mVisibleSelection;
    entry.m_customData = customData;

    return entry;
}

void
ReplaceFilopodiaOperationSet::exec()
{
    // Check requirements
    if (!FilopodiaFunctions::isFilopodiaGraph(graph))
    {
        throw McException("ReplaceFilopodiaOperationSet: graph is not filopodia graph.");
    }
    if (!(mSelection.getNumSelectedEdges() == 0 && mSelection.getNumSelectedVertices() == 1 && mSelection.getNumSelectedPoints() == 0))
    {
        throw McException("ReplaceFilopodiaOperationSet: invalid selection");
    }
    if (mNewNodeLabels.size() != mNewNodes.size())
    {
        throw McException("ReplaceFilopodiaOperationSet: invalid number of node types");
    }
    if (mNewEdgeTargets.size() != mNewEdgeSources.size())
    {
        throw McException("ReplaceFilopodiaOperationSet: invalid number of edge targets");
    }
    if (mNewEdgePoints.size() != mNewEdgeSources.size())
    {
        throw McException("ReplaceFilopodiaOperationSet: invalid number of edge point arrays");
    }
    if (mNewEdgePoints.size() != mNewEdgeLabels.size())
    {
        throw McException("ReplaceFilopodiaOperationSet: invalid number of edge labels");
    }
    for (int e = 0; e < mNewEdgeSources.size(); ++e)
    {
        if (mNewEdgeSources[e] == mNewEdgeTargets[e])
        {
            throw McException("ReplaceFilopodiaOperationSet: edge source and edge target must differ");
        }
    }

    mTimeId = FilopodiaFunctions::getTimeIdOfNode(graph, mSelection.getSelectedVertex(0));

    MergeFilopodiaOperationSet* mergeFiloOp = new MergeFilopodiaOperationSet(graph,
                                                                             mSelection,
                                                                             mVisibleSelection,
                                                                             mNewNodes,
                                                                             mNewNodeLabels,
                                                                             mNewEdgeSources,
                                                                             mNewEdgeTargets,
                                                                             mNewEdgePoints,
                                                                             mNewEdgeLabels);
    mergeFiloOp->exec();
    operations.push_back(mergeFiloOp);
    SpatialGraphSelection deleteSel = mergeFiloOp->getSelAfterOperation(mSuccessorSel);
    mNewNodesAndEdges = mergeFiloOp->getNewNodesAndEdges();

    SpatialGraphSelection newNodesAndEdges = mergeFiloOp->getNewNodesAndEdges();
    SpatialGraphSelection newBases = FilopodiaFunctions::getNodesOfTypeInSelection(graph, newNodesAndEdges, BASE_NODE);
    if (mMatchedPreviousNodes.size() != newBases.getNumSelectedVertices())
    {
        throw McException("ReplaceFilopodiaOperationSet: invalid number of previous bases");
    }

    for (int i = 0; i < newBases.getNumSelectedVertices(); ++i)
    {
        const int v = newBases.getSelectedVertex(i);
        const int prior = mMatchedPreviousNodes[i];
        const int matchId = FilopodiaFunctions::getMatchIdOfNode(graph, v);

        if (matchId == FilopodiaFunctions::getMatchLabelId(graph, UNASSIGNED))
        {
            // If match Id is unassigned find next available match Id
            // and assign previous base and new base
            SpatialGraphSelection labelSel(graph);
            labelSel.selectVertex(prior);
            labelSel.selectVertex(v);
            int matchLabel = FilopodiaFunctions::getNextAvailableManualNodeMatchId(graph);

            AssignLabelOperation* assOp = new AssignLabelOperation(graph, labelSel, SpatialGraphSelection(graph), QString::fromLatin1(FilopodiaFunctions::getManualNodeMatchAttributeName()), matchLabel);
            assOp->exec();
            operations.push_back(assOp);
        }
    }

    DeleteFilopodiaOperationSet* deleteOp = new DeleteFilopodiaOperationSet(graph, deleteSel, mergeFiloOp->getVisibleSelectionAfterOperation());
    deleteOp->exec();
    operations.push_back(deleteOp);
    mNewNodesAndEdges = deleteOp->getSelectionAfterOperation(mNewNodesAndEdges);
}

SpatialGraphSelection
ReplaceFilopodiaOperationSet::getNewNodesAndEdges() const
{
    return mNewNodesAndEdges;
}

void
FilopodiaGraphOperationSet_plugin_factory(HxSpatialGraph* graph, const OperationLogEntry& logEntry, Operation*& op)
{
    const QString operationName = logEntry.m_operationName;
    if (QString("ReplaceFilopodiaOperationSet") == operationName)
    {
        op = new ReplaceFilopodiaOperationSet(graph, logEntry);
    }
    else if (QString("DeleteFilopodiaOperationSet") == operationName)
    {
        op = new DeleteFilopodiaOperationSet(graph, logEntry);
    }
    else if (QString("AddFilopodiumOperationSet") == operationName)
    {
        op = new AddFilopodiumOperationSet(graph, logEntry);
    }
    else if (QString("ReplaceFilopodiaEdgeOperationSet") == operationName)
    {
        op = new ReplaceFilopodiaEdgeOperationSet(graph, logEntry);
    }
    else if (QString("MoveTipOperationSet") == operationName)
    {
        op = new MoveTipOperationSet(graph, logEntry);
    }
    else if (QString("MoveBaseAlongEdgeOperationSet") == operationName)
    {
        op = new MoveBaseAlongEdgeOperationSet(graph, logEntry);
    }
    else if (QString("MoveBaseOperationSet") == operationName)
    {
        op = new MoveBaseOperationSet(graph, logEntry);
    }
    else if (QString("MoveTipAlongEdgeOperationSet") == operationName)
    {
        op = new MoveTipAlongEdgeOperationSet(graph, logEntry);
    }
    else if (QString("ConnectOperationSet") == operationName)
    {
        op = new ConnectOperationSet(graph, logEntry);
    }
    else if (QString("AddAndAssignLabelOperationSet") == operationName)
    {
        op = new AddAndAssignLabelOperationSet(graph, logEntry);
    }
    else
    {
        op = 0;
    }
}
