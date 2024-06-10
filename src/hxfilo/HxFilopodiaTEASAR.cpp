#include "HxFilopodiaTEASAR.h"
#include "FilopodiaFunctions.h"
#include <hxskeleton/internal/SpDistanceMap.h>
#include <hxneurontree/internal/HxNeuronTree.h>
#include <hxfield/HxUniformLabelField3.h>
#include <hxfield/HxLoc3Uniform.h>
#include <hxlatticex/internal/HxLattice.h>
#include <hxlatticex/internal/HxLatticeGeom.h>
#include <hxcore/HxObjectPool.h>

#include <mclib/McException.h>
#include <mclib/internal/McIndexer3D.h>
#include <mclib/internal/McBlockIndexer4D.h>
#include <mclib/internal/McSparseData3D.h>
#include <mclib/internal/McRectPositions.h>

#include <hxspatialgraph/internal/CastHxSpatialGraphToHxLineSet.h>
#include <hxunits/internal/HxUnitsProviderHelpers.h>
#include <QApplication>

#include "ConvertVectorAndMcDArray.h"

#ifdef _WIN32
using std::cerr;
using std::endl;
#endif

HX_INIT_CLASS(HxFilopodiaTEASAR,HxCompModule)

enum Maps {
      VOXELSKEL
    , BDM
    , SSDM
    , SSPDM
    , TUBES
    , noOfMaps
};

HxFilopodiaTEASAR::HxFilopodiaTEASAR()
    : HxCompModule (HxUniformLabelField3::getClassTypeId())
    , portCenters (this, tr("centers"), tr("Centers"),HxSpatialGraph::getClassTypeId())
    , portLoops (this, "noEndMap", QApplication::translate("HxFilopodiaTEASAR", "No End Map"), HxUniformScalarField3::getClassTypeId ())
    , portMaps (this, "maps", QApplication::translate("HxFilopodiaTEASAR", "Maps"), noOfMaps)
    , portTubeParams (this, "tubesParams", QApplication::translate("HxFilopodiaTEASAR", "Tubes Params"), 2)
    , portFakeVoxelSizeZ (this, "fakeVoxelSizeZ", QApplication::translate("HxFilopodiaTEASAR", "FakeVoxelSizeZ"), 1)
    , portDoIt (this, "doIt", QApplication::translate("HxFilopodiaTEASAR", "Do It"))
{
    portMaps.setLabel (VOXELSKEL, QApplication::translate("HxFilopodiaTEASAR", "Voxel Skel"));
    portMaps.setLabel (BDM, QApplication::translate("HxFilopodiaTEASAR", "BDM"));
    portMaps.setLabel (SSDM, QApplication::translate("HxFilopodiaTEASAR", "SSDM"));
    portMaps.setLabel (SSPDM, QApplication::translate("HxFilopodiaTEASAR", "SSPDM"));
    portMaps.setLabel (TUBES, QApplication::translate("HxFilopodiaTEASAR", "tubes"));
    portMaps.setValue (VOXELSKEL, false);
    portMaps.setValue (BDM, false);
    portMaps.setValue (SSDM, false);
    portMaps.setValue (SSPDM, false);
    portMaps.setValue (TUBES, false);

    portTubeParams.setLabel (0, QApplication::translate("HxFilopodiaTEASAR", "slope"));
    portTubeParams.setLabel (1, QApplication::translate("HxFilopodiaTEASAR", "zeroVal"));
    portTubeParams.setValue (0, 2.5);
    portTubeParams.setValue (1, 4.0);

    portFakeVoxelSizeZ.setValue(-1.0f);
}

HxFilopodiaTEASAR::~HxFilopodiaTEASAR()
{
}

void HxFilopodiaTEASAR::update () {
    HxLattice* src = hxconnection_cast<HxLattice> (portData);

    if (src) {
      if (src->nDims() != 3) {
        theMsg->printf ("Need a three dimensional volume.");
        return;
      }
    }
    else
      return;
}


void HxFilopodiaTEASAR::compute() {
    if (!portDoIt.wasHit())
    {
        return;
    }

    HxLattice* src = dynamic_cast<HxLattice*> (portData.getSource (HxLattice::getClassTypeId ()));
    if (!src || src->nDims() != 3) {
        return;
    }
    if (src->coordType () != C_UNIFORM) {
        theMsg->printf ("only uniform coords supported");
        return;
    }
    if (src->primType () != McPrimType::MC_UINT8) {
        theMsg->printf ("only bytes supported");
        return;
    }

    HxPartRectLatticeGeom* latgeom = static_cast<HxPartRectLatticeGeom*> (portData.getSource (HxPartRectLatticeGeom::getClassTypeId()));
    if (!latgeom) {
        return;
    }
    McRectPositions positions;
    latgeom->getRectPositions (positions);

    const HxUniformLabelField3* labelField = hxconnection_cast<HxUniformLabelField3> (portData);
    const QString fileName = labelField->getFilename();

    theWorkArea->busy ();
    McDataSpace ds = src->getDataSpace();
    const int* dims = ds.dims();
    const mclong nnn = dims[0] * dims[1] * dims[2];
    McVec3f voxelSize;
    McBox3f bbox (src->getBoundingBox().box());
    voxelSize = bbox.getVoxelSize (dims);

    McVec3f fakeVoxelSize(voxelSize);
    if (portFakeVoxelSizeZ.getValue() > 0.0f) {
        fakeVoxelSize[2] = portFakeVoxelSizeZ.getValue();
    }
    const float voxelSizeAbs = voxelSize.length ();

    // Use deferred initialization of the object
    SpDistanceMap* distMap = new SpDistanceMap (0, dims, &(fakeVoxelSize[0]), theWorkArea, TO_OBJECT_BOUNDARY);

    theWorkArea->startWorking (QApplication::translate("HxFilopodiaTEASAR", "extracting Skeleton"));
    // Iterate over input lattice in slices
    // and provide data to the distance map
    {
        theWorkArea->startWorkingNoStop (QApplication::translate("HxFilopodiaTEASAR", "scanning object"));
        const int o[4] = {0, 0, 0, 0};
        const int s[4] = {dims[0], dims[1], dims[2], 1};
        int bs[4] = {s[0], s[1], s[3], 1};

        // ... use not more than 10 MB buffer
        while ((mcint64(bs[0]) * mcint64(bs[1]) * mcint64(bs[2]) > mcint64(10 << 20)) && bs[2] > 1) {
            bs[2] /= 2;
        }
        McBlockIndexer4D indexer (o, s, bs);
        int actpos[4];
        int actsize[4];
        std::vector<unsigned char> buffer (bs[0] * bs[1] * bs[2]);
        McDataSpace latspace = src->getDataSpace ();
        while (indexer.next (actpos, actsize)) {
            theWorkArea->setProgressValue (float(actpos[2]) / float(dims[2]));

            latspace.selectHyperslab (actpos, actsize, 0);
            McMemorySelection buffersel(McDataPointer((void*)&(*buffer.begin()), McPrimType(McPrimType::MC_UINT8)), McDataSpace (3, actsize));

            src->getData (latspace, buffersel);
            distMap->useData (&(*buffer.begin()), actsize[0] * actsize[1] * actsize[2]);
        }
        theWorkArea->stopWorking ();
    }

    const unsigned char* const dat = 0;
    McSuperPackedIndexing* packer = 0;
    packer = distMap->getPacker ();

     // The index of the starting point
    int refPt = -1;

    // Converting between linear and 3d indices and getting the neighbors
    McDim3l longdims(  dims[0], dims[1], dims[2] );
    HxNeighborIndexing3 idx (longdims, N26, W_ONE);

    // Pick root of skeleton tree

    HxSpatialGraph* centers = hxconnection_cast<HxSpatialGraph> (portCenters);
    if (!centers) {
        throw McException(QString("No growth cone centers"));
    }

    int gc, time, nodeId;
    FilopodiaFunctions::getGrowthConeNumberAndTimeFromFileName(fileName, gc, time);
    McVec3f rootPoint = FilopodiaFunctions::getGrowthConeCenter(centers, gc,time, nodeId);

    HxUniformCoord3* coords = dynamic_cast<HxUniformCoord3*>(labelField->lattice().coords());
    HxLoc3Uniform* location = dynamic_cast<HxLoc3Uniform*>(coords->createLocation());

    if (!location->set(rootPoint)) {
        throw McException("Tracing error: Invalid root point");
    }

    const McDim3l rootLoc = McDim3l(location->getIx(), location->getIy(), location->getIz());
    const McVec3f rootU = McVec3f(location->getUx(), location->getUy(), location->getUz());
    const McDim3l rootNearestVoxelCenter = FilopodiaFunctions::getNearestVoxelCenterFromIndex(rootLoc, rootU, dims);
    const McVec3i rootPointGlobal(int(rootNearestVoxelCenter.nx),
                                  int(rootNearestVoxelCenter.ny),
                                  int(rootNearestVoxelCenter.nz));

    int rootIdx = idx.index1D (int (rootPointGlobal[0]), int (rootPointGlobal[1]), int (rootPointGlobal[2]));

    if (rootIdx < 0 || rootIdx > dims[0] * dims[1] * dims[2]
                || ((dat && dat[rootIdx] == 0) || (packer && !packer->valid (rootIdx)))) {
        theMsg->printf ("Root point outside, please move it.");
        theWorkArea->notBusy();
        theWorkArea->stopWorking();
        return;
    }

    refPt = rootIdx;

    int refX, refY, refZ;
    idx.index3D (refPt, refX, refY, refZ);

    distMap->calcBDM ();
    theWorkArea->setProgressInfo (QApplication::translate("HxFilopodiaTEASAR", "calculating SSPDM"));

    HxData* out;
    // Calc penaltyfield
    distMap->calcSSPDM (refX, refY, refZ);
    // Calc distance field
    distMap->calcSSDM (refX, refY, refZ);

    // The primary result is stored as a lineset (neurontree)
    // together with the boundary distance map as value 0
    // At least a value half of the voxelsize (diagonal) is stored
    // to avoid zero radius
    McHandle<HxNeuronTree> lineset = HxNeuronTree::createInstance();
    lineset->setNumDataValues (1);
    lineset->composeLabel(static_cast<HxBase*>(src->getInterface (HxBase::getClassTypeId()))->getLabel(),"lineset");

    // Ahe skeleton is represented as a sparse dataset in 3D
    // at each voxel the index of the generated point is stored
    // If no point is stored -1 will be returned.
    McSparseData3D<mclong, -1> skeleton (dims[2]);

    // ... initialize with seed point
    {
        McVec3f pos3d;
        const int ipos[3] = {refX, refY, refZ};
        positions.vertexpos (ipos, &(pos3d[0]));
        lineset->points.push_back(pos3d);
        const int pointalias = lineset->points.size()-1;
        float bdm = distMap->getBD (refX, refY, refZ);
        if (bdm < voxelSizeAbs / 2.0) {
            bdm = voxelSizeAbs / 2.0;
        }
        lineset->data[0].append (bdm);

        skeleton.set (refX, refY, refZ, pointalias);
    }

    // Clear tubes, otherwise they're not initialized
    distMap->clearTubes ();

    // If a NoEndMap is given, mark areas
    HxUniformScalarField3* noEnd = static_cast<HxUniformScalarField3*>
        (portLoops.getSource ());
    if (noEnd && (noEnd->primType () == McPrimType::MC_UINT8)) {
        unsigned char* noEndData = static_cast<unsigned char*>
            (noEnd->lattice().dataPtr ());

        theWorkArea->setProgressInfo (QApplication::translate("HxFilopodiaTEASAR", "processing Loops"));
        McIndexer3D indexer (dims);
        mclong pos;
        std::vector<mclong> tmp (0);
        while (indexer.nextLong (pos)) {
            if (noEndData[pos]) {
                tmp.push_back(pos);
            }
        }
        distMap->addTube (tmp.size(), &(*tmp.begin())
                    , portTubeParams.getValue (0)
                    , portTubeParams.getValue (1));
    }

    // Find largest value not in skeleton
    // First we build a list of all voxels that may be endpoints
    // This is much faster than iterating over the whole lattice
    int actSparseMap = -1;
    std::vector<int> searchIdx[2];
    int sparseLimit = nnn / 5;
    float sparseRecalcStep = 0.02;  // Rebuild map after ...
    float sparseNextRecalc = -0.1; // First rebuild

    int portNumParts = -1;
    int numParts = 0;
    while (portNumParts < 0 || numParts < portNumParts) {

        // (re)build sparse map if necessary
        if (distMap->getTubeFillRatio () > sparseNextRecalc) {
            theWorkArea->setProgressInfo (QApplication::translate("HxFilopodiaTEASAR", "Recalculating sparse Map"));
            sparseNextRecalc = distMap->getTubeFillRatio () + sparseRecalcStep;

            // If we already have a sparse map, use it as input
            if (actSparseMap >= 0) {
                std::vector<int>& oldmap = searchIdx[actSparseMap];
                actSparseMap = (actSparseMap + 1) % 2;
                std::vector<int>& newmap = searchIdx[actSparseMap];
                newmap.resize (0);

                const int size = oldmap.size ();
                for (int n = 0; n < size; n++) {
                    if (!distMap->getTube (oldmap[n])) {
                        newmap.push_back (oldmap[n]);
                    }
                }

                oldmap.resize (0);
            } else {
                // ... else calculate a new one
                McIndexer3D indexer (dims);
                int pos;
                actSparseMap = 0;
                std::vector<int>& map = searchIdx[actSparseMap];
                map.resize(0);
                while (indexer.next (pos)) {
                    if (((dat && dat[pos]) || (packer && packer->valid (pos))) && !distMap->getTube (pos)) {
                        const int idx = map.size();
                        map.push_back(pos);
                        if (idx > sparseLimit) {
                            map.resize (0);
                            actSparseMap = -1;
                            break;
                        }
                    }
                }
            }

            if (actSparseMap >= 0) {
                theMsg->printf ("%f percent filled"
                        , 100.0 * float(searchIdx[actSparseMap].size()) / float(nnn));
            } else {
                theMsg->printf ("not sparse");
            }
        }

        numParts++;
        theWorkArea->setProgressInfo (QApplication::translate("HxFilopodiaTEASAR", "Part %1 ... ").arg(numParts));

        if (theWorkArea->wasInterrupted ()) {
            break;
        }

        float maxSSP = 0.0;
        int otherEnd = -1;
        // Try to search a real maximum lower than FLT_MAX if ALL_COMPONENTS is insensitive or selected and nothing was found,include FLT_MAX into the search.
        // This order ensures complete processing of a connected component of the object before starting with the next component.
        //
        // This is very useful if a lot of small noisy parts are floating around because they are ignored in a first try and only processed at the end when it is (hopefully) faster.
        bool searchAllComponents = true;
        for (int i = 0; (i < (searchAllComponents ? 2 : 1)) && (otherEnd < 0); i++) {
            bool ignoreMax = (i == 0) ? true : false;

            if (actSparseMap >= 0) {  // Search sparse
                std::vector<int>& map = searchIdx[actSparseMap];

                const int size = map.size ();
                for (int n = 0; n < size; n++) {
                    if (!distMap->getTube (map[n])) {
                        const float val = distMap->getSSD (map[n]);
                        if (val > FLT_MAX * 0.95) {
                            // Either we ingnore a global maximum
                            if (ignoreMax) {
                                continue;
                            } else {
                                // ... or we're done
                                maxSSP = val;
                                otherEnd = map[n];
                                break;
                            }
                        }
                        if (val > maxSSP) {
                            maxSSP = val;
                            otherEnd = map[n];
                        }
                    }
                }

            }
            else { // Search all
                printf("search all\n");

                for (int n = 0; n < nnn; n++) {
                    if (((dat && dat[n]) || (packer && packer->valid (n))) && !distMap->getTube (n)) {
                        const float val = distMap->getSSD (n);
                        if (val > FLT_MAX * 0.95) {
                            if (ignoreMax) {
                                continue;
                            } else {
                                maxSSP = val;
                                otherEnd = n;
                                break;
                            }
                        }
                        if (val > maxSSP) {
                            maxSSP = val;
                            otherEnd = n;
                        }
                    }
                }

            }
        }

        if (otherEnd < 0) {
            break;
        }

        // Extract centerline
        // As long as we don't hit a point which already belongs to the skeleton we walk down the hill in the penalty distance map adding points to the lineset and storing their indices in the skeleton and the linesetcl
        std::vector<mclong> cl; // Indices of centerline in our dataset
        std::vector<int> linesetcl; // Indices of centerline in the lineset
        int x, y, z;
        idx.index3DLong (otherEnd, x, y, z);
        while (skeleton.get (x, y, z) < 0) {
            // Create new point in lineset
            McVec3f pos3d;
            const int ipos[3] = {x, y, z};
            positions.vertexpos (ipos, &(pos3d[0]));
            lineset->points.push_back(pos3d);
            const int pointalias = lineset->points.size()-1;
            float bdm = distMap->getBD (x, y, z);
            if (bdm < voxelSizeAbs / 2.0) {
                bdm = voxelSizeAbs / 2.0;
            }
            lineset->data[0].append (bdm);

            // ... and store it in the lineset center line
            linesetcl.push_back (pointalias);

            // ... and the skeleton
            skeleton.set (x, y, z, pointalias);

            // ... and the centerline with distance map indices
            cl.push_back (otherEnd);


            // Search the steepest decline
            float minSSPD = FLT_MAX;

            int nextNumber = 0;
            const int i = x;
            const int j = y;
            const int k = z;
            int indexN, iN = 0, jN = 0, kN = 0;
            float weight;

            while (idx.nextInsideNeighbor (nextNumber, i, j, k, otherEnd
                                                , iN, jN, kN, indexN, weight)) {
                if ((dat && dat[indexN]) || (packer && packer->valid (indexN))) {
                    const float newval = distMap->getSSPD (indexN);
                    if (newval < minSSPD) {
                        minSSPD = newval;
                        otherEnd = indexN;
                        x = iN;
                        y = jN;
                        z = kN;
                    }
                }
            }
        }
        // Add last point and append centerline to lineset
        if (linesetcl.size() == 0) {
            linesetcl.push_back (skeleton.get (x, y, z));
            McVec3f pos3d = lineset->points[skeleton.get(x, y, z)];
            pos3d += 0.5 * voxelSize;
            lineset->points.push_back(pos3d);
            const int pointalias = lineset->points.size()-1;
            linesetcl.push_back (pointalias);
            lineset->data[0].append (distMap->getBD (x, y, z));
        }
        else if (linesetcl.size() == 1 && linesetcl[0] == skeleton.get (x, y, z)) {
            McVec3f pos3d = lineset->points[skeleton.get(x, y, z)];
            pos3d += 0.5 * voxelSize;
            lineset->points.push_back(pos3d);
            const int pointalias = lineset->points.size()-1;
            linesetcl.push_back (pointalias);
            lineset->data[0].append (distMap->getBD (x, y, z));
        }
        else {
            linesetcl.push_back (skeleton.get (x, y, z));
        }
        lineset->addLine (linesetcl.size (), &(*linesetcl.begin()));
        cl.push_back(otherEnd);

        theWorkArea->setProgressInfo (QApplication::translate("HxFilopodiaTEASAR", "Part %1 ... length %2").arg(numParts).arg(cl.size ()));
        if (theWorkArea->wasInterrupted ()) {
            break;
        }

        // Add tube
        distMap->addTube (cl.size(), &(*cl.begin())
                    , portTubeParams.getValue (0)
                    , portTubeParams.getValue (1));
        // Recalc the new PDM
        distMap->addLS2PDM (cl.size(), &(*cl.begin()));
        // Recalc the new SSDM
        distMap->addLS2SSDM (cl.size(), &(*cl.begin()));

        theWorkArea->setProgressValue (distMap->getTubeFillRatio ());
        if (theWorkArea->wasInterrupted ()) {
            break;
        }
    }

    theWorkArea->stopWorking ();

    // Create requested output maps
    // ... a voxel representation of the skeleton
    if (portMaps.getValue (VOXELSKEL)) {
        HxLabelLattice3* result  = new HxLabelLattice3 (dims, C_UNIFORM);
        result->setBoundingBox (&(bbox[0]));
        out = HxLattice3::create (result);
        out->setLabel ("VoxelSkeleton");
        HxFilopodiaTEASAR::addResultToPool( out );

        unsigned char* ptr = static_cast<unsigned char*>(result->dataPtr ());
        for (int k = 0; k < dims[2]; k++) {
            for (int j = 0; j < dims[1]; j++) {
                for (int i = 0; i < dims[0]; i++) {
                    if (skeleton.get (i, j, k) >= 0) {
                        *ptr = 1;
                    } else {
                        *ptr = 0;
                    }
                    ptr++;
                }
            }
        }
    }

    // ... the single seeded distance map
    if (portMaps.getValue (BDM)) {
        HxLattice3* result = new HxLattice3 (dims, 1, McPrimType::MC_FLOAT, C_UNIFORM, distMap->grabBDM());
        result->setBoundingBox (&(bbox[0]));
        out = HxLattice3::create (result);
        out->setLabel ("BDM");
        HxFilopodiaTEASAR::addResultToPool( out );
    }

    // ... the single seeded distance map
    if (portMaps.getValue (SSDM)) {
        HxLattice3* result = new HxLattice3 (dims, 1, McPrimType::MC_FLOAT, C_UNIFORM, distMap->grabSSDM ());
        result->setBoundingBox (&(bbox[0]));
        out = HxLattice3::create (result);
        out->setLabel ("SSDM");
        HxFilopodiaTEASAR::addResultToPool( out );
    }

    // ... the single seeded penalty distance map
    if (portMaps.getValue (SSPDM)) {
        HxLattice3* result = new HxLattice3 (dims, 1, McPrimType::MC_FLOAT, C_UNIFORM, distMap->grabSSPDM ());
        result->setBoundingBox (&(bbox[0]));
        out = HxLattice3::create (result);
        out->setLabel ("SSPDM");
        HxFilopodiaTEASAR::addResultToPool( out );
    }

    // ... the tubes map
    if (portMaps.getValue (TUBES)) {
        HxLattice3* result = new HxLattice3 (dims, 1, McPrimType::MC_UINT8, C_UNIFORM, distMap->grabTubes ());
        result->setBoundingBox (&(bbox[0]));
        out = HxLattice3::create (result);
        out->setLabel ("tubes");
        HxFilopodiaTEASAR::addResultToPool( out );
    }


    bool noNeedToLoop = FALSE;
    int i = 0;
    while(!noNeedToLoop)
    {

        theWorkArea->setProgressInfo (QApplication::translate("HxFilopodiaTEASAR", "Creating branches processing line %1 out of %2")
                                      .arg(i)
                                      .arg(lineset->lines.size()));
        noNeedToLoop = TRUE;
        for( ; i < lineset->lines.size(); i++)
        {
            for(int t = 1; t < lineset->getLineLength(i)-1; t++)
            {
                int vertex = lineset->getLineVertex(i, t);

                for(int j = 0; j < lineset->lines.size(); j++)
                {
                    if (vertex == lineset->getLineVertex(j, 0)
                        || vertex == lineset->getLineVertex(j, lineset->getLineLength(j)-1))
                    {
                        lineset->splitLine(i, t);
                        noNeedToLoop = FALSE;
                        break;
                    }
                }
                if (!noNeedToLoop) break;
            }
            if (!noNeedToLoop) break;
        }
    }

    lineset->autoJoinLines();


    noNeedToLoop = FALSE;
    i = 0;
    while(!noNeedToLoop)
    {
        theWorkArea->setProgressInfo (QApplication::translate("HxFilopodiaTEASAR", "Creating branches processing line %1 out of %2")
                                      .arg(i)
                                      .arg(lineset->lines.size()));
        noNeedToLoop = TRUE;
        for( ; i < lineset->lines.size(); i++)
        {
            if (lineset->getLineLength(i) == 2)
                if (lineset->getLineVertex(i, 0) == lineset->getLineVertex(i, 1))
                {
                    lineset->deleteLine(i);
                    noNeedToLoop = FALSE;
                    break;
                }
        }
    }
    
   

    HxData *tmp = lineset;
    std::vector<McString> attribs;
    attribs.push_back( McString("thickness") );

    McHandle <HxSpatialGraph> spGraph = HxSpatialGraph::createInstance();

    CastHxLineSetToHxSpatialGraph::makeSpatialGraphWithAttrib(
        (HxLineSetInterface *)(tmp->getInterface(HxLineSetInterface::getClassTypeId())),
        attribs,
        spGraph);

    spGraph->composeLabel(static_cast<HxBase*>(src->getInterface (HxBase::getClassTypeId()))->getLabel(), "Spatial-Graph");
    spGraph->isConsistent();


    if (portFakeVoxelSizeZ.getValue() !=-1 ) {
        // ComputePointThickness(spGraph, labelData);
    }


    // Check if seed is node:
    int rootNode = FilopodiaFunctions::getNodeWithCoords(spGraph, rootPoint);

    if (rootNode == -1) {

        // Seed is not node -> find edge point with seed coords and convert to node
        SpatialGraphPoint rootSpPoint = FilopodiaFunctions::getEdgePointWithCoords(spGraph, rootPoint);

        SpatialGraphSelection s1(spGraph);
        SpatialGraphSelection visSel(spGraph);
        s1.selectPoint(rootSpPoint);
        PointToVertexOperation* pointToVertexOp = new PointToVertexOperation(spGraph, s1, visSel);
        pointToVertexOp->exec();
        visSel = pointToVertexOp->getVisibleSelectionAfterOperation();

        rootNode = pointToVertexOp->getNewVertexNum();
    }

    FilopodiaFunctions::setTypeLabelsForAllNodes(spGraph, rootNode);
    setResult (spGraph);

    delete distMap;
    theWorkArea->notBusy();

}

void
HxFilopodiaTEASAR::addResultToPool(HxData* result)
{
    // In case the units management has been activated, be sure that result coordinates unit is initialized to the global working coordinates unit.
    HxUnitsProviderHelpers::setWorkingCoordinatesUnitAsCoordinatesUnit( result );

    theObjectPool->addObject( result );
}
