#include "HxPropagateContours.h"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <limits>

#include <hxcore/internal/HxWorkArea.h>
#include <hxcore/HxMessage.h>
#include <hxcore/HxController.h>

#include <hxlandmark/internal/HxLandmarkSet.h>

#include <Inventor/events/SoEvents.h>

// some helpful constant values
const float HxPropagateContours::setFront = 1.0f;
const float HxPropagateContours::notFront = 0.0f;

HX_INIT_CLASS(HxPropagateContours, HxCompModule)

// constructor
HxPropagateContours::HxPropagateContours()
    : HxCompModule(HxLattice3::getClassTypeId())
    , portLabelSeedField(this, "labelSeed", tr("Label Seed"), HxLattice3::getClassTypeId())
    , portLandmarkSeeds(this, "landmarkSeeds", tr("Landmark Seeds"), HxLandmarkSet::getClassTypeId())
    , portWhenStop(this, "StopAt", tr("Stop At"), 2)
    , portStopThreshold(this, "ScalarThreshold", tr("Scalar Threshold"))
    , portSeedRadio(this, "SeedFrom", tr("Seed From"), 2)
    , portMode(this, "Mode", tr("Mode"), 3)
    , portPointList(new HxPort3DPointList(this, "pointList"))
    , portPointSize(this, "pointSize", tr("Point Size"))
    , portAction(this, "action", tr("Action"))
{
    portAction.setLabel(0, "DoIt");

    portPointList->setNumPoints(0, 0, -1);

    portStopThreshold.setMinMax(0.0f, 1.0f);
    portStopThreshold.setValue(0.5f);

    portMode.setLabel(0, "scalar value");
    portMode.setLabel(1, "scalar value + dist");
    portMode.setLabel(2, "dist");

    portPointSize.setMinMax(0.0f, 1.0f);
    portPointSize.setValue(0.5f);

    portSeedRadio.setLabel(0, "Point List");
    portSeedRadio.setLabel(1, "Label Field");

    portWhenStop.setLabel(0, "Threshold");
    portWhenStop.setLabel(1, "First Touching Point");

    theController->addEventCallback(&keyboardEventCallback, this);

    mCurrentPixelHeapElementId = -1;
}

// Sets the max, min and middle values of the slider port
void
HxPropagateContours::initThreshold()
{
    HxUniformScalarField3* inputField =
        hxconnection_cast<HxUniformScalarField3>(portData);

    if (!inputField)
        return;

    float maxVal = 1.0f, minVal = 0.0f;
    inputField->getRange(minVal, maxVal);

    portStopThreshold.setMinMax(minVal, maxVal);
    portStopThreshold.setValue((minVal + maxVal) * 0.5f);
    return;
}

// get BBox and voxel size of input field
void
HxPropagateContours::getFieldInfo()
{
    HxUniformScalarField3* inputField =
        hxconnection_cast<HxUniformScalarField3>(portData);

    if (!inputField)
        return;

    const McDim3l& dims = inputField->lattice().getDims();
    for (int i = 0; i < 3; ++i)
        m_dims[i] = dims[i];

    const McBox3f& bbox = inputField->getBoundingBox();
    for (int i = 0; i < 6; ++i)
        m_bbox[i] = bbox[i];

    m_voxelSize[0] = (m_bbox[1] - m_bbox[0]) / (m_dims[0] - 1);
    m_voxelSize[1] = (m_bbox[3] - m_bbox[2]) / (m_dims[1] - 1);
    m_voxelSize[2] = (m_bbox[5] - m_bbox[4]) / (m_dims[2] - 1);
}

// set initial values for the points
void
HxPropagateContours::initPortPointList()
{
    int numClipped = 0;
    for (int i = 0; i < portPointList->getNumPoints(); i++)
    {
        SbVec3d pt = portPointList->getCoord(i);
        if ((pt[0] < m_bbox[0]) || (pt[0] > m_bbox[1]) ||
            (pt[1] < m_bbox[2]) || (pt[1] > m_bbox[3]) ||
            (pt[2] < m_bbox[4]) || (pt[2] > m_bbox[5]))
            numClipped++;
    }

    if (numClipped)
    {
        theMsg->warning(QString("Warning: %1 points have been clipped").arg(numClipped));
    }

    portPointList->setBoundingBox(&m_bbox[0]);

    const float pointSize = (m_voxelSize[0] + m_voxelSize[1] + m_voxelSize[2]) / 3.f;

    portPointList->setPointScale(1.f);

    portPointList->setPointSize(pointSize / 2.f);

    portPointList->setImmediateMode(1);
    portPointList->showPoints();
}

// select a point from the point list
void
HxPropagateContours::selectPoint(
    const int pointId)
{
    portPointList->selectPoint(pointId);
}

// get the world coordinates for the points (float)
McVec3f
HxPropagateContours::getWorldLocation(
    const int pointId)
{
    SbVec3d p = portPointList->getCoord(pointId);

    McVec3f location((float)p[0], (float)p[1], (float)p[2]);

    return location;
}

void
HxPropagateContours::initPointList()
{
    HxLandmarkSet* hxLandmarkSet = hxconnection_cast<HxLandmarkSet>(portLandmarkSeeds);

    if (hxLandmarkSet)
    {
        mPointListWorld = hxLandmarkSet->getPositions();
    }
    else
    {
        mPointListWorld.resize(portPointList->getNumPoints());
        for (int i = 0; i < static_cast<int>(mPointListWorld.size()); ++i)
        {
            SbVec3d pTmp = portPointList->getCoord(i);
            mPointListWorld[i] = McVec3f(pTmp[0], pTmp[1], pTmp[2]);
        }
    }

    mPointListGrid.resize(mPointListWorld.size());
    for (int i = 0; i < static_cast<int>(mPointListWorld.size()); ++i)
    {
        mPointListGrid[i] = getLatticeLocation(i);
    }
}

float
HxPropagateContours::getModulatedValue(
    int x, int y, int z, int pointId, float val)
{
    if (portMode.getValue() == 0 || portSeedRadio.getValue() == 1)
    {
        return val;
    }

    McVec3f p1(x, y, z);
    McVec3f p2(mPointListGrid[pointId][0],
               mPointListGrid[pointId][1],
               mPointListGrid[pointId][2]);

    const float dist = (p1 - p2).length();

    if (portMode.getValue() == 1)
    {
        return (val / (dist + 1));
    }
    else if (portMode.getValue() == 2)
    {
        return (1.f / (dist + 1));
    }
    else
    {
        throw("Error: Mode not supported!");
    }
}

// get the lattice coordinates for the points (integer)
McVec3i
HxPropagateContours::getLatticeLocation(
    const int pointId)
{
    const McVec3f& p = mPointListWorld[pointId];

    McVec3i locationLattice;
    locationLattice[0] = mculong(((p[0] - m_bbox[0]) / m_voxelSize[0]) + 0.5);
    locationLattice[1] = mculong(((p[1] - m_bbox[2]) / m_voxelSize[1]) + 0.5);
    locationLattice[2] = mculong(((p[2] - m_bbox[4]) / m_voxelSize[2]) + 0.5);

    return locationLattice;
}

// get the world coordinates for an integer point
McVec3f
HxPropagateContours::convertToWorldLocation(
    McVec3i& p)
{
    McVec3f locationWorld;

    locationWorld[0] = (float)p[0] * m_voxelSize[0] + m_bbox[0];
    locationWorld[1] = (float)p[1] * m_voxelSize[1] + m_bbox[2];
    locationWorld[2] = (float)p[2] * m_voxelSize[2] + m_bbox[4];

    return locationWorld;
}

void
HxPropagateContours::compute()
{
    if (!portData.getSource())
    {
        theMsg->printf("Source image has not been specified");
        return;
    }

    if (!portAction.wasHit())
        return;

    if (!mInputField)
        return;

    initCompute();

    thresholdTP = portStopThreshold.getValue();

    theWorkArea->busy();
    theWorkArea->startWorking("Propagating contours");

    propagate();

    theWorkArea->stopWorking();
    theWorkArea->notBusy();

    if (mOutputLabelField)
    {
        HxParamBundle* materials = mOutputLabelField->getParameters().getMaterials();
        if (materials)
            materials->removeAll();
        mOutputLabelField->relabel();
    }

    mOutputLabelField->composeLabel(portData.getSource()->getLabel(), "labels");
    mOutputLabelField->touchMinMax();
    setResult(resultIdField, mOutputLabelField);

    return;
}

void
HxPropagateContours::initCompute()
{
    if (seedMode) // seed from points
    {
        initFields();
        initContours();
    }
    else // seed from label field
    {
        if (!mInputLabelField)
        {
            theMsg->stream() << "Invalid or unconnected seeding label field"
                             << std::endl;
            return;
        }

        initFieldsFromSeedField();
        initContoursFromSeedField();
    }

    return;
}

void
HxPropagateContours::printVector(std::vector<PixelHeapElement>& points)
{
    std::vector<PixelHeapElement>::iterator p_it = points.begin();
    std::vector<PixelHeapElement>::iterator p_it_end = points.end();

    for (; p_it != p_it_end; ++p_it)
    {
        printPixelHeapElement(&*p_it);
    }
    return;
}

void
HxPropagateContours::printPixelHeapElement(PixelHeapElement* elem)
{
    theMsg->stream()
        << elem->label << " value: "
        << elem->pixelValue << " at "
        << elem->position.i << ", "
        << elem->position.j << ", "
        << elem->position.k << " with modulated value: "
        << elem->modulatedValue << std::endl;
    return;
}

void
HxPropagateContours::initFields()
{
    mOutputLabelField =
        dynamic_cast<HxUniformLabelField3*>(getResult(resultIdField));

    const McDim3l& dims = mInputField->lattice().getDims();
    const McBox3f& bbox = mInputField->getBoundingBox();

    if (!mOutputLabelField ||
        mOutputLabelField->lattice().primType() != McPrimType::MC_UINT16)
    {
        mOutputLabelField = new HxUniformLabelField3(dims, McPrimType::MC_UINT16);
    }
    else
    {
        mOutputLabelField->lattice().resize(dims);
    }
    mOutputLabelField->lattice().setBoundingBox(bbox);

    float background = 0.f;
    for (mclong k = 0; k < dims[2]; ++k)
    {
        for (mclong j = 0; j < dims[1]; ++j)
        {
            for (mclong i = 0; i < dims[0]; ++i)
            {
                mOutputLabelField->lattice().set(i, j, k, &background);
            }
        }
    }

    // the front values are 0 too
    mFrontField = mOutputLabelField->duplicate();
    return;
}

void
HxPropagateContours::initFieldsFromSeedField()
{
    mOutputLabelField =
        dynamic_cast<HxUniformLabelField3*>(getResult(resultIdField));

    if (!mOutputLabelField)
        mOutputLabelField = mInputLabelField->duplicate();

    const McDim3l& dims = mOutputLabelField->lattice().getDims();

    mOutputLabelField->lattice().setPrimType(mInputLabelField->lattice().primType());

    for (mclong k = 0; k < dims[2]; ++k)
    {
        for (mclong j = 0; j < dims[1]; ++j)
        {
            for (mclong i = 0; i < dims[0]; ++i)
            {
                float lbl;
                mInputLabelField->lattice().eval(i, j, k, &lbl);
                mOutputLabelField->lattice().set(i, j, k, &lbl);
            }
        }
    }

    // the front will be initialized in the ::initContoursFromSeedField() method
    mFrontField = mInputLabelField->duplicate();
    return;
}

void
HxPropagateContours::initLabelField()
{
    mInputLabelField =
        hxconnection_cast<HxUniformLabelField3>(portLabelSeedField);

    if (!mInputLabelField ||
        !mInputField)
        return;

    const McDim3l& inputDims = mInputField->lattice().getDims();
    const McDim3l& labelDims = mInputLabelField->lattice().getDims();

    if ((inputDims[0] != labelDims[0]) ||
        (inputDims[1] != labelDims[1]) ||
        (inputDims[2] != labelDims[2]))
    {
        theMsg->stream()
            << "Seed label field has different dimensions than input field"
            << std::endl;

        mInputLabelField = 0;
    }
    return;
}

void
HxPropagateContours::propagate()
{
    PixelHeapElement* elem = mHeap.getMin();

    int processedElems = 0;
    bool canceled = false;

    mWatch.start();
    mLastTime = mWatch.getTime();

    while (elem && !canceled) // while the heap is not empty
    {
        processedElems++;
        const float newTime = mWatch.getTime();
        if (newTime - mLastTime > 1.f)
        {
            theWorkArea->setProgressInfo(QString("Processed elements: %1").arg(processedElems));
            mLastTime = newTime;
        }

        mHeap.deleteElem(elem);
        McVec3i pt;
        pt[0] = elem->position.i;
        pt[1] = elem->position.j;
        pt[2] = elem->position.k;

        std::vector<PixelHeapElement*> nhood;
        getNeighbours(pt, elem->label, nhood);

        // take a look at the neighbours
        bool touchPointReached = false;
        bool thresholdReached = false;

        analyzeNeighbours(touchPointReached, thresholdReached, nhood, elem);

        if ((thresholdReached) && (whenStop))
            break;

        if ((touchPointReached) && (!whenStop))
            break;

        // process current element
        float currentLabel;
        mOutputLabelField->lattice().eval(pt[0], pt[1], pt[2], &currentLabel);
        std::vector<PixelHeapElement*> newFront;

        processCurrentElement(currentLabel, newFront, nhood);

        // keep track of maximal values
        if (seedMode)
            updateMaxima(elem->position, elem->pixelValue, currentLabel);

        // voxel is no longer front
        mFrontField->lattice().set(pt[0], pt[1], pt[2], &(notFront));

        addNeighboursToHeap(newFront);

        makePixelHeapElementAvailableForReuse(elem);

        nhood.clear();
        newFront.clear();

        elem = mHeap.getMin();
        
        // check if the user canceled
        if((processedElems % 1000 == 0) && theWorkArea->wasInterrupted())
        {
            canceled = true;
        }
    }

    if(canceled)
    {
        theMsg->printf("canceled propagation");
    }
    theMsg->printf("elapsed time: %f seconds", mWatch.stop());

    cleanUp();
}

void
HxPropagateContours::cleanUp()
{
    PixelHeapElement* elem = mHeap.getMin();
    int processedElems = 0;

    while (elem) // while the heap is not empty
    {
        processedElems++;
        if (processedElems % 100 == 0)
            theWorkArea->setProgressInfo(QString("Cleaning up heap: %1 ").arg(processedElems));

        mHeap.deleteElem(elem);
        delete elem;
        elem = mHeap.getMin();
    }

    for (int i = 0; i < mCurrentPixelHeapElementId; ++i)
    {
        delete mPixelHeapElements[i];
    }
    mPixelHeapElements.clear();
    mCurrentPixelHeapElementId = -1;

    return;
}

void
HxPropagateContours::updateMaxima(const McVec3i& position,
                                  const float& value,
                                  const float& currentLabel)
{
    if (maximalPoints.at((int)(currentLabel - 1.f)).pixelValue < value)
    {
        maximalPoints.at((int)(currentLabel - 1.f)).pixelValue = value;
        maximalPoints.at((int)(currentLabel - 1.f)).position = position;
    }
    return;
}

void
HxPropagateContours::processCurrentElement(float currentLabel,
                                           std::vector<PixelHeapElement*>& newFront,
                                           std::vector<PixelHeapElement*>& nhood)
{
    std::vector<PixelHeapElement*>::iterator n_it = nhood.begin();
    std::vector<PixelHeapElement*>::iterator n_it_end = nhood.end();
    for (; n_it != n_it_end; ++n_it)
    {
        int i = (*n_it)->position.i;
        int j = (*n_it)->position.j;
        int k = (*n_it)->position.k;

        float neighbourFront;
        mFrontField->lattice().eval(i, j, k, &neighbourFront);

        float neighbourLabel;
        mOutputLabelField->lattice().eval(i, j, k, &neighbourLabel);

        float neighbourValue;
        mInputField->lattice().eval(i, j, k, &neighbourValue);

        if ((neighbourFront == 1.0f) &&
            (neighbourLabel != 0.0f))
            mFrontField->lattice().set(i, j, k, &(notFront));

        // this is an unlabeled voxel, should label it, and add it to front and heap
        if ((neighbourFront == 0.0f) && (neighbourLabel == 0.0f))
        {
            if (whenStop && (neighbourValue <= thresholdTP))
                continue;

            mFrontField->lattice().set(i, j, k, &(setFront));
            mOutputLabelField->lattice().set(i, j, k, &currentLabel);
            pushNeighbour(i, j, k, currentLabel, newFront);
        }

        makePixelHeapElementAvailableForReuse(*n_it);
    }
}

void
HxPropagateContours::analyzeNeighbours(bool& touchPointReached,
                                       bool& thresholdReached,
                                       std::vector<PixelHeapElement*>& nhood,
                                       PixelHeapElement* elem)
{
    std::vector<PixelHeapElement*>::iterator nh_it = nhood.begin();
    std::vector<PixelHeapElement*>::iterator nh_it_end = nhood.end();
    for (; nh_it != nh_it_end; ++nh_it)
    {
        float neighbourLabel = (*nh_it)->label;
        if ((neighbourLabel != 0.0f) &&
            (neighbourLabel != elem->label))
            touchPointReached = true;
    }

    if (touchPointReached)
    {
        if (elem->pixelValue < thresholdTP)
            thresholdReached = true;
    }

    return;
}

void
HxPropagateContours::initContours()
{
    initPointList();

    for (int i = 0; i < static_cast<int>(mPointListWorld.size()); i++)
    {
        float value;
        McVec3i pt = mPointListGrid[i];
        float lbl = float(i + 1);

        mInputField->lattice().eval(pt[0], pt[1], pt[2], &value);
        mOutputLabelField->lattice().set(pt[0], pt[1], pt[2], &lbl);
        mFrontField->lattice().set(pt[0], pt[1], pt[2], &(setFront));

        const float modulatedValue = getModulatedValue(pt[0], pt[1], pt[2], i, value);
        PixelHeapElement startingPt(pt, value, modulatedValue, lbl);

        std::vector<PixelHeapElement*> nhood;
        getNeighbours(pt, lbl, nhood);
        labelNeighbours(&lbl, nhood);

        addNeighboursToHeap(nhood);

        maximalPoints.push_back(startingPt);
    }

    return;
}

void
HxPropagateContours::initContoursFromSeedField()
{
    const McDim3l& dims = mInputLabelField->lattice().getDims();

    for (mclong k = 0; k < dims[2]; ++k)
    {
        for (mclong j = 0; j < dims[1]; ++j)
        {
            for (mclong i = 0; i < dims[0]; ++i)
            {
                float lbl;
                mInputLabelField->lattice().eval(i, j, k, &lbl);

                if (lbl == 0.0f)
                {
                    mFrontField->lattice().set(i, j, k, &(notFront));
                    continue;
                }
                // found a non-0 label
                float value;
                McVec3i pt(i, j, k);
                mInputField->lattice().eval(i, j, k, &value);

                const float modulatedValue = getModulatedValue(i, j, k, lbl, value);
                PixelHeapElement startingPt(pt, value, modulatedValue, lbl);

                std::vector<PixelHeapElement*> nhood;
                getNeighbours(pt, (int)lbl, nhood);

                mFrontField->lattice().set(i, j, k, &(setFront));

                // if one of neighbours is 0 then this voxel is on a border
                std::vector<PixelHeapElement*>::iterator nh_it = nhood.begin();
                std::vector<PixelHeapElement*>::iterator nh_it_end = nhood.end();
                for (; nh_it != nh_it_end; ++nh_it)
                {
                    if ((*nh_it)->label == 0.0f)
                    {
                        labelNeighbours(&lbl, nhood);
                        addNeighboursToHeap(nhood);
                        break;
                    }
                }
            }
        }
    }
    return;
}

void
HxPropagateContours::labelNeighbours(const float* label,
                                     std::vector<PixelHeapElement*>& nhood)
{
    std::vector<PixelHeapElement*>::iterator n_it = nhood.begin();
    std::vector<PixelHeapElement*>::iterator n_it_end = nhood.end();
    for (; n_it != n_it_end; ++n_it)
    {
        mOutputLabelField->lattice().set((*n_it)->position.i,
                                         (*n_it)->position.j,
                                         (*n_it)->position.k,
                                         label);
        (*n_it)->label = *label;
    }

    return;
}

void
HxPropagateContours::getNeighbours(const McVec3i& pt,
                                   const int lbl,
                                   std::vector<PixelHeapElement*>& nhood)
{
    const McDim3l& dims = mOutputLabelField->lattice().getDims();

    if (pt[0] < ((mclong)dims[0]) - 1)
        pushNeighbour(pt[0] + 1, pt[1], pt[2], lbl, nhood);
    if (pt[0] > 0)
        pushNeighbour(pt[0] - 1, pt[1], pt[2], lbl, nhood);
    if (pt[1] < ((mclong)dims[1]) - 1)
        pushNeighbour(pt[0], pt[1] + 1, pt[2], lbl, nhood);
    if (pt[1] > 0)
        pushNeighbour(pt[0], pt[1] - 1, pt[2], lbl, nhood);
    if (pt[2] < ((mclong)dims[2]) - 1)
        pushNeighbour(pt[0], pt[1], pt[2] + 1, lbl, nhood);
    if (pt[2] > 0)
        pushNeighbour(pt[0], pt[1], pt[2] - 1, lbl, nhood);

    return;
}

void
HxPropagateContours::addNeighboursToHeap(std::vector<PixelHeapElement*>& nhood)
{
    std::vector<PixelHeapElement*>::iterator nh_it = nhood.begin();
    std::vector<PixelHeapElement*>::iterator nh_it_end = nhood.end();
    for (; nh_it != nh_it_end; ++nh_it)
    {
        mHeap.insert(*nh_it);
    }

    return;
}

void
HxPropagateContours::makePixelHeapElementAvailableForReuse(
    PixelHeapElement* elem)
{
    if (mCurrentPixelHeapElementId < static_cast<long>(mPixelHeapElements.size()) - 1)
    {
        ++mCurrentPixelHeapElementId;
        mPixelHeapElements[mCurrentPixelHeapElementId] = elem;
    }
    else
    {
        ++mCurrentPixelHeapElementId;
        mPixelHeapElements.push_back(elem);
    }
}

PixelHeapElement*
HxPropagateContours::getPixelHeapElement()
{
    if (mPixelHeapElements.size() &&
        mCurrentPixelHeapElementId != -1)
    {
        PixelHeapElement* pixelElement = mPixelHeapElements[mCurrentPixelHeapElementId];
        --mCurrentPixelHeapElementId;
        return pixelElement;
    }
    else
    {
        return new PixelHeapElement();
    }
}

void
HxPropagateContours::pushNeighbour(int x, int y, int z, int parentLbl, std::vector<PixelHeapElement*>& nhood)
{
    PixelHeapElement* element = getPixelHeapElement();

    element->position.setValue(x, y, z);

    float value;
    mInputField->lattice().eval(x, y, z, &value);
    element->pixelValue = value;

    float lbl;
    mOutputLabelField->lattice().eval(x, y, z, &lbl);
    element->label = lbl;
    element->modulatedValue = getModulatedValue(x, y, z, (int)parentLbl - 1, value);
    nhood.push_back(element);

    return;
}

void
HxPropagateContours::update()
{
    if (portData.isNew())
    {
        mInputField = hxconnection_cast<HxUniformScalarField3>(portData);
        getFieldInfo();
        initPortPointList();
        initThreshold();
    }

    if (portPointSize.isNew())
    {
        portPointList->setPointSize(portPointSize.getValue());
    }

    if (portLabelSeedField.isNew())
    {
        initLabelField();
    }

    if (!portLabelSeedField.getSource())
    {
        portSeedRadio.setValue(0);
        portSeedRadio.setEnabled(false);

        portPointList->setEnabled(true);
        seedMode = true;
    }

    if (portLabelSeedField.getSource())
    {
        portSeedRadio.setEnabled(true);
    }

    if (portSeedRadio.isNew())
    {
        if (portSeedRadio.getValue() == 0)
        {
            portPointList->setEnabled(true);
            portPointSize.setEnabled(true);
            seedMode = true;
            portMode.show();
        }

        if (portSeedRadio.getValue() == 1)
        {
            portPointList->setEnabled(false);
            portPointSize.setEnabled(false);
            seedMode = false;
            portMode.hide();
        }
    }

    if (portWhenStop.isNew())
    {
        if (portWhenStop.getValue() == 0)
        {
            portStopThreshold.setEnabled(true);
            whenStop = true;
        }

        if (portWhenStop.getValue() == 1)
        {
            portStopThreshold.setEnabled(false);
            whenStop = false;
        }
    }

    portPointList->showPoints();
}

bool
HxPropagateContours::getSeedMode()
{
    return seedMode;
}

int
HxPropagateContours::keyboardEventCallback(
    const SoEvent* event,
    HxViewer* viewer,
    void* userData)
{
    if (static_cast<HxObject*>(userData)->getTypeId() !=
        HxPropagateContours::getClassTypeId())
        return 0;

    HxPropagateContours* propagateContours =
        static_cast<HxPropagateContours*>(userData);

    if (!propagateContours || !propagateContours->getFlag(IS_SELECTED))
        return 0;

    if (SO_KEY_PRESS_EVENT(event, A))
    {
        if (!propagateContours->getSeedMode()) // if seeding from label field, do nothing
            return 0;

        propagateContours->portPointList->appendPoint();
        return 1;
    }
    if (SO_KEY_PRESS_EVENT(event, R))
    {
        if (!propagateContours->getSeedMode())
            return 0;

        int idx = propagateContours->portPointList->getCurrent();
        propagateContours->portPointList->removePoint(idx);
        return 1;
    }

    return 0;
}

HxPropagateContours::~HxPropagateContours()
{
    theController->removeEventCallback(&keyboardEventCallback, this);
    portPointList = NULL;
}
