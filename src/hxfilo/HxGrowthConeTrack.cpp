#include "HxGrowthConeTrack.h"
#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <hxspatialgraph/internal/HierarchicalLabels.h>
#include <mclib/McException.h>

#include "ConvertVectorAndMcDArray.h"

HX_INIT_CLASS(HxGrowthConeTrack,HxCompModule)

HxGrowthConeTrack::HxGrowthConeTrack() :
    HxCompModule(HxSpatialGraph::getClassTypeId()),
    portDistanceThreshold(this, tr("DistanceThreshold"), tr("DistanceThreshold")),
    portAction(this, tr("action"), tr("Action"))
{
}

HxGrowthConeTrack::~HxGrowthConeTrack()
{
}

bool preceeds(const int v1, const int v2, const HxSpatialGraph* graph, const float distanceThreshold) {
    const char* timeStepAttributeName = "TimeStep";
    const EdgeVertexAttribute* timeAtt = graph->findVertexAttribute(timeStepAttributeName);
    const int v1time = timeAtt->getIntDataAtIdx(v1);
    const int v2time = timeAtt->getIntDataAtIdx(v2);
    const McVec3f v1coords = graph->getVertexCoords(v1);
    const McVec3f v2coords = graph->getVertexCoords(v2);
    return v2time == (v1time + 1) && (v1coords-v2coords).length() < distanceThreshold;
}


void HxGrowthConeTrack::compute()
{
    if (portAction.wasHit()) {
        HxSpatialGraph* input = hxconnection_cast<HxSpatialGraph> (portData);
        if (!input) {
            throw McException(QString("No input graph"));
        }

        const float distanceThreshold = MC_MAX2(portDistanceThreshold.getValue(), 0.0f);

        McHandle<HxSpatialGraph> output = dynamic_cast<HxSpatialGraph*>(getResult());
        if (!output) {
            output = HxSpatialGraph::createInstance();
            output->setLabel("TrackedGrowthCenters");
        }
        output->copyFrom(input);
        setResult(output);

        const int numVertices = output->getNumVertices();
        std::vector<std::vector<int> > preCandidates(numVertices);
        std::vector<std::vector<int> > postCandidates(numVertices);

        for (int v=0; v<numVertices; ++v) {
            for (int u=0; u<numVertices; ++u) {
                if (u != v) {
                    if (preceeds(u, v, output, distanceThreshold)) {
                        preCandidates[v].push_back(u);
                    }
                    if (preceeds(v, u, output, distanceThreshold)) {
                        postCandidates[v].push_back(u);
                    }
                }
            }
        }
        for (int u=0; u<numVertices; ++u) {
            if (postCandidates[u].size() == 1) {
                const int v = postCandidates[u][0];
                if (preCandidates[v].size() == 1 && preCandidates[v][0] == u) {
                    output->addEdge(u, v);
                }
            }
        }

        const char* growthConeAttName = "GrowthCones";
        HierarchicalLabels* labels = output->addNewLabelGroup(growthConeAttName, true, true, false);
        EdgeVertexAttribute* vAtt = output->findVertexAttribute(growthConeAttName);
        EdgeVertexAttribute* eAtt = output->findEdgeAttribute(growthConeAttName);

        std::vector<McColor> colors;
        McBitfield vDone(numVertices);
        int growthConeNum = 0;
        for (int v=0; v<numVertices; ++v) {
            if (!vDone[v]) {
                const McString labelName = McString().printf("rootNodes_%03d", growthConeNum);
                const McColor newColor = McColor::mostDifferentColorRGB(colors);
                const SbColor labelColor(newColor.r, newColor.g, newColor.b);

                const int labelId = labels->addLabel(0, labelName.getString(), labelColor);
                const SpatialGraphSelection sel = output->getConnectedComponent(v);

                SpatialGraphSelection::Iterator iter(sel);
                int u = iter.vertices.nextSelected();
                while (u != -1) {
                    vAtt->setIntDataAtIdx(u, labelId);
                    vDone.set(u, true);
                    u = iter.vertices.nextSelected();
                }

                int e = iter.edges.nextSelected();
                while (e != -1) {
                    eAtt->setIntDataAtIdx(e, labelId);
                    e = iter.edges.nextSelected();
                }

                colors.push_back(newColor);
                ++growthConeNum;
            }
        }
    }
}

