/////////////////////////////////////////////////////////////////
// 
// StartTransformationGenerator3d.cpp
//
// Main Authors: Baum
// 
/////////////////////////////////////////////////////////////////

#include <mclib/McBitfield.h>
#include <mclib/internal/McAlignPointSets.h>
#include <mclib/McVec3.h>
#include <mclib/internal/McAssert.h>

#include "StartTransformationGenerator3d.h"
#include "PointRepresentation.h"
#include "Transformation.h"
#include "McCliqueDetection.h"

#include <vector>

StartTransformationGenerator3d::StartTransformationGenerator3d()
{
    this->minCliqueSize = 3;
    this->maxNumStartTransformations = -1;
    this->transformType = 0;
}

void
StartTransformationGenerator3d::setMinimumCliqueSize(const int size)
{
    this->minCliqueSize = size;
}

void 
StartTransformationGenerator3d::setMaxNumStartTransformations(const int numStartTransformations)
{
    this->maxNumStartTransformations = numStartTransformations;
}


void 
StartTransformationGenerator3d::setTransformType(const int transType) 
{
    mcassert(transType >= 0 && transType <= 2);
    this->transformType = transType;
}

bool
StartTransformationGenerator3d::compute(const PointRepresentation * pointSet1, 
                                        const PointRepresentation * pointSet2,
                                        std::vector<Transformation>          & startTransformations,
                                        std::vector<int>                     & cliqueSizes)
{
    // initialize connectivity of correspondence graph
    //   - each node is connected to itself; 
    //     this is needed for the clique detection algorithm
    std::vector<McBitfield> connected;

    std::vector<McVec2i> corrGraph;
    pointSet1->computeCorrespondenceGraphVertices(pointSet2,
                                                  corrGraph);

    // compute edges of correspondence graph
    pointSet1->computeCorrespondenceGraphEdges(pointSet2, 
                                               corrGraph,
                                               connected);
    
    std::vector<int> notAndCand(corrGraph.size());
    for ( size_t i=0; i<corrGraph.size(); i++ ) {
        notAndCand[i] = i;
    }

    int nCliques = 0;
    std::vector<int> clique;
    std::vector<std::vector<int> > cliques;
    bool returnStatus = 
        McCliqueDetection::computeCliquesBronKerbosch(clique, 
                                                      notAndCand, 
                                                      0, 
                                                      notAndCand.size(),
                                                      connected, 
                                                      nCliques, 
                                                      cliques, 
                                                      minCliqueSize,
                                                      maxNumStartTransformations);

    const std::vector<McVec3f> & coordsOfReference = pointSet1->getCoords();
    const std::vector<McVec3f> & coordsOfQuery     = pointSet2->getCoords();

    std::vector<int> pointsOfReference;
    std::vector<int> pointsOfQuery;
    McMat4f transform;
    Transformation transformation;

    // compute start transformations
    startTransformations.reserve(cliques.size());
    for ( size_t i=0; i<cliques.size(); i++ ) {
        // check whether point sets found using clique detection are
        // really close enough
        pointsOfReference.resize(cliques[i].size());
        pointsOfQuery.resize(cliques[i].size());
        for ( size_t j=0; j<cliques[i].size(); j++ ) {
            pointsOfReference[j] = corrGraph[cliques[i][j]][0];
            pointsOfQuery[j]     = corrGraph[cliques[i][j]][1];
        }

        mcAlignPointSets(transform, 
                         &coordsOfReference[0], 
                         &coordsOfQuery[0], 
                         pointsOfReference.data(),
                         pointsOfQuery.data(),
                         pointsOfReference.size(), 
                         transformType);

        transformation.setTransformation3d(transform);
        startTransformations.push_back(transformation);
        cliqueSizes.push_back(cliques[i].size());
    }

    return returnStatus;
}
  
bool StartTransformationGenerator3d::compute(const PointRepresentation * pointSet1, 
                                             const PointRepresentation * pointSet2,
                                             std::vector<Transformation>          & startTransformations)
{
    std::vector<int> cliqueSizes;
    return compute(pointSet1, pointSet2, startTransformations, cliqueSizes);
}
