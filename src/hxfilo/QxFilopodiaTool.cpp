#include "QxFilopodiaTool.h"
#include "FilopodiaOperationSet.h"
#include "HxFilopodiaTrack.h"
#include "HxFilopodiaStats.h"
#include "HxShortestPathToPointMap.h"
#include "HxSplitLabelField.h"
#include "hxcontourtree/HxContourTreeSegmentation.h"
#include <hxspreadsheet/internal/HxSpreadSheet.h>
#include <hxfield/HxLoc3Uniform.h>
#include "FilopodiaFunctions.h"
#include "hxneuroneditor/internal/HxMPRViewer.h"
#include <hxcore/HxObjectPool.h>
#include <hxcore/HxViewer.h>
#include <mclib/McException.h>
#include <mclib/internal/McRandom.h>
#include <mclib/McBox3i.h>
#include <mclib/McMath.h>
#include <hxcore/internal/SoTabBoxDraggerVR.h>
#include <Inventor/draggers/SoDragger.h>
#include <QFileDialog>
#include <QTime>
#include <QSet>
#include <QDebug>
#include <string>
#include <vsvolren/internal/VsScene.h>

#include "ConvertVectorAndMcDArray.h"

void
computeDijkstra(HxSpatialGraph* graph, QString outputDir, const float intensityWeight, const float topBrightness)
{
    theMsg->printf("Compute Dijkstra maps.");
    const int numberOfGrowthCones = FilopodiaFunctions::getNumberOfGrowthCones(graph);
    if (numberOfGrowthCones < 1)
    {
        qDebug() << "no growth cones";
        return;
    }

    int minGC = 1;
    int maxGC = numberOfGrowthCones + 1;

    TimeMinMax timeMinMax = FilopodiaFunctions::getTimeMinMaxFromGraphLabels(graph);
    int minT = timeMinMax.minT;
    int maxT = timeMinMax.maxT + 1;

    if (graph->getNumVertices() == 1)
    {
        EdgeVertexAttribute* gcAtt = graph->findVertexAttribute(FilopodiaFunctions::getGrowthConeAttributeName());
        if (!gcAtt)
        {
            qDebug() << "no gc att";
            return;
        }

        minGC = FilopodiaFunctions::getGcIdOfNode(graph, 0);
        maxGC = minGC + 1;

        minT = FilopodiaFunctions::getTimeOfNode(graph, 0);
        maxT = minT + 1;
    }

    // Compute dijkstra maps
    for (int gc = minGC; gc < maxGC; ++gc)
    {
        SpatialGraphSelection nodesOfGc = FilopodiaFunctions::getNodesWithGcId(graph, gc);
        SpatialGraphSelection rootNodesOfGc = FilopodiaFunctions::getNodesOfTypeInSelection(graph, nodesOfGc, ROOT_NODE);
        if (rootNodesOfGc.getNumSelectedVertices() == 0)
        {
            qDebug() << "no nodes for gc" << gc;
            continue;
        }

        QString gcFolder = QString(outputDir + "/GrowthCone_%1").arg(gc);
        QString grayFolder = gcFolder + "/Gray";
        QString dijkstraFolder = gcFolder + "/Dijkstra";
        if (!QDir(dijkstraFolder).exists())
        {
            QDir().mkdir(dijkstraFolder);
        }

        const QDir imageDir(grayFolder);
        QMap<int, McHandle<HxUniformScalarField3> > grayImages;
        QFileInfoList fileInfoListImages = imageDir.entryInfoList(QDir::Files);

        for (int f = 0; f < fileInfoListImages.size(); ++f)
        {
            const int time = FilopodiaFunctions::getTimeFromFileNameNoGC(fileInfoListImages[f].fileName());

            McHandle<HxUniformScalarField3> image = FilopodiaFunctions::loadUniformScalarField3(fileInfoListImages[f].filePath());
            if (!image)
            {
                throw McException(QString("Could not read image from file %1").arg(fileInfoListImages[f].filePath()));
            }

            if (image->primType() == McPrimType::MC_FLOAT ||
                image->primType() == McPrimType::MC_DOUBLE)
            {
                FilopodiaFunctions::convertToUnsignedChar(image);
                image->composeLabel(image->getLabel(), QString("toUint8"));
            }
            grayImages.insert(time, image);
            theObjectPool->removeObject(image);
        }
        for (int t = minT; t < maxT; ++t)
        {
            qDebug() << "\t\tgc" << gc << "time" << t;
            HxUniformScalarField3* currentImage = grayImages.value(t);
            if (!currentImage)
            {
                continue;
            }
            currentImage->setLabel("currentImage");

            const McDim3l dims = currentImage->lattice().getDims();

            const int root = FilopodiaFunctions::getRootNodeFromTimeStep(graph, t);
            const McVec3f rootCoords = graph->getVertexCoords(root);

            // Compute dijkstra maps
            McHandle<HxUniformLabelField3> dijkstraMap = HxUniformLabelField3::createInstance();
            HxShortestPathToPointMap::computeDijkstraMap(currentImage,
                                                         rootCoords,
                                                         McDim3l(0, 0, 0),
                                                         McDim3l(dims[0] - 1, dims[1] - 1, dims[2] - 1),
                                                         intensityWeight,
                                                         topBrightness,
                                                         dijkstraMap);

            if (!dijkstraMap)
            {
                qDebug() << "dijkstra map does not exists";
                continue;
            }

            QString dijkstraName;
            if (t < 10)
            {
                dijkstraName = QString("dijkstra_gc%1_t0%2.am").arg(gc).arg(t);
            }
            else
            {
                dijkstraName = QString("dijkstra_gc%1_t%2.am").arg(gc).arg(t);
            }

            dijkstraMap->setLabel(dijkstraName);
            dijkstraMap->writeAmiraMeshRLE(qPrintable(dijkstraFolder + "/" + dijkstraName));

            //theObjectPool->addObject(dijkstraMap); // Add map to the object pool
        }
    }
}

QxFilopodiaTool::QxFilopodiaTool(HxNeuronEditorSubApp* editor)
    : QxNeuronEditorToolBox(editor)
    , mUiParent(0)
    , mDir("")
    , mImageDir("")
    , mDijkstraDir("")
    , mCurrentTime(-1)
    , mGraph(0)
    , mImage(0)
    , mDijkstra(0)
    , mFilamentStats(0)
    , mFilopodiaStats(0)
{
    mConsistency = HxSpreadSheet::createInstance();
    mConsistency->setLabel(QString("Filopodia Inconsistencies"));
}

QxFilopodiaTool::~QxFilopodiaTool()
{
    if (mUiParent)
        delete mUiParent;
}

QWidget*
QxFilopodiaTool::toolcard()
{
    if (!mUiParent)
    {
        mUiParent = new QWidget;
        mUi.setupUi(mUiParent);

        mUi.beforeSpinBox->setMinimum(0);
        mUi.afterSpinBox->setMinimum(0);

        mUi.filoCheckBox->setCheckState(Qt::Unchecked);
        mUi.showAllTimeStepsCheckBox->setCheckState(Qt::Checked);
        mUi.brightnessCheckBox->setCheckState(Qt::Checked);
        mUi.trackingCheckBox->setCheckState(Qt::Checked);

        mUi.speedFilterLineEdit->setText("0.1");

        QDoubleValidator* distanceThresholdValidator = new QDoubleValidator(mUi.distanceThresholdLineEdit);
        distanceThresholdValidator->setBottom(0.0);
        mUi.distanceThresholdLineEdit->setValidator(distanceThresholdValidator);
        mUi.distanceThresholdLineEdit->setText("1.0");

        // Validators for root template/search parameters
        QIntValidator* rootTemplateRadiusXYValidator = new QIntValidator(this);
        rootTemplateRadiusXYValidator->setBottom(1);
        mUi.rootTemplateRadiusXYLineEdit->setValidator(rootTemplateRadiusXYValidator);
        mUi.rootTemplateRadiusXYLineEdit->setText("35");

        QIntValidator* rootTemplateRadiusZValidator = new QIntValidator(this);
        rootTemplateRadiusZValidator->setBottom(1);
        mUi.rootTemplateRadiusZLineEdit->setValidator(rootTemplateRadiusZValidator);
        mUi.rootTemplateRadiusZLineEdit->setText("3");

        QIntValidator* rootSearchRadiusXYValidator = new QIntValidator(this);
        rootSearchRadiusXYValidator->setBottom(1);
        mUi.rootSearchRadiusXYLineEdit->setValidator(rootSearchRadiusXYValidator);
        mUi.rootSearchRadiusXYLineEdit->setText("7");

        QIntValidator* rootSearchRadiusZValidator = new QIntValidator(this);
        rootSearchRadiusZValidator->setBottom(1);
        mUi.rootSearchRadiusZLineEdit->setValidator(rootSearchRadiusZValidator);
        mUi.rootSearchRadiusZLineEdit->setText("2");

        // Validators for base node template/search parameters
        QIntValidator* baseTemplateRadiusXYValidator = new QIntValidator(this);
        baseTemplateRadiusXYValidator->setBottom(1);
        mUi.baseTemplateRadiusXYLineEdit->setValidator(baseTemplateRadiusXYValidator);
        mUi.baseTemplateRadiusXYLineEdit->setText("25");

        QIntValidator* baseTemplateRadiusZValidator = new QIntValidator(this);
        baseTemplateRadiusZValidator->setBottom(1);
        mUi.baseTemplateRadiusZLineEdit->setValidator(baseTemplateRadiusZValidator);
        mUi.baseTemplateRadiusZLineEdit->setText("5");

        QIntValidator* baseSearchRadiusXYValidator = new QIntValidator(this);
        baseSearchRadiusXYValidator->setBottom(1);
        mUi.baseSearchRadiusXYLineEdit->setValidator(baseSearchRadiusXYValidator);
        mUi.baseSearchRadiusXYLineEdit->setText("10");

        QIntValidator* baseSearchRadiusZValidator = new QIntValidator(this);
        baseSearchRadiusZValidator->setBottom(1);
        mUi.baseSearchRadiusZLineEdit->setValidator(baseSearchRadiusZValidator);
        mUi.baseSearchRadiusZLineEdit->setText("2");

        // Validators for tip template/search parameters
        QDoubleValidator* lengthThresholdValidator = new QDoubleValidator(this);
        lengthThresholdValidator->setBottom(1);
        mUi.lengthThresholdLineEdit->setValidator(lengthThresholdValidator);
        mUi.lengthThresholdLineEdit->setText("0.5");

        QDoubleValidator* correlationThresholdValidator = new QDoubleValidator(this);
        correlationThresholdValidator->setBottom(1);
        mUi.correlationThresholdLineEdit->setValidator(correlationThresholdValidator);
        mUi.correlationThresholdLineEdit->setText("0.8");

        QIntValidator* tipTemplateRadiusXYValidator = new QIntValidator(this);
        tipTemplateRadiusXYValidator->setBottom(1);
        mUi.tipTemplateRadiusXYLineEdit->setValidator(tipTemplateRadiusXYValidator);
        mUi.tipTemplateRadiusXYLineEdit->setText("5");

        QIntValidator* tipTemplateRadiusZValidator = new QIntValidator(this);
        tipTemplateRadiusZValidator->setBottom(1);
        mUi.tipTemplateRadiusZLineEdit->setValidator(tipTemplateRadiusZValidator);
        mUi.tipTemplateRadiusZLineEdit->setText("2");

        QIntValidator* tipSearchRadiusXYValidator = new QIntValidator(this);
        tipSearchRadiusXYValidator->setBottom(1);
        mUi.tipSearchRadiusXYLineEdit->setValidator(tipSearchRadiusXYValidator);
        mUi.tipSearchRadiusXYLineEdit->setText("7");

        QIntValidator* tipSearchRadiusZValidator = new QIntValidator(this);
        tipSearchRadiusZValidator->setBottom(1);
        mUi.tipSearchRadiusZLineEdit->setValidator(tipSearchRadiusZValidator);
        mUi.tipSearchRadiusZLineEdit->setText("2");

        // Validators for file preparation
        QIntValidator* segmentationThresholdValidator = new QIntValidator(this);
        segmentationThresholdValidator->setBottom(1);
        mUi.threshLineEdit->setValidator(segmentationThresholdValidator);
        mUi.threshLineEdit->setText("5");

        QIntValidator* segmentationPersistenceValidator = new QIntValidator(this);
        segmentationPersistenceValidator->setBottom(1);
        mUi.persistenceLineEdit->setValidator(segmentationPersistenceValidator);
        mUi.persistenceLineEdit->setText("150");

        QDoubleValidator* intensityWeightValidator = new QDoubleValidator(this);
        intensityWeightValidator->setBottom(1);
        mUi.intensityLineEdit->setValidator(intensityWeightValidator);
        mUi.intensityLineEdit->setText("50.0");

        QIntValidator* brightnessValidator = new QIntValidator(this);
        brightnessValidator->setBottom(1);
        mUi.topBrightnessLineEdit->setValidator(brightnessValidator);
        mUi.topBrightnessLineEdit->setText("50");

        mUi.offsetXSpinBox->setRange(-1000000, 1000000);
        mUi.offsetYSpinBox->setRange(-1000000, 1000000);
        mUi.offsetZSpinBox->setRange(-1000000, 1000000);
        mUi.boxSizeXSpinBox->setRange(1, 1000000);
        mUi.boxSizeYSpinBox->setRange(1, 1000000);
        mUi.boxSizeZSpinBox->setRange(1, 1000000);

        connect(mUi.selectImageDirButton, SIGNAL(pressed()), this, SLOT(selectDir()));
        connect(mUi.selectOutputDirButton, SIGNAL(pressed()), this, SLOT(selectOutputDir()));
        connect(mUi.timeSlider, SIGNAL(valueChanged(int)), this, SLOT(setTime(int)));
        connect(mUi.timeSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setTime(int)));
        connect(mUi.beforeSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateGraphVisibility()));
        connect(mUi.afterSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateGraphVisibility()));
        connect(mUi.showAllTimeStepsCheckBox, SIGNAL(stateChanged(int)), this, SLOT(updateGraphVisibility()));
        connect(mUi.filoCheckBox, SIGNAL(stateChanged(int)), this, SLOT(updateGraphVisibility()));
        connect(mUi.addFilopodiaLabelsButton, SIGNAL(pressed()), this, SLOT(addFilopodiaLabels()));
        connect(mUi.consistencyButton, SIGNAL(pressed()), this, SLOT(updateConsistencyTable()));
        connect(mUi.bulbousButton, SIGNAL(pressed()), this, SLOT(addBulbousLabel()));
        connect(mUi.extendBulbousButton, SIGNAL(pressed()), this, SLOT(extendBulbousLabel()));
        connect(mUi.statisticsButton, SIGNAL(pressed()), this, SLOT(updateStatistic()));
        connect(mUi.trackingButton, SIGNAL(pressed()), this, SLOT(updateTracking()));
        connect(mUi.propagateAllButton, SIGNAL(pressed()), this, SLOT(propagateAllFilopodia()));
        connect(mUi.propagateSelButton, SIGNAL(pressed()), this, SLOT(propagateSelFilopodia()));
        connect(mUi.rootButton, SIGNAL(pressed()), this, SLOT(propagateRoots()));
        connect(mUi.startLogButton, SIGNAL(pressed()), this, SLOT(startLog()));
        connect(mUi.stopLogButton, SIGNAL(pressed()), this, SLOT(stopLog()));
        connect(mEditor, SIGNAL(spatialGraphChanged(HxNeuronEditorSubApp::SpatialGraphChanges)), this, SLOT(updateAfterSpatialGraphChange(HxNeuronEditorSubApp::SpatialGraphChanges)));
        connect(mEditor, SIGNAL(imageChanged()), this, SLOT(updateAfterImageChanged()));

        connect(mUi.showBBoxCheckBox, SIGNAL(toggled(bool)), this, SLOT(toggleBoxDragger(bool)));
        connect(mUi.growthConeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateBoxDragger()));
        connect(mUi.growthConeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setBoxSpinBoxes()));
        connect(mUi.boxSizeXSpinBox, SIGNAL(valueChanged(int)), this, SLOT(boxSpinBoxChanged()));
        connect(mUi.boxSizeYSpinBox, SIGNAL(valueChanged(int)), this, SLOT(boxSpinBoxChanged()));
        connect(mUi.boxSizeZSpinBox, SIGNAL(valueChanged(int)), this, SLOT(boxSpinBoxChanged()));
        connect(mUi.offsetXSpinBox, SIGNAL(valueChanged(int)), this, SLOT(boxSpinBoxChanged()));
        connect(mUi.offsetYSpinBox, SIGNAL(valueChanged(int)), this, SLOT(boxSpinBoxChanged()));
        connect(mUi.offsetZSpinBox, SIGNAL(valueChanged(int)), this, SLOT(boxSpinBoxChanged()));
        connect(mUi.autoEstimateButton, SIGNAL(pressed()), this, SLOT(autoEstimateBoundingBoxes()));
        connect(mUi.cropAndDijkstraButton, SIGNAL(pressed()), this, SLOT(computeCropAndDijkstra()));

        updateToolboxState();
    }

    return mUiParent;
}

void
QxFilopodiaTool::updateToolcard()
{
}

void
QxFilopodiaTool::addFilopodiaLabels()
{
    theMsg->printf("Add filopodia labels.");
    HxSpatialGraph* graph = mEditor->getSpatialGraph();

    if (!graph)
    {
        return;
    }

    try
    {
        FilopodiaFunctions::addAllFilopodiaLabelGroups(graph, mTimeMinMax);
    }
    catch (McException& e)
    {
        theMsg->printf(QString("%1").arg(e.what()));
    }

    mEditor->updateAfterSpatialGraphChange(HxNeuronEditorSubApp::SpatialGraphLabelTreeChange | HxNeuronEditorSubApp::SpatialGraphLabelChange);
}

void
QxFilopodiaTool::updateConsistencySelection(int row, int column)
{
    const int compTabId = mConsistency->findColumn("Component", HxSpreadSheet::Column::STRING);
    const int idTabId = mConsistency->findColumn("ID", HxSpreadSheet::Column::STRING);
    const int timeTabId = mConsistency->findColumn("Time Step", HxSpreadSheet::Column::INT);
    const int probTabId = mConsistency->findColumn("Problem", HxSpreadSheet::Column::STRING);

    const int tableId = mConsistency->findTable("Filopodia Inconsistencies");
    if (tableId == -1)
    {
        qDebug() << "Inconsistency table not found";
        return;
    }

    if ((compTabId == -1) || (idTabId == -1) || timeTabId == -1 || probTabId == -1)
    {
        qDebug() << "Column not found";
        return;
    }

    SpatialGraphSelection highlightSel(mGraph);

    const HxSpreadSheet::Column* compColumn = mConsistency->column(compTabId, tableId);
    const HxSpreadSheet::Column* idColumn = mConsistency->column(idTabId, tableId);
    const HxSpreadSheet::Column* timeColumn = mConsistency->column(timeTabId, tableId);
    const HxSpreadSheet::Column* probColumn = mConsistency->column(probTabId, tableId);

    const QString comp = QString::fromLatin1(compColumn->stringValue(row).getString());
    const QString id = QString::fromLatin1(idColumn->stringValue(row).getString());
    const int time = timeColumn->intValue(row);
    const QString prob = QString::fromLatin1(probColumn->stringValue(row).getString());

    if (time == -1)
    {
        mUi.showAllTimeStepsCheckBox->setCheckState(Qt::Checked);
    }
    else
    {
        setTime(time);
        mUi.showAllTimeStepsCheckBox->setCheckState(Qt::Unchecked);
    }

    if (prob == "has a gap" || prob == "contains multiple match IDs" || prob == "too few elements")
    {
        const int filoId = FilopodiaFunctions::getFilopodiaIdFromLabelName(mGraph, id);
        highlightSel = FilopodiaFunctions::getFilopodiaSelectionForAllTimeSteps(mGraph, filoId);
    }
    else if (prob == "connects two timesteps")
    {
        highlightSel.selectEdge(id.toInt());
    }
    else if (prob == "no time label" || prob == "no base" || prob == "more than one base" || prob == "more than one incident edge" || prob == "no incident edge" || prob == "not two incident edges" || prob == "too few incident edges" || prob == "more or less than one base")
    {
        if (comp == "edge")
        {
            highlightSel.selectEdge(id.toInt());
        }
        else
        {
            highlightSel.selectVertex(id.toInt());
        }
    }

    mEditor->setHighlightedElements(highlightSel);
}

void
addConsistencyColumn(McHandle<HxSpreadSheet> ss, const QString comp, const QString id, const int timestep, const QString problem, const QString solution)
{
    int newRow = ss->addRow();

    ss->column(0, 0)->setValue(newRow, qPrintable(comp));
    ss->column(1, 0)->setValue(newRow, qPrintable(id));
    ss->column(2, 0)->setValue(newRow, timestep);
    ss->column(3, 0)->setValue(newRow, qPrintable(problem));
    ss->column(4, 0)->setValue(newRow, qPrintable(solution));
}

void
checkNumBases(const HxSpatialGraph* graph, const int currentTime, McHandle<HxSpreadSheet> ss = 0)
{
    const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, currentTime);
    const SpatialGraphSelection endNodesFromTime = FilopodiaFunctions::getNodesOfTypeForTime(graph, TIP_NODE, currentTime);

    // We expect that each ending node has one base on path to root

    EdgeVertexAttribute* timeAtt = graph->findVertexAttribute(FilopodiaFunctions::getTimeStepAttributeName());
    if (!timeAtt)
    {
        throw McException(QString("No attribute found with name %1").arg(QString::fromLatin1(FilopodiaFunctions::getTimeStepAttributeName())));
    }

    EdgeVertexAttribute* typeAtt = graph->findVertexAttribute(FilopodiaFunctions::getTypeLabelAttributeName());
    if (!typeAtt)
    {
        throw McException(QString("No attribute found with name %1").arg(QString::fromLatin1(FilopodiaFunctions::getTypeLabelAttributeName())));
    }

    SpatialGraphSelection::Iterator it(endNodesFromTime);
    for (int v = it.vertices.nextSelected(); v != -1; v = it.vertices.nextSelected())
    {
        SpatialGraphSelection pathToRoot = graph->getShortestPath(v, rootNode);

        SpatialGraphSelection basesOnPath = FilopodiaFunctions::getNodesOfTypeInSelection(graph, pathToRoot, BASE_NODE);
        if (basesOnPath.getNumSelectedVertices() == 0)
        {
            const QString msg = QString("Consistency Check: Tip %1 in time step %2 has no base.").arg(v).arg(currentTime);
            theMsg->printf(msg);
            if (ss == 0)
            {
                HxMessage::error(msg, "ok");
            }
            else
            {
                addConsistencyColumn(ss, "tip", QString("%1").arg(v), currentTime, "no base", "add base");
            }
        }
        else if (basesOnPath.getNumSelectedVertices() > 1)
        {
            const QString msg = QString("Consistency Check: Tip %1 in time step %2 has more than one base.").arg(v).arg(currentTime);
            theMsg->printf(msg);
            if (ss == 0)
            {
                HxMessage::error(msg, "ok");
            }
            else
            {
                addConsistencyColumn(ss, "tip", QString("%1").arg(v), currentTime, "more than one base", "remove base");
            }
        }
    }
}

void
checkIncidentEdges(const HxSpatialGraph* graph, const int currentTime, McHandle<HxSpreadSheet> ss = 0)
{
    // We expect that ending nodes have one incident edge
    const SpatialGraphSelection endNodesOfTime = FilopodiaFunctions::getNodesOfTypeForTime(graph, TIP_NODE, currentTime);

    SpatialGraphSelection::Iterator endIt(endNodesOfTime);
    for (int v = endIt.vertices.nextSelected(); v != -1; v = endIt.vertices.nextSelected())
    {
        IncidenceList incidentEdges = graph->getIncidentEdges(v);

        if (incidentEdges.size() >= 2)
        {
            const QString msg = QString("Consistency Check: Tip %1 in time step %2 has to many incident edges.").arg(v).arg(currentTime);
            theMsg->printf(msg);
            if (ss == 0)
            {
                HxMessage::error(msg, "ok");
            }
            else
            {
                addConsistencyColumn(ss, "tip", QString("%1").arg(v), currentTime, "more than one incident edge", "check problem");
            }
        }
        else if (incidentEdges.size() < 1)
        {
            const QString msg = QString("Consistency Check: Tip %1 in time step %2 has no incident edges.").arg(v).arg(currentTime);
            theMsg->printf(msg);
            if (ss == 0)
            {
                HxMessage::error(msg, "ok");
            }
            else
            {
                addConsistencyColumn(ss, "tip", QString("%1").arg(v), currentTime, "no incident edge", "remove tip");
            }
        }
    }

    // We expect that bases have two incident edges
    const SpatialGraphSelection basesOfTime = FilopodiaFunctions::getNodesOfTypeForTime(graph, BASE_NODE, currentTime);
    SpatialGraphSelection::Iterator baseIt(basesOfTime);
    for (int v = baseIt.vertices.nextSelected(); v != -1; v = baseIt.vertices.nextSelected())
    {
        IncidenceList incidentEdges = graph->getIncidentEdges(v);

        if (incidentEdges.size() != 2)
        {
            const QString msg = QString("Consistency Check: Base %1 in time step %2 has not two incident edges.").arg(v).arg(currentTime);
            theMsg->printf(msg);
            if (ss == 0)
            {
                HxMessage::error(msg, "ok");
            }
            else
            {
                addConsistencyColumn(ss, "base", QString("%1").arg(v), currentTime, "not two incident edges", "check problem");
            }
        }
    }

    // We expect that branching nodes have more than two incident edges
    const SpatialGraphSelection branchNodesOfTime = FilopodiaFunctions::getNodesOfTypeForTime(graph, BRANCHING_NODE, currentTime);

    SpatialGraphSelection::Iterator branchIt(branchNodesOfTime);
    for (int v = branchIt.vertices.nextSelected(); v != -1; v = branchIt.vertices.nextSelected())
    {
        IncidenceList incidentEdges = graph->getIncidentEdges(v);

        if (incidentEdges.size() < 3)
        {
            const QString msg = QString("Consistency Check: Branchnode %1 in time step %2 has too few incident edges.").arg(v).arg(currentTime);
            theMsg->printf(msg);
            if (ss == 0)
            {
                HxMessage::error(msg, "ok");
            }
            else
            {
                addConsistencyColumn(ss, "branch", QString("%1").arg(v), currentTime, "too few incident edges", "convert node to point");
            }
        }
    }
}

void
checkFilopodiaSize(const HxSpatialGraph* graph, const int currentTime, McHandle<HxSpreadSheet> ss = 0)
{
    // We expect that each filopodia has more than 2 nodes (at least one base and one endnode) and at least one edge
    SpatialGraphSelection endNodesOfTime = FilopodiaFunctions::getNodesOfTypeForTime(graph, TIP_NODE, currentTime);
    const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, currentTime);

    SpatialGraphSelection::Iterator itEnd(endNodesOfTime);
    for (int v = itEnd.vertices.nextSelected(); v != -1; v = itEnd.vertices.nextSelected())
    {
        SpatialGraphSelection pathToRoot = graph->getShortestPath(rootNode, v);
        SpatialGraphSelection baseNodes = FilopodiaFunctions::getNodesOfTypeInSelection(graph, pathToRoot, BASE_NODE);
        if (baseNodes.getNumSelectedVertices() != 1)
        {
            const QString msg = QString("Consistency Check: Tip %1 in timestep %2 has more or less than one base.").arg(v).arg(currentTime);
            theMsg->printf(msg);
            if (ss == 0)
            {
                HxMessage::error(msg, "ok");
            }
            else
            {
                addConsistencyColumn(ss, "tip", QString("%1").arg(v), currentTime, "more or less than one base", "add or remove base");
            }
        }
    }

    SpatialGraphSelection basesOfTime = FilopodiaFunctions::getNodesOfTypeForTime(graph, BASE_NODE, currentTime);

    SpatialGraphSelection::Iterator itBase(basesOfTime);
    for (int v = itBase.vertices.nextSelected(); v != -1; v = itBase.vertices.nextSelected())
    {
        const int filoId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, v);
        SpatialGraphSelection filoSelection = FilopodiaFunctions::getFilopodiaSelectionOfNode(graph, v);
        if (filoSelection.getNumSelectedVertices() < 2 || filoSelection.getNumSelectedEdges() < 1)
        {
            const QString labelName = FilopodiaFunctions::getFilopodiaLabelNameFromID(graph, filoId);
            const QString msg = QString("Consistency Check: Filopodium %1 with base %2 in time step %3 has too few elements.").arg(labelName).arg(v).arg(currentTime);
            theMsg->printf(msg);
            if (ss == 0)
            {
                HxMessage::error(msg, "ok");
            }
            else
            {
                addConsistencyColumn(ss, "filopodium", labelName, currentTime, "too few elements", "check if tip, base and one edge exist");
            }
        }
    }
}

void
checkMatchIds(const HxSpatialGraph* graph, const int currentTime, McHandle<HxSpreadSheet> ss = 0)
{
    // We expect that each match ID occurs once per timestep
    SpatialGraphSelection basesOfTime = FilopodiaFunctions::getNodesOfTypeForTime(graph, BASE_NODE, currentTime);
    const int unassignedMatchId = FilopodiaFunctions::getMatchLabelId(graph, UNASSIGNED);

    QSet<int> set;
    SpatialGraphSelection::Iterator it(basesOfTime);
    for (int v = it.vertices.nextSelected(); v != -1; v = it.vertices.nextSelected())
    {
        const int matchId = FilopodiaFunctions::getMatchIdOfNode(graph, v);
        if (matchId == unassignedMatchId)
        {
            continue;
        }

        if (!set.contains(matchId))
        {
            set.insert(matchId);
        }
        else
        {
            const QString labelName = FilopodiaFunctions::getLabelNameFromMatchId(graph, matchId);
            const QString msg = QString("Consistency Check: In time step %1 there are multiple filopodia with same match ID %2. Please separate them.")
                                    .arg(currentTime)
                                    .arg(labelName);
            theMsg->printf(msg);
            if (ss == 0)
            {
                HxMessage::error(msg, "ok");
            }
            else
            {
                addConsistencyColumn(ss, "filopodia", "?", currentTime, "multiple filopodia with same match ID", "isolate filopodia");
            }
        }
    }
}

void
checkFilopodiaGaps(const HxSpatialGraph* graph, McHandle<HxSpreadSheet> ss = 0)
{
    // We expect that filopodia has no gaps in timeline
    HierarchicalLabels* filoLabel = graph->getLabelGroup(FilopodiaFunctions::getFilopodiaAttributeName());
    const int unassignedMatchId = FilopodiaFunctions::getMatchLabelId(graph, UNASSIGNED);
    const int numFiloLabels = filoLabel->getNumLabels();

    for (int f = 0; f < numFiloLabels; ++f)
    {
        if (f == FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED) ||
            f == FilopodiaFunctions::getFilopodiaLabelId(graph, IGNORED) ||
            f == FilopodiaFunctions::getFilopodiaLabelId(graph, AXON) ||
            f == 0)
        {
            continue;
        }
        else
        {
            SpatialGraphSelection filoSel = FilopodiaFunctions::getFilopodiaSelectionForAllTimeSteps(graph, f);
            SpatialGraphSelection filoBaseSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, filoSel, BASE_NODE);

            if (!filoBaseSel.isEmpty())
            {
                // We expect that there are no gaps in the filopodia tracks
                TimeMinMax lifeTime = FilopodiaFunctions::getFilopodiumLifeTime(graph, f);
                float rate = float(float(lifeTime.maxT - lifeTime.minT + 1) / float(filoBaseSel.getNumSelectedVertices()));

                if (rate != 1.0)
                {
                    const QString labelName = FilopodiaFunctions::getFilopodiaLabelNameFromID(graph, f);
                    const QString msg = QString("Consistency Check: Filopodium %1 has a gap.")
                                            .arg(labelName);
                    theMsg->printf(msg);
                    if (ss == 0)
                    {
                        HxMessage::error(msg, "ok");
                    }
                    else
                    {
                        addConsistencyColumn(ss, "filopodium", labelName, -1, "has a gap", "fill gap or isolate");
                    }
                }

                // We expect that all nodes of a filo track have the same matchId or are unassigned
                SpatialGraphSelection::Iterator it(filoBaseSel);
                QSet<int> setOfMatchIds;

                for (int v = it.vertices.nextSelected(); v != -1; v = it.vertices.nextSelected())
                {
                    const int matchId = FilopodiaFunctions::getMatchIdOfNode(graph, v);
                    if (!setOfMatchIds.contains(matchId) && matchId != unassignedMatchId)
                    {
                        setOfMatchIds.insert(matchId);
                    }
                }

                if (setOfMatchIds.size() > 1)
                {
                    const QString labelName = FilopodiaFunctions::getFilopodiaLabelNameFromID(graph, f);
                    const QString msg = QString("Consistency Check: Filo label %1 contains multiple matchIds.")
                                            .arg(labelName);
                    theMsg->printf(msg);
                    if (ss == 0)
                    {
                        HxMessage::error(msg, "ok");
                    }
                    else
                    {
                        addConsistencyColumn(ss, "filopodia", labelName, -1, "contains multiple match IDs", "isolate");
                    }
                }
            }
        }
    }
}

// Check that all vertices and edges have a valid time label
void
checkTimeLabels(const HxSpatialGraph* graph, McHandle<HxSpreadSheet> ss = 0)
{
    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        if (FilopodiaFunctions::getTimeIdOfNode(graph, v) <= 0)
        {
            const QString msg = QString("Consistency Check: Node %1 has no time label.").arg(v);
            theMsg->printf(msg);
            if (ss == 0)
            {
                HxMessage::error(msg, "ok");
            }
            else
            {
                addConsistencyColumn(ss, "node", QString("%1").arg(v), -1, "no time label", "delete");
            }
        }
    }
    for (int e = 0; e < graph->getNumEdges(); ++e)
    {
        const int source = graph->getEdgeSource(e);
        const int target = graph->getEdgeTarget(e);
        if (FilopodiaFunctions::getTimeIdOfNode(graph, source) != FilopodiaFunctions::getTimeIdOfNode(graph, target))
        {
            const QString msg = QString("Consistency Check: Edge %1 connects two timesteps. Please delete this edge.").arg(e);
            theMsg->printf(msg);
            if (ss == 0)
            {
                HxMessage::error(msg, "ok");
            }
            else
            {
                addConsistencyColumn(ss, "edge", QString("%1").arg(e), -1, "connects two timesteps", "delete");
            }
        }

        if (FilopodiaFunctions::getTimeIdOfEdge(graph, e) <= 0)
        {
            const QString msg = QString("Consistency Check: Edge %1 has no time label.").arg(e);
            theMsg->printf(msg);
            if (ss == 0)
            {
                HxMessage::error(msg, "ok");
            }
            else
            {
                addConsistencyColumn(ss, "edge", QString("%1").arg(e), -1, "no time label", "delete");
            }
        }
    }
}

void
checkBulbousLabels(const HxSpatialGraph* graph, McHandle<HxSpreadSheet> ss = 0)
{
    // If there are filopodia with bulbous labels we assume that there are no gaps between first and last occusion
    const EdgeVertexAttribute* bulbousAtt = graph->findVertexAttribute(FilopodiaFunctions::getBulbousAttributeName());
    if (!bulbousAtt)
    {
        return;
    }

    SpatialGraphSelection bulbousSel = FilopodiaFunctions::getBasesWithBulbousLabel(graph);
    std::vector<int> filoList = FilopodiaFunctions::getFiloIdsWithBulbousLabel(graph, bulbousSel);

    for (int i = 0; i < filoList.size(); ++i)
    {
        const int filoId = filoList[i];
        const QString filoName = FilopodiaFunctions::getFilopodiaLabelNameFromID(graph, filoId);
        const SpatialGraphSelection bulbousFilo = FilopodiaFunctions::getNodesWithFiloIdFromSelection(graph, bulbousSel, filoId);

        int startBulbousTime = std::numeric_limits<int>::max();
        int endBulbousTime = std::numeric_limits<int>::min();

        SpatialGraphSelection::Iterator it(bulbousFilo);
        for (int v = it.vertices.nextSelected(); v != -1; v = it.vertices.nextSelected())
        {
            const int timeId = FilopodiaFunctions::getTimeIdOfNode(graph, v);
            if (timeId < startBulbousTime)
                startBulbousTime = timeId;
            if (timeId > endBulbousTime)
                endBulbousTime = timeId;
        }

        float rate = float(float(endBulbousTime - startBulbousTime + 1) / float(bulbousFilo.getNumSelectedVertices()));
        if (rate != 1.0)
        {
            const QString msg = QString("Consistency Check: Define base and end of bulbous filopodia %1.").arg(filoName);
            theMsg->printf(msg);
            if (ss == 0)
            {
                HxMessage::error(msg, "ok");
            }
            else
            {
                addConsistencyColumn(ss, "filopodia", filoName, -1, "incomplete bulbous label", "extend bulbous label");
            }
        }
    }
}

void
QxFilopodiaTool::updateConsistencyTable()
{
    theMsg->printf("Create spreadsheets for consistency checks.");
    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    if (!graph)
    {
        return;
    }

    // Create Consistency Table
    try
    {
        QxFilopodiaTool::createConsistencyTab(graph, mConsistency);
        mConsistency->portShow.touch();
        mConsistency->update();
    }
    catch (McException& e)
    {
        theMsg->printf(QString("%1").arg(e.what()));
    }
}

void
QxFilopodiaTool::checkConsistencyAfterGeometryChange(const HxSpatialGraph* graph, const int currentTime, McHandle<HxSpreadSheet> ss)
{
    checkNumBases(graph, currentTime, ss);
    checkIncidentEdges(graph, currentTime, ss);
    checkFilopodiaSize(graph, currentTime, ss);
    checkMatchIds(graph, currentTime, ss);
}

void
QxFilopodiaTool::createConsistencyTab(const HxSpatialGraph* graph, McHandle<HxSpreadSheet> ss)
{
    qDebug() << "create inconsistency tab";
    QTime startTime = QTime::currentTime();

    ss->clear();
    ss->setTableName("Filopodia Inconsistencies", 0);

    ss->addColumn("Component", HxSpreadSheet::Column::STRING, 0);
    ss->addColumn("ID", HxSpreadSheet::Column::STRING, 0);
    ss->addColumn("Time Step", HxSpreadSheet::Column::INT, 0);
    ss->addColumn("Problem", HxSpreadSheet::Column::STRING, 0);
    ss->addColumn("Solution", HxSpreadSheet::Column::STRING, 0);

    checkBulbousLabels(graph, ss);
    checkTimeLabels(graph, ss);
    checkFilopodiaGaps(graph, ss);
    for (int t = mTimeMinMax.minT; t <= mTimeMinMax.maxT; ++t)
    {
        checkConsistencyAfterGeometryChange(graph, t, ss);
    }
    QTime endTime = QTime::currentTime();
    qDebug() << "\ncheck consistency:" << startTime.msecsTo(endTime) << "msec";
}

void
QxFilopodiaTool::updateLabelAfterGeometryChange(HxSpatialGraph* graph)
{
    qDebug() << "\n\tupdateLabelsAfterGeometryChange";

    updateLocationLabels();

    if (mUi.trackingCheckBox->checkState())
    {
        updateTracking();
    }
    else
    {
        mUi.propagateAllButton->setEnabled(false);
        mUi.propagateSelButton->setEnabled(false);
    }
}

float
QxFilopodiaTool::getIntensityWeight() const
{
    return mUi.intensityLineEdit->text().toFloat();
}

void
QxFilopodiaTool::toggleBoxDragger(bool value)
{
    HxViewer* viewer = mEditor->get3DViewer();
    if (!viewer)
    {
        return;
    }

    SoNode* sceneGraph = viewer->getSceneGraph();
    if (!sceneGraph)
    {
        return;
    }

    if (value)
    {
        mBoxDragger = new SoTabBoxDraggerVR();
        mBoxDragger->addValueChangedCallback(boxDraggerChangedCB, this);
        sceneGraph->internalAddChild(mBoxDragger);
        updateBoxDragger();
    }
    else
    {
        sceneGraph->internalRemoveChild(mBoxDragger);
        mBoxDragger = 0;
    }
}

void
QxFilopodiaTool::boxDraggerChangedCB(void* userData, SoDragger*)
{
    QxFilopodiaTool* tool = (QxFilopodiaTool*)userData;
    tool->boxDraggerChanged();
}

BoxSpec
QxFilopodiaTool::fixBoxSpec(const BoxSpec& spec)
{
    BoxSpec fixed;
    fixed.size = spec.size;
    fixed.offset = spec.offset;

    for (int i = 0; i <= 2; ++i)
    {
        if (fixed.size[i] < 1)
        {
            fixed.size[i] = 1;
        }
        if (fixed.size[i] > mImage->lattice().getDims()[i])
        {
            fixed.size[i] = mImage->lattice().getDims()[i];
        }
    }

    return fixed;
}

void
QxFilopodiaTool::boxDraggerChanged()
{
    if (!mImage)
    {
        return;
    }

    const QString currentGCLabel = mUi.growthConeComboBox->currentText();
    const int currentGCid = FilopodiaFunctions::getGrowthConeIdFromLabel(mGraph, currentGCLabel);

    if (currentGCid == -1)
    {
        return;
    }

    const int rootNode = FilopodiaFunctions::getRootNodeFromTimeAndGC(mGraph, mCurrentTime, currentGCid);

    if (rootNode == -1)
    {
        return;
    }

    const SbVec3f t = mBoxDragger->translation.getValue();
    const SbVec3f s = mBoxDragger->scaleFactor.getValue();

    const McVec3f rootCoords = mGraph->getVertexCoords(rootNode);
    const McDim3l rootVoxel = FilopodiaFunctions::getNearestVoxelCenterFromPointCoords(rootCoords, mImage);
    const McVec3f voxelSize = mImage->getVoxelSize();

    BoxSpec newSpec;
    for (int i = 0; i <= 2; ++i)
    {
        newSpec.offset[i] = int(McRoundEven((t[i] - s[i]) / voxelSize[i]) - rootVoxel[i]);
        newSpec.size[i] = int(McRoundEven(s[i] * 2.0f / voxelSize[i]));
    }

    const BoxSpec fixedSpec = fixBoxSpec(newSpec);
    const BoxSpec currentSpec = mBoxSpecs.value(currentGCid);

    if (fixedSpec.size != currentSpec.size ||
        fixedSpec.offset != currentSpec.offset)
    {
        mBoxSpecs[currentGCid] = fixedSpec;
        setBoxSpinBoxes();
    }
}

void
QxFilopodiaTool::updateGrowthConeList()
{
    if (!mGraph || !FilopodiaFunctions::isRootNodeGraph(mGraph))
    {
        mUi.growthConeComboBox->clear();
        return;
    }

    const QString currentText = mUi.growthConeComboBox->currentText();

    QStringList labels;
    QList<int> ids;

    const int numGC = FilopodiaFunctions::getNumberOfGrowthCones(mGraph);
    int indexToSet = 0;

    for (int i = 0; i < numGC; ++i)
    {
        const QString label = FilopodiaFunctions::getGrowthConeLabelFromNumber(mGraph, i);
        labels.append(label);
        if (label == currentText)
        {
            indexToSet = i;
        }

        const int gcID = FilopodiaFunctions::getGrowthConeIdFromLabel(mGraph, label);
        ids.append(gcID);
    }

    mUi.growthConeComboBox->clear();
    mUi.growthConeComboBox->addItems(labels);
    mUi.growthConeComboBox->setCurrentIndex(indexToSet);

    const QList<int> existingKeys = mBoxSpecs.keys();
    for (int k = 0; k < existingKeys.size(); ++k)
    {
        if (!ids.contains(existingKeys[k]))
        {
            mBoxSpecs.remove(existingKeys[k]);
        }
    }

    for (int k = 0; k < ids.size(); ++k)
    {
        if (!mBoxSpecs.contains(ids[k]))
        {
            BoxSpec spec;
            mBoxSpecs.insert(ids[k], spec);
        }
    }

    setBoxSpinBoxes();
}

void
QxFilopodiaTool::updateBoxDragger()
{
    if (!mBoxDragger || !mImage)
    {
        return;
    }

    if (!FilopodiaFunctions::isFilopodiaGraph(mGraph) && !FilopodiaFunctions::isRootNodeGraph(mGraph))
    {
        toggleBoxDragger(false);
        return;
    }

    const QString currentGCLabel = mUi.growthConeComboBox->currentText();
    const int currentGCid = FilopodiaFunctions::getGrowthConeIdFromLabel(mGraph, currentGCLabel);

    if (currentGCid == -1)
    {
        return;
    }

    const int rootNode = FilopodiaFunctions::getRootNodeFromTimeAndGC(mGraph, mCurrentTime, currentGCid);

    if (rootNode == -1)
    {
        return;
    }

    const McVec3f rootCoords = mGraph->getVertexCoords(rootNode);
    HxUniformCoord3* coords = dynamic_cast<HxUniformCoord3*>(mImage->lattice().coords());
    HxLoc3Uniform* location = dynamic_cast<HxLoc3Uniform*>(coords->createLocation());

    if (!location->set(rootCoords))
    {
        qDebug() << "Root coords not in image";
        toggleBoxDragger(false);
        return;
    }

    const McDim3l rootVoxel = FilopodiaFunctions::getNearestVoxelCenterFromPointCoords(rootCoords, mImage);
    const BoxSpec& spec = mBoxSpecs.value(currentGCid);

    const McVec3f voxelSize = mImage->getVoxelSize();
    const McVec3f scale = 0.5f * McVec3f(spec.size.i, spec.size.j, spec.size.k).compprod(voxelSize);
    const McVec3f translate = McVec3f(rootVoxel.nx + spec.offset.i,
                                      rootVoxel.ny + spec.offset.j,
                                      rootVoxel.nz + spec.offset.k)
                                  .compprod(voxelSize) +
                              scale;

    mBoxDragger->scaleFactor.setValue(scale.x, scale.y, scale.z);
    mBoxDragger->translation.setValue(translate.x, translate.y, translate.z);
}

void
QxFilopodiaTool::setBoxSpinBoxes()
{
    if (!mGraph ||
        (!FilopodiaFunctions::isFilopodiaGraph(mGraph) && !FilopodiaFunctions::isRootNodeGraph(mGraph)))
    {
        const BoxSpec spec;
        mUi.boxSizeXSpinBox->setValue(spec.size.i);
        mUi.boxSizeYSpinBox->setValue(spec.size.j);
        mUi.boxSizeZSpinBox->setValue(spec.size.k);
        mUi.offsetXSpinBox->setValue(spec.offset.i);
        mUi.offsetYSpinBox->setValue(spec.offset.j);
        mUi.offsetZSpinBox->setValue(spec.offset.k);
        return;
    }

    const QString currentGCLabel = mUi.growthConeComboBox->currentText();
    const int currentGCid = FilopodiaFunctions::getGrowthConeIdFromLabel(mGraph, currentGCLabel);

    const BoxSpec& spec = mBoxSpecs.value(currentGCid);
    mUi.boxSizeXSpinBox->setValue(spec.size.i);
    mUi.boxSizeYSpinBox->setValue(spec.size.j);
    mUi.boxSizeZSpinBox->setValue(spec.size.k);
    mUi.offsetXSpinBox->setValue(spec.offset.i);
    mUi.offsetYSpinBox->setValue(spec.offset.j);
    mUi.offsetZSpinBox->setValue(spec.offset.k);
}

void
QxFilopodiaTool::boxSpinBoxChanged()
{
    if (!mGraph || !FilopodiaFunctions::hasGrowthConeLabels(mGraph))
    {
        return;
    }

    const QString currentGCLabel = mUi.growthConeComboBox->currentText();
    const int currentGCid = FilopodiaFunctions::getGrowthConeIdFromLabel(mGraph, currentGCLabel);

    BoxSpec& spec = mBoxSpecs[currentGCid];
    spec.size[0] = mUi.boxSizeXSpinBox->value();
    spec.size[1] = mUi.boxSizeYSpinBox->value();
    spec.size[2] = mUi.boxSizeZSpinBox->value();
    spec.offset[0] = mUi.offsetXSpinBox->value();
    spec.offset[1] = mUi.offsetYSpinBox->value();
    spec.offset[2] = mUi.offsetZSpinBox->value();

    updateBoxDragger();
}

// CTS - contour tree segmentation
void
QxFilopodiaTool::autoEstimateBoundingBoxes()
{
    HxSpatialGraph* rootNodesGraph = mEditor->getSpatialGraph();
    if (!rootNodesGraph)
    {
        return;
    }
    if (!mImage)
    {
        return;
    }

    const int threshold = getThreshold();
    const int persistence = getPersistence();

    const int numberOfGrowthCones = FilopodiaFunctions::getNumberOfGrowthCones(rootNodesGraph);

    for (int n = 0; n < numberOfGrowthCones; ++n)
    {
        const int growthConeId = FilopodiaFunctions::getGrowthConeLabelIdFromNumber(rootNodesGraph, n);
        const int v = FilopodiaFunctions::getRootNodeFromTimeAndGC(rootNodesGraph, mCurrentTime, growthConeId);
        if (v == -1)
        {
            HxMessage::warning(QString("Cannot estimate bounding box for growth cone %1, time %2: no root node found")
                                   .arg(mCurrentTime)
                                   .arg(growthConeId),
                               "Ok");
            continue;
        }

        McHandle<HxContourTreeSegmentation> contourTreeSegmentation = HxContourTreeSegmentation::createInstance();
        contourTreeSegmentation->portData.connect(mImage);
        contourTreeSegmentation->portAutoSetValues.setValue(0, false);
        contourTreeSegmentation->fire();
        contourTreeSegmentation->portThreshold.setMinMax(0, 255);
        contourTreeSegmentation->portThreshold.setValue(threshold);
        contourTreeSegmentation->portPersistenceValue.setValue(persistence);
        contourTreeSegmentation->portMinimalSegmentSize.setValue(1000);
        contourTreeSegmentation->fire();
        contourTreeSegmentation->portDoIt.hit();
        contourTreeSegmentation->fire();

        McHandle<HxUniformLabelField3> labels = dynamic_cast<HxUniformLabelField3*>(contourTreeSegmentation->getResult());
        if (!labels)
        {
            HxMessage::warning(QString("Cannot create segmentation for growth cone %1 in time step %2.").arg(growthConeId).arg(mCurrentTime), "ok");
            continue;
        }

        labels->setLabel("Labels");

        SpatialGraphSelection singleRootSel(rootNodesGraph);
        singleRootSel.selectVertex(v);
        McHandle<HxSpatialGraph> singleRootGraph = rootNodesGraph->getSubgraph(singleRootSel);

        McHandle<HxSplitLabelField> splitLabelField = HxSplitLabelField::createInstance();
        splitLabelField->portData.connect(labels);
        splitLabelField->portImage.connect(mImage);
        splitLabelField->portSeeds.connect(singleRootGraph);
        splitLabelField->portBoundarySize.setValue(10);
        splitLabelField->fire();
        splitLabelField->portDoIt.hit();
        splitLabelField->fire();

        McHandle<HxUniformLabelField3> croppedLabels = dynamic_cast<HxUniformLabelField3*>(splitLabelField->getResult(0));
        if (!croppedLabels)
        {
            HxMessage::warning(QString("Cannot crop growth cone %1 in time step %2: Growth cone center is not in object.\nPlease move seed and redo cropping.\n")
                                   .arg(growthConeId)
                                   .arg(mCurrentTime),
                               "ok");
            continue;
        }

        croppedLabels->setLabel("CroppedLabels");

        const McDim3l dims = croppedLabels->lattice().getDims();
        const McVec3f rootCoords = rootNodesGraph->getVertexCoords(v);
        const McDim3l rootVoxel = FilopodiaFunctions::getNearestVoxelCenterFromPointCoords(rootCoords, mImage);

        const McVec3f croppedLabelsBoxMin = croppedLabels->getBoundingBox().getMin();
        const McDim3l croppedLabelsBoxMinVoxel = FilopodiaFunctions::getNearestVoxelCenterFromPointCoords(croppedLabelsBoxMin, mImage);

        BoxSpec spec;
        for (int i = 0; i <= 2; ++i)
        {
            spec.size[i] = dims[i];
            spec.offset[i] = croppedLabelsBoxMinVoxel[i] - rootVoxel[i];
        }

        mBoxSpecs.insert(growthConeId, spec);

        mUi.showBBoxCheckBox->setCheckState(Qt::Checked);

        setBoxSpinBoxes();
    }
}

McBox3i
getCropCoords(const BoxSpec& boxSpec,
              const HxUniformScalarField3* image,
              const McVec3f rootCoords)
{
    HxUniformCoord3* coords = dynamic_cast<HxUniformCoord3*>(image->lattice().coords());
    HxLoc3Uniform* location = dynamic_cast<HxLoc3Uniform*>(coords->createLocation());

    if (!location->set(rootCoords))
    {
        throw McException("Error computing crop coordinates: Invalid root point");
    }

    const McDim3l dims = image->lattice().getDims();
    const McDim3l rootLoc = McDim3l(location->getIx(), location->getIy(), location->getIz());
    const McVec3f rootU = McVec3f(location->getUx(), location->getUy(), location->getUz());
    const McDim3l rootNearestVoxelCenter = FilopodiaFunctions::getNearestVoxelCenterFromIndex(rootLoc, rootU, dims);
    const McVec3i rootIdx(int(rootNearestVoxelCenter.nx),
                          int(rootNearestVoxelCenter.ny),
                          int(rootNearestVoxelCenter.nz));

    delete location;

    McVec3i cropMin = rootIdx + boxSpec.offset;
    McVec3i cropMax = cropMin + boxSpec.size;

    for (int i = 0; i <= 2; ++i)
    {
        if (cropMin[i] < 0)
        {
            cropMin[i] = 0;
        }
        if (cropMax[i] >= dims[i])
        {
            cropMax[i] = dims[i] - 1;
        }
    }

    return McBox3i(cropMin, cropMax);
}

void
QxFilopodiaTool::computeCropAndDijkstra()
{
    const QTime startTime = QTime::currentTime();
    qDebug() << "Crop images per each growth cone and calculate Dijkstra maps.";
    theMsg->printf("Crop images per each growth cone and calculate Dijkstra maps.");

    HxSpatialGraph* rootNodesGraph = mEditor->getSpatialGraph();
    if (!rootNodesGraph)
    {
        return;
    }

    if (mImages.isEmpty())
    {
        HxMessage::error(QString("Cannot prepare files: Please select image directory.\n"), "ok");
        return;
    }

    if (mOutputDir.length() == 0)
    {
        HxMessage::error(QString("Cannot prepare files: Please select output directory.\n"), "ok");
        return;
    }

    // User input parameters
    const float intensityWeight = getIntensityWeight();
    const int topBrightness = getTopBrightness();

    const int numberOfGrowthCones = FilopodiaFunctions::getNumberOfGrowthCones(rootNodesGraph);

    for (int n = 0; n < numberOfGrowthCones; ++n)
    {
        const int growthConeId = FilopodiaFunctions::getGrowthConeLabelIdFromNumber(rootNodesGraph, n);
        if (mBoxSpecs.value(growthConeId).isUndefined())
        {
            const QString gcLabel = FilopodiaFunctions::getGrowthConeLabelFromNumber(rootNodesGraph, n);
            HxMessage::error(QString("Cannot prepare files: Bounding box for growth cone %1 has not been defined.\n").arg(gcLabel), "ok");
            return;
        }
    }

    for (int n = 0; n < numberOfGrowthCones; ++n)
    {
        const int growthConeNumber = n + 1;
        const int growthConeId = FilopodiaFunctions::getGrowthConeLabelIdFromNumber(rootNodesGraph, n);
        QString gcFolder = QString(mOutputDir + "/GrowthCone_%1").arg(growthConeNumber);
        if (!QDir(gcFolder).exists())
        {
            QDir().mkdir(gcFolder);
        }

        // Save a SpatialGraph for each growth cone
        const SpatialGraphSelection nodesOfGc = FilopodiaFunctions::getNodesWithGcId(rootNodesGraph, growthConeId);
        const SpatialGraphSelection rootsOfGc = FilopodiaFunctions::getNodesOfTypeInSelection(rootNodesGraph, nodesOfGc, ROOT_NODE);
        McHandle<HxSpatialGraph> rootsForGCGraph = rootNodesGraph->getSubgraph(rootsOfGc);
        rootsForGCGraph->setLabel(QString("rootNodes_gc%1").arg(n));

        // Add filopodia labels to result tree
        McHandle<HxSpatialGraph> resultForGCGraph = rootsForGCGraph->duplicate();
        FilopodiaFunctions::addTimeLabelAttribute(resultForGCGraph, mTimeMinMax);
        FilopodiaFunctions::addManualGeometryLabelAttribute(resultForGCGraph);
        FilopodiaFunctions::addTypeLabelAttribute(resultForGCGraph);
        FilopodiaFunctions::addManualNodeMatchLabelAttribute(resultForGCGraph);
        FilopodiaFunctions::addLocationLabelAttribute(resultForGCGraph);
        FilopodiaFunctions::addFilopodiaLabelAttribute(resultForGCGraph);

        rootsForGCGraph->saveAmiraMeshASCII(qPrintable(QString(gcFolder + "/rootNodes_gc%1.am").arg(growthConeNumber)));
        resultForGCGraph->saveAmiraMeshASCII(qPrintable(QString(gcFolder + "/resultTree_gc%1.am").arg(growthConeNumber)));

        // Save cropped image and Dijkstra map for each root node
        QString grayFolder = gcFolder + "/Gray";
        if (!QDir(grayFolder).exists())
        {
            QDir().mkdir(grayFolder);
        }

        QString dijkstraFolder = gcFolder + "/Dijkstra";
        if (!QDir(dijkstraFolder).exists())
        {
            QDir().mkdir(dijkstraFolder);
        }

        SpatialGraphSelection::Iterator nodeIt(rootsOfGc);
        for (int v = nodeIt.vertices.nextSelected(); v != -1; v = nodeIt.vertices.nextSelected())
        {
            SpatialGraphSelection singleRootSel(rootNodesGraph);
            singleRootSel.selectVertex(v);

            const int timeId = FilopodiaFunctions::getTimeIdOfNode(rootNodesGraph, v);
            const int time = FilopodiaFunctions::getTimeStepFromTimeId(rootNodesGraph, timeId);
            qDebug() << "Processing GC" << growthConeId << "Time" << time;

            McHandle<HxUniformScalarField3> croppedImage = mImages.value(time)->duplicate();
            const McVec3f rootCoords = rootNodesGraph->getVertexCoords(v);
            const McBox3i cropCoords = getCropCoords(mBoxSpecs.value(growthConeId), croppedImage, rootCoords);
            const McVec3i cropMin = cropCoords.getMin();
            const McVec3i cropMax = cropCoords.getMax();
            croppedImage->lattice().crop(cropMin.getValue(), cropMax.getValue(), "");
            const McDim3l dims = croppedImage->lattice().getDims();

            const QString grayFileName = QString("gray_gc%1_t%2.am").arg(growthConeId).arg(time, 2, 10, QChar('0'));
            const QString grayFullPath = grayFolder + "/" + grayFileName;
            qDebug() << "Writing" << grayFullPath;
            croppedImage->writeAmiraMeshBinary(qPrintable(grayFullPath));

            // Compute dijkstra maps
            McHandle<HxUniformLabelField3> dijkstraMap = HxUniformLabelField3::createInstance();
            dijkstraMap->setLabel("DijkstraMap");

            McHandle<HxUniformScalarField3> distanceMap = HxUniformScalarField3::createInstance();

            HxShortestPathToPointMap::computeDijkstraMap(croppedImage,
                                                         rootCoords,
                                                         McDim3l(0, 0, 0),
                                                         McDim3l(dims[0] - 1, dims[1] - 1, dims[2] - 1),
                                                         intensityWeight,
                                                         topBrightness,
                                                         dijkstraMap,
                                                         distanceMap);

            const QString dijkstraFileName = QString("dijkstra_gc%1_t%2.am").arg(growthConeId).arg(time, 2, 10, QChar('0'));
            const QString dijkstraFullPath = dijkstraFolder + "/" + dijkstraFileName;
            qDebug() << "Writing" << dijkstraFullPath;
            dijkstraMap->writeAmiraMeshRLE(qPrintable(dijkstraFullPath));
        }
    }

    rootNodesGraph->saveAmiraMeshASCII(qPrintable(mOutputDir + "/gc_track.am"));

    const QTime endTime = QTime::currentTime();
    qDebug() << "Processing time:" << startTime.secsTo(endTime) << "sec.";
}

int
QxFilopodiaTool::getTopBrightness() const
{
    const int useTopBrightness = mUi.brightnessCheckBox->checkState();

    if (useTopBrightness == Qt::Checked)
    {
        return mUi.topBrightnessLineEdit->text().toInt();
    }
    else
    {
        return 255;
    }
}

int
QxFilopodiaTool::getThreshold() const
{
    return mUi.threshLineEdit->text().toInt();
}

int
QxFilopodiaTool::getPersistence() const
{
    return mUi.persistenceLineEdit->text().toInt();
}

void
QxFilopodiaTool::updateAfterSpatialGraphChange(HxNeuronEditorSubApp::SpatialGraphChanges changes)
{
    if (HxNeuronEditorSubApp::SpatialGraphDataSetChange & changes)
    {
        HxSpatialGraph* graph = mEditor->getSpatialGraph();
        if (!graph)
        {
            mGraph = 0;
            mTimeMinMax = TimeMinMax();
            mCurrentTime = -1;
        }
        else if (graph != mGraph)
        {
            mGraph = graph;
            updateTimeMinMax();
            setTime(mTimeMinMax.minT);
        }
    }

    if (mGraph && FilopodiaFunctions::isFilopodiaGraph(mGraph))
    {
        if (HxNeuronEditorSubApp::SpatialGraphGeometryChange & changes)
        {
            HxSpatialGraph* graph = mEditor->getSpatialGraph();
            if (graph != mGraph)
            {
                mGraph = graph;
            }
            updateLabelAfterGeometryChange(graph);
        }
        updateGraphVisibility();
    }

    if (HxNeuronEditorSubApp::SpatialGraphLabelTreeChange & changes ||
        HxNeuronEditorSubApp::SpatialGraphLabelChange & changes ||
        HxNeuronEditorSubApp::SpatialGraphDataSetChange & changes)
    {
        updateGrowthConeList();
    }

    updateToolboxState();
}

void
addTimeStepToSelection(const int time, const HxSpatialGraph* graph, SpatialGraphSelection& sel)
{
    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        if (FilopodiaFunctions::getTimeIdOfNode(graph, v) == FilopodiaFunctions::getTimeIdFromTimeStep(graph, time))
        {
            sel.selectVertex(v);
        }
    }
    for (int e = 0; e < graph->getNumEdges(); ++e)
    {
        if (FilopodiaFunctions::getTimeIdOfEdge(graph, e) == FilopodiaFunctions::getTimeIdFromTimeStep(graph, time))
        {
            sel.selectEdge(e);
        }
    }
}

void
removeGCFromSelection(const HxSpatialGraph* graph, SpatialGraphSelection& sel)
{
    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        if (FilopodiaFunctions::nodeHasLocation(GROWTHCONE, v, graph))
        {
            sel.deselectVertex(v);
        }
    }
    for (int e = 0; e < graph->getNumEdges(); ++e)
    {
        if (FilopodiaFunctions::edgeHasLocation(GROWTHCONE, e, graph))
        {
            sel.deselectEdge(e);
        }
    }
}

void
QxFilopodiaTool::updateGraphVisibility()
{
    const HxSpatialGraph* graph = mEditor->getSpatialGraph();
    if (!graph)
    {
        return;
    }

    if (!FilopodiaFunctions::isFilopodiaGraph(graph) && !FilopodiaFunctions::isRootNodeGraph(graph))
    {
        return;
    }

    const int showAllTimeStepsState = mUi.showAllTimeStepsCheckBox->checkState();
    const int showFiloState = mUi.filoCheckBox->checkState();

    const int numBefore = mUi.beforeSpinBox->value();
    const int numAfter = mUi.afterSpinBox->value();

    SpatialGraphSelection sel(graph);

    if (showAllTimeStepsState != Qt::Checked)
    {
        sel.clear();
        for (int t = mCurrentTime - numBefore; t <= mCurrentTime + numAfter; ++t)
        {
            addTimeStepToSelection(t, graph, sel);
        }
    }
    else
    {
        sel.selectAllVerticesAndEdges();
    }

    if (showFiloState == Qt::Checked && FilopodiaFunctions::isFilopodiaGraph(graph))
    {
        removeGCFromSelection(graph, sel);
    }

    sel.selectAllPointsOnEdgesFromSelection(sel);

    mEditor->setVisibleElements(sel);
}

void
QxFilopodiaTool::updateLocationLabels()
{
    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    try
    {
        FilopodiaFunctions::assignGrowthConeAndFilopodiumLabels(graph);
        mEditor->updateAfterSpatialGraphChange(HxNeuronEditorSubApp::SpatialGraphLabelChange);
    }
    catch (McException& e)
    {
        theMsg->printf(QString("%1").arg(e.what()));
    }
}

void
QxFilopodiaTool::updateStatistic()
{
    theMsg->printf("Create spreadsheets for filopodia statistics.");
    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    if (!graph)
    {
        return;
    }

    QxFilopodiaTool::updateConsistencyTable();
    if (mConsistency->nRows() > 0)
    {
        HxMessage::error(QString("Cannot update statistic spreadsheets.\nGraph is not consistent. Please check table of inconsistencies.\n"), "ok");
        return;
    }

    // Fill selection per existing label
    const char* filopodiaAttName = FilopodiaFunctions::getFilopodiaAttributeName();
    HierarchicalLabels* filoLabels = graph->getLabelGroup(filopodiaAttName);
    const McDArray<int> existingLabelIds = filoLabels->getChildIds(0);

    QMap<int, SpatialGraphSelection> selectionPerLabel;

    for (int i = 0; i < existingLabelIds.size(); ++i)
    {
        selectionPerLabel.insert(existingLabelIds[i], SpatialGraphSelection(graph));
    }
    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        const int filoId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, v);
        SpatialGraphSelection& sel = selectionPerLabel[filoId];
        sel.selectVertex(v);
    }
    for (int e = 0; e < graph->getNumEdges(); ++e)
    {
        const int filoId = FilopodiaFunctions::getFilopodiaIdOfEdge(graph, e);
        SpatialGraphSelection& sel = selectionPerLabel[filoId];
        sel.selectEdge(e);
    }
    selectionPerLabel.remove(0); // Do not reuse root label
    selectionPerLabel.remove(FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED));
    selectionPerLabel.remove(FilopodiaFunctions::getFilopodiaLabelId(graph, AXON));
    selectionPerLabel.remove(FilopodiaFunctions::getFilopodiaLabelId(graph, IGNORED));

    QList<int> emptyIds;
    QList<int> usedIds;

    // Find used and unused (empty) labels
    for (QMap<int, SpatialGraphSelection>::ConstIterator it = selectionPerLabel.constBegin();
         it != selectionPerLabel.constEnd();
         ++it)
    {
        const SpatialGraphSelection& sel = it.value();
        if (sel.isEmpty())
        {
            emptyIds.append(it.key());
        }
        else
        {
            usedIds.append(it.key());
        }
    }

    QList<int> labelsToRemove;
    for (int i = 0; i < emptyIds.size(); ++i)
    {
        const int emptyId = emptyIds[i];
        if (usedIds.size() > 0 && usedIds.last() > emptyId)
        {
            const int usedId = usedIds.last();
            AssignLabelOperation op(graph, selectionPerLabel[usedId], SpatialGraphSelection(graph), QString::fromLatin1(filopodiaAttName), emptyId);
            op.exec();
            usedIds.removeLast();
            labelsToRemove.append(usedId);

            // Reuse the color so the user sees no visual changes due to relabeling
            SbColor color;
            filoLabels->getLabelColor(usedId, color);
            filoLabels->setLabelColor(emptyId, McColor(color[0], color[1], color[2]));
        }
        else
        {
            labelsToRemove.append(emptyId);
        }
    }

    qDebug() << "Removing" << labelsToRemove.size() << "labels";

    for (int i = 0; i < labelsToRemove.size(); ++i)
    {
        McDArray<int> removedLabels;
        filoLabels->removeLabel(labelsToRemove[i], removedLabels);
    }

    if (labelsToRemove.size() > 0)
    {
        mEditor->updateAfterSpatialGraphChange(HxNeuronEditorSubApp::SpatialGraphLabelTreeChange);
    }

    // Compute statistics
    try
    {
        if (!mFilamentStats)
        {
            mFilamentStats = HxSpreadSheet::createInstance();
            mFilamentStats->composeLabel(graph->getLabel(), "FilamentStatistics");
            theObjectPool->addObject(mFilamentStats);
        }

        if (!mFilopodiaStats)
        {
            mFilopodiaStats = HxSpreadSheet::createInstance();
            mFilopodiaStats->composeLabel(graph->getLabel(), "FilopodiaStatistics");
            theObjectPool->addObject(mFilopodiaStats);
        }

        if (!mLengthStats)
        {
            mLengthStats = HxSpreadSheet::createInstance();
            mLengthStats->composeLabel(graph->getLabel(), "LengthStatistics");
            theObjectPool->addObject(mLengthStats);
        }

        const float speedFilter = mUi.speedFilterLineEdit->text().toFloat();
        HxFilopodiaStats::createFilamentTab(graph, mFilamentStats);
        HxFilopodiaStats::createLengthTab(graph, mLengthStats, mFilamentStats);
        HxFilopodiaStats::createFilopodiaTab(mGraph, mFilopodiaStats, mFilamentStats, speedFilter);

        mFilamentStats->portShow.touch();
        mFilamentStats->update();

        mFilopodiaStats->portShow.touch();
        mFilopodiaStats->update();

        mLengthStats->portShow.touch();
        mLengthStats->update();
    }
    catch (McException& e)
    {
        theMsg->printf(QString("%1").arg(e.what()));
    }
}

void
QxFilopodiaTool::updateTracking()
{
    updateLocationLabels();
    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    const float distThreshold = mUi.distanceThresholdLineEdit->text().toFloat();

    const HxNeuronEditorSubApp::SpatialGraphChanges changes = HxFilopodiaTrack::trackFilopodia(graph, distThreshold, false);
    if (changes)
    {
        mEditor->updateAfterSpatialGraphChange(changes);
    }
    mUi.propagateAllButton->setEnabled(true);
    mUi.propagateSelButton->setEnabled(true);
}

void
QxFilopodiaTool::selectDir()
{
    mDir = QFileDialog::getExistingDirectory(theApp->mainWidget(), tr("Select image Directory"), QString(), QFileDialog::ShowDirsOnly);

    if (!mDir.isEmpty())
    {
        QString imageDirStr = mDir + "/Gray";
        QDir imageDir(imageDirStr);
        QString dijkstraDirStr = mDir + "/Dijkstra";
        QDir dijkstraDir(dijkstraDirStr);

        if (imageDir.exists())
        {
            mImageDir = imageDirStr;
        }
        else
        {
            mImageDir = mDir;
        }

        if (dijkstraDir.exists())
        {
            mDijkstraDir = dijkstraDirStr;
        }

        mUi.imageDirValueLabel->setText(mDir);
        mUi.imageDirValueLabel->setToolTip(mDir);

        updateFiles();
        updateTimeMinMax();
        theMsg->printf(QString("Select input dir %1").arg(mDir));

        if (mCurrentTime >= mTimeMinMax.minT && mCurrentTime <= mTimeMinMax.maxT)
        {
            setImageForCurrentTime(); // Toolbox initialized before, retain time.
        }
        else
        {
            setTime(mTimeMinMax.minT); // First use of toolbox.
        }
    }
    if (mImageDir.isEmpty())
    {
        const QString defaultText("<Please select>");
        mUi.imageDirValueLabel->setText(defaultText);
        mUi.imageDirValueLabel->setToolTip(defaultText);
    }
}

void
QxFilopodiaTool::selectOutputDir()
{
    mOutputDir = QFileDialog::getExistingDirectory(theApp->mainWidget(), tr("Select output Directory"), QString(), QFileDialog::ShowDirsOnly);

    mUi.outputDirValueLabel->setText(mOutputDir);
    mUi.outputDirValueLabel->setToolTip(mOutputDir);
    theMsg->printf(QString("Select output dir %1").arg(mOutputDir));
}

void
QxFilopodiaTool::setTime(int time)
{
    if (mCurrentTime == time)
    {
        return;
    }

    mCurrentTime = time;

    mUi.timeSlider->setValue(mCurrentTime);
    mUi.timeSpinBox->setValue(mCurrentTime);

    setImageForCurrentTime();
    updateGraphVisibility();
    updateBoxDragger();
    setBoxSpinBoxes();
    theMsg->printf("\nTimestep: %i", time);
}

void
QxFilopodiaTool::setEmptyImage()
{
    if (!mImage)
    {
        return;
    }

    const QString warning(QString("No image for time step %1").arg(mCurrentTime));

    HxLattice3& lat = mImage->lattice();
    const McDim3l& dims = lat.getDims();
    const float zero = 0.0f;
    for (int z = 0; z < dims.nz; ++z)
    {
        for (int y = 0; y < dims.ny; ++y)
        {
            for (int x = 0; x < dims.nx; ++x)
            {
                lat.set(x, y, z, &zero);
            }
        }
    }
    mImage->setLabel(warning);
}

void
QxFilopodiaTool::connectConsistency()
{
    theObjectPool->addObject(mConsistency);
    mConsistency->portShow.touch();
    mConsistency->update();
    QxSpreadSheetViewer* consistencyViewer = mConsistency->getViewer();
    if (consistencyViewer)
    {
        try
        {
            connect(consistencyViewer, SIGNAL(cellClicked(int, int)), this, SLOT(updateConsistencySelection(int, int)));
            consistencyViewer->show();
        }
        catch (McException& e)
        {
            theMsg->printf(QString("%1").arg(e.what()));
        }
    }
}

void
QxFilopodiaTool::updateToolboxState()
{
    mUi.VisualizationGroupBox->setEnabled(true);

    if (mGraph)
    {
        const bool isFiloGraph = FilopodiaFunctions::isFilopodiaGraph(mGraph);
        const bool isRootNodeGraph = FilopodiaFunctions::isRootNodeGraph(mGraph);

        if (isFiloGraph)
        {
            connectConsistency();
        }
        else
        {
            theObjectPool->removeObject(mConsistency);
        }

        // Filo tab
        mUi.filoStartGroupBox->setEnabled(true);
        mUi.trackingGroupBox->setEnabled(isFiloGraph);
        mUi.PropagateGroupBox->setEnabled(isFiloGraph);
        mUi.statisticGroupBox->setEnabled(isFiloGraph);
        mUi.addFilopodiaLabelsButton->setEnabled(!isFiloGraph);

        // Root tab
        mUi.propRootGroupBox->setEnabled(!isRootNodeGraph);
        mUi.boundingBoxGroupBox->setEnabled(isRootNodeGraph);
    }
    else
    {
        // Filo tab
        mUi.filoStartGroupBox->setEnabled(false);
        mUi.trackingGroupBox->setEnabled(false);
        mUi.PropagateGroupBox->setEnabled(false);
        mUi.statisticGroupBox->setEnabled(false);

        // Root tab
        mUi.propRootGroupBox->setEnabled(false);
        mUi.boundingBoxGroupBox->setEnabled(false);
    }
}

void
QxFilopodiaTool::setImageForCurrentTime()
{
    if (mImages.contains(mCurrentTime))
    {
        HxUniformScalarField3* newImage = mImages.value(mCurrentTime);

        if (!newImage)
        {
            throw McException(QString("No image data for time step %1.").arg(mCurrentTime));
        }

        if (mImage)
        {
            HxUniformScalarField3* oldImage = mImage;
            mImage = newImage;
            McPlane previousPlaneSettings = mEditor->getMPRViewer()->slice()->plane();
            VsSlice* newPlane = mEditor->getMPRViewer()->slice();

            McHandle<VsCamera> cam = mEditor->getMPRViewer()->camera();
            McRotation rotation = cam->orientation();
            McVec3f position = cam->position();
            float height = cam->height();

            theObjectPool->addObject(newImage);
            mEditor->updateAfterImageChange(false);

            if (oldImage != newImage)
            {
                theObjectPool->removeObject(oldImage);
                newPlane->setPlane(previousPlaneSettings);
                newPlane->touch();
                cam->setOrientation(rotation);
                cam->setPosition(position);
                cam->setHeight(height);
            }
        }
        else
        {
            mImage = newImage;
            theObjectPool->addObject(newImage);
            mEditor->updateAfterImageChange();
        }
    }

    if (mDijkstras.contains(mCurrentTime))
    {
        mDijkstra = mDijkstras.value(mCurrentTime);

        if (!mDijkstra)
        {
            throw McException(QString("No dijkstra map for time step %1.").arg(mCurrentTime));
        }
    }
}

void
QxFilopodiaTool::updateFiles()
{
    mImages.clear();
    mDijkstras.clear();

    try
    {
        const QDir imageDir(mImageDir);
        QFileInfoList fileInfoListImages = imageDir.entryInfoList(QDir::Files);

        for (int f = 0; f < fileInfoListImages.size(); ++f)
        {
            const QString file = fileInfoListImages[f].fileName();
            if (!file.endsWith(".am"))
            {
                continue;
            }

            const int time = FilopodiaFunctions::getTimeFromFileNameNoGC(fileInfoListImages[f].fileName());

            if (mImages.contains(time))
            {
                throw McException(QString("Duplicate timestamp %1").arg(time));
            }

            McHandle<HxUniformScalarField3> image = FilopodiaFunctions::loadUniformScalarField3(fileInfoListImages[f].filePath());
            if (!image)
            {
                throw McException(QString("Could not read image from file %1").arg(fileInfoListImages[f].filePath()));
            }

            if (image->primType() == McPrimType::MC_FLOAT ||
                image->primType() == McPrimType::MC_DOUBLE)
            {
                FilopodiaFunctions::convertToUnsignedChar(image);
                image->composeLabel(image->getLabel(), QString("toUint8"));
            }
            mImages.insert(time, image);
            theObjectPool->removeObject(image);
        }

        if (mImages.isEmpty())
        {
            const QString defaultText("<Please select>");
            mUi.imageDirValueLabel->setText(defaultText);
            mUi.imageDirValueLabel->setToolTip(defaultText);
            throw McException("updateFiles: image directory does not contain .am files");
        }

        const int numMissingImages = (mImages.keys().last() - mImages.keys().first() + 1) - mImages.size();
        if (numMissingImages != 0)
        {
            throw McException(QString("Warning: Missing %1 images").arg(numMissingImages));
        }

        if (mDijkstraDir.length() > 0)
        {
            const QDir dijkstraDir(mDijkstraDir);
            QFileInfoList fileInfoListDijkstra = dijkstraDir.entryInfoList(QDir::Files);

            for (int f = 0; f < fileInfoListDijkstra.size(); ++f)
            {
                const int time = FilopodiaFunctions::getTimeFromFileNameNoGC(fileInfoListDijkstra[f].fileName());

                if (mDijkstras.contains(time))
                {
                    throw McException(QString("Duplicate timestamp %1").arg(time));
                }

                McHandle<HxUniformScalarField3> dijkstra = FilopodiaFunctions::loadUniformScalarField3(fileInfoListDijkstra[f].filePath());
                if (!dijkstra)
                {
                    throw McException(QString("Could not read dijkstra map from file %1").arg(fileInfoListDijkstra[f].filePath()));
                }

                mDijkstras.insert(time, dijkstra);
                theObjectPool->removeObject(dijkstra);
            }

            if (!mDijkstras.isEmpty())
            {
                const int missingDijkstra = (mDijkstras.keys().last() - mDijkstras.keys().first() + 1) - mDijkstras.size();
                if (missingDijkstra != 0)
                {
                    theMsg->printf(QString("Warning: Missing %1 dijkstra files").arg(missingDijkstra));
                }
            }
        }
    }
    catch (McException& e)
    {
        mImages.clear();
        mDijkstras.clear();
        theMsg->printf(e.what());
    }
}

void
QxFilopodiaTool::updateAfterImageChanged()
{
    if (!mEditor->getImageData())
    {
        mImage = 0;
        mDijkstra = 0;
    }
}

void
QxFilopodiaTool::propagateAllFilopodia()
{
    HxSpatialGraph* graph = mEditor->getSpatialGraph();

    if (!graph)
    {
        return;
    }

    if (mCurrentTime == mTimeMinMax.maxT)
    {
        return;
    }

    if (mImages.isEmpty())
    {
        HxMessage::error(QString("Cannot propagate filopodia. Please select image directory.\n"), "ok");
        return;
    }

    QxFilopodiaTool::updateConsistencyTable();
    if (mConsistency->nRows() > 0)
    {
        HxMessage::error(QString("Cannot propagate filopodia.\nGraph is not consistent. Please check table of inconsistencies.\n"), "ok");
        return;
    }

    QTime propStartTime = QTime::currentTime();

    McVec3i baseSearchRadius;
    baseSearchRadius[0] = mUi.baseSearchRadiusXYLineEdit->text().toInt();
    baseSearchRadius[1] = baseSearchRadius[0];
    baseSearchRadius[2] = mUi.baseSearchRadiusZLineEdit->text().toInt();

    McVec3i baseTemplateRadius;
    baseTemplateRadius[0] = mUi.baseTemplateRadiusXYLineEdit->text().toInt();
    baseTemplateRadius[1] = baseTemplateRadius[0];
    baseTemplateRadius[2] = mUi.baseTemplateRadiusZLineEdit->text().toInt();

    McVec3i tipSearchRadius;
    tipSearchRadius[0] = mUi.tipSearchRadiusXYLineEdit->text().toInt();
    tipSearchRadius[1] = tipSearchRadius[0];
    tipSearchRadius[2] = mUi.tipSearchRadiusZLineEdit->text().toInt();

    McVec3i tipTemplateRadius;
    tipTemplateRadius[0] = mUi.tipTemplateRadiusXYLineEdit->text().toInt();
    tipTemplateRadius[1] = tipTemplateRadius[0];
    tipTemplateRadius[2] = mUi.tipTemplateRadiusZLineEdit->text().toInt();

    SpatialGraphSelection baseSel(graph);
    baseSel = FilopodiaFunctions::getNodesOfTypeForTime(graph, BASE_NODE, mCurrentTime);

    try
    {
        ReplaceFilopodiaOperationSet* op = FilopodiaFunctions::propagateFilopodia(graph,
                                                                                  baseSel,
                                                                                  mCurrentTime,
                                                                                  mImages.value(mCurrentTime),
                                                                                  mImages.value(mCurrentTime + 1),
                                                                                  mDijkstras.value(mCurrentTime + 1),
                                                                                  baseSearchRadius,
                                                                                  baseTemplateRadius,
                                                                                  tipSearchRadius,
                                                                                  tipTemplateRadius,
                                                                                  mUi.correlationThresholdLineEdit->text().toFloat(),
                                                                                  mUi.lengthThresholdLineEdit->text().toFloat(),
                                                                                  getIntensityWeight(),
                                                                                  getTopBrightness());
        SpatialGraphSelection highlightSel(graph);
        if (op != 0)
        {
            mEditor->execNewOp(op);
            highlightSel.resize(graph->getNumVertices(), graph->getNumEdges());
            const SpatialGraphSelection newNodesAndEdges = op->getNewNodesAndEdges();
            const SpatialGraphSelection newBases = FilopodiaFunctions::getNodesOfTypeInSelection(graph, newNodesAndEdges, BASE_NODE);

            highlightSel.addSelection(newBases);
            setTime(mCurrentTime + 1);

            theMsg->printf("\t%i of %i filopodia were propagated\n", newBases.getNumSelectedVertices(), baseSel.getNumSelectedVertices());
        }
        mEditor->setHighlightedElements(highlightSel);
    }
    catch (McException& e)
    {
        HxMessage::error(QString("Could not propagate filopodia:\n%1.").arg(e.what()), "ok");
    }

    QTime propEndTime = QTime::currentTime();
    qDebug() << "\n\tpropagate all filopodia:" << propStartTime.msecsTo(propEndTime) << "msec";
    return;
}

void
QxFilopodiaTool::propagateSelFilopodia()
{
    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    if (!graph)
    {
        return;
    }

    if (mCurrentTime == mTimeMinMax.maxT)
    {
        return;
    }

    if (mImages.isEmpty())
    {
        HxMessage::error(QString("Cannot propagate selected filopodia. Please select image directory.\n"), "ok");
        return;
    }

    QxFilopodiaTool::updateConsistencyTable();
    if (mConsistency->nRows() > 0)
    {
        HxMessage::error(QString("Cannot propagate filopodia.\nGraph is not consistent. Please check table of inconsistencies.\n"), "ok");
        return;
    }

    McVec3i baseSearchRadius;
    baseSearchRadius[0] = mUi.baseSearchRadiusXYLineEdit->text().toInt();
    baseSearchRadius[1] = baseSearchRadius[0];
    baseSearchRadius[2] = mUi.baseSearchRadiusZLineEdit->text().toInt();

    McVec3i baseTemplateRadius;
    baseTemplateRadius[0] = mUi.baseTemplateRadiusXYLineEdit->text().toInt();
    baseTemplateRadius[1] = baseTemplateRadius[0];
    baseTemplateRadius[2] = mUi.baseTemplateRadiusZLineEdit->text().toInt();

    McVec3i tipSearchRadius;
    tipSearchRadius[0] = mUi.tipSearchRadiusXYLineEdit->text().toInt();
    tipSearchRadius[1] = tipSearchRadius[0];
    tipSearchRadius[2] = mUi.tipSearchRadiusZLineEdit->text().toInt();

    McVec3i tipTemplateRadius;
    tipTemplateRadius[0] = mUi.tipTemplateRadiusXYLineEdit->text().toInt();
    tipTemplateRadius[1] = tipTemplateRadius[0];
    tipTemplateRadius[2] = mUi.tipTemplateRadiusZLineEdit->text().toInt();

    SpatialGraphSelection highSel(graph);
    highSel = mEditor->getHighlightedElements();

    SpatialGraphSelection baseSel(graph);
    if (highSel.isEmpty())
    {
        SpatialGraphSelection basesOfTime = FilopodiaFunctions::getNodesOfTypeForTime(graph, BASE_NODE, mCurrentTime);
        const int numBases = basesOfTime.getNumSelectedVertices();
        const int baseNode = basesOfTime.getSelectedVertex(numBases - 1);
        baseSel.selectVertex(baseNode);
    }
    else
    {
        baseSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, highSel, BASE_NODE);
    }

    try
    {
        ReplaceFilopodiaOperationSet* op = FilopodiaFunctions::propagateFilopodia(graph,
                                                                                  baseSel,
                                                                                  mCurrentTime,
                                                                                  mImages.value(mCurrentTime),
                                                                                  mImages.value(mCurrentTime + 1),
                                                                                  mDijkstras.value(mCurrentTime + 1),
                                                                                  baseSearchRadius,
                                                                                  baseTemplateRadius,
                                                                                  tipSearchRadius,
                                                                                  tipTemplateRadius,
                                                                                  mUi.correlationThresholdLineEdit->text().toFloat(),
                                                                                  mUi.lengthThresholdLineEdit->text().toFloat(),
                                                                                  getIntensityWeight(),
                                                                                  getTopBrightness());
        SpatialGraphSelection highlightSel(graph);
        if (op != 0)
        {
            mEditor->execNewOp(op);
            highlightSel.resize(graph->getNumVertices(), graph->getNumEdges());
            const SpatialGraphSelection newNodesAndEdges = op->getNewNodesAndEdges();
            const SpatialGraphSelection newBases = FilopodiaFunctions::getNodesOfTypeInSelection(graph, newNodesAndEdges, BASE_NODE);

            highlightSel.addSelection(newBases);
            setTime(mCurrentTime + 1);
        }
        mEditor->setHighlightedElements(highlightSel);
    }
    catch (McException& e)
    {
        HxMessage::error(QString("Could not propagate filopodia:\n%1.").arg(e.what()), "ok");
    }

    return;
}

void
QxFilopodiaTool::propagateRoots()
{
    qDebug() << "Propagate roots.";
    theMsg->printf("Propagate roots.");
    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    if (!graph)
    {
        qDebug() << "\tCannot propagate root nodes: please load graph.";
        theMsg->printf("\tCannot propagate root nodes: please load graph.");
        return;
    }

    if (mImages.isEmpty())
    {
        HxMessage::error(QString("Cannot propagate root nodes. Please select image directory.\n"), "ok");
        return;
    }

    if (mOutputDir.length() == 0)
    {
        HxMessage::error(QString("Cannot propagate root nodes. Please select output directory.\n"), "ok");
        return;
    }

    // Add labels (time, type, growth cones)
    HierarchicalLabels* gcLabels = FilopodiaFunctions::addGrowthConeLabelAttribute(graph);
    FilopodiaFunctions::addTimeLabelAttribute(graph, mTimeMinMax);
    FilopodiaFunctions::addTypeLabelAttribute(graph);

    // Existing nodes get labels: type=root, time=minT, growthcone=next
    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        FilopodiaFunctions::setTimeIdOfNode(graph, v, FilopodiaFunctions::getTimeIdFromTimeStep(graph, mTimeMinMax.minT));
        FilopodiaFunctions::setNodeType(graph, v, ROOT_NODE);

        const QString gcLabelName = FilopodiaFunctions::getLabelNameFromGrowthConeNumber(v + 1);
        int gcId = gcLabels->getLabelIdFromName(McString(gcLabelName));
        if (gcId == -1)
        {
            const SbColor color(McRandom::rand(), McRandom::rand(), McRandom::rand());
            gcId = gcLabels->addLabel(0, qPrintable(gcLabelName), color);
        }
        FilopodiaFunctions::setGcIdOfNode(graph, v, gcId);
    }

    McVec3i rootNodeSearchRadius;
    rootNodeSearchRadius[0] = mUi.rootSearchRadiusXYLineEdit->text().toInt();
    rootNodeSearchRadius[1] = rootNodeSearchRadius[0];
    rootNodeSearchRadius[2] = mUi.rootSearchRadiusZLineEdit->text().toInt();

    McVec3i rootNodeTemplateRadius;
    rootNodeTemplateRadius[0] = mUi.rootTemplateRadiusXYLineEdit->text().toInt();
    rootNodeTemplateRadius[1] = rootNodeTemplateRadius[0];
    rootNodeTemplateRadius[2] = mUi.rootTemplateRadiusZLineEdit->text().toInt();

    try
    {
        AddRootsOperationSet* op = FilopodiaFunctions::propagateRootNodes(graph, mImages, rootNodeSearchRadius, rootNodeTemplateRadius);

        if (op != 0)
        {
            mEditor->execNewOp(op);
        }
    }
    catch (McException& e)
    {
        HxMessage::error(QString("Could not propagate root nodes:\n%1.").arg(e.what()), "ok");
    }

    // Save root nodes
    const int numberOfGrowthCones = FilopodiaFunctions::getNumberOfGrowthCones(graph);

    for (int n = 1; n < numberOfGrowthCones + 1; ++n)
    {
        QString gcFolder = QString(mOutputDir + "/GrowthCone_%1").arg(n);
        if (!QDir(gcFolder).exists())
        {
            QDir().mkdir(gcFolder);
        }

        const SpatialGraphSelection nodeSel = FilopodiaFunctions::getNodesWithGcId(graph, n);
        const SpatialGraphSelection rootSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, nodeSel, ROOT_NODE);

        McHandle<HxSpatialGraph> rootGraph = HxSpatialGraph::createInstance();
        rootGraph->setLabel(QString("rootNodes_gc%1").arg(n));
        rootGraph = graph->getSubgraph(rootSel);

        // Add filopodia labels to result tree
        McHandle<HxSpatialGraph> resultGraph = rootGraph->duplicate();
        FilopodiaFunctions::addTimeLabelAttribute(resultGraph, mTimeMinMax);
        FilopodiaFunctions::addManualGeometryLabelAttribute(resultGraph);
        FilopodiaFunctions::addTypeLabelAttribute(resultGraph);
        FilopodiaFunctions::addManualNodeMatchLabelAttribute(resultGraph);
        FilopodiaFunctions::addLocationLabelAttribute(resultGraph);
        FilopodiaFunctions::addFilopodiaLabelAttribute(resultGraph);

        rootGraph->saveAmiraMeshASCII(qPrintable(QString(gcFolder + "/rootNodes_gc%1.am").arg(n)));
        resultGraph->saveAmiraMeshASCII(qPrintable(QString(gcFolder + "/resultTree_gc%1.am").arg(n)));
    }

    theObjectPool->addObject(graph);
    graph->saveAmiraMeshASCII(qPrintable(mOutputDir + "/gc_track.am"));

    SpatialGraphSelection visSel(graph);
    visSel.selectAllVertices();
    mEditor->setVisibleElements(visSel);

    return;
}

void
QxFilopodiaTool::updateTimeMinMax()
{
    mTimeMinMax = TimeMinMax();

    if (mGraph)
    {
        try
        {
            const TimeMinMax graphMinMax = FilopodiaFunctions::getTimeMinMaxFromGraphLabels(mGraph);
            mTimeMinMax.minT = MC_MIN2(graphMinMax.minT, mTimeMinMax.minT);
            mTimeMinMax.maxT = MC_MAX2(graphMinMax.maxT, mTimeMinMax.maxT);
        }
        catch (McException& e)
        {
            // Graph has no time labels. Probably not a filopodia dataset.
        }
    }

    if (mImages.size() > 0)
    {
        TimeMinMax imageMinMax;
        imageMinMax.minT = mImages.keys().first();
        imageMinMax.maxT = mImages.keys().last();

        mTimeMinMax.minT = MC_MIN2(imageMinMax.minT, mTimeMinMax.minT);
        mTimeMinMax.maxT = MC_MAX2(imageMinMax.maxT, mTimeMinMax.maxT);
    }

    if (mTimeMinMax.isValid())
    {
        mUi.timeSlider->setEnabled(true);
        mUi.timeSpinBox->setEnabled(true);

        mUi.timeSlider->setMinimum(mTimeMinMax.minT);
        mUi.timeSlider->setMaximum(mTimeMinMax.maxT);
        mUi.timeSpinBox->setMinimum(mTimeMinMax.minT);
        mUi.timeSpinBox->setMaximum(mTimeMinMax.maxT);
    }
    else
    {
        mUi.timeSlider->setEnabled(false);
        mUi.timeSpinBox->setEnabled(false);
        mTimeMinMax.minT = 0;
        mTimeMinMax.maxT = 0;
    }

    if (FilopodiaFunctions::isFilopodiaGraph(mGraph) || FilopodiaFunctions::isRootNodeGraph(mGraph))
    {
        qDebug() << "New time range" << mTimeMinMax.minT << mTimeMinMax.maxT;
    }
}

int
QxFilopodiaTool::getCurrentTime() const
{
    return mCurrentTime;
}

HxUniformScalarField3*
QxFilopodiaTool::getDijkstraMap() const
{
    return mDijkstra;
}

void
QxFilopodiaTool::startLog()
{
    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    if (!graph)
    {
        return;
    }

    if (mDir.isEmpty())
    {
        QxFilopodiaTool::selectDir();
    }

    const QDir logDir = mDir + "/LOG";
    mEditor->startLogging(logDir);
    theMsg->printf("Start logging");
    qDebug() << "Start logging";
}

void
QxFilopodiaTool::stopLog()
{
    mEditor->stopLogging();
    theMsg->printf("Stop logging");
    qDebug() << "Stop logging";
}

void
prepareFilesForOneNode(HxSpatialGraph* graph,
                       SpatialGraphSelection nodeSel,
                       QMap<int, McHandle<HxUniformScalarField3> > images,
                       QString outputDir,
                       const int threshold,
                       const int persistence)
{
    const int nodeId = nodeSel.getSelectedVertex(0);

    const EdgeVertexAttribute* gcAtt = graph->findVertexAttribute(FilopodiaFunctions::getGrowthConeAttributeName());
    if (!gcAtt)
    {
        qDebug() << "Graph has no growth cone attribute.";
        return;
    }

    const int currentTime = FilopodiaFunctions::getTimeOfNode(graph, nodeId);
    const int gcId = FilopodiaFunctions::getGcIdOfNode(graph, nodeId);
    qDebug() << "\t\tgc" << gcId << "time" << currentTime;

    QString gcFolder = QString(outputDir + "/GrowthCone_%1").arg(gcId);

    HxUniformScalarField3* currentImage = images.value(currentTime);
    McHandle<HxSpatialGraph> seeds = HxSpatialGraph::createInstance();
    seeds = graph->getSubgraph(nodeSel);

    // Compute segmentation
    McHandle<HxContourTreeSegmentation> contourTreeSegmentation = HxContourTreeSegmentation::createInstance();
    contourTreeSegmentation->portData.connect(currentImage);
    contourTreeSegmentation->portAutoSetValues.setValue(0, false);
    contourTreeSegmentation->fire();
    contourTreeSegmentation->portThreshold.setMinMax(0, 255);
    contourTreeSegmentation->portThreshold.setValue(threshold);
    contourTreeSegmentation->portPersistenceValue.setValue(persistence);
    contourTreeSegmentation->portMinimalSegmentSize.setValue(1000);
    contourTreeSegmentation->fire();
    contourTreeSegmentation->portDoIt.hit();
    contourTreeSegmentation->fire();

    McHandle<HxUniformLabelField3> segmentation = dynamic_cast<HxUniformLabelField3*>(contourTreeSegmentation->getResult());
    if (!segmentation)
    {
        throw McException("Could not create contour tree segmentation");
    }
    segmentation->setLabel("Segmentation");

    // Split data
    McHandle<HxSplitLabelField> splitLabelField = HxSplitLabelField::createInstance();
    splitLabelField->portData.connect(segmentation);
    splitLabelField->portImage.connect(currentImage);
    splitLabelField->portSeeds.connect(seeds);
    splitLabelField->portBoundarySize.setValue(10);
    splitLabelField->fire();
    splitLabelField->portDoIt.hit();
    splitLabelField->fire();

    QString labelName;
    QString grayName;

    if (currentTime < 10)
    {
        labelName = QString("label_gc%1_t0%2.am").arg(gcId).arg(currentTime);
        grayName = QString("gray_gc%1_t0%2.am").arg(gcId).arg(currentTime);
    }
    else
    {
        labelName = QString("label_gc%1_t%2.am").arg(gcId).arg(currentTime);
        grayName = QString("gray_gc%1_t%2.am").arg(gcId).arg(currentTime);
    }

    QString labelFolder = gcFolder + "/Labels";
    if (!QDir(labelFolder).exists())
    {
        QDir().mkdir(labelFolder);
    }

    QString grayFolder = gcFolder + "/Gray";
    if (!QDir(grayFolder).exists())
    {
        QDir().mkdir(grayFolder);
    }

    McHandle<HxUniformLabelField3> label = dynamic_cast<HxUniformLabelField3*>(splitLabelField->getResult(0));
    if (!label)
    {
        HxMessage::warning(QString("Cannot crop growthcone %1 in time step %2: Growth cone center is not in object.\nPlease move seed and redo cropping.\n").arg(gcId).arg(currentTime), "ok");
        theMsg->printf("Please move node of growth cone %i in time step %i", gcId, currentTime);
        return;
    }
    label->setLabel(labelName);
    label->writeAmiraMeshRLE(qPrintable(labelFolder + "/" + labelName));

    McHandle<HxUniformScalarField3> gray = dynamic_cast<HxUniformScalarField3*>(splitLabelField->getResult(1));
    if (!gray)
    {
        HxMessage::warning(QString("Cannot crop growthcone %1 in time step %2: Growth cone center is not in object.\nPlease move seed and redo cropping.\n").arg(gcId).arg(currentTime), "ok");
        return;
    }
    gray->setLabel(grayName);
    gray->writeAmiraMeshBinary(qPrintable(grayFolder + "/" + grayName));
}

void
QxFilopodiaTool::prepareFiles()
{
    qDebug() << "\nprepare files";
    HxSpatialGraph* centerGraph = mEditor->getSpatialGraph();
    if (!centerGraph)
    {
        return;
    }

    if (mImages.isEmpty())
    {
        HxMessage::error(QString("Cannot prepare files: Please select image directory.\n"), "ok");
        return;
    }

    if (mOutputDir.length() == 0)
    {
        HxMessage::error(QString("Cannot prepare files: Please select output directory.\n"), "ok");
        return;
    }

    qDebug() << "\toutput dir:" << mOutputDir;

    // Save root nodes
    const int numberOfGrowthCones = FilopodiaFunctions::getNumberOfGrowthCones(centerGraph);

    for (int n = 1; n < numberOfGrowthCones + 1; ++n)
    {
        QString gcFolder = QString(mOutputDir + "/GrowthCone_%1").arg(n);
        if (!QDir(gcFolder).exists())
        {
            QDir().mkdir(gcFolder);
        }

        const SpatialGraphSelection nodeSel = FilopodiaFunctions::getNodesWithGcId(centerGraph, n);
        const SpatialGraphSelection rootSel = FilopodiaFunctions::getNodesOfTypeInSelection(centerGraph, nodeSel, ROOT_NODE);

        McHandle<HxSpatialGraph> rootGraph = HxSpatialGraph::createInstance();
        rootGraph->setLabel(QString("rootNodes_gc%1").arg(n));
        rootGraph = centerGraph->getSubgraph(rootSel);

        // Add filopodia labels to result tree
        McHandle<HxSpatialGraph> resultGraph = rootGraph->duplicate();
        FilopodiaFunctions::addTimeLabelAttribute(resultGraph, mTimeMinMax);
        FilopodiaFunctions::addManualGeometryLabelAttribute(resultGraph);
        FilopodiaFunctions::addTypeLabelAttribute(resultGraph);
        FilopodiaFunctions::addManualNodeMatchLabelAttribute(resultGraph);
        FilopodiaFunctions::addLocationLabelAttribute(resultGraph);
        FilopodiaFunctions::addFilopodiaLabelAttribute(resultGraph);

        rootGraph->saveAmiraMeshASCII(qPrintable(QString(gcFolder + "/rootNodes_gc%1.am").arg(n)));
        resultGraph->saveAmiraMeshASCII(qPrintable(QString(gcFolder + "/resultTree_gc%1.am").arg(n)));
    }

    centerGraph->saveAmiraMeshASCII(qPrintable(mOutputDir + "/gc_track.am"));

    const SpatialGraphSelection highSel = mEditor->getHighlightedElements();
    SpatialGraphSelection nodeSel(centerGraph);

    if (highSel.getNumSelectedVertices() == 0)
    {
        nodeSel.selectAllVertices();
        qDebug() << "\tprepare all nodes";
    }
    else
    {
        nodeSel = highSel;
        qDebug() << "\tprepare" << nodeSel.getNumSelectedVertices() << "node(s)";
    }

    const float intensityWeight = getIntensityWeight();
    const int topBrightness = getTopBrightness();

    for (int i = 0; i < nodeSel.getNumSelectedVertices(); ++i)
    {
        SpatialGraphSelection centerSel(centerGraph);
        centerSel.selectVertex(nodeSel.getSelectedVertex(i));
        prepareFilesForOneNode(centerGraph, centerSel, mImages, mOutputDir, mUi.threshLineEdit->text().toInt(), mUi.persistenceLineEdit->text().toInt());

        McHandle<HxSpatialGraph> subGraph = HxSpatialGraph::createInstance();
        subGraph = centerGraph->getSubgraph(centerSel);
        computeDijkstra(subGraph, mOutputDir, intensityWeight, topBrightness);
    }
}

void
QxFilopodiaTool::calculateDijkstra()
{
    qDebug() << "\ncalculate Dijkstra";
    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    if (!graph)
    {
        return;
    }

    if (mImages.isEmpty())
    {
        qDebug() << "no images";
        return;
    }

    if (mOutputDir.length() == 0)
    {
        qDebug() << "no output dir";
        return;
    }

    if (mOutputDir.isEmpty())
    {
        qDebug() << "Output direction is empty";
        return;
    }

    const SpatialGraphSelection highSel = mEditor->getHighlightedElements();
    SpatialGraphSelection rootSel(graph);

    if (highSel.getNumSelectedVertices() == 0)
    {
        const int gcId = FilopodiaFunctions::getGcIdOfNode(graph, 0);
        if (gcId < 1)
        {
            qDebug() << "invalid gcId";
            return;
        }

        const SpatialGraphSelection nodeSel = FilopodiaFunctions::getNodesWithGcId(graph, gcId);
        rootSel = FilopodiaFunctions::getNodesWithFiloIdFromSelection(graph, nodeSel, ROOT_NODE);
        qDebug() << "\tcalculate dijkstra map for" << nodeSel.getNumSelectedVertices() << "nodes";
    }
    else
    {
        const int rootNodeFromTime = FilopodiaFunctions::getRootNodeFromTimeStep(graph, mCurrentTime);
        rootSel.selectVertex(rootNodeFromTime);
        qDebug() << "\tcalculate dijkstra map for timestep" << mCurrentTime;
    }

    McHandle<HxSpatialGraph> subGraph = HxSpatialGraph::createInstance();
    subGraph = graph->getSubgraph(rootSel);

    qDebug() << "compute";
    try
    {
        computeDijkstra(subGraph, mOutputDir, getIntensityWeight(), getTopBrightness());
    }
    catch (McException& e)
    {
        HxMessage::warning(QString("%1").arg(e.what()), "ok");
    }
}

void
QxFilopodiaTool::addBulbousLabel()
{
    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    if (!graph)
    {
        return;
    }

    FilopodiaFunctions::addBulbousLabel(graph);
    mEditor->updateAfterSpatialGraphChange(HxNeuronEditorSubApp::SpatialGraphLabelTreeChange | HxNeuronEditorSubApp::SpatialGraphLabelChange);
}

void
QxFilopodiaTool::extendBulbousLabel()
{
    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    if (!graph)
    {
        return;
    }

    QxFilopodiaTool::updateConsistencyTable();

    if (mConsistency->nRows() > 0)
    {
        const int problemColId = mConsistency->findColumn("Problem", HxSpreadSheet::Column::STRING);
        const HxSpreadSheet::Column* problemCol = mConsistency->column(problemColId, mConsistency->findTable("Filopodia Inconsistencies"));

        for (int r = 0; r < mConsistency->nRows(); ++r)
        {
            McString value = problemCol->stringValue(r);
            if (!(value == QString("incomplete bulbous label")))
            {
                HxMessage::error(QString("Cannot extend bulbous label.\nGraph is not consistent. Please check table of inconsistencies.\n"), "ok");
                return;
            }
        }
    }

    try
    {
        FilopodiaFunctions::extendBulbousLabel(graph);
    }
    catch (McException& e)
    {
        HxMessage::warning(QString("%1").arg(e.what()), "ok");
    }

    mEditor->updateAfterSpatialGraphChange(HxNeuronEditorSubApp::SpatialGraphLabelTreeChange | HxNeuronEditorSubApp::SpatialGraphLabelChange);
}
