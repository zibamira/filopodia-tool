#include "HxClipSpatialGraph.h"
#include "FilopodiaFunctions.h"
#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <hxsurface/internal/FaceOctree.h>
#include <mclib/McException.h>

#include "ConvertVectorAndMcDArray.h"

HX_INIT_CLASS(HxClipSpatialGraph,HxCompModule)

HxClipSpatialGraph::HxClipSpatialGraph() :
    HxCompModule(HxSpatialGraph::getClassTypeId()),
    portSurface(this, tr("Surface"), tr("Surface"), HxSurface::getClassTypeId()),
    portAction(this, tr("action"), tr("Action"))
{
}

HxClipSpatialGraph::~HxClipSpatialGraph()
{
}

bool hasPointOnSurface(HxSpatialGraph* graph,
                       FaceOctree<HxSurface>& octree,
                       SpatialGraphPoint& pointOnSurface)
{
    McDArray<int> intersectedTriangles;
    std::vector<McVec3f> intersectionPoints;
    for (int e=0; e<graph->getNumEdges(); ++e) {
        const std::vector<McVec3f> points = graph->getEdgePoints(e);
        for (int p=1; p<points.size()-1; ++p) {
            if (octree.lineIntersectsFaces(points[p], points[p], intersectedTriangles, intersectionPoints)) {
                pointOnSurface = SpatialGraphPoint(e, p);
                return true;
            }
        }
    }
    return false;
}


bool hasIntersection(HxSpatialGraph* graph,
                     FaceOctree<HxSurface>& octree,
                     SpatialGraphPoint& pointAfterIntersection,
                     McVec3f& intersectionPoint)
{
    const float eps = 0.001f;
    McDArray<int> intersectedTriangles;
    std::vector<McVec3f> intersectionPoints;

    for (int e=0; e<graph->getNumEdges(); ++e) {
        const std::vector<McVec3f> points = graph->getEdgePoints(e);
        for (int p=1; p<points.size(); ++p) {
            if (octree.lineIntersectsFaces(points[p-1], points[p], intersectedTriangles, intersectionPoints, true)) {
                pointAfterIntersection = SpatialGraphPoint(e, p);
                intersectionPoint = intersectionPoints[0];

                // Do not add point when intersection is close to first/last point on edge (i.e. a node)
                if (p==1 && (points[p-1]-intersectionPoint).length()<eps) {
                    return false;
                }
                if (p==points.size()-1 && (points[p]-intersectionPoint).length()<eps) {
                    return false;
                }
                return true;
            }
        }
    }
    return false;
}


void insertNode(HxSpatialGraph* graph,
                const SpatialGraphPoint& pointAfterIntersection,
                const McVec3f& intersectionPoint)
{
    std::vector<McVec3f> points(1);
    points[0] = intersectionPoint;

    graph->insertEdgePoints(pointAfterIntersection.edgeNum, pointAfterIntersection.pointNum, points);
    FilopodiaFunctions::convertPointToNode(graph, pointAfterIntersection);
}


bool isPointInsideSurface(const McVec3f& point,
                          const HxSurface* surface,
                          FaceOctree<HxSurface>& octree)
{
    const McVec3f ray = McVec3f(1.0f, 0.0f, 0.0f);
    McVec3f intersectionPoint;
    const int intersectedTriangle = octree.findFirstIntersectedTriangle(point, ray, intersectionPoint);
    if (intersectedTriangle < 0) {
        return false;

    }
    const float eps = 0.001;
    if ((intersectionPoint-point).length() < eps) {
        return false; // On triangle
    }

    const McVec3f normal = surface->getTriangleNormal(intersectedTriangle);
    return normal.dot(intersectionPoint-point) > 0.0;
}


bool isSegmentInsideSurface(const HxSpatialGraph* graph,
                            const int segmentNum,
                            const HxSurface* surface,
                            FaceOctree<HxSurface>& octree)
{
    const std::vector<McVec3f> points = graph->getEdgePoints(segmentNum);
    if (points.size() == 2) {
        const McVec3f center = (points[0] + points[1]) * 0.5f;
        return isPointInsideSurface(center, surface, octree);
    }
    else {
        const int centerIdx = points.size()/2;
        return isPointInsideSurface(points[centerIdx], surface, octree);
    }
}


void removeGraphInsideSurface(HxSpatialGraph* graph,
                              const HxSurface* surface,
                              FaceOctree<HxSurface>& octree)
{
    SpatialGraphSelection insideSelection(graph);

    for (int v=0; v<graph->getNumVertices(); ++v) {
        const McVec3f coords = graph->getVertexCoords(v);
        if (isPointInsideSurface(coords, surface, octree)) {
            insideSelection.selectVertex(v);
        }
    }

    for (int e=0; e<graph->getNumEdges(); ++e) {
        if (isSegmentInsideSurface(graph, e, surface, octree)) {
            insideSelection.selectEdge(e);
        }
    }

    DeleteOperation deleteOp = DeleteOperation(graph, insideSelection, SpatialGraphSelection(graph));
    deleteOp.exec();

    theMsg->printf("Removed %d segments and %d nodes", insideSelection.getNumSelectedEdges(), insideSelection.getNumSelectedVertices());
}


void clipSpatialGraphWithSurface(HxSpatialGraph* graph, const HxSurface* surface) {
    McHandle<HxSurface> surf = dynamic_cast<HxSurface*>(surface->duplicate());
    surf->computeNormalsPerTriangle();
    FaceOctree<HxSurface> octree;
    octree.insertAllTriangles(surf, surf->getBoundingBox(), &surf->normals[0]);

    theMsg->printf("Checking points on surface ...");
    SpatialGraphPoint pointOnSurface;
    int numConversions = 0;
    while (hasPointOnSurface(graph, octree, pointOnSurface)) {
        FilopodiaFunctions::convertPointToNode(graph, pointOnSurface);
        ++numConversions;
    }
    theMsg->printf("Converted %d points on surface to nodes", numConversions);

    theMsg->printf("Checking intersection points ...");
    SpatialGraphPoint pointAfterIntersection;
    McVec3f intersectionPoint;
    int numIntersections = 0;
    while (hasIntersection(graph, octree, pointAfterIntersection, intersectionPoint)) {
        insertNode(graph, pointAfterIntersection, intersectionPoint);
        ++numIntersections;
    }
    theMsg->printf("Inserted %d intersection nodes", numIntersections);

    removeGraphInsideSurface(graph, surface, octree);
}


void HxClipSpatialGraph::compute()
{
    if (portAction.wasHit()) {
        HxSpatialGraph* inputGraph = hxconnection_cast<HxSpatialGraph> (portData);
        if (!inputGraph) {
            throw McException(QString("No input graph"));
        }

        HxSurface* surface = hxconnection_cast<HxSurface> (portSurface);
        if (!surface) {
            throw McException(QString("No input surface"));
        }

        McHandle<HxSpatialGraph> outputGraph = dynamic_cast<HxSpatialGraph*>(getResult());
        if (outputGraph) {
            outputGraph->copyFrom(inputGraph);
        }
        else {
            outputGraph = inputGraph->duplicate();
        }
        outputGraph->composeLabel(inputGraph->getLabel(), "clipped");

        clipSpatialGraphWithSurface(outputGraph, surface);
        setResult(outputGraph);
    }
}

