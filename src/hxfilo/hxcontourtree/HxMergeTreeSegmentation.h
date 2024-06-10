#ifndef HXMERGETREESEGMENTATION_H
#define HXMERGETREESEGMENTATION_H


#include <hxcontourtree/api.h>

#include <hxcore/HxCompModule.h>
#include <hxcore/HxConnection.h>
#include <hxcore/HxPortRadioBox.h>
#include <hxcore/HxPortOnOff.h>
#include <hxcore/HxPortFloatTextN.h>
#include <hxcore/HxPortFloatSlider.h>
#include <hxcore/HxPortDoIt.h>

#include <mclib/McPrimType.h>
#include <hxfield/HxCoordType.h>
#include <mclib/McBox3f.h>
#include <mclib/McDim3l.h>

#include <hxcontourtree/Lattice3Mesh.h>
#include <hxcontourtree/MergeTree.h>




class HxLattice3;
class HxUniformLabelField3;


class HXCONTOURTREE_API HxMergeTreeSegmentation : public HxCompModule
{
    HX_HEADER(HxMergeTreeSegmentation)

public:
    virtual void compute() override;
    virtual void update() override;

    Lattice3MeshBase* getLattice3Mesh() { return m_lattice3Mesh; }
    MergeTreeBase* getMergeTree() { return m_mergeTree; }


    HxConnection portBackwardDeformation;
    HxConnection portForwardDeformation;
    HxPortRadioBox portInterpretation;
    HxPortRadioBox portConnectivity;
    HxPortRadioBox portMergeTree;
    HxPortRadioBox portSortNeighbours;
    HxPortOnOff portUseGlobalRange;
    HxPortFloatTextN portGlobalRange;
    HxPortFloatTextN portRange;
    HxPortRadioBox portPersistenceMode;
    HxPortFloatSlider portPersistence;
    HxPortRadioBox portSegmentationMode;
    HxPortOnOff portConsistentExtremaLabels;
    HxPortOnOff portNestedCoresWithSideBranches;
    HxPortDoIt portDoIt;

private:
    void extractSrcLatInformation(const HxLattice3* srcLat);
    bool checkData();
    bool checkDeformation();
    HxUniformLabelField3* createOrReuseResult();


    HxLattice3* m_srcLat;

    McDim3l m_srcLatDims;
    int m_srcLatNDataVar;
    McPrimType m_srcLatPrimType;
    HxCoordType m_srcLatCoordType;
    McBox3f m_srcLatBBox;
    float m_dataMin;
    float m_dataMax;

    HxLattice3* m_lastSrcLat;
    bool m_newData;


    HxLattice3* m_bdLat;
    HxLattice3* m_fdLat;
    HxLattice3* m_lastBD;
    HxLattice3* m_lastFD;
    bool m_newDeformation;


    Interpretation m_interpretation;
    Interpretation m_lastInterpretation;
    bool m_newInterpretation;


    Connectivity m_connectivity;
    Connectivity m_last3Dconnectivity;
    Connectivity m_last2Dconnectivity;
    Connectivity m_lastConnectivity;
    bool m_newConnectivity;


    MergeMode m_mergeMode;
    MergeMode m_lastMergeMode;
    bool m_newMergeMode;


    SortNeighbours m_sortNeighbours;
    SortNeighbours m_lastSortNeighbours;
    bool m_newSortNeighbours;


    float m_lastGlobalMin;
    float m_lastGlobalMax;
    bool m_newGlobalRange;


    float m_lastMin;
    float m_lastMax;
    bool m_newRange;


    PersistenceMode m_persistenceMode;
    PersistenceMode m_lastPersistenceMode;
    bool m_newPersistenceMode;


    float m_lastGlobalPersistence;
    float m_lastAdaptivePersistence;
    float m_lastPersistence;
    bool m_newPersistence;


    SegmentationMode m_segmentationMode;
    SegmentationMode m_lastSegmentationMode;
    bool m_newSegmentationMode;


    bool m_lastConsistentExtremaLabels;
    bool m_newConsistentExtremaLabels;


    bool m_lastNestedCoresWithSideBranches;
    bool m_newNestedCoresWithSideBranches;


    Lattice3MeshBase* m_lattice3Mesh;
    MergeTreeBase* m_mergeTree;
};


#endif // HXMERGETREESEGMENTATION_H
