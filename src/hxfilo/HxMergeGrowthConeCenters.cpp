#include "HxMergeGrowthConeCenters.h"
#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <hxspatialgraph/internal/HierarchicalLabels.h>
#include <mclib/McException.h>
#include <hxcore/HxMessage.h>

#include "ConvertVectorAndMcDArray.h"

HX_INIT_CLASS(HxMergeGrowthConeCenters,HxCompModule)

HxMergeGrowthConeCenters::HxMergeGrowthConeCenters() :
    HxCompModule(HxSpatialGraph::getClassTypeId()),
    portColormap(this, tr("Colormap"), tr("Colormap")),
    portAction(this, tr("action"), tr("Action")),
    mTimeStep(0)
{
}

HxMergeGrowthConeCenters::~HxMergeGrowthConeCenters()
{
}

void HxMergeGrowthConeCenters::compute()
{
    const char* timeStepAttributeName = "TimeStep";

    if (portAction.wasHit()) {
        HxSpatialGraph* input = hxconnection_cast<HxSpatialGraph> (portData);
        if (!input) {
            throw McException(QString("No input graph"));
        }

        McHandle<HxSpatialGraph> output = dynamic_cast<HxSpatialGraph*>(getResult());
        if (!output) {
            mTimeStep = 0;
            output = input->duplicate();
            output->setLabel("MergedGrowthConeCenters");
            HierarchicalLabels* outputLabels = output->addNewLabelGroup(timeStepAttributeName, false, true, false);
            const McString newLabelName = McString().printf("T_%03d", mTimeStep);
            const int newLabelId = outputLabels->addLabel(0, newLabelName, SbColor());
            EdgeVertexAttribute* timeStepAtt = output->findVertexAttribute(timeStepAttributeName);
            for (int v=0; v<output->getNumVertices(); ++v) {
                timeStepAtt->setIntDataAtIdx(v, newLabelId);
            }
            theMsg->printf(QString("MergeGrowthConeCenters: creating timestep %1 (adding %2 nodes. total: %3)")
                           .arg(mTimeStep).arg(input->getNumVertices()).arg(output->getNumVertices()));
        }
        else {
            const McString newLabelName = McString().printf("T_%03d", mTimeStep);
            HierarchicalLabels* outputLabels = output->getLabelGroup(timeStepAttributeName);
            const int newLabelId = outputLabels->addLabel(0, newLabelName, SbColor());

            McHandle<HxSpatialGraph> graphToMerge = input->duplicate();
            graphToMerge->addNewLabelGroup(timeStepAttributeName, false, true, false);
            EdgeVertexAttribute* timeStepAtt = graphToMerge->findVertexAttribute(timeStepAttributeName);
            for (int v=0; v<graphToMerge->getNumVertices(); ++v) {
                timeStepAtt->setIntDataAtIdx(v, newLabelId);
            }
            output->merge(graphToMerge);
            theMsg->printf(QString("MergeGrowthConeCenters: adding timestep %1 (adding %2 nodes. total: %3)")
                           .arg(mTimeStep).arg(input->getNumVertices()).arg(output->getNumVertices()));
        }

        HierarchicalLabels* labels = output->getLabelGroup(timeStepAttributeName);
        const int maxLabel = labels->getMaxLabelId();
        portColormap.setMinMax(1, maxLabel);
        McDArray<int> childLabelIds = labels->getChildIds(0);
        for (int i=0; i<childLabelIds.size(); ++i) {
            const SbColor c = portColormap.getColor(float(childLabelIds[i]));
            const McColor color(c[0], c[1], c[2]);
            labels->setLabelColor(childLabelIds[i], color);
        }
        labels->setLabelColor(0, McColor(0.0f, 0.0f, 0.0f));

        setResult(output);

        mTimeStep++;
    }
}

