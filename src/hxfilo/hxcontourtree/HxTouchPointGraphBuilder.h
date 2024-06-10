#ifndef HXTOUCHPOINTGRAPHBUILDER_H
#define HXTOUCHPOINTGRAPHBUILDER_H

#include <vector>

#include "api.h"

#include <hxcore/HxCompModule.h>
#include <hxcore/HxMessage.h>
#include <hxcore/HxPortDoIt.h>
#include <hxcore/HxPort3DPointList.h>
#include <hxcore/HxPortFloatSlider.h>
#include <hxcore/HxPortRadioBox.h>

#include <hxcore/HxConnection.h>

#include <hxfield/HxUniformLabelField3.h>
#include <hxfield/HxUniformScalarField3.h>

#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <hxspatialgraph/internal/EdgeVertexAttribute.h>

#include <mclib/internal/McFHeap.h>
#include <mclib/McVec3i.h>
#include <mclib/internal/McWatch.h>

class SoEvent;
class HxViewer; // for the keyboard call back, i guess

// copied from HxCoralSegmentation
struct PixelHeapElement : public McFHeapElement
{
    McVec3i position;
    float pixelValue;
    int label;
    std::vector<float> labels;

    int operator < (const PixelHeapElement& other)
    {
        return pixelValue > other.pixelValue;
    }

    PixelHeapElement()
    {}

    PixelHeapElement(McVec3i & pos, float val, float lbl) 
        : position(pos), pixelValue(val), label(lbl) 
    {
    }

    ~PixelHeapElement()
    {
        labels.clear();
    }
};

class HXCONTOURTREE_API HxTouchPointGraphBuilder : public HxCompModule
{
    HX_HEADER(HxTouchPointGraphBuilder);

    public:
        virtual void compute();
        void update();

        // these need to be public
        HxPortRadioBox    portWhenStop;
        HxPortFloatSlider portStopThreshold;
        HxPortFloatSlider portDisplayThreshold;
        McHandle<HxPort3DPointList> portPointList;
        HxPortFloatSlider portPointSize;
        HxPortDoIt        portAction;

    private:
        void selectPoint(const int pointId);
        void initCompute();
        void initThreshold(HxPortFloatSlider * slider);
        void initContours();
        void initFields();
        void propagate();
        void cleanUp();
        
        // graph
        void initGraph();
        void connectGraph();
        void cleanUpComputation();

        void addNeighboursToHeap(std::vector<PixelHeapElement*> & nhood);
        void getNeighbours(const McVec3i & pt, std::vector<PixelHeapElement*> & nhood);
        void pushNeighbour(int x, int y, int z, std::vector<PixelHeapElement*> & nhood);
        void labelNeighbours(const float * label, std::vector<PixelHeapElement*> & nhood);
        void analyzeNeighbours(bool & touchPointReached, bool & thresholdReached, std::vector<PixelHeapElement*> & nhood, PixelHeapElement * elem);
        void processCurrentElement(float currentLabel, std::vector<PixelHeapElement*> & newFront, std::vector<PixelHeapElement*> & nhood);
        void updateMaxima(const McVec3i & position, const float & value, const float & currentLabel);
        void printPixelHeapElement(PixelHeapElement * elem);
        void printVector(std::vector<PixelHeapElement> & points);
        
        //graph
        void addIntPointToGraph(McVec3i & pt, int & index, const int id);
        void addPointsToGraph(std::vector<PixelHeapElement> & points, std::vector<int> & indexes, const int id);
        void addEdges(std::vector<int> & sources, std::vector<int> & targets);

        bool getSeedMode();

        // for getting the point coordinates
        float   m_bbox[6];
        mculong m_dims[3];
        McVec3f m_voxelSize;
        
        void getFieldInfo();
        void initPointList();
        McVec3f getWorldLocation(const int pointId);
        McVec3i getLatticeLocation(const int pointId);
        McVec3f convertToWorldLocation(McVec3i & p);
        //

        McHandle<HxUniformScalarField3> mInputField;
        McHandle<HxUniformScalarField3> mOutputLabelField;
        McHandle<HxUniformScalarField3> mFrontField;
        
        McHandle<HxSpatialGraph> graph;

        McFHeap<PixelHeapElement> mHeap;

        static const float setFront;
        static const float notFront;
        
        static const int idTouching = 0;
        static const int idStarting = 1;
        static const int idMax      = 2;

        static const int resultIdField = 0;
        static const int resultIdGraph = 1;

        float thresholdTP;
        std::vector<PixelHeapElement> startingPoints;
        std::vector<PixelHeapElement> maximalPoints;
        std::vector<PixelHeapElement> touchingPoints;
        
        std::vector<int> startingIdx;
        std::vector<int> maximalIdx;
        std::vector<int> touchingIdx;

        EdgeVertexAttribute * valueAttrib;
        EdgeVertexAttribute * idAttrib;

        bool whenStop;  //when true, will stop when the stopping threshold is reached
                        //otherwise it will stop at the first touching point,

        McWatch mWatch;
        float mLastTime;

        static int keyboardEventCallback(const SoEvent*, HxViewer*, void*);
};

#endif
