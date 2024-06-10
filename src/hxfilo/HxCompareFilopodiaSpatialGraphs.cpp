#include "HxCompareFilopodiaSpatialGraphs.h"

#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <hxspatialgraph/internal/HxSpatialGraphView.h>
#include <hxspatialgraph/internal/SpatialGraphOperationSet.h>
#include <hxspatialgraph/internal/HierarchicalLabels.h>
#include <hxspatialgraph/internal/SpatialGraphFunctions.h>
#include "FilopodiaFunctions.h"
#include <hxcore/HxObjectPool.h>
#include <mclib/McException.h>
#include <hxcore/internal/HxReadAmiraMesh.h>
#include <qdebug.h>

#include <hxspreadsheet/internal/HxSpreadSheet.h>

#include "ConvertVectorAndMcDArray.h"

HX_INIT_CLASS(HxCompareFilopodiaSpatialGraphs, HxCompModule);

HxCompareFilopodiaSpatialGraphs::HxCompareFilopodiaSpatialGraphs()
    : HxCompModule(HxSpatialGraph::getClassTypeId())
    , portTargetGraph(this, "targetGraph", tr("Target Graph"), HxSpatialGraph::getClassTypeId())
    , portTwoOrNWayComparison(this, "comparison", tr("Comparison"), 2)
    , portFileNames(this, "fileNames", tr("Filenames"), HxPortFilename::MULTI_FILE)
    , portMaxSamplingDistance(this, "MaximumSamplingDistance", tr("Maximum Sampling Distance"), 1)
    , portMaxCorrespondenceDistance(this, "MaximumCorrespondenceDistance", tr("Maximum Correspondence Distance"), 1)
    , portAction(this, "action", tr("Action"))
    , cNumberOfMatchesAttributeName("NumberOfMatchingGraphs")
{
    portTwoOrNWayComparison.setLabel(TWO_WAY_COMPARISON, "2-Way");
    portTwoOrNWayComparison.setLabel(N_WAY_COMPARISON, "N-Way");
    portTwoOrNWayComparison.setValue(TWO_WAY_COMPARISON);

    portMaxSamplingDistance.setValue(1.0f);
    portMaxCorrespondenceDistance.setValue(3.0f);

}

HxCompareFilopodiaSpatialGraphs::~HxCompareFilopodiaSpatialGraphs()
{
}

void
HxCompareFilopodiaSpatialGraphs::compute()
{
    if (portAction.wasHit())
    {
        if (portTwoOrNWayComparison.getValue() == TWO_WAY_COMPARISON)
        {
            twoWayComparison();
        }
        else
        {
            nWayComparison();
        }
    }
}

void
HxCompareFilopodiaSpatialGraphs::update()
{
    if (portTwoOrNWayComparison.isNew())
    {
        if (portTwoOrNWayComparison.getValue() == TWO_WAY_COMPARISON)
        {
            portFileNames.hide();
        }
        else
        {
            portFileNames.show();
        }
    }

}

bool hasPointWithinRadius(const HxSpatialGraph* graph, const McVec3f& point, const float radius) {

    for (int e=0; e<graph->getNumEdges(); ++e) {
        const std::vector<McVec3f>& points = graph->getEdgePoints(e);
        for (int p=0; p<points.size(); ++p) {
            if ((points[p]-point).length() < radius) {
                return true;
            }
        }
    }
    return false;
}

std::vector<HxSpatialGraph*>
HxCompareFilopodiaSpatialGraphs::cleanAndResampleGraphs(std::vector<const HxSpatialGraph*>& spatialGraphs) const
{
    std::vector<HxSpatialGraph*> result;
    for (std::vector<const HxSpatialGraph*>::const_iterator it = spatialGraphs.begin(); it != spatialGraphs.end(); ++it)
    {
        const HxSpatialGraph* g = *it;
        HxSpatialGraph* cleanedGraph = dynamic_cast<HxSpatialGraph*>(g->duplicate());
        cleanedGraph->composeLabel(g->getLabel(), "clean");
        cleanSpatialGraph(cleanedGraph);
        const float samplingDistance = portMaxSamplingDistance.getValue();
        SpatialGraphFunctions::resampleWithMaximumPointDistance(cleanedGraph, samplingDistance);
        result.push_back(cleanedGraph);
    }
    return result;
}

HxSpatialGraph* getFilopodiaGraphOfTimestep(const HxSpatialGraph* graph, const int t) {

    SpatialGraphSelection filopodiaSelection = FilopodiaFunctions::getGrowthConeSelectionOfTimeStep(graph, t);

    HxSpatialGraph* filopodiaGraph = graph->getSubgraph(filopodiaSelection);
    return filopodiaGraph;
}


void
addMatchedEdgeAttribute(HxSpatialGraph* graph)
{
    const char* attName = "matchedEdgeLabels";

    EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(attName);

    const McString matched = "matched";
    const McString notMatched = "notMatched";

    HierarchicalLabels* labels;
    if (eAtt)
    {
        labels = graph->getLabelGroup(attName);
        if (!labels->hasLabel(matched))
        {
            labels->addLabel(0, matched, SbColor(0.0f, 0.0f, 1.0f));
        }
        if (!labels->hasLabel(notMatched))
        {
            labels->addLabel(0, notMatched, SbColor(1.0f, 0.0f, 0.0f));
        }
    }
    else if (!eAtt)
    {
        HierarchicalLabels* labels = graph->addNewLabelGroup(attName, true, false, false);
        const int notMatchedId = labels->addLabel(0, notMatched, SbColor(1.0f, 0.0f, 0.0f));
        labels->addLabel(0, matched, SbColor(0.0f, 0.0f, 1.0f));

        eAtt = graph->findEdgeAttribute(attName);
        for (int e = 0; e < graph->getNumEdges(); ++e)
        {
            eAtt->setIntDataAtIdx(e, notMatchedId);
        }
    }
    else
    {
        throw McException("Invalid existing matched attribute");
    }
}


std::vector<HxSpatialGraph*>
getMatchedEdges(std::vector<HxSpatialGraph*> graphs) {

    std::vector<HxSpatialGraph*> result;
    const int numberGraphs = graphs.size();
    for (int i = 0; i < numberGraphs; ++i)
    {
        HxSpatialGraph* graph = graphs[i]->duplicate();
        PointAttribute* pAtt = graph->findEdgePointAttribute("NumberOfMatchingGraphs");
        graph->addAttribute("matchedEdgeLabels", HxSpatialGraph::EDGE, McPrimType::MC_INT32, 1);
        EdgeVertexAttribute* eAtt = graph->findEdgeAttribute("matchedEdgeLabels");

        for (int e=0; e<graph->getNumEdges(); ++e) {

            std::vector<int> matchValues;
            if (graph->getNumEdgePoints(e) > 2) {
                for (int p=1; p<graph->getNumEdgePoints(e)-1; ++p) {
                    matchValues.push_back(pAtt->getIntDataAtPoint(e,p));
                }
            }

            std::sort(matchValues.begin(),matchValues.end());

            const float oct = floor(0.2 * matchValues.size());
            eAtt->setIntDataAtIdx(e, matchValues[int(oct)]);
            qDebug() << "e" << e << "oct" << matchValues[int(oct)];

        }

        result.push_back(graph);
    }
    return result;
}


std::vector<HxSpatialGraph*> checkNumberAttributes(std::vector<HxSpatialGraph*>& graphs) {

    std::vector<HxSpatialGraph*> result;
    result.push_back(graphs[0]->duplicate());
    result.push_back(graphs[1]->duplicate());

    for (int a = 0; a<result[0]->numAttributes(HxSpatialGraph::VERTEX); ++a) {
        GraphAttribute* att1 = result[0]->attribute(HxSpatialGraph::VERTEX, a);
        GraphAttribute* att2 = result[1]->findAttribute(HxSpatialGraph::VERTEX, att1->getName());
        if (!att2) {
            qDebug() << "VERTEX attributes" << att1->getName() << "not found in other graph. Delete.";
            result[0]->deleteAttribute(att1);
        }
    }
    for (int a = 0; a<result[1]->numAttributes(HxSpatialGraph::VERTEX); ++a) {
        GraphAttribute* att2 = result[1]->attribute(HxSpatialGraph::VERTEX, a);
        GraphAttribute* att1 = result[0]->findAttribute(HxSpatialGraph::VERTEX, att2->getName());
        if (!att1) {
            qDebug() << "VERTEX attributes" << att2->getName() << "not found in other graph. Delete.";
            result[1]->deleteAttribute(att2);
        }
    }

    for (int a=0; a<result[0]->numAttributes(HxSpatialGraph::EDGE); ++a) {
        GraphAttribute* att1 = result[0]->attribute(HxSpatialGraph::EDGE, a);
        GraphAttribute* att2 = result[1]->findAttribute(HxSpatialGraph::EDGE, att1->getName());
        if (!att2) {
            qDebug() << "EDGE attributes" << att1->getName() << "not found in other graph. Delete.";
            result[0]->deleteAttribute(att1);
        }
    }
    for (int a=0; a<result[1]->numAttributes(HxSpatialGraph::EDGE); ++a) {
        GraphAttribute* att2 = result[1]->attribute(HxSpatialGraph::EDGE, a);
        GraphAttribute* att1 = result[0]->findAttribute(HxSpatialGraph::EDGE, att2->getName());
        if (!att1) {
            qDebug() << "EDGE attributes" << att2->getName() << "not found in other graph. Delete.";
            result[1]->deleteAttribute(att2);
        }
    }

    for (int a=0; a<result[0]->numAttributes(HxSpatialGraph::POINT); ++a) {
        GraphAttribute* att1 = result[0]->attribute(HxSpatialGraph::POINT, a);
        GraphAttribute* att2 = result[1]->findAttribute(HxSpatialGraph::POINT, att1->getName());
        if (!att2) {
            qDebug() << "POINT attributes" << att1->getName() << "not found in other graph. Delete.";
            result[0]->deleteAttribute(att1);
        }
    }
    for (int a=0; a<result[1]->numAttributes(HxSpatialGraph::POINT); ++a) {
        GraphAttribute* att2 = result[1]->attribute(HxSpatialGraph::POINT, a);
        GraphAttribute* att1 = result[0]->findAttribute(HxSpatialGraph::POINT, att2->getName());
        if (!att1) {
            qDebug() << "POINT attributes" << att2->getName() << "not found in other graph. Delete.";
            result[1]->deleteAttribute(att2);
        }
    }

    return result;
}

void
HxCompareFilopodiaSpatialGraphs::twoWayComparison()
{
    const HxSpatialGraph* sourceSpatialGraph = hxconnection_cast<HxSpatialGraph>(portData);
    const HxSpatialGraph* targetSpatialGraph = hxconnection_cast<HxSpatialGraph>(portTargetGraph);
    if (!sourceSpatialGraph || !targetSpatialGraph)
    {
        theMsg->printf("Two-way comparison failed. SourceGraph and/or TargetGraph missing.");
        return;
    }

    // Check if filopodia graph
    if (!FilopodiaFunctions::isFilopodiaGraph(sourceSpatialGraph) || !FilopodiaFunctions::isFilopodiaGraph(targetSpatialGraph)) {
        theMsg->printf("Two-way comparison failed. SourceGraph and/or TargetGraph are/is not filopodia graph.");
        return;
    }

    // Check if graphs have same timesteps
    TimeMinMax timeSource = FilopodiaFunctions::getTimeMinMaxFromGraphLabels(sourceSpatialGraph);
    TimeMinMax timeTarget = FilopodiaFunctions::getTimeMinMaxFromGraphLabels(targetSpatialGraph);
    TimeMinMax time;

    if (timeSource.minT == timeTarget.minT && timeSource.maxT == timeTarget.maxT) {
        time = timeSource;
    } else {
        theMsg->printf("Two-way comparison per time step failed. SourceGraph and TargetGraph have different time steps.");
        return;
    }

    std::vector<HxSpatialGraph*> completeMatchedGraphs;
    completeMatchedGraphs.push_back(HxSpatialGraph::createInstance());
    completeMatchedGraphs.push_back(HxSpatialGraph::createInstance());

    // Compare graphs for each timestep
    for (int t=time.minT; t<time.maxT+1; ++t) {

        //Get filopodia of graphs for time step
        const HxSpatialGraph* sourceFilopodia = getFilopodiaGraphOfTimestep(sourceSpatialGraph, t);
        const HxSpatialGraph* targetFilopodia = getFilopodiaGraphOfTimestep(targetSpatialGraph, t);

        std::vector<const HxSpatialGraph*> inputGraphs;
        inputGraphs.push_back(sourceFilopodia);
        inputGraphs.push_back(targetFilopodia);

        std::vector<HxSpatialGraph*> graphsToMatch = cleanAndResampleGraphs(inputGraphs);
        std::vector<HxSpatialGraph*> matchedGraphs = matchGraphs(graphsToMatch, portMaxCorrespondenceDistance.getValue());

        // Add match Attribute for edges
        std::vector<HxSpatialGraph*> matchedEdgeGraphs = getMatchedEdges(matchedGraphs);
        if (completeMatchedGraphs[0]->getNumVertices() == 0) {
            completeMatchedGraphs[0] = matchedEdgeGraphs[0]->duplicate();
            completeMatchedGraphs[1] = matchedEdgeGraphs[1]->duplicate();
        } else {
            completeMatchedGraphs[0]->merge(matchedEdgeGraphs[0]);
            completeMatchedGraphs[1]->merge(matchedEdgeGraphs[1]);
        }
    }

    completeMatchedGraphs[0]->composeLabel(sourceSpatialGraph->getLabel(), "source");
    completeMatchedGraphs[1]->composeLabel(targetSpatialGraph->getLabel(), "target");

    outputGraphAndViewer(completeMatchedGraphs[0], 2);
    outputGraphAndViewer(completeMatchedGraphs[1], 2);

    // Merge
    std::vector<HxSpatialGraph*> mergeGraphs = checkNumberAttributes(completeMatchedGraphs);

    HxSpatialGraph* mergedGraph = mergeGraphs[0]->duplicate();
    mergedGraph->merge(mergeGraphs[1]);
    mergedGraph->touch();
    mergedGraph->fire();
    mergedGraph->composeLabel(mergeGraphs[0]->getLabel(), "merged");
    outputGraphAndViewer(mergedGraph, 2);

    HxSpreadSheet* matchStatisticsSheet = createMatchStatisticsSpreadsheet(completeMatchedGraphs);
    theObjectPool->addObject(matchStatisticsSheet);

}

void
removeAllGraphsFromObjectPool(std::vector<const HxSpatialGraph*> inputGraphs)
{
    for (std::vector<const HxSpatialGraph*>::iterator it = inputGraphs.begin(); it != inputGraphs.end(); ++it)
    {
        HxSpatialGraph* graph = (HxSpatialGraph*)*it;
        theObjectPool->removeObject(graph);
    }
}

void
HxCompareFilopodiaSpatialGraphs::nWayComparison()
{

    std::vector<const HxSpatialGraph*> inputGraphs = readSpatialGraphs();
    std::vector<const HxSpatialGraph*> graphs;

    // Check if filopodia graph
    for (std::vector<const HxSpatialGraph*>::iterator it = inputGraphs.begin(); it != inputGraphs.end(); ++it)
    {
        HxSpatialGraph* graph = (HxSpatialGraph*)*it;
        if (FilopodiaFunctions::isFilopodiaGraph(graph)) {
            graphs.push_back(graph);
        } else {
            theMsg->printf("Graph %s is not filopodia graph. -> remove", graph->getName());
            theObjectPool->removeObject(graph);
        }
    }

    // Check if graphs have same timesteps
    TimeMinMax time = TimeMinMax(-1,-1);
    for (std::vector<const HxSpatialGraph*>::iterator it = graphs.begin(); it != graphs.end(); ++it)
    {
        HxSpatialGraph* graph = (HxSpatialGraph*)*it;
        const TimeMinMax timeGraph = FilopodiaFunctions::getTimeMinMaxFromGraphLabels(graph);

        if ( (time.minT == timeGraph.minT && time.maxT == timeGraph.maxT) || (time.minT == -1 && time.maxT == -1) ) {
            time = timeGraph;
        } else {
            theMsg->printf("N-way comparison failed. Graph %s has different time steps.-> remove", graph->getName());
            removeAllGraphsFromObjectPool(graphs);
            return;
        }
    }

    if (graphs.size() < 2)
    {
        theMsg->printf("N-way comparison failed. Not enough graphs to compare.");
        removeAllGraphsFromObjectPool(graphs);
        return;
    }

    // Compare graphs for each timestep
    std::vector<HxSpatialGraph*> completeMatchedGraphs;
    for (unsigned int i = 0; i < graphs.size(); ++i)
    {
        completeMatchedGraphs.push_back(HxSpatialGraph::createInstance());
    }

    for (int t=time.minT; t<time.maxT+1; ++t) {

        // Get filopodia of graphs for time step
        std::vector<const HxSpatialGraph*> inputGraphsOfTime;
        for (std::vector<const HxSpatialGraph*>::iterator it = graphs.begin(); it != graphs.end(); ++it)
        {
            const HxSpatialGraph* graph = (HxSpatialGraph*)*it;
            const HxSpatialGraph* graphOfTime = getFilopodiaGraphOfTimestep(graph, t);
            inputGraphsOfTime.push_back(graphOfTime);
        }

        std::vector<HxSpatialGraph*> graphsToMatch = cleanAndResampleGraphs(inputGraphsOfTime);
        std::vector<HxSpatialGraph*> matchedGraphs = matchGraphs(graphsToMatch, portMaxCorrespondenceDistance.getValue());

        // Add match Attribute for edges
        std::vector<HxSpatialGraph*> matchedEdgeGraphs = getMatchedEdges(matchedGraphs);
        for (unsigned int i = 0; i < matchedEdgeGraphs.size(); ++i)
        {
            if (completeMatchedGraphs[i]->getNumVertices() == 0) {
                completeMatchedGraphs[i] = matchedEdgeGraphs[i]->duplicate();
            } else {
                completeMatchedGraphs[i]->merge(matchedEdgeGraphs[i]);
            }
        }
    }

    for (unsigned int i = 0; i < completeMatchedGraphs.size(); ++i)
    {
        completeMatchedGraphs[i]->composeLabel(graphs[i]->getLabel(), "matched");
        outputGraphAndViewer(completeMatchedGraphs[i], graphs.size());
    }

    HxSpreadSheet* matchStatisticsSheet = createMatchStatisticsSpreadsheet(completeMatchedGraphs);
    setResult(matchStatisticsSheet);

    removeAllGraphsFromObjectPool(graphs);

}


void
HxCompareFilopodiaSpatialGraphs::outputGraphAndViewer(HxSpatialGraph* graph, const int totalNumberOfGraphs)
{
    theObjectPool->addObject(graph);

    McHandle<HxSpatialGraphView> view = HxSpatialGraphView::createInstance();
    view->composeLabel(graph->getLabel(), "View");
    view->connect(graph);
    theObjectPool->addObject(view);
    view->portEdgeColoring.setValue(QString::fromLatin1(cNumberOfMatchesAttributeName.getString()));
    view->portItemsToShow.portDeprecated.setValue(0, 0);
    view->portEdgeColormap.setMinMax(0.0f, float(totalNumberOfGraphs - 1));
    view->fire();
}

void
HxCompareFilopodiaSpatialGraphs::cleanSpatialGraph(HxSpatialGraph* graph) const
{
    removeContours(graph);
    removeIntermediateNodes(graph);
}

void
HxCompareFilopodiaSpatialGraphs::removeContours(HxSpatialGraph* graph) const
{
    const int numContoursRemoved = SpatialGraphFunctions::removeContours(graph);
    if (numContoursRemoved > 0)
    {
        theMsg->printf(QString("Removed %1 contours from %2\n").arg(numContoursRemoved).arg(graph->getLabel()));
    }
}

void
HxCompareFilopodiaSpatialGraphs::removeIntermediateNodes(HxSpatialGraph* graph) const
{
    const int numVerticesBefore = graph->getNumVertices();

    SpatialGraphSelection selection(graph);
    selection.selectAllVerticesAndEdges();
    FindAndConvertVerticesToPointsOperationSet op = FindAndConvertVerticesToPointsOperationSet(graph, selection, selection);
    op.exec();

    const int numVerticesRemoved = numVerticesBefore - graph->getNumVertices();
    if (numVerticesRemoved > 0)
    {
        theMsg->printf(QString("Removed %1 intermediate vertices from %2\n").arg(numVerticesRemoved).arg(graph->getLabel()));
    }
}

std::vector<const HxSpatialGraph*>
HxCompareFilopodiaSpatialGraphs::readSpatialGraphs()
{
    std::vector<const HxSpatialGraph*> resultGraphs;
    for (int i = 0; i < portFileNames.getFileCount(); ++i)
    {
        const QString fn = portFileNames.getFilename(i);

        std::vector<HxData*> result(0);
        // TEMP
        auto tempResult = convertVector2McDArray(result);
        if (readAmiraMesh(fn, tempResult))
        {
            HxData* data = tempResult[0];
            if (data->isOfType(HxSpatialGraph::getClassTypeId()))
            {
                const HxSpatialGraph* graph = dynamic_cast<HxSpatialGraph*>(data);
                theMsg->printf(QString("Adding graph %1\n").arg(fn));
                resultGraphs.push_back(graph);
            }
            else
            {
                throw McException(QString("Error: file %1 is not a HxSpatialGraph").arg(fn));
            }
        }
        else
        {
            throw McException(QString("Error: Cannot read file %1").arg(fn));
        }
    }
    return resultGraphs;
}


std::vector<HxSpatialGraph*>
HxCompareFilopodiaSpatialGraphs::matchGraphs(const std::vector<HxSpatialGraph*>& graphs,
                                             const float maxCorrespondenceDistance)
{
    std::vector<HxSpatialGraph*> result;

    for (unsigned int i = 0; i < graphs.size(); ++i)
    {
        HxSpatialGraph* graph = graphs[i]->duplicate();
        graph->addAttribute(cNumberOfMatchesAttributeName.getString(), HxSpatialGraph::POINT, McPrimType::MC_INT32, 1);
        PointAttribute* pAtt = graph->findEdgePointAttribute(cNumberOfMatchesAttributeName.getString());
        for (int e = 0; e < graph->getNumEdges(); ++e)
        {
            for (int p = 0; p < graph->getNumEdgePoints(e); ++p)
            {
                pAtt->setIntDataAtPoint(0, e, p);
            }
        }
        result.push_back(graph);
    }

    for (unsigned int source = 0; source < result.size(); ++source)
    {
        HxSpatialGraph* sourceGraph = result[source];
        PointAttribute* pAtt = sourceGraph->findEdgePointAttribute(cNumberOfMatchesAttributeName.getString());

        for (unsigned int target = 0; target < graphs.size(); ++target)
        {
            if (source != target)
            {
                for (int e = 0; e < sourceGraph->getNumEdges(); ++e)
                {
                    for (int p = 0; p < sourceGraph->getNumEdgePoints(e); ++p)
                    {
                        const McVec3f point = sourceGraph->getEdgePoint(e, p);
                        if (hasPointWithinRadius(result[target], point, maxCorrespondenceDistance))
                        {
                            const int numMatchingGraphs = pAtt->getIntDataAtPoint(e, p) + 1;
                            pAtt->setIntDataAtPoint(numMatchingGraphs, e, p);
                        }
                    }
                }
            }
        }
    }
    return result;
}

HxSpreadSheet*
HxCompareFilopodiaSpatialGraphs::createMatchStatisticsSpreadsheet(const std::vector<HxSpatialGraph*>& graphs) const
{
    const unsigned int numberOfGraphs = graphs.size();

    HxSpreadSheet* sheet = HxSpreadSheet::createInstance();
    sheet->setLabel("MatchStatistics");
    sheet->addColumn("Description", HxSpreadSheet::Column::STRING);
    sheet->setNumRows(2*(numberOfGraphs + 1));

    sheet->column(0)->setValue(0, "Total length");
    sheet->column(0)->setValue(numberOfGraphs + 1, "Total number of filaments");
    for (unsigned int i = 0; i < numberOfGraphs; ++i)
    {
        const McString strLength = McString().printf("Length matched with %d other graphs", i);
        sheet->column(0)->setValue(i + 1, strLength.getString());

        const McString strFilament = McString().printf("Number of filaments matched with %d other graphs", i);
        sheet->column(0)->setValue(i + numberOfGraphs + 2, strFilament.getString());
    }

    for (unsigned int currentGraph = 0; currentGraph < numberOfGraphs; ++currentGraph)
    {
        const McString columnName = McString(graphs[currentGraph]->getLabel());
        sheet->addColumn(columnName.getString(), HxSpreadSheet::Column::FLOAT);
        const MatchStatistics matchStatistics = getGraphStatisticsForGraph(graphs[currentGraph], numberOfGraphs);

        sheet->column(currentGraph + 1)->setValue(0, float(matchStatistics.getTotalLength()));
        sheet->column(currentGraph + 1)->setValue(numberOfGraphs + 1, matchStatistics.getTotalNumberFilaments());
        for (unsigned int otherGraph = 0; otherGraph < numberOfGraphs; ++otherGraph)
        {
            sheet->column(currentGraph + 1)->setValue(otherGraph + 1, float(matchStatistics.getLengthInCommonWithNOtherGraphs(otherGraph)));
            sheet->column(currentGraph + 1)->setValue(otherGraph + numberOfGraphs + 2, matchStatistics.getNumberFilamentsInCommonWithNOtherGraphs(otherGraph));
        }
    }

    return sheet;
}

HxCompareFilopodiaSpatialGraphs::MatchStatistics
HxCompareFilopodiaSpatialGraphs::getGraphStatisticsForGraph(const HxSpatialGraph* graph,
                                                                  const int totalNumberOfGraphs) const
{
    const PointAttribute* pointAtt = graph->findEdgePointAttribute(cNumberOfMatchesAttributeName);
    const EdgeVertexAttribute* edgeAtt = graph->findEdgeAttribute("matchedEdgeLabels");
    mcassert(pointAtt);

    MatchStatistics matchStatistics(totalNumberOfGraphs);

    for (int e = 0; e < graph->getNumEdges(); ++e)
    {
        const int numMatchedFilaments = edgeAtt->getIntDataAtIdx(e);
        qDebug() << "e" << e << "numMatchedFilaments" << numMatchedFilaments;
        matchStatistics.addToNumberFilamentsInCommonWithNOtherGraphs(numMatchedFilaments, 1);
        for (int p = 1; p < graph->getNumEdgePoints(e); ++p)
        {
            const McVec3f previousPoint = graph->getEdgePoint(e, p - 1);
            const McVec3f currentPoint = graph->getEdgePoint(e, p);
            const double halfSegmentLength = 0.5 * (currentPoint - previousPoint).length();
            const int numMatchesPreviousPoint = pointAtt->getIntDataAtPoint(e, p - 1);
            const int numMatchesCurrentPoint = pointAtt->getIntDataAtPoint(e, p);

            matchStatistics.addToLengthInCommonWithNOtherGraphs(numMatchesPreviousPoint, halfSegmentLength);
            matchStatistics.addToLengthInCommonWithNOtherGraphs(numMatchesCurrentPoint, halfSegmentLength);
        }
    }

    return matchStatistics;
}
