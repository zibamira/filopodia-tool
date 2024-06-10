#include "ExFilopodiaTraceTool.h"
#include "FilopodiaOperationSet.h"
#include "FilopodiaFunctions.h"
#include "QxFilopodiaTool.h"
#include "VsFilopodiaPickThreshold.h"
#include <QDebug>
#include <hxneuroneditor/internal/HxNeuronEditorSubApp.h>
#include <hxneuroneditor/internal/NeuronEditorPickingThresholds.h>
#include <hxneuroneditor/internal/ExSpatialGraphPickFunctions.h>
#include <hxneuroneditor/internal/HxMPRViewer.h>
#include <vsvolren/internal/VsSlice.h>
#include <vsvolren/internal/VsData.h>
#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <hxspatialgraph/internal/SpatialGraphOperationSet.h>
#include <hxvisageviewer/internal/ExBaseViewer.h>
#include <hxvisageviewer/internal/ExCoordUtility.h>
#include <hxfield/HxUniformScalarField3.h>
#include <mclib/McLine.h>
#include <mclib/McException.h>
#include <QTime>
#include <QSharedPointer>
#include <hxcore/HxController.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/events/SoMouseButtonEvent.h>

#include "ConvertVectorAndMcDArray.h"

////////////////////////////////////////////////////////////////////////////////
ExFilopodiaTraceTool::ExFilopodiaTraceTool(QxFilopodiaTool* filopodiaTool, int surroundWidthInVoxels)
    : mEditor(0)
    , mToolbox(filopodiaTool)
    , mSurroundWidthInVoxels(surroundWidthInVoxels)
    , mSnapToGraph(1)
{
    mEditor = 0;
    mExclusive = true;
}

////////////////////////////////////////////////////////////////////////////////
bool ExFilopodiaTraceTool::supports(Ex::ViewerType) const
{
    return true;
}

////////////////////////////////////////////////////////////////////////////////
bool
ExFilopodiaTraceTool::pick(ExBaseViewer*, int, int, float&)
{
    return false;
}

HxUniformScalarField3*
ExFilopodiaTraceTool::getImageOrThrow() const
{
    HxUniformScalarField3* image = dynamic_cast<HxUniformScalarField3*>(mEditor->getImageData());
    if (!image)
    {
        throw McException(QString("No image"));
    }
    return image;
}

////////////////////////////////////////////////////////////////////////////////
bool
ExFilopodiaTraceTool::processMouseEvent(ExBaseViewer* viewer,
                                        Vs::MouseEvent event,
                                        int x,
                                        int y,
                                        Vs::ButtonState state,
                                        Ex::CursorShape& outCursor)
{
    QTime traceStartTime = QTime::currentTime();

    VsSlice* slice = viewer->slice();
    if (!slice)
        return false;

    VsVolume* vol = slice->volume(0);
    if (!vol)
        return false;

    const bool mouseMove = (event == Vs::ME_Move);
    const bool mouseClick = (event == Vs::ME_LeftButtonDown);
    const bool leftButtonDown = (int(state) == Vs::BS_LeftButton);
    const bool leftButtonDownAndShift = (int(state) == (0 | Vs::BS_ShiftButton | Vs::BS_LeftButton));
    const bool leftButtonDownAndCtrl = (int(state) == (0 | Vs::BS_ControlButton | Vs::BS_LeftButton));

    if (!mouseMove && !(mouseClick && (leftButtonDown || leftButtonDownAndShift || leftButtonDownAndCtrl)))
    {
        return false;
    }
    SpatialGraphSelection sel = mEditor->getHighlightedElements();

    const int numSelVertices = sel.getNumSelectedVertices();
    const int numSelPoints = sel.getNumSelectedPoints();
    const int numSelEdges = sel.getNumSelectedEdges();

    if (numSelEdges > 1 || numSelVertices > 1 || numSelPoints > 1)
    {
        return false;
    }

    const bool appending = (numSelVertices + numSelPoints + numSelEdges == 1);

    PickedItem pickedItem;

    int nodeIdx = -1;
    int edgeIdx = -1;
    McVec3f pickedPoint;
    SpatialGraphPoint edgePoint;
    SpatialGraphPoint edgePointBefore;
    const McVec2i pickedScreenCoords(x, y);

    if (mSnapToGraph && nodePicked(viewer, pickedScreenCoords, pickedPoint, nodeIdx))
    {
        pickedItem = NODE;
    }
    else if (mSnapToGraph && pointPicked(viewer, pickedScreenCoords, pickedPoint, edgePoint))
    {
        pickedItem = POINT;
    }
    else if (mSnapToGraph && edgePicked(viewer, pickedScreenCoords, pickedPoint, edgePointBefore, edgeIdx))
    {
        pickedItem = SEGMENT;
    }
    else if (validIntensityPicked(viewer, slice, pickedScreenCoords, pickedPoint))
    {
        pickedItem = VALID_INTENSITY;
    }
    else
    {
        pickedItem = INVALID_INTENSITY;
    }

    if (pickedItem == INVALID_INTENSITY)
    {
        return false;
    }

    outCursor = getCursor(pickedItem, appending);

    if (event != Vs::ME_LeftButtonDown)
    {
        return false;
    }

    HxSpatialGraph* graph = mEditor->getSpatialGraph();

    if (!graph)
    {
        mEditor->createNewSpatialGraph();
        graph = mEditor->getSpatialGraph();
    }

    if (!FilopodiaFunctions::isFilopodiaGraph(graph))
    {
        return false;
    }

    const float intensityWeight = mToolbox->getIntensityWeight();
    const float topBrightness = mToolbox->getTopBrightness();

    if ((numSelVertices + numSelPoints + numSelEdges) == 0)
    {
        if (pickedItem == POINT)
        {
            sel.clear();
            sel.selectPoint(edgePoint);
            mEditor->setHighlightedElements(sel);
        }
        else if (pickedItem == NODE)
        {
            sel.clear();
            sel.selectVertex(nodeIdx);
            mEditor->setHighlightedElements(sel);
        }
        else if (pickedItem == SEGMENT)
        {
            sel.clear();
            sel.selectEdge(edgeIdx);
            mEditor->setHighlightedElements(sel);
        }
        else
        {
            qDebug() << "Tracing tool: \n\tAdd filopodium.";
            theMsg->printf("Tracing tool: \tAdd filopodium.");

            if (!graph)
            {
                return false;
            }

            HxUniformScalarField3* image = getImageOrThrow();
            HxUniformScalarField3* priorMap = mToolbox->getDijkstraMap();

            try
            {
                AddFilopodiumOperationSet* op = FilopodiaFunctions::addFilopodium(graph,
                                                                                  pickedPoint,
                                                                                  mToolbox->getCurrentTime(),
                                                                                  image,
                                                                                  priorMap);
                if (op != 0)
                {
                    mEditor->execNewOp(op);
                }
            }
            catch (McException& e)
            {
                theMsg->printf(QString("%1").arg(e.what()));
                HxMessage::error(QString("Cannot add Filopodium in 2D: \n%1").arg(e.what()), "ok");
            }
        }
    }

    else if ((numSelVertices + numSelPoints + numSelEdges) == 1)
    {
        PickedItem selectedItem;
        if (sel.getNumSelectedVertices() == 1)
        {
            selectedItem = NODE;
        }
        else if (sel.getNumSelectedPoints() == 1)
        {
            selectedItem = POINT;
        }
        else
        {
            selectedItem = SEGMENT;
        }

        if (selectedItem == NODE && pickedItem == NODE)
        {
            if (sel.getSelectedVertex(0) == nodeIdx)
            {
                qDebug() << "Tracing tool: \n\tNode/Node: clear picked node";
                sel.deselectVertex(nodeIdx);
                mEditor->setHighlightedElements(sel);
            }
            else
            {
                qDebug() << "Tracing tool: \n\tNode/Node: not implemented";
            }
        }
        else if (selectedItem == NODE && pickedItem == POINT)
        {
            sel.selectPoint(edgePoint);

            if (leftButtonDownAndCtrl)
            {
                qDebug() << "Tracing tool: \n\tConnect tip with edge point.";
                theMsg->printf("Tracing tool: \tConnect tip with edge point.");

                HxUniformScalarField3* image = getImageOrThrow();

                try
                {
                    ConnectOperationSet* connectNodeOp = FilopodiaFunctions::connectNodes(graph, sel, image, intensityWeight, topBrightness);
                    if (connectNodeOp != 0)
                    {
                        mEditor->execNewOp(connectNodeOp);
                    }
                }
                catch (McException& e)
                {
                    theMsg->printf(QString("%1").arg(e.what()));
                    HxMessage::error(QString("Cannot connect nodes: \n%1").arg(e.what()), "ok");
                }
            }
            else
            {
                const int selectedNode = sel.getSelectedVertex(0);
                SpatialGraphPoint selectedPoint = sel.getSelectedPoint(0);

                if (FilopodiaFunctions::hasNodeType(graph, TIP_NODE, selectedNode))
                {
                    IncidenceList incidentEdges = graph->getIncidentEdges(selectedNode);
                    if (incidentEdges.size() == 1)
                    {
                        const int incidentEdge = incidentEdges[0];
                        if (incidentEdge == selectedPoint.edgeNum)
                        {
                            qDebug() << "Tracing tool: \n\tMove tip along edge.";
                            theMsg->printf("Tracing tool: \tMove tip along edge.");

                            // set required labels
                            const int timeId = FilopodiaFunctions::getTimeIdOfNode(graph, selectedNode);
                            const int filoId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, selectedNode);
                            const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, FilopodiaFunctions::getTimeStepFromTimeId(graph, timeId));

                            McDArray<int> tipLabels;
                            tipLabels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
                            tipLabels.push_back(timeId);
                            tipLabels.push_back(FilopodiaFunctions::getTypeLabelId(graph, TIP_NODE));
                            tipLabels.push_back(FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM));
                            tipLabels.push_back(filoId);
                            tipLabels.push_back(FilopodiaFunctions::getMatchLabelId(graph, IGNORED));
                            tipLabels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, MANUAL));

                            try
                            {
                                MoveTipAlongEdgeOperationSet* moveNodeOp = new MoveTipAlongEdgeOperationSet(graph, sel, mEditor->getVisibleElements(), tipLabels);
                                if (moveNodeOp != 0)
                                {
                                    mEditor->execNewOp(moveNodeOp);
                                }
                            }
                            catch (McException& e)
                            {
                                theMsg->printf(QString("%1").arg(e.what()));
                                HxMessage::error(QString("Cannot move tip along edge: \n%1").arg(e.what()), "ok");
                            }
                        }
                        else
                        {
                            HxMessage::warning(QString("Could not move tip along edge: \nselected edge point must be located at incident edge of selected tip."), "ok");
                            return false;
                        }
                    }
                    else
                    {
                        HxMessage::warning(QString("Could not move tip along edge: \nselected tip has more than one incident edge."), "ok");
                        return false;
                    }
                }
                else if (FilopodiaFunctions::hasNodeType(graph, BASE_NODE, selectedNode))
                {
                    IncidenceList incidentEdges = graph->getIncidentEdges(selectedNode);
                    if (incidentEdges.size() == 2)
                    {
                        qDebug() << "Tracing tool: \n\tMove base along edge.";
                        theMsg->printf("Tracing tool: \tMove base along edge.");

                        // set required labels
                        const int timeId = FilopodiaFunctions::getTimeIdOfNode(graph, selectedNode);
                        const int filoId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, selectedNode);
                        const int matchId = FilopodiaFunctions::getMatchIdOfNode(graph, selectedNode);
                        const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, FilopodiaFunctions::getTimeStepFromTimeId(graph, timeId));

                        McDArray<int> baseLabels;
                        baseLabels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
                        baseLabels.push_back(timeId);
                        baseLabels.push_back(FilopodiaFunctions::getTypeLabelId(graph, BASE_NODE));
                        baseLabels.push_back(FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM));
                        baseLabels.push_back(filoId);
                        baseLabels.push_back(matchId);
                        baseLabels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, MANUAL));

                        try
                        {
                            MoveBaseAlongEdgeOperationSet* moveNodeOp = new MoveBaseAlongEdgeOperationSet(graph, sel, mEditor->getVisibleElements(), baseLabels);
                            if (moveNodeOp != 0)
                            {
                                mEditor->execNewOp(moveNodeOp);
                            }
                        }
                        catch (McException& e)
                        {
                            theMsg->printf(QString("%1").arg(e.what()));
                            HxMessage::error(QString("Cannot move base along edge: \n%1").arg(e.what()), "ok");
                        }
                    }
                    else
                    {
                        HxMessage::error(QString("Could not move base along edge: \nBase %1 has more than two incident edges.").arg(selectedNode), "ok");
                        return false;
                    }
                }
            }
        }
        else if (selectedItem == NODE && pickedItem == SEGMENT)
        {
            qDebug() << "Tracing tool: \n\tNode/Segment: not implemented";
            // insert node on edge, connect
        }
        else if (selectedItem == NODE && pickedItem == VALID_INTENSITY)
        {
            const int selectedNode = sel.getSelectedVertex(0);

            if (FilopodiaFunctions::hasNodeType(graph, TIP_NODE, selectedNode))
            {
                if (leftButtonDownAndShift)
                {
                    qDebug() << "Tracing tool: \n\tSet new base for selected tip.";
                    theMsg->printf("Tracing tool: \tSet new base for selected tip.");

                    HxUniformScalarField3* image = getImageOrThrow();

                    try
                    {
                        MoveBaseOperationSet* op = FilopodiaFunctions::moveBaseOfTip(graph, selectedNode, pickedPoint, image, intensityWeight, topBrightness);
                        if (op != 0)
                        {
                            mEditor->execNewOp(op);
                        }
                    }
                    catch (McException& e)
                    {
                        theMsg->printf(QString("%1").arg(e.what()));
                        HxMessage::error(QString("Cannot move base of tip: \n%1").arg(e.what()), "ok");
                    }
                }
                else
                {
                    qDebug() << "Tracing tool: \n\tMove tip.";
                    theMsg->printf("Tracing tool: \tMove tip.");

                    HxUniformScalarField3* image = getImageOrThrow();

                    try
                    {
                        MoveTipOperationSet* op = FilopodiaFunctions::moveTip(graph,
                                                                              selectedNode,
                                                                              pickedPoint,
                                                                              image,
                                                                              mSurroundWidthInVoxels,
                                                                              intensityWeight,
                                                                              topBrightness,
                                                                              true);

                        if (op != 0)
                        {
                            if (!mEditor->execNewOp(op))
                            {
                                delete op;
                                return false;
                            }
                        }
                    }
                    catch (McException& e)
                    {
                        theMsg->printf(QString("%1").arg(e.what()));
                        HxMessage::error(QString("Cannot move tip: \n%1").arg(e.what()), "ok");
                    }
                }
            }
            else if (FilopodiaFunctions::hasNodeType(graph, BASE_NODE, selectedNode))
            {
                qDebug() << "Tracing tool: \n\tMove base.";
                theMsg->printf("Tracing tool: \tMove base.");

                if ((graph->getVertexCoords(selectedNode) - pickedPoint).length() > 2.0)
                {
                    qDebug() << "\tNew base position is too far away. (" << (graph->getVertexCoords(selectedNode) - pickedPoint).length() << ") System concludes selection by mistake.";
                    theMsg->printf("\tNew base position is too far away. System concludes selection by mistake -> Don't move base.");
                    return false;
                }

                HxUniformScalarField3* image = getImageOrThrow();

                try
                {
                    MoveBaseOperationSet* op = FilopodiaFunctions::moveBase(graph, selectedNode, pickedPoint, image, intensityWeight, topBrightness);
                    if (op != 0)
                    {
                        mEditor->execNewOp(op);
                    }
                }
                catch (McException& e)
                {
                    theMsg->printf(QString("%1").arg(e.what()));
                    HxMessage::error(QString("Cannot move base: \n%1").arg(e.what()), "ok");
                }
            }
            else if (FilopodiaFunctions::hasNodeType(graph, ROOT_NODE, selectedNode))
            {
                qDebug() << "Tracing tool: \n\t Move root node. Not implemented.";
                theMsg->printf("Tracing tool: \t Move root node. Not implemented.");

                IncidenceList incidentEdges = graph->getIncidentEdges(selectedNode);
                if (incidentEdges.size() != 0)
                {
                    HxMessage::error("Could not move root node. Root node can just be moved when nothing is connected.", "ok");
                    return false;
                }
            }
        }
        else if (selectedItem == SEGMENT && pickedItem == VALID_INTENSITY)
        {
            qDebug() << "Tracing tool: \n\tTrace selected edge throw picked point.";
            theMsg->printf("Tracing tool: \tTrace selected edge throw picked point.");

            HxUniformScalarField3* image = getImageOrThrow();
            const int selectedEdge = sel.getSelectedEdge(0);

            try
            {
                ReplaceFilopodiaEdgeOperationSet* op = FilopodiaFunctions::replaceEdge(graph,
                                                                                       selectedEdge,
                                                                                       pickedPoint,
                                                                                       image,
                                                                                       mSurroundWidthInVoxels,
                                                                                       intensityWeight,
                                                                                       topBrightness);

                if (op != 0)
                {
                    mEditor->execNewOp(op);
                }
            }
            catch (McException& e)
            {
                theMsg->printf(QString("%1").arg(e.what()));
                HxMessage::error(QString("Cannot replace edge: \n%1").arg(e.what()), "ok");
            }
        }
        else if (selectedItem == POINT && pickedItem == NODE)
        {
            qDebug() << "Tracing tool: \n\tPoint/Node: not implemented";
            theMsg->printf("Tracing tool: \tPoint/Node: not implemented.");
        }
        else if (selectedItem == POINT && pickedItem == POINT)
        {
            if (sel.getSelectedPoint(0) == edgePoint)
            {
                sel.deselectPoint(edgePoint);
                mEditor->setHighlightedElements(sel);
            }
            else
            {
                qDebug() << "Tracing tool: \n\tPoint/Point: convert and connect not implemented";
            }
        }
        else if (selectedItem == POINT && pickedItem == SEGMENT)
        {
            qDebug() << "Tracing tool: \n\tPoint/Segment: not implemented";
        }
        else if (selectedItem == POINT && pickedItem == VALID_INTENSITY)
        {
            qDebug() << "Tracing tool: \n\tPoint/Intensity: not implemented";
        }
    }
    QTime traceEndTime = QTime::currentTime();
    qDebug() << "\nTracing tool:" << traceStartTime.msecsTo(traceEndTime) << "msec";

    return true;
}

bool
ExFilopodiaTraceTool::nodePicked(ExBaseViewer* viewer,
                                 const McVec2i& pickedScreenPoint,
                                 McVec3f& pickedPoint,
                                 int& nodeIdx)
{
    ExSpatialGraphPickFunctions pickFunctions(viewer,
                                              mEditor->pickingThresholds(),
                                              mEditor->getSpatialGraph(),
                                              mEditor->getVisibleElements());

    nodeIdx = pickFunctions.getPickedNodeClosestToViewPoint(pickedScreenPoint);
    if (nodeIdx >= 0)
    {
        pickedPoint = mEditor->getSpatialGraph()->getVertexCoords(nodeIdx);
        return true;
    }
    else
    {
        return false;
    }
}

bool
ExFilopodiaTraceTool::pointPicked(ExBaseViewer* viewer,
                                  const McVec2i& pickedScreenPoint,
                                  McVec3f& pickedPoint,
                                  SpatialGraphPoint& edgePoint)
{
    ExSpatialGraphPickFunctions pickFunctions(viewer,
                                              mEditor->pickingThresholds(),
                                              mEditor->getSpatialGraph(),
                                              mEditor->getVisibleElements());

    if (pickFunctions.getPickedPoint(pickedScreenPoint, edgePoint))
    {
        pickedPoint = mEditor->getSpatialGraph()->getEdgePoint(edgePoint.edgeNum, edgePoint.pointNum);
        return true;
    }
    else
    {
        return false;
    }
}

bool
ExFilopodiaTraceTool::edgePicked(ExBaseViewer* viewer,
                                 const McVec2i& pickedScreenPoint,
                                 McVec3f& pickedPoint,
                                 SpatialGraphPoint& pointBefore,
                                 int& edgeIdx)
{
    ExSpatialGraphPickFunctions pickFunctions(viewer,
                                              mEditor->pickingThresholds(),
                                              mEditor->getSpatialGraph(),
                                              mEditor->getVisibleElements());

    int pBefore, pAfter;
    edgeIdx = pickFunctions.getPickedEdge(pickedScreenPoint,
                                          pBefore,
                                          pAfter,
                                          pickedPoint);
    if (edgeIdx >= 0)
    {
        pointBefore.edgeNum = edgeIdx;
        pointBefore.pointNum = pBefore;
        return true;
    }
    else
    {
        return false;
    }
}

bool
getPickedPointOptimimalIntensity(const McVec3f& pointOnThickSlice,
                                 const VsSlice* slice,
                                 HxNeuronEditorSubApp* editor,
                                 const McLine& line,
                                 McVec3f& pickedPointOptimalIntensity)
{
    VsVolume* volume = slice->volume(0);
    if (!volume)
    {
        return false;
    }

    VsData* data = volume->data(0);
    if (!data)
    {
        return false;
    }

    const McVec3f voxelSize = data->voxelSpacing();
    const float step = MC_MIN3(voxelSize.x, voxelSize.y, voxelSize.z);
    const McPlane plane = slice->plane();
    const float thickness = slice->thickness();

    int numSteps = 1;

    if (thickness > 0.001f)
    {
        McVec3f pNear, pFar;
        const float halfThickness = thickness * 0.5f;

        McPlane farPlane(plane);
        farPlane.offset(halfThickness);
        farPlane.intersect(line, pFar);

        McPlane nearPlane(plane);
        nearPlane.offset(-halfThickness);
        nearPlane.intersect(line, pNear);

        const float segLength = (pFar - pNear).length();
        if (step > 0.001f)
        {
            numSteps = MC_MAX2(1, int(segLength / step));
        }
    }

    QSharedPointer<VsEvaluator> intensityEvaluator(data->createEvaluator());
    const McVec3f dir = plane.getNormal();

    float density, pickedDensity;

    if (editor->getFilamentType() == HxNeuronEditorSubApp::BRIGHT_FILAMENTS)
    {
        float maxDensity = std::numeric_limits<float>::min();
        for (int i = -numSteps / 2; i <= numSteps / 2; ++i)
        {
            const McVec3f pos = pointOnThickSlice + dir * (step * i);

            if (!intensityEvaluator->set(pos))
            {
                continue;
            }

            intensityEvaluator->eval(&density);
            if (editor->isValueInsideAllowedIntensityRange(density) && (density > maxDensity))
            {
                maxDensity = density;
                pickedPointOptimalIntensity = pos;
            }
        }
        pickedDensity = maxDensity;
    }
    else
    {
        float minDensity = std::numeric_limits<float>::max();
        for (int i = -numSteps / 2; i <= numSteps / 2; ++i)
        {
            const McVec3f pos = pointOnThickSlice + dir * (step * i);

            if (!intensityEvaluator->set(pos))
            {
                continue;
            }

            intensityEvaluator->eval(&density);
            if (editor->isValueInsideAllowedIntensityRange(density) && (density < minDensity))
            {
                minDensity = density;
                pickedPointOptimalIntensity = pos;
            }
        }
        pickedDensity = minDensity;
    }

    return editor->isValueInsideAllowedIntensityRange(pickedDensity);
}

bool
ExFilopodiaTraceTool::validIntensityPicked(ExBaseViewer* viewer,
                                           VsSlice* slice,
                                           const McVec2i& pickedScreenPoint,
                                           McVec3f& pickedPointOptimalIntensity)
{
    McLine line;
    if (!pickedLine(viewer, pickedScreenPoint.x, pickedScreenPoint.y, line))
    {
        return false;
    }

    McVec3f pickedPointOnThickSlice;
    const McPlane plane = slice->plane();

    if (!plane.intersect(line, pickedPointOnThickSlice))
    {
        return false;
    }

    return getPickedPointOptimimalIntensity(pickedPointOnThickSlice, slice, mEditor, line, pickedPointOptimalIntensity);
}

Ex::CursorShape
ExFilopodiaTraceTool::getCursor(const ExFilopodiaTraceTool::PickedItem pickedItem,
                                const bool appending)
{
    if (pickedItem == NODE && appending)
        return Ex::SeekCursorNodeAppend;
    if (pickedItem == NODE && !appending)
        return Ex::SeekCursorNode;
    if (pickedItem == POINT && appending)
        return Ex::SeekCursorPointAppend;
    if (pickedItem == POINT && !appending)
        return Ex::SeekCursorPoint;
    if (pickedItem == SEGMENT && appending)
        return Ex::SeekCursorEdgeAppend;
    if (pickedItem == SEGMENT && !appending)
        return Ex::SeekCursorEdge;
    if (pickedItem == VALID_INTENSITY && appending)
        return Ex::SeekCursorAppend;
    if (pickedItem == VALID_INTENSITY && !appending)
        return Ex::SeekCursor;

    return Ex::CrossCursor;
}

const McString
ExFilopodiaTraceTool::identifier() const
{
    return "ExFilopodiaTraceTool";
}

////////////////////////////////////////////////////////////////////////////////
QxFilopodiaTraceTool::QxFilopodiaTraceTool(HxNeuronEditorSubApp* editor, QxFilopodiaTool* filopodiaTool)
    : QxNeuronEditorModalTool(editor)
    , mFilopodiaToolbox(filopodiaTool)
    , mSurroundWidthInVoxels(10)
{
    mFilopodiaTraceTool = new ExFilopodiaTraceTool(filopodiaTool, mSurroundWidthInVoxels);
    setActivateAllEnabled(false);
    mFilopodiaTraceTool->setSnapping(1);
}

bool
QxFilopodiaTraceTool::supportsViewer(HxViewerBase* viewer)
{
    if (!mEditor->getImageData())
    {
        return false;
    }

    const bool isMPRViewer = bool(dynamic_cast<HxMPRViewer*>(viewer));
    const bool is3DViewer = bool(dynamic_cast<HxViewer*>(viewer));

    return isMPRViewer || is3DViewer;
}

void
QxFilopodiaTraceTool::pickCB(void* userData, SoEventCallback* node)
{
    QxFilopodiaTraceTool* tool = (QxFilopodiaTraceTool*)userData;
    HxNeuronEditorSubApp* editor = tool->mEditor;

    const SoMouseButtonEvent* event = (SoMouseButtonEvent*)node->getEvent();
    if (!SO_MOUSE_PRESS_EVENT(event, BUTTON1) || !event->wasShiftDown())
    {
        return;
    }

    VsVolren* volren = editor->getVolren();
    if (!volren)
    {
        qDebug() << "QxFilopodiaTraceTool: no volren";
        return;
    }

    HxUniformScalarField3* image = dynamic_cast<HxUniformScalarField3*>(editor->getImageData());
    HxUniformScalarField3* dijkstraMap = tool->mFilopodiaToolbox->getDijkstraMap();

    if (!image || !dijkstraMap)
    {
        qDebug() << "QxFilopodiaTraceTool: no image or dijkstra map";
        return;
    }

    HxSpatialGraph* graph = editor->getSpatialGraph();
    if (!graph)
    {
        qDebug() << "QxFilopodiaTraceTool: no graph";
        return;
    }

    // From mouse click position in window coords, get 3D position on slice
    const SbVec2s mousePos = event->getPosition();
    const SbVec2s windowSize = editor->get3DViewer()->getGlxSize();

    if (windowSize[0] == 0 || windowSize[1] == 0)
    {
        qDebug() << "QxFilopodiaTraceTool: window size is 0";
        return;
    }

    const float aspectRatio = float(windowSize[0]) / windowSize[1];
    const SoCamera* camera = editor->get3DViewer()->getCamera();
    SbViewVolume viewVolume = camera->getViewVolume(aspectRatio);

    // Rescale view volume when aspect ratio is less than 1.
    // See http://developer90.openinventor.com/node/18798
    if (aspectRatio < 1.0f)
    {
        viewVolume.scale(1.0f / aspectRatio);
    }

    const SbVec2f normalizedMousePos = SbVec2f(float(mousePos[0]) / windowSize[0], float(mousePos[1]) / windowSize[1]);
    SbLine sbLine;
    viewVolume.projectPointToLine(normalizedMousePos, sbLine);

    const McVec3f position(sbLine.getPosition()[0], sbLine.getPosition()[1], sbLine.getPosition()[2]);
    const McVec3f direction(sbLine.getDirection()[0], sbLine.getDirection()[1], sbLine.getDirection()[2]);

    McVec3f pickPos3D;
    QScopedPointer<VsFilopodiaFirstMaximumPickThreshold> threshold(new VsFilopodiaFirstMaximumPickThreshold(volren));

    if (volren->volpick(position, direction, false, false, pickPos3D, threshold.data()))
    {
        const float pickedIntensity = threshold->intensity();
        if (!editor->isValueInsideAllowedIntensityRange(pickedIntensity))
        {
            qDebug() << "QxFilopodiaTraceTool: invalid intensity picked:" << pickedIntensity;
            return;
        }

        const SpatialGraphSelection sel = editor->getHighlightedElements();
        Operation* op = 0;
        if (sel.isEmpty())
        {
            theMsg->printf("Tracing tool: \tAdd filopodium in 3D.");

            try
            {
                op = FilopodiaFunctions::addFilopodium(graph,
                                                       pickPos3D,
                                                       tool->mFilopodiaToolbox->getCurrentTime(),
                                                       image,
                                                       dijkstraMap,
                                                       false);
            }
            catch (McException& e)
            {
                theMsg->printf(QString("%1").arg(e.what()));
                HxMessage::error(QString("Cannot add Filopodium in 3D: \n%1").arg(e.what()), "ok");
            }
        }

        else if (sel.getNumSelectedVertices() == 1)
        {
            theMsg->printf("Tracing tool: \tMove tip in 3D.");

            const int selectedNode = sel.getSelectedVertex(0);

            if (FilopodiaFunctions::hasNodeType(graph, TIP_NODE, selectedNode))
            {
                const float intensityWeight = tool->mFilopodiaToolbox->getIntensityWeight();
                const float topBrightness = tool->mFilopodiaToolbox->getTopBrightness();

                try
                {
                    op = FilopodiaFunctions::moveTip(graph,
                                                     selectedNode,
                                                     pickPos3D,
                                                     image,
                                                     tool->mSurroundWidthInVoxels,
                                                     intensityWeight,
                                                     topBrightness,
                                                     false);
                }
                catch (McException& e)
                {
                    theMsg->printf(QString("%1").arg(e.what()));
                    HxMessage::error(QString("Cannot move tip in 3D: \n%1").arg(e.what()), "ok");
                }
            }
        }

        if (op)
        {
            if (!editor->execNewOp(op))
            {
                delete op;
                return;
            }

            McPlane plane(McVec3f(0, 0, 1), pickPos3D);

            VsSlice* slice = editor->getMPRViewer()->slice();
            AddFilopodiumOperationSet* addOp = dynamic_cast<AddFilopodiumOperationSet*>(op);
            if (addOp)
            {
                const SpatialGraphSelection newNodesAndEdges = addOp->getNewNodesAndEdges();
                const SpatialGraphSelection newBases = FilopodiaFunctions::getNodesOfTypeInSelection(graph, newNodesAndEdges, BASE_NODE);

                if (newBases.getNumSelectedVertices() == 1)
                {
                    editor->setHighlightedElements(newBases);
                    const int baseNode = newBases.getSelectedVertex(0);
                    const McVec3f basePos = graph->getVertexCoords(baseNode);
                    plane = McPlane(McVec3f(0, 0, 1), basePos);
                }
            }
            slice->setPlane(plane);
        }
    }
    else
    {
        qDebug() << "Tracing tool: \tCould not pick point in 3D.";
        theMsg->printf("Tracing tool: \tCould not pick point in 3D.");
    }
    node->setHandled();
}

void
QxFilopodiaTraceTool::onActivate(HxViewerBase*)
{
    HxMPRViewer* mprViewer = mEditor->getMPRViewer();
    if (!mprViewer)
    {
        return;
    }

    mprViewer->setActiveTool(mFilopodiaTraceTool);
    mFilopodiaTraceTool->setEditor(mEditor);
    mEditor->setInteractive(true);

    theController->addPickCallback(pickCB, this);
}

void
QxFilopodiaTraceTool::onDeactivate(HxViewerBase*)
{
    HxMPRViewer* mprViewer = mEditor->getMPRViewer();
    if (!mprViewer)
    {
        return;
    }
    mprViewer->setActiveTool(0);

    theController->removePickCallback(pickCB, this);
}
