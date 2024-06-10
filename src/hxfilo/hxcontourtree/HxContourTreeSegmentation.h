#ifndef HX_CONTOUR_TREE_SEGMENTATION_H
#define HX_CONTOUR_TREE_SEGMENTATION_H

#include <mclib/McHandle.h>

#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>
#include <hxcore/HxPortFloatSlider.h>
#include <hxcore/HxPortGeneric.h>
#include <hxcore/HxPortRadioBox.h>
#include <hxcore/HxPortToggleList.h>
#include <hxcore/HxPortButtonList.h>
#include <hxplot/PzEasyPlot.h>

#include "ContourTree.h"

#include "api.h"
#include "SetUnionDataStructure.h"

class SimplicialMesh3DForHexahedralMesh;
class HxUniformScalarField3;
class HxLattice3;
class McHistogram;

class HXCONTOURTREE_API HxContourTreeSegmentation : public HxCompModule
{
    HX_HEADER(HxContourTreeSegmentation);

public:
    HxPortToggleList portAutoSetValues;

    HxPortFloatSlider portThreshold;

    HxPortRadioBox portPersistenceMode;

    HxPortRadioBox portPersistenceOptions;

    HxPortFloatSlider portPersistenceValue;

    HxPortFloatSlider portMinimalSegmentSize;

    HxPortRadioBox portSortNeighborsBy;

    HxPortToggleList portFastSegmentation;

    HxPortButtonList portCalculatePlot;

    HxPortDoIt portDoIt;

protected:
    void update();
    void compute();

private:
    HxContourTreeSegmentation(const HxContourTreeSegmentation&);
    HxContourTreeSegmentation& operator=(const HxContourTreeSegmentation&);

    void recomputeRange();
    void recomputeHistogram();
    void setScalarValueWidth();
    void initThresholdPort();
    void initPersistencePort();
    float getPersistenceValue();
    void initOutputLabelField();
    int computeSegmentation(const float persistenceValue, const mculong minNumberVoxels, AugmentedContourTree* augmentedJoinTree = 0);
    void calculatePlot();
    void calculatePlotForFastSegmentation();
    void sortNeighbors(const std::vector<mculong>& neighbors,
                       const std::vector<float>& values,
                       std::vector<mculong>& sortedNeighborIds,
                       SetUnionDataStructure& setUnion);
    void sortNeighborsAccordingToValues(const std::vector<float>& values,
                                        std::vector<mculong>& sortedNeighborIds);
    void sortNeighborsAccordingToMajorityVote(const std::vector<mculong>& neighbors,
                                              std::vector<mculong>& sortedNeighborIds,
                                              SetUnionDataStructure& setUnion);
    void relabelOutputLabelField(SetUnionDataStructure& setUnion);
    mcuint32 evalOutputLabelField(HxLattice3& lattice,
                                  mculong meshVertexId);
    void updateRelativePersistenceValue();
    void updatePlot(const mclong numberOfPlotPoints,
                    std::vector<float>& persistanceValues,
                    std::vector<float>& numberOfSegments,
                    const float maxPersistanceValue);
    void initFastContourTreeSegmentation();
    int fastContourTreeSegmentation(const float persistenceValue, const mculong minNumberVoxels);
    void getRange(McHistogram& histogram,
                  float& vMin,
                  float& vMax,
                  const float lowerBound,
                  const float upperBound);

private:
    float m_minValue;
    float m_maxValue;
    float m_minValueHistogram;
    float m_maxValueHistogram;
    float m_scalarValueWidth;
    float m_relativePersistenceValue;
    McHandle<SimplicialMesh3DForHexahedralMesh> m_mesh;
    HxUniformLabelField3* m_outputLabelField;
    bool m_recomputeHistogram;
    McHandle<PzEasyPlot> m_plot;
    bool m_plotParametersChanged;
    bool m_needFastSegmentationInitialization;

    HxUniformScalarField3* m_inputField;

    McHandle<ContourTree> m_contourTree;
    SetUnionDataStructure m_setUnionFinestSegmentation;
    std::vector<float> m_maxValuesOfComponentFinestSegmentation;
    std::vector<mculong> m_sizeOfComponentFinestSegmentation;
    int m_numberOfSegmentsFinestSegmentation;
};

#endif
