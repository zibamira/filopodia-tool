#include "HxLabelInsideOutside.h"
#include "FilopodiaFunctions.h"
#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <mclib/McException.h>


HX_INIT_CLASS(HxLabelInsideOutside,HxCompModule)

HxLabelInsideOutside::HxLabelInsideOutside() :
    HxCompModule(HxSpatialGraph::getClassTypeId()),
    portAction(this, tr("action"), tr("Action"))
{
}


HxLabelInsideOutside::~HxLabelInsideOutside()
{
}


void HxLabelInsideOutside::compute()
{
    if (!portAction.wasHit()) return;

    HxSpatialGraph* graph = hxconnection_cast<HxSpatialGraph> (portData);
    if (!graph) {
        throw McException(QString("No input graph."));
    }

    const HierarchicalLabels* timeLabels = graph->getLabelGroup(FilopodiaFunctions::getTimeStepAttributeName());
    const HierarchicalLabels* nodeTypeLabels = graph->getLabelGroup(FilopodiaFunctions::getTypeLabelAttributeName());

    if (!(timeLabels && nodeTypeLabels)) {
        throw McException(QString("no labels."));
    }

    McHandle<HxSpatialGraph> outputGraph = dynamic_cast<HxSpatialGraph*>(getResult());
    if (outputGraph) {
        outputGraph->copyFrom(graph);
    }
    else {
        outputGraph = graph->duplicate();
    }
    outputGraph->composeLabel(graph->getLabel(), "insideoutside");

    FilopodiaFunctions::assignGrowthConeAndFilopodiumLabels(outputGraph);

    setResult(outputGraph);
}


