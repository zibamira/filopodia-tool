#include "HxShortestPathToPointMap.h"
#include "FilopodiaFunctions.h"
#include <hxfield/HxUniformLabelField3.h>
#include <hxfield/HxLoc3Uniform.h>
#include <hxcore/HxObjectPool.h>
#include <mclib/McException.h>
#include <QDebug>

HX_INIT_CLASS(HxShortestPathToPointMap,HxCompModule)


HxShortestPathToPointMap::HxShortestPathToPointMap()
    : HxCompModule (HxUniformScalarField3::getClassTypeId())
    , portGraph (this, tr("graph"), tr("Graph"),HxSpatialGraph::getClassTypeId())
    , portCoordsOrGraph(this, "Start position", tr("Start position"), 2)
    , portCoords(this,"PointCoords", "Point Coordinates", 3)
    , portIntensityWeight(this, "intensityWeight", "Intensity Weight", 1)
    , portIntensityPower(this, "intensityPower", "Intensity Power", 1)
    , portTopBrightness(this, "topBrightness", "Top Brightness", 1)
    , portOutput(this, "output", "Output", 2)
    , portDoIt (this, "doIt", "Do It")
{
    portCoordsOrGraph.setLabel(0, "from coordinates");
    portCoordsOrGraph.setLabel(1, "from graph");
    portCoordsOrGraph.setValue(0);

    portIntensityWeight.setValue(100.0);
    portIntensityPower.setValue(1.0);
    portTopBrightness.setValue(255);

    portCoords.setValue(0, 0);
    portCoords.setValue(1, 0);
    portCoords.setValue(2, 0);

    portOutput.setLabel(OUTPUT_DISTANCEMAP,"Distance Map");
    portOutput.setValue(OUTPUT_DISTANCEMAP, 1);
    portOutput.setLabel(OUTPUT_PRIORMAP,"Prior Map");
    portOutput.setValue(OUTPUT_PRIORMAP, 1);
}

HxShortestPathToPointMap::~HxShortestPathToPointMap()
{
}


void HxShortestPathToPointMap::compute() {

    if (!portDoIt.wasHit()) return;

    if (!(portOutput.getValue(OUTPUT_DISTANCEMAP) || portOutput.getValue(OUTPUT_PRIORMAP))) return;

    const float intensityWeight = portIntensityWeight.getValue();
    const int topBrightness = portTopBrightness.getValue();
    const float intensitityPower = portIntensityPower.getValue();

    McHandle<HxUniformScalarField3> image = hxconnection_cast<HxUniformScalarField3>(portData);
    if (!image) {
        return;
    }

    McDim3l dims = image->lattice().getDims();
    McBox3f bbox = image->getBoundingBox();

    // Distance map
    McHandle<HxUniformScalarField3> distanceMap = mcinterface_cast<HxUniformScalarField3>(getResult(0));
    if (distanceMap && !distanceMap->isOfType(HxUniformScalarField3::getClassTypeId())) {
        distanceMap = 0;
    } else if (distanceMap) {
        const McDim3l& outdims = distanceMap->lattice().getDims();
        if (dims[0]!=outdims[0] || dims[1]!=outdims[1] || dims[2]!=outdims[2]||
                distanceMap->primType() != McPrimType::MC_FLOAT)
        {
            distanceMap = 0;
        }
    }
    if (!distanceMap) {
        distanceMap = new HxUniformScalarField3(dims, McPrimType::MC_FLOAT);
        distanceMap->setBoundingBox(bbox);
        distanceMap->setLabel("DistanceMap");
    }

    // Prior map
    McHandle<HxUniformLabelField3> priorMap = mcinterface_cast<HxUniformLabelField3>(getResult(1));
    if (priorMap && !priorMap->isOfType(HxUniformLabelField3::getClassTypeId())) {
        priorMap = 0;
    } else if (priorMap) {
        const McDim3l& outdims = priorMap->lattice().getDims();
        if (dims[0]!=outdims[0] || dims[1]!=outdims[1] || dims[2]!=outdims[2] ||
            priorMap->primType() != McPrimType::MC_UINT8)
        {
            priorMap = 0;
        }
    }
    if (!priorMap) {
        priorMap = new HxUniformLabelField3(dims, McPrimType::MC_UINT8);
        priorMap->setBoundingBox(bbox);
        priorMap->setLabel("PriorMap");
    }

    // Start position
    McVec3f rootPoint;
    if (portCoordsOrGraph.getValue() == 0) {

        rootPoint[0] = portCoords.getValue(0);
        rootPoint[1] = portCoords.getValue(1);
        rootPoint[2] = portCoords.getValue(2);

    } else {

        HxSpatialGraph* centers = hxconnection_cast<HxSpatialGraph> (portGraph);
        if (!centers) {
            throw McException(QString("No start position"));
        }
        if (centers->getNumVertices() != 1) {
            throw McException(QString("Please set only one vertex to indicate start position."));
        }

        rootPoint = centers->getVertexCoords(0);

        portCoords.setValue(0, rootPoint.x);
        portCoords.setValue(1, rootPoint.y);
        portCoords.setValue(2, rootPoint.z);
    }

    HxShortestPathToPointMap::computeDijkstraMap(image,
                                            rootPoint,
                                            McDim3l(0, 0, 0),
                                            McDim3l(dims[0]-1, dims[1]-1, dims[2]-1),
                                            intensityWeight,
                                            topBrightness,
                                            priorMap,
                                            distanceMap,
                                            intensitityPower);

    if (portOutput.getValue(OUTPUT_DISTANCEMAP) == 1) setResult(OUTPUT_DISTANCEMAP, distanceMap);
    if (portOutput.getValue(OUTPUT_PRIORMAP) == 1) setResult(OUTPUT_PRIORMAP, priorMap);
}



void HxShortestPathToPointMap::computeDijkstraMap(const HxUniformScalarField3 *image,
                                             const McVec3f rootPoint,
                                             const McDim3l startVoxel,
                                             const McDim3l endVoxel,
                                             const float intensityWeight,
                                             const int topBrightness,
                                             HxUniformLabelField3 *dijkstraMap,
                                             HxUniformScalarField3 *distanceMap,
                                             const float intensityPower)
{
    if (!image) {
        throw McException("No image provided");
    }
    if (!dijkstraMap) {
        throw McException("No dijkstra map provided");
    }
    if (!distanceMap) {
        throw McException("No distance map provided");
    }

    const McDim3l inDims = image->lattice().getDims();

    if ((startVoxel.nx < 0 || startVoxel.nx > endVoxel.nx) ||
        (startVoxel.ny < 0 || startVoxel.ny > endVoxel.ny) ||
        (startVoxel.nz < 0 || startVoxel.nz > endVoxel.nz) ||
        (endVoxel.nx < startVoxel.nx || endVoxel.nx >= inDims.nx) ||
        (endVoxel.ny < startVoxel.ny || endVoxel.ny >= inDims.ny) ||
        (endVoxel.nz < startVoxel.nz || endVoxel.nz >= inDims.nz))
    {
        throw McException("Invalid dimensions");
    }

    HxUniformCoord3* coords = dynamic_cast<HxUniformCoord3*>(image->lattice().coords());
    HxLoc3Uniform* location = dynamic_cast<HxLoc3Uniform*>(coords->createLocation());

    if (!location->set(rootPoint)) {
        throw McException("computeDijkstraMap: Invalid root point");
    }

    const McDim3l rootLoc = McDim3l(location->getIx(), location->getIy(), location->getIz());
    const McVec3f rootU = McVec3f(location->getUx(), location->getUy(), location->getUz());
    const McDim3l rootNearestVoxelCenter = FilopodiaFunctions::getNearestVoxelCenterFromIndex(rootLoc, rootU, inDims);

    if ((rootNearestVoxelCenter.nx < startVoxel.nx || rootNearestVoxelCenter.nx > endVoxel.nx) ||
        (rootNearestVoxelCenter.ny < startVoxel.ny || rootNearestVoxelCenter.ny > endVoxel.ny) ||
        (rootNearestVoxelCenter.nz < startVoxel.nz || rootNearestVoxelCenter.nz > endVoxel.nz))
    {
        throw McException("computeDijkstraMap: Root point outside volume");
    }

    const McVec3i rootIdxOrig(int(rootNearestVoxelCenter.nx),
                              int(rootNearestVoxelCenter.ny),
                              int(rootNearestVoxelCenter.nz));

    const McVec3i startVec(int(startVoxel.nx),
                           int(startVoxel.ny),
                           int(startVoxel.nz));

    const McVec3f voxelSize = image->getVoxelSize();

    const McDim3l outDims(endVoxel.nx-startVoxel.nx+1, endVoxel.ny-startVoxel.ny+1, endVoxel.nz-startVoxel.nz+1);
    dijkstraMap->lattice().resize(outDims);
    distanceMap->lattice().resize(outDims);

    McVec3f bboxMin, bboxMax;
    bboxMin[0] = image->getBoundingBox().getMin()[0] + voxelSize[0]*startVoxel.nx;
    bboxMin[1] = image->getBoundingBox().getMin()[1] + voxelSize[1]*startVoxel.ny;
    bboxMin[2] = image->getBoundingBox().getMin()[2] + voxelSize[2]*startVoxel.nz;

    bboxMax[0] = image->getBoundingBox().getMin()[0] + voxelSize[0]*endVoxel.nx;
    bboxMax[1] = image->getBoundingBox().getMin()[1] + voxelSize[1]*endVoxel.ny;
    bboxMax[2] = image->getBoundingBox().getMin()[2] + voxelSize[2]*endVoxel.nz;

    dijkstraMap->setBoundingBox(McBox3f(bboxMin, bboxMax));
    distanceMap->setBoundingBox(McBox3f(bboxMin, bboxMax));

    // Initialization of distanceMap and priorMap
    const unsigned char UNDEFINED = 255;

    for (int x=0; x<outDims.nx;++x) {
        for (int y=0; y<outDims.ny;++y) {
            for (int z=0; z<outDims.nz;++z) {
                distanceMap->set(x, y, z, std::numeric_limits<float>::max());
                dijkstraMap->set(x, y, z, UNDEFINED);
            }
        }
    }

    std::vector<std::vector<std::vector<McFHeapElementDijkstra*> > > elements;
    elements.resize(outDims.nx);
    for (int x=0; x<outDims.nx; ++x) {
        elements[x].resize(outDims.ny);
        for (int y=0; y<outDims.ny; ++y) {
            elements[x][y].resize(outDims.nz);
            std::fill(elements[x][y].begin(),elements[x][y].end(),nullptr);
        }
    }

    McFHeap<McFHeapElementDijkstra> q;

    McFHeapElementDijkstra* rootElem = new McFHeapElementDijkstra;
    const McVec3i rootIdxCrop = rootIdxOrig - startVec;
    rootElem->distance = 0.0;
    rootElem->coordinates = rootIdxCrop;
    elements[rootIdxCrop.i][rootIdxCrop.j][rootIdxCrop.k] = rootElem;

    q.insert(rootElem);
    dijkstraMap->set(rootIdxCrop[0], rootIdxCrop[1], rootIdxCrop[2], FilopodiaFunctions::vectorToDirection(McVec3i(0, 0, 0)));
    distanceMap->set(rootIdxCrop[0],rootIdxCrop[1],rootIdxCrop[2], 0.0f);

    const mclong totalVoxels = outDims.nx * outDims.ny * outDims.nz;
    int processedVoxels = 0;

    McFHeapElementDijkstra* minElem = q.getMin();
    while (minElem != 0) {
        q.deleteMin();
        const McVec3i currentIdxCrop = minElem->coordinates;
        const McVec3i currentIdx = currentIdxCrop + startVec;
        const float currentImageVal = MC_MIN2(topBrightness, image->evalReg(currentIdx[0], currentIdx[1], currentIdx[2]));
        const float currentDist = distanceMap->evalReg(currentIdxCrop[0], currentIdxCrop[1], currentIdxCrop[2]);
        for (int dz=-1; dz<=1; ++dz) {
            for (int dy=-1; dy<=1; ++dy) {
                for (int dx=-1; dx<=1; ++dx) {
                    if (dx == 0 && dy == 0 && dz == 0) {
                        continue;
                    }

                    const int x = currentIdxCrop[0] + dx;
                    const int y = currentIdxCrop[1] + dy;
                    const int z = currentIdxCrop[2] + dz;

                    if (x < 0 || x >= outDims.nx) continue;
                    if (y < 0 || y >= outDims.ny) continue;
                    if (z < 0 || z >= outDims.nz) continue;

                    const McVec3i neighborIdxCrop(x, y, z);
                    const McVec3i neighborIdx = neighborIdxCrop + startVec;
                    const float neighborImageVal = MC_MIN2(topBrightness, image->evalReg(neighborIdx[0], neighborIdx[1], neighborIdx[2]));
                    const float length = voxelSize.compprod(McVec3f(dx, dy, dz)).length();
                    const float intensityPenalty = intensityWeight / (0.0001f + 0.5f * (currentImageVal+neighborImageVal));
                    const float neighborDist = distanceMap->evalReg(neighborIdxCrop[0], neighborIdxCrop[1], neighborIdxCrop[2]);
                    const float newDist = currentDist + length + pow(intensityPenalty, intensityPower);

                    if (newDist < neighborDist) {
                        distanceMap->set(neighborIdxCrop[0], neighborIdxCrop[1], neighborIdxCrop[2], newDist);
                        const unsigned char prior = FilopodiaFunctions::vectorToDirection(McVec3i(-dx, -dy, -dz));
                        dijkstraMap->set(neighborIdxCrop[0], neighborIdxCrop[1], neighborIdxCrop[2], prior);

                        McFHeapElementDijkstra* currentElem = elements[neighborIdxCrop.i][neighborIdxCrop.j][neighborIdxCrop.k];
                        if (currentElem == 0) {
                            McFHeapElementDijkstra* elem = new McFHeapElementDijkstra;
                            elem->distance = newDist;
                            elem->coordinates = neighborIdxCrop;
                            elements[neighborIdxCrop.i][neighborIdxCrop.j][neighborIdxCrop.k] = elem;
                            q.insert(elem);
                        }
                        else {
                            q.deleteElem(currentElem);
                            currentElem->distance = newDist;
                            q.insert(currentElem);
                        }
                    }
                }
            }
        }
        processedVoxels += 1;
        if (processedVoxels % 1000000 == 0) {
            printf("processed %d/%ld ( %f %% )\n", processedVoxels, totalVoxels, 100.0f * float(processedVoxels)/float(totalVoxels));
        }
        minElem = q.getMin();
    }

    for (int x=0; x<outDims.nx; ++x) {
        for (int y=0; y<outDims.ny; ++y) {
            for (int z=0; z<outDims.nz; ++z) {
                if (elements[x][y][z] != 0) {
                    delete elements[x][y][z];
                }
            }
        }
    }

}

void
HxShortestPathToPointMap::update()
{
    if (portCoordsOrGraph.isNew())
    {
        if (portCoordsOrGraph.getValue() == 0)
        {
            portGraph.hide();
        }
        else
        {
            portGraph.show();
        }
    }

    if (portGraph.isNew())
    {
        HxSpatialGraph* centers = hxconnection_cast<HxSpatialGraph> (portGraph);
        if (!centers) {
            return;
        }
        if (centers->getNumVertices() != 1) {
            throw McException(QString("Please set only one vertex as start point."));
        }

        const McVec3f rootPoint = centers->getVertexCoords(0);

        portCoords.setValue(0, rootPoint.x);
        portCoords.setValue(1, rootPoint.y);
        portCoords.setValue(2, rootPoint.z);

    }

}
