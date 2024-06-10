#ifndef HXPROPAGATECONTOURS_H
#define HXPROPAGATECONTOURS_H

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
    float modulatedValue;
    int label;

    int
    operator<(const PixelHeapElement& other)
    {
        return modulatedValue > other.modulatedValue;
    }

    PixelHeapElement()
    {
    }

    PixelHeapElement(McVec3i& pos, float val, float modVal, float lbl)
        : position(pos)
        , pixelValue(val)
        , modulatedValue(1.f)
        , label(lbl)
    {
    }

    ~PixelHeapElement()
    {
    }
};

class HXCONTOURTREE_API HxPropagateContours : public HxCompModule
{
    HX_HEADER(HxPropagateContours);

public:
    virtual void compute();
    void update();

    HxConnection portLabelSeedField;
    HxConnection portLandmarkSeeds;

    // these need to be public
    HxPortRadioBox portWhenStop;
    HxPortFloatSlider portStopThreshold;
    HxPortRadioBox portSeedRadio;
    HxPortMultiMenu portMode;
    McHandle<HxPort3DPointList> portPointList;
    HxPortFloatSlider portPointSize;
    HxPortDoIt portAction;

private:
    void selectPoint(const int pointId);
    void initCompute();
    void initThreshold();
    void initLabelField();
    void initContours();
    void initContoursFromSeedField();
    void initFields();
    void initFieldsFromSeedField();
    void propagate();
    void cleanUp();

    void addNeighboursToHeap(std::vector<PixelHeapElement*>& nhood);
    void getNeighbours(const McVec3i& pt, int parentLbl, std::vector<PixelHeapElement*>& nhood);
    void pushNeighbour(int x, int y, int z, int parentLbl, std::vector<PixelHeapElement*>& nhood);
    void labelNeighbours(const float* label, std::vector<PixelHeapElement*>& nhood);
    void analyzeNeighbours(bool& touchPointReached, bool& thresholdReached, std::vector<PixelHeapElement*>& nhood, PixelHeapElement* elem);
    void processCurrentElement(float currentLabel, std::vector<PixelHeapElement*>& newFront, std::vector<PixelHeapElement*>& nhood);
    void updateMaxima(const McVec3i& position, const float& value, const float& currentLabel);
    void printPixelHeapElement(PixelHeapElement* elem);
    void printVector(std::vector<PixelHeapElement>& points);

    bool getSeedMode();

    void makePixelHeapElementAvailableForReuse(PixelHeapElement* elem);
    PixelHeapElement* getPixelHeapElement();

    // for getting the point coordinates
    float m_bbox[6];
    mculong m_dims[3];
    McVec3f m_voxelSize;

    void getFieldInfo();
    void initPortPointList();
    void initPointList();
    McVec3f getWorldLocation(const int pointId);
    McVec3i getLatticeLocation(const int pointId);
    McVec3f convertToWorldLocation(McVec3i& p);
    float getModulatedValue(int x, int y, int z, int pointId, float val);
    //

    McHandle<HxUniformScalarField3> mInputField;
    McHandle<HxUniformLabelField3> mInputLabelField;
    McHandle<HxUniformLabelField3> mOutputLabelField;
    McHandle<HxUniformScalarField3> mFrontField;

    McFHeap<PixelHeapElement> mHeap;
    std::vector<PixelHeapElement*> mPixelHeapElements;
    mclong mCurrentPixelHeapElementId;
    std::vector<McVec3f> mPointListWorld;
    std::vector<McVec3i> mPointListGrid;

    static const float setFront;
    static const float notFront;

    static const int resultIdField = 0;

    float thresholdTP;
    std::vector<PixelHeapElement> maximalPoints;

    bool seedMode; //when true seed from points, otherwise from labels
    bool whenStop; //when true, will stop when the stopping threshold is reached
                   //otherwise it will stop at the first touching point,

    McWatch mWatch;
    float mLastTime;

    static int keyboardEventCallback(const SoEvent*, HxViewer*, void*);
};

#endif
