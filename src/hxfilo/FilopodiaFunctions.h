#ifndef FILOPODIAFUNCTIONS_H
#define FILOPODIAFUNCTIONS_H

#include <QMap>
#include <QFileInfo>
#include <hxneuroneditor/internal/HxNeuronEditorSubApp.h>
#include <hxspatialgraph/internal/HierarchicalLabels.h>
#include <hxfield/HxUniformScalarField3.h>
#include <hxfield/HxUniformLabelField3.h>

#include "api.h"

class HxSpatialGraph;
class SpatialGraphSelection;
struct SpatialGraphPoint;
class SpatialGraphOperation;
class PointToVertexOperation;
class HxUniformScalarField3;
class ReplaceFilopodiaOperationSet;
class ReplaceFilopodiaEdgeOperationSet;
class AddFilopodiumOperationSet;
class MoveEdgeOperationSet;
class ConnectOperationSet;
class MoveBaseOperationSet;
class AddRootsOperationSet;
class MoveTipOperationSet;

enum FilopodiaNodeType
{
    TIP_NODE,
    BASE_NODE,
    BRANCHING_NODE,
    ROOT_NODE
};
enum FilopodiaManualGeometry
{
    AUTOMATIC,
    MANUAL
};
enum FilopodiaLocation
{
    FILOPODIUM,
    GROWTHCONE
};
enum MatchLabel
{
    UNASSIGNED,
    IGNORED,
    AXON
};
enum FilopodiaBulbous
{
    BULBOUS,
    NONBULBOUS
};

struct TimeMinMax
{
    TimeMinMax()
        : minT(std::numeric_limits<int>::max())
        , maxT(std::numeric_limits<int>::min())
    {
    }

    TimeMinMax(const int tmin, const int tmax)
        : minT(tmin)
        , maxT(tmax)
    {
    }

    bool
    isValid() const
    {
        return (minT >= 0) && (maxT >= 0) && (maxT >= minT);
    }

    int minT;
    int maxT;
};

struct Path
{
    std::vector<int> nodeNums;
    std::vector<int> segmentNums;
    std::vector<bool> segmentReversed;
};

struct PointOnPath
{
    Path path;
    SpatialGraphPoint point;
};

namespace FilopodiaFunctions
{
    // --------------------------------------------------------------------------------------------------------------
    // ----------- Functions for File Reading  -----------
    QStringList getFileListFromDirectory(const QString& dirName,
                                         const QString type);
    QString getFileNameFromPath(QString filePath);
    QString getOutputFileName(const QString& fileName,
                              const QString& outputDir);
    HxUniformScalarField3* loadUniformScalarField3(const QString& fileName);
    HxUniformLabelField3* loadUniformLabelField3(const QString& fileName);
    HxSpatialGraph* readSpatialGraph(const QFileInfo& fileInfo);
    void convertToUnsignedChar(HxUniformScalarField3* inputField);
    void getGrowthConeNumberAndTimeFromFileName(const QString& path,
                                                int& gc,
                                                int& time);
    int getGrowthConeNumberFromFileName(const QString& path);
    int getTimeFromFileName(const QString& path);
    int getTimeFromFileNameNoGC(const QString& path);

    // --------------------------------------------------------------------------------------------------------------
    // ----------- Functions for labels -----------
    bool isFilopodiaGraph(const HxSpatialGraph* graph);
    bool isRootNodeGraph(const HxSpatialGraph* graph);
    HXFILOPODIA_API void addAllFilopodiaLabelGroups(HxSpatialGraph* graph, TimeMinMax TimeMinMax);
    bool checkIfGraphHasAttribute(const HxSpatialGraph* graph, const char* attributeName);

    // ----------- Functions for Time -----------
    const char* getTimeStepAttributeName();
    SbColor getColorForTime(const int t,
                            const int minT,
                            const int maxT);
    int addTimeLabelAttribute(HxSpatialGraph* graph,
                              TimeMinMax timeMinMax,
                              int time);
    HierarchicalLabels* addTimeLabelAttribute(HxSpatialGraph* graph,
                                              const TimeMinMax timeMinMax);
    QString getLabelNameFromTime(const int time);
    int getTimeFromLabelName(const McString& labelName);
    int getTimeIdFromTimeStep(const HxSpatialGraph* graph,
                              const int timestep);
    int getTimeStepFromTimeId(const HxSpatialGraph* graph,
                              const int timeLabelId);
    TimeMinMax getTimeMinMaxFromGraphLabels(const HxSpatialGraph* graph);
    int getTimeIdOfNode(const HxSpatialGraph* graph,
                        const int v);
    void setTimeIdOfNode(HxSpatialGraph* graph,
                         const int node,
                         const int timeId);
    int getTimeOfNode(const HxSpatialGraph* graph,
                      const int v);
    int getTimeOfEdge(const HxSpatialGraph* graph,
                      const int e);
    int getTimeIdOfEdge(const HxSpatialGraph* graph,
                        const int s);
    void setTimeIdOfEdge(HxSpatialGraph* graph,
                         const int edge,
                         const int timeId);
    SpatialGraphSelection getSelectionForTimeStep(const HxSpatialGraph* graph,
                                                  const int timeStep);

    // ----------- Functions for Node Type -----------
    const char* getTypeLabelAttributeName();
    void setTypeLabelsForAllNodes(HxSpatialGraph* graph,
                                  int rootNode);
    void addTypeLabelAttribute(HxSpatialGraph* graph);
    const char* getTypeLabelName(const FilopodiaNodeType nodeType);
    HXFILOPODIA_API int getTypeLabelId(const HxSpatialGraph* graph,
                                       const FilopodiaNodeType nodeType);
    int getRootNodeFromTimeStep(const HxSpatialGraph* graph,
                                const int time);
    int getRootNodeFromTimeAndGC(const HxSpatialGraph* graph,
                                 const int time,
                                 const int gcId);
    SpatialGraphSelection getNodesOfTypeForTime(const HxSpatialGraph* graph,
                                                const FilopodiaNodeType type,
                                                const int time);
    SpatialGraphSelection getNodesOfTypeInSelection(const HxSpatialGraph* graph,
                                                    const SpatialGraphSelection& sel,
                                                    const FilopodiaNodeType type);
    SpatialGraphSelection getNodesOfType(const HxSpatialGraph* graph,
                                         const FilopodiaNodeType type);
    int getTypeIdOfNode(const HxSpatialGraph* graph, const int v);
    bool hasNodeType(const HxSpatialGraph* graph, const FilopodiaNodeType nodeType, const int node);
    void setNodeType(HxSpatialGraph* graph, const int node, const FilopodiaNodeType nodeType);

    // ----------- Functions for Filopodia IDs -----------
    const char* getFilopodiaAttributeName();
    void addFilopodiaLabelAttribute(HxSpatialGraph* graph);
    void addOrClearFilopodiaLabelAttribute(HxSpatialGraph* graph);
    HXFILOPODIA_API int getFilopodiaLabelId(const HxSpatialGraph* graph,
                                            const MatchLabel label);
    QString getFilopodiaLabelName(const MatchLabel label);
    int getFilopodiaIdFromLabelName(const HxSpatialGraph* graph, const QString& label);
    QString getFilopodiaLabelNameFromNumber(const int number);
    QString getFilopodiaLabelNameFromID(const HxSpatialGraph* graph,
                                        const int ID);
    int getNumberOfFilopodia(const HxSpatialGraph* graph);
    int getNextAvailableFilopodiaId(const HxSpatialGraph* graph);
    int getFilopodiaIdOfNode(const HxSpatialGraph* graph, const int v);
    void setFilopodiaIdOfNode(HxSpatialGraph* graph, const int v, const int filoId);
    int getFilopodiaIdOfEdge(const HxSpatialGraph* graph, const int e);
    void setFilopodiaIdOfEdge(HxSpatialGraph* graph, const int e, const int filoId);
    bool hasFiloId(const HxSpatialGraph* graph, const int v, const int filoId);
    TimeMinMax getFilopodiumLifeTime(const HxSpatialGraph* graph, const int filopodiumId);
    TimeMinMax getFilopodiumMatchLifeTime(const HxSpatialGraph* graph, const int matchId);
    bool isRealFilopodium(const HxSpatialGraph* graph, const int filopodiumId);
    SpatialGraphSelection getFilopodiaSelectionForTime(const HxSpatialGraph* graph,
                                                       const int filoId,
                                                       const int timeId);
    SpatialGraphSelection getFilopodiaSelectionOfNode(const HxSpatialGraph* graph,
                                                      const int nodeId);
    SpatialGraphSelection getFilopodiaSelectionForAllTimeSteps(const HxSpatialGraph* graph,
                                                               const int filopodiaLabelId);
    SpatialGraphSelection getNodesWithFiloIdFromSelection(const HxSpatialGraph* graph, SpatialGraphSelection sel, const int filoId);

    // ----------- Functions for Manual Node Match IDs -----------
    HXFILOPODIA_API int getMatchLabelId(const HxSpatialGraph* graph,
                                        const MatchLabel label);
    QString getLabelNameFromMatchId(const HxSpatialGraph* graph,
                                    const int matchId);
    SpatialGraphSelection getNodesWithMatchId(const HxSpatialGraph* graph, const int matchId);
    QString getMatchLabelName(const MatchLabel label);
    const char* getManualNodeMatchAttributeName();
    void addManualNodeMatchLabelAttribute(HxSpatialGraph* graph);
    int getMatchIdOfNode(const HxSpatialGraph* graph,
                         const int v);
    HXFILOPODIA_API int getNextAvailableManualNodeMatchId(HxSpatialGraph* graph);
    int getUnusedManualNodeMatchId(const HxSpatialGraph* graph);
    QString getNewManualNodeMatchLabelName(const HxSpatialGraph* graph);
    void setManualNodeMatchId(HxSpatialGraph* graph, const int node, const int id);

    // ----------- Functions for Growth Cone -----------
    HierarchicalLabels* addGrowthConeLabelAttribute(HxSpatialGraph* graph);
    const char* getGrowthConeAttributeName();
    bool hasGrowthConeLabels(const HxSpatialGraph* graph);
    QString getLabelNameFromGrowthConeNumber(const int growthConeNumber);
    McVec3f getGrowthConeCenter(const HxSpatialGraph* graph,
                                const int growthCone,
                                const int time,
                                int& nodeId);
    int getGrowthConeIdFromLabel(const HxSpatialGraph* graph,
                                 const QString& label);
    SpatialGraphSelection getNodesWithGcId(const HxSpatialGraph* graph,
                                           const int growthConeId);
    int getNumberOfGrowthCones(const HxSpatialGraph* graph);
    QString getGrowthConeLabelFromNumber(const HxSpatialGraph* graph, const int childNum);
    int getGrowthConeLabelIdFromNumber(const HxSpatialGraph* graph, const int childNum);
    int getGcIdOfNode(const HxSpatialGraph* graph,
                      const int v);
    void setGcIdOfNode(HxSpatialGraph* graph, const int node, const int id);
    int getGcIdOfEdge(const HxSpatialGraph* graph,
                      const int s);
    void setGcIdOfEdge(HxSpatialGraph* graph, const int edge, const int id);
    void transformGcLabelGroup(HxSpatialGraph* graph);

    // ----------- Functions for Location -----------
    const char* getLocationAttributeName();
    void addLocationLabelAttribute(HxSpatialGraph* graph);
    HXFILOPODIA_API int getLocationLabelId(const HxSpatialGraph* graph, const FilopodiaLocation location);
    const char* getLocationLabelName(const FilopodiaLocation location);
    void getGrowthConeAndFilopodiumSelection(HxSpatialGraph* graph,
                                             const int time,
                                             SpatialGraphSelection& growthSel,
                                             SpatialGraphSelection& filoSel);
    bool checkIfNodeIsPartOfGrowthCone(const HxSpatialGraph* graph,
                                       const int node,
                                       const int rootNode);
    bool checkIfEdgeIsPartOfGrowthCone(const HxSpatialGraph* graph,
                                       const int edge,
                                       const int rootNode);
    bool checkIfPointIsPartOfGrowthCone(const HxSpatialGraph* graph,
                                        const SpatialGraphPoint point,
                                        const int rootNode);
    int getLocationIdOfNode(const HxSpatialGraph* graph,
                            const int v);
    void setLocationIdOfNode(HxSpatialGraph* graph,
                             const int v,
                             const FilopodiaLocation loc);
    int getLocationIdOfEdge(const HxSpatialGraph* graph,
                            const int e);
    void setLocationIdOfEdge(HxSpatialGraph* graph,
                             const int e,
                             const FilopodiaLocation loc);
    SpatialGraphSelection getFilopodiumSelectionOfTimeStep(const HxSpatialGraph* graph,
                                                        const int timeStep);
    SpatialGraphSelection getGrowthConeSelectionOfTimeStep(const HxSpatialGraph* graph,
                                                       const int timeStep);
    SpatialGraphSelection getFilopodiumSelectionFromNode(const HxSpatialGraph* graph,
                                                         const int node);
    SpatialGraphSelection getFilopodiumSelectionFromEdge(const HxSpatialGraph* graph,
                                                         const int edge);
    void labelGrowthConeAndFilopodiumSelection(HxSpatialGraph* graph,
                                               const int time);
    void assignGrowthConeAndFilopodiumLabels(HxSpatialGraph* graph);
    SpatialGraphSelection getLocationSelection(const HxSpatialGraph* graph, const FilopodiaLocation loc);
    bool nodeHasLocation(const FilopodiaLocation loc, const int nodeId, const HxSpatialGraph* graph);
    bool edgeHasLocation(const FilopodiaLocation loc, const int edgeId, const HxSpatialGraph* graph);

    // ----------- Functions for ManualGeometry -----------
    void addManualGeometryLabelAttribute(HxSpatialGraph* graph);
    const char* getManualGeometryLabelName(const FilopodiaManualGeometry ManualGeometry);
    const char* getManualGeometryLabelAttributeName();
    void setManualGeometryLabel(HxSpatialGraph* graph);
    HXFILOPODIA_API int getManualGeometryLabelId(const HxSpatialGraph* graph,
                                                 const FilopodiaManualGeometry ManualGeometry);
    int getGeometryIdOfNode(const HxSpatialGraph* graph, const int v);
    void setGeometryIdOfNode(HxSpatialGraph* graph, const int node, const int geometry);
    int getGeometryIdOfEdge(const HxSpatialGraph* graph, const int e);
    void setGeometryIdOfEdge(HxSpatialGraph* graph, const int edge, const int geometry);

    // ----------- Functions for Bulbous label   -----------
    const char* getBulbousAttributeName();
    const char* getBulbousLabelName(const FilopodiaBulbous bulbous);
    void addBulbousLabel(HxSpatialGraph* graph);
    int getBulbousIdOfNode(const HxSpatialGraph* graph, const int v);
    int getBulbousIdOfEdge(const HxSpatialGraph* graph, const int e);
    void setBulbousIdOfNode(HxSpatialGraph* graph, const int node, const FilopodiaBulbous bulbous);
    void setBulbousIdOfEdge(HxSpatialGraph* graph, const int edge, const FilopodiaBulbous bulbous);
    int getBulbousLabelId(const HxSpatialGraph* graph, const FilopodiaBulbous bulbous);
    void extendBulbousLabel(HxSpatialGraph* graph);
    bool hasBulbousLabel(const HxSpatialGraph* graph, const FilopodiaBulbous bulbous, const int node);
    SpatialGraphSelection getBasesWithBulbousLabel(const HxSpatialGraph* graph);
    std::vector<int> getFiloIdsWithBulbousLabel(const HxSpatialGraph* graph, SpatialGraphSelection bulbousSel);

    // --------------------------------------------------------------------------------------------------------------
    // ----------- Functions for tracing -----------
    SpatialGraphPoint getEdgePointWithCoords(HxSpatialGraph* graph,
                                             McVec3f coords);
    int getNodeWithCoords(HxSpatialGraph* graph,
                          McVec3f coords);
    float getEdgeLength(const HxSpatialGraph* graph,
                        const int edge);
    unsigned char vectorToDirection(const McVec3i v);
    McVec3i directionToVector(const unsigned char d);

    std::vector<McVec3f> trace(const HxSpatialGraph* graph,
                            const HxUniformScalarField3* image,
                            const McVec3f& startPoint,
                            const McVec3f& targetPoint,
                            const int surround,
                            const float intensityWeight,
                            const int topBrightness,
                            const SpatialGraphSelection& edgesFromTime,
                            SpatialGraphPoint& intersectionPoint,
                            int& intersectionNode);
    std::vector<McVec3f> traceWithDijkstra(const HxSpatialGraph* graph,
                                        const HxUniformScalarField3* priorMap,
                                        const McVec3f& startPoint,
                                        const McVec3f& rootPoint,
                                        const SpatialGraphSelection& edgesFromTime,
                                        SpatialGraphPoint& intersectionPoint,
                                        int& intersectionNode);
    SpatialGraphPoint testVarianceAlongEdge(const HxSpatialGraph* graph,
                                            const int edgeNum,
                                            const HxUniformScalarField3* image);
    int findBasePoint(const std::vector<McVec3f>& pathRootToEnd,
                      HxUniformScalarField3* image,
                      const float significantThreshold);
    McDim3l getNearestVoxelCenterFromPointCoords(const McVec3f& point,
                                                 const HxUniformScalarField3* image);
    McDim3l getNearestVoxelCenterFromIndex(const McDim3l& ijk,
                                           const McVec3f& fraction,
                                           const McDim3l& dims);
    SpatialGraphPoint intersectionOfTwoEdges(const HxSpatialGraph* graph,
                                             const McVec3f& edgePoint,
                                             const std::vector<int> edgesFromTime);
    std::vector<McVec3f> getPointsOfNewBranch(const HxSpatialGraph* graph,
                                           const SpatialGraphSelection& edgesToCheck,
                                           const std::vector<McVec3f>& tracedPoints,
                                           const McVec3f& voxelSize,
                                           SpatialGraphPoint& intersectionPoint,
                                           int& intersectionNode);
    Path getPathFromRootToTip(const HxSpatialGraph* graph,
                              const int rootNode,
                              const int endingNode);
    int getNextEdgeOnPath(const SpatialGraphSelection& sel,
                          const IncidenceList& incidentEdges);
    SpatialGraphPoint getBasePointOnPath(const Path& path,
                                         const HxSpatialGraph* graph,
                                         HxUniformScalarField3* image,
                                         const float significantThreshold);
    SpatialGraphSelection getSelectionTilBranchingNode(const HxSpatialGraph* graph,
                                                       const int selectedNode,
                                                       int& branchNode);

    // --------------------------------------------------------------------------------------------------------------
    // Semiautomatic Functions
    HxUniformScalarField3* cropTip(const HxUniformScalarField3* image,
                                   const McDim3l endNode,
                                   const McVec3i boundarySize);
    SpatialGraphSelection getSuccessorOfFilopodium(const HxSpatialGraph* graph,
                                                   const int v,
                                                   SpatialGraphSelection& successorSel);
    bool getNodePositionInNextTimeStep(const HxUniformScalarField3* templateImage,
                                       HxUniformScalarField3* nextImage,
                                       const McVec3f& searchCenter,
                                       const McVec3i& searchRadius,
                                       const McVec3f& templateCenter,
                                       const McVec3i& templateRadius,
                                       const float correlationThreshold,
                                       const bool doGrayValueRangeCheck,
                                       McVec3f& newNode,
                                       float& correlation);
    QMap<int, QFileInfo> getFilePerTimestep(const QString imageDir, const TimeMinMax timeMinMax);
    TimeMinMax getInfluenceArea(HxSpatialGraph* graph,
                                const int node);

    // --------------------------------------------------------------------------------------------------------------
    // Prepare Operations

    PointToVertexOperation* convertPointToNode(HxSpatialGraph* graph,
                                               const SpatialGraphPoint& point);
    HxSpatialGraph* createFilopodiaBranch(const HxSpatialGraph* graph,
                                          const McVec3f newCoords,
                                          const int rootNode,
                                          const std::vector<McVec3f> points,
                                          SpatialGraphSelection& intersectionSel,
                                          HxUniformScalarField3* image);
    HxSpatialGraph* createBranchToRoot(const HxSpatialGraph* graph,
                                       const McVec3f newCoords,
                                       const SpatialGraphSelection deleteSel,
                                       SpatialGraphSelection& intersectionSel,
                                       const HxUniformScalarField3* image,
                                       const int intensityWeight,
                                       const float topBrightness);
    HxSpatialGraph* createBranchToRootWithBase(const HxSpatialGraph* graph,
                                               const int tipNode,
                                               const McVec3f newCoords,
                                               const SpatialGraphSelection deleteSel,
                                               SpatialGraphSelection& intersectionSel,
                                               const HxUniformScalarField3* image,
                                               const int intensityWeight,
                                               const float topBrightness);
    HxSpatialGraph* createBranchToPoint(const HxSpatialGraph* graph,
                                        const SpatialGraphSelection deleteSel,
                                        const SpatialGraphSelection& intersectionSel,
                                        HxUniformScalarField3* image,
                                        const int intensityWeight,
                                        const float topBrightness);
    HxSpatialGraph* createGraphForNextTimeStep(const HxSpatialGraph* graph,
                                               const int nextTime,
                                               const SpatialGraphSelection baseSel,
                                               const HxUniformScalarField3* currentImage,
                                               HxUniformScalarField3* nextImage,
                                               const HxUniformScalarField3* nextDijkstraMap,
                                               const McVec3i baseSearchRadius,
                                               const McVec3i baseTemplateRadius,
                                               const McVec3i tipSearchRadius,
                                               const McVec3i tipTemplateRadius,
                                               const float correlationThreshold,
                                               const float lengthThreshold,
                                               const float intensityWeight,
                                               const int topBrightness,
                                               McDArray<int>& previousBases);
    AddFilopodiumOperationSet* addFilopodium(HxSpatialGraph* graph,
                                             const McVec3f newCoords,
                                             const int currentTime,
                                             HxUniformScalarField3* image,
                                             HxUniformScalarField3* priorMap,
                                             const bool addedIn2DViewer = true);
    ConnectOperationSet* connectNodes(HxSpatialGraph* graph,
                                      const SpatialGraphSelection selectedElements,
                                      HxUniformScalarField3* image,
                                      const float intensityWeight,
                                      const int topBrightness);
    MoveBaseOperationSet* moveBase(HxSpatialGraph* graph,
                                   const int startNode,
                                   const McVec3f newCoords,
                                   HxUniformScalarField3* image,
                                   const float intensityWeight,
                                   const int topBrightness);
    MoveBaseOperationSet* moveBaseOfTip(HxSpatialGraph* graph,
                                        const int endNode,
                                        const McVec3f newCoords,
                                        HxUniformScalarField3* image,
                                        const float intensityWeight,
                                        const int topBrightness);
    MoveTipOperationSet* moveTip(HxSpatialGraph* graph,
                                 const int oldTip,
                                 const McVec3f newCoords,
                                 HxUniformScalarField3* image,
                                 int surroundWidthInVoxels,
                                 float intensityWeight,
                                 int topBrightness,
                                 const bool addedIn2DViewer);
    ReplaceFilopodiaEdgeOperationSet* replaceEdge(HxSpatialGraph* graph,
                                                  const int edge,
                                                  const McVec3f pickedPoint,
                                                  const HxUniformScalarField3* image,
                                                  const int surroundWidthInVoxels,
                                                  const float intensityWeight,
                                                  const int topBrightness);
    ReplaceFilopodiaOperationSet* propagateFilopodia(HxSpatialGraph* graph,
                                                     const SpatialGraphSelection& baseSel,
                                                     const int currentTime,
                                                     const HxUniformScalarField3* currentImage,
                                                     HxUniformScalarField3* nextImage,
                                                     const HxUniformScalarField3* nextDijkstraMap,
                                                     const McVec3i baseSearchRadius,
                                                     const McVec3i baseTemplateRadius,
                                                     const McVec3i tipSearchRadius,
                                                     const McVec3i tipTemplateRadius,
                                                     const float correlationThreshold,
                                                     const float lengthThreshold,
                                                     const float intensityWeight,
                                                     const int topBrightness);
    AddRootsOperationSet* propagateRootNodes(HxSpatialGraph* graph,
                                             const QMap<int, McHandle<HxUniformScalarField3> > images,
                                             const McVec3i rootNodeSearchRadius,
                                             const McVec3i rootNodeTemplateRadius);
}

#endif
