#include "HxTouchPointGraphBuilder.h"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include <hxcore/internal/HxWorkArea.h>
#include <hxcore/HxMessage.h>
#include <hxcore/HxController.h>

#include <Inventor/events/SoEvents.h>

// some helpful constant values
const float HxTouchPointGraphBuilder::setFront = 1.0f;
const float HxTouchPointGraphBuilder::notFront = 0.0f;

HX_INIT_CLASS(HxTouchPointGraphBuilder, HxCompModule)

// constructor
HxTouchPointGraphBuilder::HxTouchPointGraphBuilder() :
    HxCompModule(HxLattice3::getClassTypeId()),
    portWhenStop        (this, "StopAt", tr("Stop At"), 2),
    portStopThreshold   (this, "ScalarThreshold", tr("Scalar Threshold")),
    portDisplayThreshold(this, "DisplayThreshold", tr("Display Threshold")),
    portPointList       (new HxPort3DPointList(this, "pointList")),
    portPointSize       (this, "pointSize", tr("Point Size")),
    portAction          (this, "action", tr("Action"))
{
    this->portAction.setLabel(0, "DoIt");

    portPointList->setNumPoints(0, 0, -1);

    this->portStopThreshold.setMinMax(0.0f, 1.0f);
    this->portStopThreshold.setValue(0.5f);
    
    this->portDisplayThreshold.setMinMax(0.0f, 1.0f);
    this->portDisplayThreshold.setValue(0.5f);

    this->portPointSize.setMinMax(0.0f, 1.0f);
    this->portPointSize.setValue(0.5f);

    this->portWhenStop.setLabel(0, "Threshold");
    this->portWhenStop.setLabel(1, "First Touching Point");

    theController->addEventCallback(&keyboardEventCallback, this);
}

// Sets the max, min and middle values of the slider port
void
HxTouchPointGraphBuilder::initThreshold(HxPortFloatSlider * slider)
{
    HxUniformScalarField3 *inputField =
        hxconnection_cast<HxUniformScalarField3>(portData);

    if ( ! inputField )
        return;

    float maxVal = 1.0f, minVal = 0.0f;
    inputField->getRange(minVal, maxVal);

    slider->setMinMax(minVal, maxVal);
    slider->setValue((minVal + maxVal) * 0.5f);
    return;
}

// get BBox and voxel size of input field
void
HxTouchPointGraphBuilder::getFieldInfo()
{
    HxUniformScalarField3 *inputField =
        hxconnection_cast<HxUniformScalarField3>(portData);

    if ( ! inputField )
        return;

    const McDim3l& dims = inputField->lattice().getDims();
    for ( int i=0; i<3; ++i )
        m_dims[i] = dims[i];

    const McBox3f& bbox = inputField->getBoundingBox();
    for ( int i=0; i<6; ++i )
        m_bbox[i] = bbox[i];

    m_voxelSize[0] = (m_bbox[1] - m_bbox[0]) / (m_dims[0] - 1 );
    m_voxelSize[1] = (m_bbox[3] - m_bbox[2]) / (m_dims[1] - 1 );
    m_voxelSize[2] = (m_bbox[5] - m_bbox[4]) / (m_dims[2] - 1 );
}

// set initial values for the points
void
HxTouchPointGraphBuilder::initPointList()
{
    int numClipped = 0;
    for(int i = 0; i < portPointList->getNumPoints(); i++)
    {
        SbVec3d pt = portPointList->getCoord(i);
        if ((pt[0] < m_bbox[0]) || (pt[0] > m_bbox[1]) ||
            (pt[1] < m_bbox[2]) || (pt[1] > m_bbox[3]) ||
            (pt[2] < m_bbox[4]) || (pt[2] > m_bbox[5]))
            numClipped++;
    }

    if(numClipped)
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
HxTouchPointGraphBuilder::selectPoint(
    const int pointId)
{
    portPointList->selectPoint(pointId);
}

// get the world coordinates for the points (float)
McVec3f
HxTouchPointGraphBuilder::getWorldLocation(
    const int pointId)
{
    SbVec3d p = portPointList->getCoord(pointId);

    McVec3f location((float)p[0], (float)p[1], (float)p[2]);

    return location;
}

// get the lattice coordinates for the points (integer)
McVec3i
HxTouchPointGraphBuilder::getLatticeLocation(
    const int pointId)
{
    SbVec3d p = portPointList->getCoord(pointId);

    McVec3i locationLattice;
    locationLattice[0] = mculong(((p[0] - m_bbox[0]) / m_voxelSize[0]) + 0.5);
    locationLattice[1] = mculong(((p[1] - m_bbox[2]) / m_voxelSize[1]) + 0.5);
    locationLattice[2] = mculong(((p[2] - m_bbox[4]) / m_voxelSize[2]) + 0.5);

    return locationLattice;
}

// get the world coordinates for an integer point
McVec3f
HxTouchPointGraphBuilder::convertToWorldLocation(
    McVec3i & p)
{
    McVec3f locationWorld;

    locationWorld[0] =(float)p[0]* m_voxelSize[0] + m_bbox[0];
    locationWorld[1] =(float)p[1]* m_voxelSize[1] + m_bbox[2];
    locationWorld[2] =(float)p[2]* m_voxelSize[2] + m_bbox[4];

    return locationWorld;
}

void
HxTouchPointGraphBuilder::compute()
{
    if (!portData.getSource())
    {
        theMsg->printf("Source image has not been specified");
        return;
    }

    if (!portAction.wasHit()) return;

    mInputField = hxconnection_cast<HxUniformScalarField3>(portData);

    if (!mInputField)  return;
    
    this->cleanUpComputation();

    this->initCompute();

    this->thresholdTP = this->portStopThreshold.getValue();

    theWorkArea->busy();
    theWorkArea->startWorking("Propagating contours");

    this->propagate();

    theWorkArea->stopWorking();
    theWorkArea->notBusy();
    
    this->connectGraph();

    mOutputLabelField->composeLabel(portData.getSource()->getLabel(), "labels");
    mOutputLabelField->touchMinMax();
    setResult(this->resultIdField, mOutputLabelField);
    setResult(this->resultIdGraph, this->graph);
    return;
}

void
HxTouchPointGraphBuilder::cleanUpComputation()
{
    this->startingPoints.clear();
    this->maximalPoints.clear();
    this->touchingPoints.clear();

    this->startingIdx.clear();
    this->maximalIdx.clear();
    this->touchingIdx.clear();

    return;
 }

void
HxTouchPointGraphBuilder::initCompute()
{
    this->initFields();
    this->initContours();
    this->initGraph();

    return;
}

void
HxTouchPointGraphBuilder::printVector(std::vector<PixelHeapElement> & points)
{
    std::vector<PixelHeapElement>::iterator p_it     = points.begin();
    std::vector<PixelHeapElement>::iterator p_it_end = points.end();

    for(; p_it != p_it_end; ++p_it)
    {
        this->printPixelHeapElement(&*p_it);
    }
    return;
}

void
HxTouchPointGraphBuilder::printPixelHeapElement(PixelHeapElement * elem)
{
    theMsg->stream()
        << elem->label << " value: "
        << elem->pixelValue << " at "
        << elem->position.i << ", "
        << elem->position.j << ", "
        << elem->position.k << std::endl;
    return;
}

void
HxTouchPointGraphBuilder::initFields()
{
    mOutputLabelField =
        dynamic_cast<HxUniformScalarField3*>(getResult(this->resultIdField));

    const McDim3l& dims = mInputField->lattice().getDims();
    const McBox3f& bbox = mInputField->getBoundingBox();

    if (!mOutputLabelField ||
        mOutputLabelField->lattice().primType() != McPrimType::MC_UINT8)
    {
        mOutputLabelField = new HxUniformScalarField3(dims, McPrimType::MC_UINT8);
    }
    else
    {
        mOutputLabelField->lattice().resize(dims);
    }
    mOutputLabelField->lattice().setBoundingBox(bbox);

    float background = 0.f;
    for (mclong k=0 ; k < dims[2] ; ++k)
    {
        for (mclong j=0 ; j < dims[1] ; ++j)
        {
            for (mclong i=0 ; i < dims[0] ; ++i)
            {
                mOutputLabelField->lattice().set(i, j, k, &background);
            }
        }
    }

    // the front values are 0 too
    mFrontField   = mOutputLabelField->duplicate();
    return;
}

void
HxTouchPointGraphBuilder::propagate()
{
    PixelHeapElement* elem = mHeap.getMin();

    int processedElems = 0;

    mWatch.start();
    mLastTime = mWatch.getTime();

    while (elem) // while the heap is not empty
    {
        processedElems++;
        const float newTime = mWatch.getTime();
        if ( newTime - mLastTime > 1.f )
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
        this->getNeighbours(pt, nhood);

        // take a look at the neighbours
        bool touchPointReached = false;
        bool thresholdReached = false;

        this->analyzeNeighbours(touchPointReached, thresholdReached, nhood, elem);

        if ((thresholdReached) && (whenStop))
            break;

        if ((touchPointReached) && (!whenStop))
            break;

        // process current element
        float currentLabel;
        this->mOutputLabelField->lattice().eval(pt[0], pt[1], pt[2], &currentLabel);
        std::vector<PixelHeapElement*> newFront;

        this->processCurrentElement(currentLabel, newFront, nhood);

        // keep track of maximal values
        this->updateMaxima(elem->position, elem->pixelValue, currentLabel);

        // voxel is no longer front
        this->mFrontField->lattice().set(pt[0], pt[1], pt[2], &(this->notFront));

        this->addNeighboursToHeap(newFront);

        delete elem;
        nhood.clear();
        newFront.clear();

        elem = mHeap.getMin();
    }

    theMsg->printf("elapsed time: %f seconds", mWatch.stop());

    this->cleanUp();
}

void
HxTouchPointGraphBuilder::cleanUp()
{
    PixelHeapElement* elem = mHeap.getMin();
    int processedElems = 0;

    while (elem) // while the heap is not empty
    {
        processedElems++;
        if ( processedElems % 100 == 0 )
            theWorkArea->setProgressInfo(QString("Cleaning up heap: %1 ").arg(processedElems));

        mHeap.deleteElem(elem);
        delete elem;
        elem = mHeap.getMin();
    }

    return;
}

void
HxTouchPointGraphBuilder::updateMaxima(const McVec3i & position,
                                  const float & value,
                                  const float & currentLabel)
{
    if (maximalPoints.at((int)(currentLabel - 1.f)).pixelValue < value)
    {
        maximalPoints.at((int)(currentLabel - 1.f)).pixelValue = value;
        maximalPoints.at((int)(currentLabel - 1.f)).position   = position;
    }
    return;
}

void
HxTouchPointGraphBuilder::processCurrentElement(float currentLabel,
                                           std::vector<PixelHeapElement*> & newFront,
                                           std::vector<PixelHeapElement*> & nhood)
{
    std::vector<PixelHeapElement*>::iterator n_it = nhood.begin();
    std::vector<PixelHeapElement*>::iterator n_it_end = nhood.end();
    for(; n_it != n_it_end; ++n_it)
    {
        int i = (*n_it)->position.i;
        int j = (*n_it)->position.j;
        int k = (*n_it)->position.k;

        float neighbourFront;
        this->mFrontField->lattice().eval(i, j, k, &neighbourFront);

        float neighbourLabel;
        this->mOutputLabelField->lattice().eval(i, j, k, &neighbourLabel);

        float neighbourValue;
        this->mInputField->lattice().eval(i, j, k, &neighbourValue);

        if((neighbourFront == 1.0f) &&
           (neighbourLabel != 0.0f))
            this->mFrontField->lattice().set(i, j, k, &(this->notFront));

        // this is an unlabeled voxel, should label it, and add it to front and heap
        if ((neighbourFront == 0.0f) && (neighbourLabel == 0.0f))
        {
            if ((whenStop) && (neighbourValue <= this->thresholdTP))
                continue;

            this->mFrontField->lattice().set(i, j, k, &(this->setFront));
            this->mOutputLabelField->lattice().set(i, j, k, &currentLabel);
            this->pushNeighbour(i, j, k, newFront);
        }

        delete (*n_it);
    }
}

void
HxTouchPointGraphBuilder::analyzeNeighbours(bool & touchPointReached,
                                       bool & thresholdReached,
                                       std::vector<PixelHeapElement*> & nhood,
                                       PixelHeapElement * elem)
{
    std::vector<PixelHeapElement*>::iterator nh_it     = nhood.begin();
    std::vector<PixelHeapElement*>::iterator nh_it_end = nhood.end();
    for(; nh_it != nh_it_end; ++nh_it)
    {
        float neighbourLabel = (*nh_it)->label;
        if((neighbourLabel != 0.0f) &&
           (neighbourLabel != elem->label))
            elem->labels.push_back(neighbourLabel);
    }

    if(elem->labels.size() > 0)
    {
        touchPointReached = true;
        
        this->touchingPoints.push_back(*elem);

        if(elem->pixelValue < this->thresholdTP)
            thresholdReached = true;
    }

    return;
}

void
HxTouchPointGraphBuilder::initContours()
{
    int numPoints = portPointList->getNumPoints();

    for(int i = 0; i < numPoints; i++)
    {
        float value;
        McVec3i pt    = this->getLatticeLocation(i);
        float lbl     = float(i + 1);

        this->mInputField        ->lattice().eval(pt[0], pt[1], pt[2], &value);
        this->mOutputLabelField  ->lattice().set(pt[0], pt[1], pt[2], &lbl);
        this->mFrontField        ->lattice().set(pt[0], pt[1], pt[2], &(this->setFront));

        PixelHeapElement startingPt(pt, value, lbl);
        
        this->startingPoints.push_back(startingPt);
        this->maximalPoints.push_back(startingPt);

        std::vector<PixelHeapElement*> nhood;
        this->getNeighbours(pt, nhood);
        this->labelNeighbours(&lbl, nhood);

        this->addNeighboursToHeap(nhood);
    }

    return;
}

void
HxTouchPointGraphBuilder::initGraph()
{
    this->graph = dynamic_cast<HxSpatialGraph*>(getResult(this->resultIdGraph));

    if(!(this->graph))
        this->graph = HxSpatialGraph::createInstance();

    this->graph->clear();

    this->graph->composeLabel(portData.getSource()->getLabel(), "path graph");

    this->valueAttrib = static_cast<EdgeVertexAttribute*>(
        this->graph->addAttribute("Value", HxSpatialGraph::VERTEX, McPrimType::MC_FLOAT, 1));

    // Currently only mc_float and mc_int32 are supported <---- HxSpatialGraph.cpp
    this->idAttrib = static_cast<EdgeVertexAttribute*>(
        this->graph->addAttribute("Id", HxSpatialGraph::VERTEX, McPrimType::MC_INT32, 1));

    this->addPointsToGraph(this->startingPoints, this->startingIdx, this->idStarting);

    return;
}

void
HxTouchPointGraphBuilder::addPointsToGraph(std::vector<PixelHeapElement> & points,
                                      std::vector<int> & indexes,
                                      const int id)
{
    std::vector<PixelHeapElement>::iterator p_it     = points.begin();
    std::vector<PixelHeapElement>::iterator p_it_end = points.end();

    for(; p_it != p_it_end; ++p_it)
    {
        int index;
        this->addIntPointToGraph(p_it->position, index, id);
        indexes.push_back(index);
    }
    return;
}

void
HxTouchPointGraphBuilder::connectGraph()
{
    this->addPointsToGraph(this->maximalPoints, this->maximalIdx, this->idMax);

    this->addEdges(this->startingIdx, this->maximalIdx);

    int processedPoints = 0;
    int numberPoints = touchingPoints.size();

    // connect each touchpoint with the starting points that are touching at it
    std::vector<PixelHeapElement>::iterator tp_it     = touchingPoints.begin();
    std::vector<PixelHeapElement>::iterator tp_it_end = touchingPoints.end();
    for(; tp_it != tp_it_end; ++tp_it)
    {
        processedPoints++;
        if ( processedPoints % 100 == 0 )
            theWorkArea->setProgressInfo(QString("Connecting points: %1 / %2").arg(processedPoints).arg(numberPoints));

        if(tp_it->pixelValue < this->portDisplayThreshold.getValue())
            continue;

        int index = 0;
        this->addIntPointToGraph(tp_it->position, index, this->idTouching);

        this->graph->addEdge(
            this->startingIdx.at(int(tp_it->label) - 1), index);

        for(mculong i = 0; i < tp_it->labels.size(); i++)
        {
            this->graph->addEdge(this->startingIdx.at(int(tp_it->labels.at(i)) - 1), index);
        }
    }

    return;
}

void
HxTouchPointGraphBuilder::addEdges(std::vector<int> & sources, 
                              std::vector<int> & targets)
{
    if(sources.size() != targets.size())
        return;
    if((sources.size() == 0) || (targets.size() == 0))
        return;
    for(mculong i = 0; i < sources.size(); i++)
    {
        this->graph->addEdge(sources.at(i), targets.at(i));
    }
    return;
}

void
HxTouchPointGraphBuilder::addIntPointToGraph(McVec3i & pt, int & index, const int id)
{
    float value;

    McVec3f vert = this->convertToWorldLocation(pt);
    index = this->graph->addVertex(vert);
    this->mInputField->lattice().eval(pt.i, pt.j, pt.k, &value);
    this->valueAttrib->setFloatDataAtIdx(index, value);
    this->idAttrib->setIntDataAtIdx(index, id);

    return;
}

void
HxTouchPointGraphBuilder::labelNeighbours(const float * label,
                                     std::vector<PixelHeapElement*> & nhood)
{
    std::vector<PixelHeapElement*>::iterator n_it = nhood.begin();
    std::vector<PixelHeapElement*>::iterator n_it_end = nhood.end();
    for(; n_it != n_it_end; ++n_it)
    {
        this->mOutputLabelField->lattice().set((*n_it)->position.i,
                                             (*n_it)->position.j,
                                             (*n_it)->position.k,
                                             label);
        (*n_it)->label = *label;
    }

    return;
}

void
HxTouchPointGraphBuilder::getNeighbours(const McVec3i & pt,
                                   std::vector<PixelHeapElement*> & nhood)
{
    const McDim3l& dims = this->mOutputLabelField->lattice().getDims();

    if (pt[0] < ((mclong)dims[0])-1)
        this->pushNeighbour(pt[0]+1, pt[1], pt[2], nhood);
    if (pt[0] > 0)
        this->pushNeighbour(pt[0]-1, pt[1], pt[2], nhood);
    if (pt[1] < ((mclong)dims[1])-1)
        this->pushNeighbour(pt[0], pt[1]+1, pt[2], nhood);
    if (pt[1] > 0)
        this->pushNeighbour(pt[0], pt[1]-1, pt[2], nhood);
    if (pt[2] < ((mclong)dims[2])-1)
        this->pushNeighbour(pt[0], pt[1], pt[2]+1, nhood);
    if (pt[2] > 0)
        this->pushNeighbour(pt[0], pt[1], pt[2]-1, nhood);

    return;
}

void
HxTouchPointGraphBuilder::addNeighboursToHeap(std::vector<PixelHeapElement*> & nhood)
{
    std::vector<PixelHeapElement*>::iterator nh_it = nhood.begin();
    std::vector<PixelHeapElement*>::iterator nh_it_end = nhood.end();
    for(; nh_it != nh_it_end; ++nh_it)
    {
        mHeap.insert(*nh_it);
    }

    return;
}

void
HxTouchPointGraphBuilder::pushNeighbour(int x, int y, int z,
                                   std::vector<PixelHeapElement*> & nhood)
{
    PixelHeapElement * element = new PixelHeapElement();

    element->position.setValue(x, y, z);

    float value;
    mInputField->lattice().eval(x, y, z, &value);
    element->pixelValue = value;

    float lbl;
    this->mOutputLabelField->lattice().eval(x, y, z, &lbl);
    element->label = lbl;

    nhood.push_back(element);

    return;
}

void
HxTouchPointGraphBuilder::update()
{
    if ( portData.isNew() )
    {
        getFieldInfo();
        initPointList();
        initThreshold(&(this->portStopThreshold));
        initThreshold(&(this->portDisplayThreshold));
    }
    
    if(this->portDisplayThreshold.isNew())
    {
        this->initGraph();
        this->connectGraph();
        setResult(this->resultIdGraph, this->graph);
     }

    if(this->portPointSize.isNew())
    {
        this->portPointList->setPointSize(this->portPointSize.getValue());
    }

    if(this->portWhenStop.isNew())
    {
        if(this->portWhenStop.getValue() == 0)
        {
            this->portStopThreshold.setEnabled(true);
            this->whenStop = true;
        }

        if(this->portWhenStop.getValue() == 1)
        {
            this->portStopThreshold.setEnabled(false);
            this->whenStop = false;
        }
    }

    portPointList->showPoints();
}

int
HxTouchPointGraphBuilder::keyboardEventCallback(
    const SoEvent* event,
    HxViewer* viewer,
    void* userData)
{
    if ( static_cast<HxObject*>(userData)->getTypeId() != 
         HxTouchPointGraphBuilder::getClassTypeId() )
        return 0;

    HxTouchPointGraphBuilder * graphBuilder =
        static_cast<HxTouchPointGraphBuilder*>(userData);

    if ( ! graphBuilder || ! graphBuilder->getFlag( IS_SELECTED ) )
        return 0;

    if ( SO_KEY_PRESS_EVENT(event, A))
    {
        graphBuilder->portPointList->appendPoint();
        return 1;
    }
    if ( SO_KEY_PRESS_EVENT(event, R))
    {
        int idx = graphBuilder->portPointList->getCurrent();
        graphBuilder->portPointList->removePoint(idx);
        return 1;
    }

    return 0;
}

HxTouchPointGraphBuilder::~HxTouchPointGraphBuilder()
{
    theController->removeEventCallback(&keyboardEventCallback, this);
    portPointList = NULL;
}
