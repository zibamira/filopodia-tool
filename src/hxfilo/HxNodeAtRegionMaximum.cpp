#include "HxNodeAtRegionMaximum.h"
#include <hxfield/HxUniformLabelField3.h>
#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <mclib/McException.h>
#include <hxcore/HxMessage.h>
#include <qdebug.h>


HX_INIT_CLASS(HxNodeAtRegionMaximum,HxCompModule)

HxNodeAtRegionMaximum::HxNodeAtRegionMaximum() :
    HxCompModule(HxUniformScalarField3::getClassTypeId()),
    portGlobalOrRegion(this, "Maxima", tr("Maxima"), 2),
    portRegions(this, tr("Regions"), tr("Regions"), HxUniformLabelField3::getClassTypeId()),
    portRemoveBoundaryNodes(this, tr("RemoveBoundaryNodes"), tr("RemoveBoundaryNodes")),
    portAction(this, tr("action"), tr("Action"))
{
    portGlobalOrRegion.setLabel(0, "global");
    portGlobalOrRegion.setLabel(1, "in regions");
    portGlobalOrRegion.setValue(0);
}

HxNodeAtRegionMaximum::~HxNodeAtRegionMaximum()
{
}

bool isBoundaryIndex(const McVec3i& idx, const McDim3l& dims) {
    if (idx.i == 0 || idx.i == dims.nx-1) {
        return true;
    }
    if (idx.j == 0 || idx.j == dims.ny-1) {
        return true;
    }
    if (idx.k == 0 || idx.k == dims.nz-1) {
        return true;
    }
    return false;
}


void getNodesAtRegionMaximum(const HxUniformScalarField3* scalarField,
                             HxSpatialGraph* graph,
                             const bool removeBoundaryNodes,
                             HxUniformLabelField3* labelField=0)
{

    const McDim3l& dims = scalarField->lattice().getDims();

    int maxLabels = 1;
    if (labelField) {

        if(labelField->lattice().primType() != McPrimType::MC_UINT8) {
            throw McException(QString("Only 8-bit label fields supported"));
        }

        const McDim3l& dimsLabel = labelField->lattice().getDims();

        if (dims[0]!=dimsLabel[0] || dims[1]!=dimsLabel[1] || dims[2]!=dimsLabel[2]) {
            throw McException(QString("Data and label field of regions must have same size"));
        }

        float minVal, maxVal;
        labelField->getRange(minVal, maxVal);
        maxLabels = int(maxVal + 0.5) + 1;
    }

    std::vector<std::vector<McVec3i> > pointsPerLabel(maxLabels);
    std::vector<float> maxValuePerLabel(maxLabels);
    std::fill(maxValuePerLabel.begin(), maxValuePerLabel.end(), std::numeric_limits<float>::min());

    float scalarVal;
    int label = 0;

    for (int z=0; z<dims[2]; ++z) {
        for (int y=0; y<dims[1]; ++y) {
            for (int x=0; x<dims[0]; ++x) {
                scalarField->lattice().eval(x, y, z, &scalarVal);
                if (labelField) {
                    labelField->lattice().evalNative(x, y, z, &label);
                    if (label == 0) continue;                           // Scip background label
                }

                if (scalarVal > maxValuePerLabel[label]) {
                    if (!removeBoundaryNodes || !isBoundaryIndex(McVec3i(x, y, z), dims)) {
                        maxValuePerLabel[label] = scalarVal;
                        pointsPerLabel[label].clear();
                        pointsPerLabel[label].push_back(McVec3i(x,y,z));
                    }
                } else if (scalarVal == maxValuePerLabel[label]) {
                    if (!removeBoundaryNodes || !isBoundaryIndex(McVec3i(x, y, z), dims)) {
                        pointsPerLabel[label].push_back(McVec3i(x,y,z));
                    }
                }
            }
        }
    }

    graph->clear();
    EdgeVertexAttribute* vAtt = graph->addVertexAttribute("MaxValue", McPrimType::MC_FLOAT, 1);

    McHandle<HxCoord3> coords = scalarField->lattice().coords();
    for (int l=0; l<maxLabels; ++l) {
        float maxValue = maxValuePerLabel[l];
        for (int i=0; i<pointsPerLabel[l].size(); ++i) {
            McVec3f pos = coords->pos(pointsPerLabel[l][i]);
            const int vertexId = graph->addVertex(McVec3f(pos[0], pos[1], pos[2]));
            vAtt->setFloatDataAtIdx(vertexId, maxValue);
        }
    }
}


void HxNodeAtRegionMaximum::compute()
{
    if (!portAction.wasHit()) return;

    HxUniformScalarField3* inputField = hxconnection_cast<HxUniformScalarField3> (portData);
    if (!inputField) {
        throw McException(QString("No input field"));
    }

    McHandle<HxSpatialGraph> output = dynamic_cast<HxSpatialGraph*>(getResult());
    if (!output) {
        output = HxSpatialGraph::createInstance();
        output->setLabel("MaximaNodes");
    }

    const bool removeBoundaryNodes = portRemoveBoundaryNodes.getValue(0);

    if (portGlobalOrRegion.getValue()==1) {
        HxUniformLabelField3* regionField = hxconnection_cast<HxUniformLabelField3> (portRegions);
        if (!regionField) {
            throw McException(QString("Please connect label field or choose \"global maximum\"."));
        }
        getNodesAtRegionMaximum(inputField, output, removeBoundaryNodes, regionField);

    } else {
        getNodesAtRegionMaximum(inputField, output, removeBoundaryNodes);
    }

    setResult(output);
    theMsg->printf(QString("NodeAtRegionMaximum: found %1 nodes").arg(output->getNumVertices()));

}

void
HxNodeAtRegionMaximum::update()
{
    if (portGlobalOrRegion.isNew())
    {
        if (portGlobalOrRegion.getValue() == 0)
        {
            portRegions.hide();
        }
        else
        {
            portRegions.show();
        }
    }
}
