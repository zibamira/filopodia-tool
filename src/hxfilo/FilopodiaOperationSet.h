#ifndef FILOPODIAOPERATIONSET_H
#define FILOPODIAOPERATIONSET_H

#include "api.h"
#include <hxspatialgraph/internal/SpatialGraphOperationSet.h>
#include <hxfield/HxUniformScalarField3.h>


//////////////////////////////////////////////////////////////////////////////////////////////
/// This operation labels selected components.
class HXFILOPODIA_API AddAndAssignLabelOperationSet : public OperationSet
{
public:
    AddAndAssignLabelOperationSet(HxSpatialGraph* sg, SpatialGraphSelection sel, SpatialGraphSelection vis, const QString& attName, const int newLabelId);
    AddAndAssignLabelOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry);

    virtual OperationLogEntry getLogEntry() const;
    virtual void exec();
    virtual void undo();
    virtual SpatialGraphSelection getSelectionAfterOperation(const SpatialGraphSelection& sel) const;

protected:
    QString mAttName;
    int mNewLabelId;

private:
    SpatialGraphSelection mRememberOldSelection;
    std::vector<int> mRememberOldVertexLabelId;
    std::vector<int> mRememberOldEdgeLabelId;
    std::vector<int> mRememberOldPointLabelId;
};

//////////////////////////////////////////////////////////////////////////////////////////////
class HXFILOPODIA_API ConvertFilopodiaPointOperationSet : public OperationSet {
public:
    ConvertFilopodiaPointOperationSet(HxSpatialGraph* sg,
                                      const SpatialGraphSelection &selectedElements,
                                      const SpatialGraphSelection &visibleElements,
                                      const McDArray<int> labels=McDArray<int>());

    ConvertFilopodiaPointOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry);

    virtual ~ConvertFilopodiaPointOperationSet();
    virtual OperationLogEntry getLogEntry() const;
    virtual void exec();
    int getNewVertexNum() const;

private:
    SpatialGraphPoint        mPoint;
    int                      mNewVertexNum;
    McDArray<int>            mLabels;
};

//////////////////////////////////////////////////////////////////////////////////////////////
class HXFILOPODIA_API AddFilopodiaVertexOperationSet : public OperationSet {
public:
    AddFilopodiaVertexOperationSet(HxSpatialGraph* sg,
                              const McVec3f& coords,
                              const SpatialGraphSelection &selectedElements,
                              const SpatialGraphSelection &visibleElements,
                              const McDArray<int> labels=McDArray<int>());

    AddFilopodiaVertexOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry);

    virtual ~AddFilopodiaVertexOperationSet();
    virtual OperationLogEntry getLogEntry() const;
    virtual void exec();
    int getNewVertexNum() const;

private:
    McVec3f mCoords;
    int mNewVertexNum;
    McDArray<int> mLabels;
};


//////////////////////////////////////////////////////////////////////////////////////////////
class HXFILOPODIA_API AddFilopodiaEdgeOperationSet : public OperationSet {
public:
    AddFilopodiaEdgeOperationSet(HxSpatialGraph* sg,
                                 const SpatialGraphSelection &selectedElements,
                                 const SpatialGraphSelection &visibleElements,
                                 const std::vector<McVec3f>& points = std::vector<McVec3f>(),
                                 const McDArray<int> labels=McDArray<int>());
    AddFilopodiaEdgeOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry);

    virtual ~AddFilopodiaEdgeOperationSet();
    virtual OperationLogEntry getLogEntry() const;
    virtual void exec();
    int getNewEdgeNum() const;

private:
    std::vector<McVec3f> mPoints;
    int mNewEdgeNum;
    McDArray<int> mLabels;
};


//////////////////////////////////////////////////////////////////////////////////////////////
class HXFILOPODIA_API ReplaceFilopodiaEdgeOperationSet : public OperationSet {
public:
    ReplaceFilopodiaEdgeOperationSet(HxSpatialGraph* sg,
                                     const SpatialGraphSelection& selectedElements,
                                     const SpatialGraphSelection& visibleElements,
                                     const std::vector<McVec3f>& points,
                                     const McDArray<int>& labels,
                                     const SpatialGraphSelection &selToDelete);

    ReplaceFilopodiaEdgeOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry);

    virtual ~ReplaceFilopodiaEdgeOperationSet();
    virtual OperationLogEntry getLogEntry() const;
    virtual void exec();
    int getNewEdgeNum() const;


private:
    int                           mNewEdgeNum;
    std::vector<McVec3f>             mPoints;
    SpatialGraphSelection         mSelToDelete;
    McDArray<int>                 mLabels;
    int                           mTimeId;
};


//////////////////////////////////////////////////////////////////////////////////////////////
class HXFILOPODIA_API AddRootsOperationSet : public OperationSet {
public:
    AddRootsOperationSet(HxSpatialGraph* sg,
                         const SpatialGraphSelection &selectedElements,
                         const SpatialGraphSelection &visibleElements,
                         const std::vector<McVec3f> &newNodes,
                         const std::vector<McDArray<int> > &newNodeLabels);

    AddRootsOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry);

    virtual ~AddRootsOperationSet();
    virtual OperationLogEntry getLogEntry() const;
    virtual void exec();

private:
    std::vector<McVec3f>            mNewNodes;
    std::vector<McDArray<int> >     mNewNodeLabels;
};


//////////////////////////////////////////////////////////////////////////////////////////////
class HXFILOPODIA_API DeleteFilopodiaOperationSet : public OperationSet {
public:
    DeleteFilopodiaOperationSet(HxSpatialGraph* sg, const SpatialGraphSelection &selectedElements, const SpatialGraphSelection &visibleElements);
    DeleteFilopodiaOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry);

    virtual ~DeleteFilopodiaOperationSet();
    virtual OperationLogEntry getLogEntry() const;
    virtual void exec();

private:
    int         mTimeId;
    int         mOldGeo;

};

//////////////////////////////////////////////////////////////////////////////////////////////
class HXFILOPODIA_API MergeFilopodiaOperationSet : public OperationSet {
public:
    MergeFilopodiaOperationSet(HxSpatialGraph* sg,
                               const SpatialGraphSelection& selectedElements,
                               const SpatialGraphSelection& visibleElements,
                               const std::vector<McVec3f>& newNodes,
                               const std::vector<McDArray<int> >& newNodeLabels,
                               const McDArray<int>& newEdgeSources,
                               const McDArray<int>& newEdgeTargets,
                               const std::vector<std::vector<McVec3f> >& newEdgePoints,
                               const std::vector<McDArray<int> >& newEdgeLabels,
                               const McDArray<int> connectLabels=McDArray<int>());

    MergeFilopodiaOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry);

    virtual ~MergeFilopodiaOperationSet();
    virtual OperationLogEntry getLogEntry() const;
    virtual void exec();
    SpatialGraphSelection getNewNodesAndEdges() const;
    int getConnectNode() const;
    SpatialGraphSelection getSelAfterOperation(SpatialGraphSelection sel) const;


private:
     std::vector<McVec3f>            mNewNodes;
     std::vector<McDArray<int> >     mNewNodeLabels;
     McDArray<int>                mNewEdgeSources;
     McDArray<int>                mNewEdgeTargets;
     std::vector<std::vector<McVec3f> > mNewEdgePoints;
     std::vector<McDArray<int> >     mNewEdgeLabels;
     McDArray<int>               mConnectLabels;
     SpatialGraphSelection        mNewNodesAndEdges;
     int                          mNewConnection;
};


//////////////////////////////////////////////////////////////////////////////////////////////
class HXFILOPODIA_API AddFilopodiumOperationSet : public OperationSet {
public:
    AddFilopodiumOperationSet(HxSpatialGraph* sg,
                              const SpatialGraphSelection &selectedElements,
                              const SpatialGraphSelection &visibleElements,
                              const std::vector<McVec3f> &newNodes,
                              const std::vector<McDArray<int> > &newNodeLabels,
                              const McDArray<int> &newEdgeSources,
                              const McDArray<int> &newEdgeTargets,
                              const std::vector<std::vector<McVec3f> > &newEdgePoints,
                              const std::vector<McDArray<int> > &newEdgeLabels,
                              const McDArray<int> connectLabels,
                              const bool addedIn2DViewer=true);

    AddFilopodiumOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry);

    virtual ~AddFilopodiumOperationSet();
    virtual OperationLogEntry getLogEntry() const;
    virtual void exec();
    SpatialGraphSelection getNewNodesAndEdges() const;
    int getConnectNode() const;

private:
     std::vector<McVec3f>            mNewNodes;
     std::vector<McDArray<int> >     mNewNodeLabels;
     McDArray<int>                mNewEdgeSources;
     McDArray<int>                mNewEdgeTargets;
     std::vector<std::vector<McVec3f> > mNewEdgePoints;
     std::vector<McDArray<int> >     mNewEdgeLabels;
     McDArray<int>                mConnectLabels;
     bool                         mAddedIn2DViewer;
     int                          mTimeId;
     SpatialGraphSelection        mNewNodesAndEdges;
     int                          mNewConnection;
};


//////////////////////////////////////////////////////////////////////////////////////////////
class HXFILOPODIA_API MoveTipOperationSet : public OperationSet {
public:
    MoveTipOperationSet(HxSpatialGraph* sg,
                        const SpatialGraphSelection& selectedElements,
                        const SpatialGraphSelection& visibleElements,
                        const std::vector<McVec3f>& newNodes,
                        const std::vector<McDArray<int> >& newNodeLabels,
                        const McDArray<int>& newEdgeSources,
                        const McDArray<int>& newEdgeTargets,
                        const std::vector<std::vector<McVec3f> >& newEdgePoints,
                        const std::vector<McDArray<int> >& newEdgeLabels,
                        const SpatialGraphSelection &selToDelete,
                        const bool addedIn2DViewer=true);

    MoveTipOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry);

    virtual ~MoveTipOperationSet();
    virtual OperationLogEntry getLogEntry() const;
    virtual void exec();
    SpatialGraphSelection getNewNodesAndEdges() const;


private:
     std::vector<McVec3f>            mNewNodes;
     std::vector<McDArray<int> >     mNewNodeLabels;
     McDArray<int>                mNewEdgeSources;
     McDArray<int>                mNewEdgeTargets;
     std::vector<std::vector<McVec3f> > mNewEdgePoints;
     std::vector<McDArray<int> >     mNewEdgeLabels;
     SpatialGraphSelection        mSelToDelete;
     bool                         mAddedIn2DViewer;
     SpatialGraphSelection        mNewNodesAndEdges;
     int                          mTimeId;
     McVec3f                      mOldPos;
     int                          mOldGeo;
     McVec3f                      mNewPos;
};


//////////////////////////////////////////////////////////////////////////////////////////////
class HXFILOPODIA_API MoveTipAlongEdgeOperationSet : public OperationSet {
public:
    MoveTipAlongEdgeOperationSet(HxSpatialGraph* sg,
                                 const SpatialGraphSelection &selectedElements,
                                 const SpatialGraphSelection &visibleElements,
                                 const McDArray<int> labels);
    MoveTipAlongEdgeOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry);

    virtual ~MoveTipAlongEdgeOperationSet();
    virtual OperationLogEntry getLogEntry() const;
    virtual void exec();
    int getNewVertexNum() const;

private:
    McDArray<int>   mLabels;
    int             mTimeId;
    McVec3f         mOldPos;
    int             mOldGeo;
    int             mNewVertexNum;
    McVec3f         mNewPos;
};


//////////////////////////////////////////////////////////////////////////////////////////////
class HXFILOPODIA_API MoveBaseOperationSet : public OperationSet {
public:
    MoveBaseOperationSet(HxSpatialGraph* sg,
                         const SpatialGraphSelection &selectedElements,
                         const SpatialGraphSelection &visibleElements,
                         const std::vector<McVec3f> &newNodes,
                         const std::vector<McDArray<int> > &newNodeLabels,
                         const McDArray<int> &newEdgeSources,
                         const McDArray<int> &newEdgeTargets,
                         const std::vector<std::vector<McVec3f> > &newEdgePoints,
                         const std::vector<McDArray<int> > &newEdgeLabels,
                         const SpatialGraphSelection &deleteSel,
                         const McDArray<int>& connectLabels=McDArray<int>());

    MoveBaseOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry);

    virtual ~MoveBaseOperationSet();
    virtual OperationLogEntry getLogEntry() const;
    virtual void exec();
    SpatialGraphSelection getNewNodesAndEdges() const;
    int getConnectNode() const;

private:
    std::vector<McVec3f>            mNewNodes;
    std::vector<McDArray<int> >     mNewNodeLabels;
    McDArray<int>                mNewEdgeSources;
    McDArray<int>                mNewEdgeTargets;
    std::vector<std::vector<McVec3f> > mNewEdgePoints;
    std::vector<McDArray<int> >     mNewEdgeLabels;
    SpatialGraphSelection        mSelToDelete;
    McDArray<int>                mConnectLabels;
    int                          mTimeId;
    McVec3f                      mOldPos;
    int                          mOldGeo;
    SpatialGraphSelection        mNewNodesAndEdges;
    int                          mNewConnection;
    McVec3f                      mNewPos;
};


//////////////////////////////////////////////////////////////////////////////////////////////
class HXFILOPODIA_API MoveBaseAlongEdgeOperationSet : public OperationSet {
public:
    MoveBaseAlongEdgeOperationSet(HxSpatialGraph* sg,
                                  const SpatialGraphSelection &selectedElements,
                                  const SpatialGraphSelection &visibleElements,
                                  McDArray<int> labels);
    MoveBaseAlongEdgeOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry);

    virtual ~MoveBaseAlongEdgeOperationSet();
    virtual OperationLogEntry getLogEntry() const;
    virtual void exec();
    int getNewBase() const;

private:
    McDArray<int>   mLabels;
    int             mTimeId;
    McVec3f         mOldPos;
    int             mOldGeo;
    int             mNewBase;
    McVec3f         mNewPos;
};


//////////////////////////////////////////////////////////////////////////////////////////////
class HXFILOPODIA_API ConnectOperationSet : public OperationSet {
public:
    ConnectOperationSet(HxSpatialGraph* sg,
                         const SpatialGraphSelection &selectedElements,
                         const SpatialGraphSelection &visibleElements,
                         const std::vector<McVec3f> &newNodes,
                         const std::vector<McDArray<int> > &newNodeLabels,
                         const McDArray<int> &newEdgeSources,
                         const McDArray<int> &newEdgeTargets,
                         const std::vector<std::vector<McVec3f> > &newEdgePoints,
                         const std::vector<McDArray<int> > &newEdgeLabels,
                         const McDArray<int> &connectLabels,
                         const SpatialGraphSelection &deleteSel);

    ConnectOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry);

    virtual ~ConnectOperationSet();
    virtual OperationLogEntry getLogEntry() const;
    virtual void exec();

private:
    SpatialGraphSelection        mSelToDelete;
    std::vector<McVec3f>            mNewNodes;
    std::vector<McDArray<int> >     mNewNodeLabels;
    McDArray<int>                mNewEdgeSources;
    McDArray<int>                mNewEdgeTargets;
    std::vector<std::vector<McVec3f> > mNewEdgePoints;
    std::vector<McDArray<int> >     mNewEdgeLabels;
    McDArray<int>                mConnectLabels;
    int                          mTimeId;
};


//////////////////////////////////////////////////////////////////////////////////////////////
class HXFILOPODIA_API ReplaceFilopodiaOperationSet : public OperationSet {
public:
    ReplaceFilopodiaOperationSet(HxSpatialGraph* sg,
                                 const SpatialGraphSelection& selectedElements,
                                 const SpatialGraphSelection& visibleElements,
                                 const std::vector<McVec3f>& newNodes,
                                 const std::vector<McDArray<int> >& newNodeLabels,
                                 const McDArray<int>& matchedPreviousNodes,
                                 const McDArray<int>& newEdgeSources,
                                 const McDArray<int>& newEdgeTargets,
                                 const std::vector<std::vector<McVec3f> >& newEdgePoints,
                                 const std::vector<McDArray<int> >& newEdgeLabels,
                                 const SpatialGraphSelection& successorSel);

    ReplaceFilopodiaOperationSet(HxSpatialGraph* sg, const OperationLogEntry& logEntry);

    virtual ~ReplaceFilopodiaOperationSet();
    virtual OperationLogEntry getLogEntry() const;
    virtual void exec();
    SpatialGraphSelection getNewNodesAndEdges() const;

private:
     std::vector<McVec3f>            mNewNodes;
     std::vector<McDArray<int> >     mNewNodeLabels;
     McDArray<int>                mMatchedPreviousNodes;
     McDArray<int>                mNewEdgeSources;
     McDArray<int>                mNewEdgeTargets;
     std::vector<std::vector<McVec3f> > mNewEdgePoints;
     std::vector<McDArray<int> >     mNewEdgeLabels;
     SpatialGraphSelection        mSuccessorSel;
     int                          mTimeId;
     SpatialGraphSelection        mNewNodesAndEdges;
};




/** Factory function to create SpatialGraphOperations known only at runtime (required for logging)
 *  We use extern "C" to avoid name mangling so HxDSO can find it. */
#ifdef __cplusplus
extern "C" {
#endif
    void HXFILOPODIA_API FilopodiaGraphOperationSet_plugin_factory(HxSpatialGraph* graph, const OperationLogEntry& logEntry, Operation *&op);
#ifdef __cplusplus
}
#endif


#endif
