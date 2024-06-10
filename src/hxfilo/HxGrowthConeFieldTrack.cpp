#include "HxGrowthConeFieldTrack.h"
#include "FilopodiaFunctions.h"
#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <hxdistmap/internal/HxComputeDistanceMap.h>
#include <hxfield/HxUniformLabelField3.h>
#include <mclib/McException.h>
#include <mclib/internal/McRandom.h>
#include <hxcore/internal/HxReadAmiraMesh.h>
#include <hxcore/HxObjectPool.h>
#include <QDir>
#include <QDebug>

#include "ConvertVectorAndMcDArray.h""

HX_INIT_CLASS(HxGrowthConeFieldTrack,HxCompModule)

HxGrowthConeFieldTrack::HxGrowthConeFieldTrack() :
    HxCompModule(HxUniformLabelField3::getClassTypeId()),
    portInputDir(this, tr("inputDirectory"), tr("Input Directory"), HxPortFilename::LOAD_DIRECTORY),
    portOutputDir(this, tr("outputDirectory"), tr("Output Directory"), HxPortFilename::SAVE_DIRECTORY),
    portOutputGraph(this, tr("saveCentersAs"), tr("Save centers as"), HxPortFilename::ANY_FILE),
    portMinOverlapFraction(this, tr("minOverlapFraction"), tr("Min. overlap fraction"), 1),
    portAction(this, tr("action"), tr("Action"))
{
    portMinOverlapFraction.setMinMax(0.0f, 1.0f);
    portMinOverlapFraction.setValue(0.65f);
}

HxGrowthConeFieldTrack::~HxGrowthConeFieldTrack()
{
}

/// Used for either mapping: pair of (label in time step t, label in t+1) or
/// or relabeling: (old label in time step t, new label in time step t).
struct LabelPair {
    LabelPair(const int fir, const int sec) : first(fir), second(sec) {}

    bool operator==(const LabelPair& other) const {
        return (first == other.first && second == other.second);
    }

    bool operator<(const LabelPair& other) const {
        if (first != other.first) {
            return first < other.first;
        }
        else {
            return second < other.second;
        }

    }

    int first;
    int second;
};


typedef QHash<LabelPair, long>        OverlapStatistics; /// Number of overlapping voxels for each label combination
typedef QHash<int, OverlapStatistics> OverlapPerTimeStep; /// Above, key is time step
typedef QHash<int, long>              VolumePerLabel; /// Number of voxels for each label
typedef QHash<int, VolumePerLabel>    VolumePerLabelPerTimeStep; /// Above, key is time step
typedef QMap<int, QFileInfo>          FileInfoMap; /// Key is time step
typedef QHash<int, int>               LabelMapping; /// Correspondence between labels in one time step
typedef QHash<int, LabelMapping>      LabelMappingPerTimeStep; /// Above, key is time step


inline uint qHash(const LabelPair& key) {
     return qHash(key.first)^qHash(key.second);
}


QDebug operator<< (QDebug stream, const OverlapStatistics& stats) {
    OverlapStatistics::const_iterator i;
    for (i=stats.begin(); i!=stats.end(); ++i) {
        const LabelPair& pair = i.key();
        stream.nospace() <<  pair.first << " -> " << pair.second << " : " << i.value() << "\n";
    }
    return stream;
}

void printOverlapPercentages(const OverlapStatistics& overlaps,
                             const VolumePerLabel& volumesFirst,
                             const VolumePerLabel& volumesSecond)
{
    for (OverlapStatistics::const_iterator i=overlaps.begin(); i!=overlaps.end(); ++i) {
        const LabelPair& pair = i.key();
        if (pair.first != 0 && pair.second != 0) {
        qDebug().nospace() <<  pair.first << " -> " << pair.second << " : " << i.value() << "  :  "
            << float(i.value())/float(volumesFirst.value(pair.first)) << " % -> "
            << float(i.value())/float(volumesSecond.value(pair.second)) <<" %";
        }
    }
    qDebug() << "\n";
}


QDebug operator<< (QDebug stream, const VolumePerLabel& volumes) {
    VolumePerLabel::const_iterator i;
    for (i=volumes.begin(); i!=volumes.end(); ++i) {
        stream.nospace() <<  i.key() << " : " << i.value() << "\n";
    }
    return stream;
}


QDebug operator<< (QDebug stream, const LabelMapping& mapping) {
    for (LabelMapping::const_iterator it=mapping.begin(); it!=mapping.end(); ++it) {
        stream.nospace() <<  it.key() << " -> " << it.value() << "\n";
    }
    return stream;
}


/// Returns a map t -> QFileInfo: the input file for each time step.
FileInfoMap getInputFiles(const QString& inputDir) {
    QDir dir(inputDir);
    if (!dir.exists()) {
        throw McException(QString("Input directory does not exist"));
    }

    QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files);

    FileInfoMap map;
    for (int f=0; f<fileInfoList.size(); ++f) {
        const int time = FilopodiaFunctions::getTimeFromFileNameNoGC(fileInfoList[f].fileName());
        if (map.contains(time)) {
            throw McException(QString("Duplicate timestamp %1").arg(time));
        }
        map.insert(time, fileInfoList[f]);
    }
    const int firstTime = map.keys().first();
    const int lastTime = map.keys().last();

    const int missing = (lastTime-firstTime + 1) - map.size();
    if (missing != 0) {
        throw McException(QString("Missing %1 input labels fields").arg(missing));
    }

    return map;
}


/// Load label field and put it in object pool
HxUniformLabelField3* loadLabelField(const QString &fileName) {
    std::vector<HxData*> result(0);
    //TEMP
    auto tempResult = convertVector2McDArray(result);
    if (readAmiraMesh(fileName, tempResult)) {
        HxData* data = tempResult[0];
        if (data->isOfType(HxUniformLabelField3::getClassTypeId())) {
            return dynamic_cast<HxUniformLabelField3*>(data);
        }
        else {
            throw McException(QString("File is not a label field").arg(fileName));
        }
    }
    else {
        throw McException(QString("Cannot read label field (%1)").arg(fileName));
    }
}


/// Compute overlap between the different labels in two labelfields.
/// A label pair is a combination of a label in the first and in the second labelfield.
/// The overlap is the number of voxels where this combination occurs.
template<typename T1, typename T2> OverlapStatistics getOverlap(const T1* firstPtr,
                                                                const T2* secondPtr,
                                                                const McDim3l& dims)
{
    OverlapStatistics stats;

    mclong idx = 0;
    for (int z=0; z<dims[2]; ++z) {
        for (int y=0; y<dims[1]; ++y) {
            for (int x=0; x<dims[0]; ++x) {
                const int labelFirst  = int(firstPtr[idx]);
                const int labelSecond = int(secondPtr[idx]);

                if (labelFirst != 0 || labelSecond != 0) {
                    const LabelPair pair(labelFirst, labelSecond);
                    if (stats.contains(pair)) {
                        stats[pair] += 1;
                    }
                    else {
                        stats.insert(pair, 1);
                    }
                }
                ++idx;
            }
        }
    }
    return stats;
}


/// Compute the number of voxels for each label.
template<typename T> VolumePerLabel getVolumes(const T* ptr,
                                               const McDim3l& dims)
{
    VolumePerLabel volumes;

    mclong idx = 0;
    for (int z=0; z<dims[2]; ++z) {
        for (int y=0; y<dims[1]; ++y) {
            for (int x=0; x<dims[0]; ++x) {
                const int label = int(ptr[idx]);

                if (label != 0) {
                    if (volumes.contains(label)) {
                        volumes[label] += 1;
                    }
                    else {
                        volumes.insert(label, 1);
                    }
                }
                ++idx;
            }
        }
    }
    return volumes;
}


/// Returns a new label field, where the label for each object corresponds
/// to the label in the previous time step, based on the computed mapping.
template<typename T> HxUniformLabelField3* getRelabeledField(const T* ptr,
                                                             const McDim3l& dims,
                                                             const LabelMapping& mapping)
{
    HxUniformLabelField3* newLabels = new HxUniformLabelField3(McDim3l(dims[0], dims[1], dims[2]), McPrimType::MC_UINT8); //, mcuint8);
    mcuint8* newPtr = newLabels->labelLattice().getLabels<mcuint8>();
    mclong idx = 0;
    for (int z=0; z<dims[2]; ++z) {
        for (int y=0; y<dims[1]; ++y) {
            for (int x=0; x<dims[0]; ++x) {
                const int oldLabel = int(ptr[idx]);
                if (mapping.contains(oldLabel)) {
                    newPtr[idx] = mapping.value(oldLabel);
                }
                else {
                    newPtr[idx] = 0;
                }
                ++idx;
            }
        }
    }
    return newLabels;
}


typedef QHash<int, McVec3f> CenterPerLabel;


/// Computes a map labelId->Point representing the center of each label region.
CenterPerLabel getCentersPerLabel(HxUniformLabelField3* labelField) {
    McHandle<HxComputeDistanceMap> computeDistMap = HxComputeDistanceMap::createInstance();
    computeDistMap->portData.connect(labelField);
    computeDistMap->portChamferMetric.setValue(1);
    computeDistMap->portType.setValue(0); //Euclidian
    computeDistMap->portAction.hit();
    computeDistMap->fire();

    McHandle<HxUniformScalarField3> distMap = dynamic_cast<HxUniformScalarField3*>(computeDistMap->getResult());
    if (!distMap) {
        throw McException("Error computing distance field of label field");
    }

    float minVal, maxVal;
    labelField->getRange(minVal, maxVal);
    const int maxLabels = int(maxVal + 0.5) + 1;

    std::vector<std::vector<McVec3i> > pointsPerLabel(maxLabels);
    std::vector<float> maxValuePerLabel(maxLabels);
    std::fill(maxValuePerLabel.begin(),maxValuePerLabel.end(),std::numeric_limits<float>::min());

    const McDim3l& dims = distMap->lattice().getDims();

    float scalarVal;
    int label;

    mculong idx = 0;
    for (int z=0; z<dims[2]; ++z) {
        for (int y=0; y<dims[1]; ++y) {
            for (int x=0; x<dims[0]; ++x) {
                distMap->lattice().eval(x, y, z, &scalarVal);
                label = int(labelField->labelPtr()[idx]);

                if (scalarVal == maxValuePerLabel[label]) {
                    pointsPerLabel[label].push_back(McVec3i(x, y, z));
                }
                else if (scalarVal > maxValuePerLabel[label]) {
                    maxValuePerLabel[label] = scalarVal;
                    pointsPerLabel[label].clear();
                    pointsPerLabel[label].push_back(McVec3i(x, y, z));
                }
                ++idx;
            }
        }
    }

    McHandle<HxCoord3> coords = labelField->lattice().coords();

    CenterPerLabel centerPerLabel;
    for (int i=1; i<maxValuePerLabel.size(); ++i) {
        if (pointsPerLabel[i].size() > 0) {
            if (pointsPerLabel[i].size() >= 2) {
                theMsg->printf("Warning: multiple (%d) maxima for region %d. Using only first.", pointsPerLabel[i].size(), i);
            }

            const McVec3i& idx = pointsPerLabel[i][0];
            const McVec3f pos = coords->pos(idx);
            centerPerLabel.insert(i, pos);

            //qDebug() << "Max dist for label " << i << ":" << maxValuePerLabel[i];
        }
    }

    return centerPerLabel;
}


/// Adds the center of each label region to a SpatialGraph.
/// Each center gets a time stamp and growth cone number.
void addCentersToSpatialGraph(HxSpatialGraph* graph,
                              HxUniformLabelField3* labelField,
                              const int timeStep)
{
    const char* timeAttName = FilopodiaFunctions::getTimeStepAttributeName();
    HierarchicalLabels* timeLabels = graph->getLabelGroup(timeAttName);
    if (!timeLabels) {
        timeLabels = graph->addNewLabelGroup(timeAttName, true, true, false);
    }

    const char* gcAttName = FilopodiaFunctions::getGrowthConeAttributeName();
    HierarchicalLabels* gcLabels = graph->getLabelGroup(gcAttName);
    if (!gcLabels) {
        gcLabels = graph->addNewLabelGroup(gcAttName, false, true, false);
    }

    EdgeVertexAttribute* timeAtt = graph->findVertexAttribute(timeAttName);
    EdgeVertexAttribute* gcAtt = graph->findVertexAttribute(gcAttName);

    const QString timeLabelName = FilopodiaFunctions::getLabelNameFromTime(timeStep);
    int timeLabelId = timeLabels->getLabelIdFromName(McString(timeLabelName));
    if (timeLabelId == -1) {
        const SbColor color(McRandom::rand(), McRandom::rand(), McRandom::rand());
        timeLabelId = timeLabels->addLabel(0, qPrintable(timeLabelName), color);
    }

    const CenterPerLabel centers = getCentersPerLabel(labelField);
    for (CenterPerLabel::const_iterator it=centers.begin(); it!=centers.end(); ++it) {
        const int label = it.key();
        const QString gcLabelName = FilopodiaFunctions::getLabelNameFromGrowthConeNumber(label);
        int gcLabelId = gcLabels->getLabelIdFromName(gcLabelName);
        if (gcLabelId == -1) {
            const SbColor color(McRandom::rand(), McRandom::rand(), McRandom::rand());
            gcLabelId = gcLabels->addLabel(0, qPrintable(gcLabelName), color);
        }

        const int v = graph->addVertex(it.value());
        timeAtt->setIntDataAtIdx(v, timeLabelId);
        gcAtt->setIntDataAtIdx(v, gcLabelId);
    }
}

/// Creates new label field with consistent labeling of growth cones,
/// according to the computed mapping. The SpatialGraph containing the centers is computed
/// here as well to avoid have to read the label fields from disk again.
void createRelabeledFieldsAndSpatialGraph(const FileInfoMap& fileMap,
                                          const LabelMappingPerTimeStep& relabelingPerTimeStep,
                                          const QDir& outputDir,
                                          const QString& centerFileName)
{
    QStringList rootFileList = centerFileName.split("/");
    QString rootFileDir;

    if (rootFileList.size() > 1) {
        for (int i=0; i<rootFileList.size()-1;++i) {
            rootFileDir = rootFileDir + rootFileList[i] + "/";
        }
    } else {
        rootFileList = centerFileName.split("\\");
        for (int i=0; i<rootFileList.size()-1;++i) {
            rootFileDir = rootFileDir + rootFileList[i] + "\\";
        }
    }

    const QList<int> timeStamps = fileMap.keys();

    McHandle<HxSpatialGraph> centerGraph = HxSpatialGraph::createInstance();
    centerGraph->setLabel("GrowthConeCenters");

    for (int t=0; t<timeStamps.size(); ++t) {
        McHandle<HxUniformLabelField3> oldField = loadLabelField(fileMap.value(timeStamps[t]).absoluteFilePath());
        McPrimType T = oldField->labelLattice().primType();
        const McDim3l& dims = oldField->labelLattice().getDims();

        McHandle<HxUniformLabelField3> newField;
        if (T == McPrimType::MC_UINT8) {
            newField = getRelabeledField<mcuint8>(oldField->labelLattice().getLabels<mcuint8>(), dims, relabelingPerTimeStep.value(t));
        }
        else if (T == McPrimType::MC_UINT16) {
            newField = getRelabeledField<mcuint16>(oldField->labelLattice().getLabels<mcuint16>(), dims, relabelingPerTimeStep.value(t));
        }
        else if (T == McPrimType::MC_INT32) {
            newField = getRelabeledField<mcint32>(oldField->labelLattice().getLabels<mcint32>(), dims, relabelingPerTimeStep.value(t));
        }

        newField->lattice().setBoundingBox(oldField->getBoundingBox());

        QString basename = fileMap.value(timeStamps[t]).baseName();
        const int pos = basename.lastIndexOf("_t");
        if (pos == -1) {
            throw McException("Invalid filename: no timestep found");
        }
        basename.insert(pos, QString(".tracked"));
        const QString fileName = outputDir.absolutePath() + QDir::separator() + basename + ".am";

        //qDebug() << "Saving" << fileName;
        newField->writeAmiraMeshRLE(qPrintable(fileName));
        theObjectPool->removeObject(oldField);

        addCentersToSpatialGraph(centerGraph, newField, timeStamps[t]);
    }

    const char* timeAttName = FilopodiaFunctions::getTimeStepAttributeName();
    HierarchicalLabels* timeLabels = centerGraph->getLabelGroup(timeAttName);
    const int maxLabel = timeLabels->getMaxLabelId();
    const McDArray<int> childLabelIds = timeLabels->getChildIds(0);
    for (int i=0; i<childLabelIds.size(); ++i) {
        const float gb = float(childLabelIds[i])/float(maxLabel);
        const McColor color(1.0f, 1.0f - gb, 1.0f - gb);
        timeLabels->setLabelColor(childLabelIds[i], color);
    }

    // save rootNode

    const int numberOfGrowthCones = FilopodiaFunctions::getNumberOfGrowthCones(centerGraph);

    for (int n=1; n<numberOfGrowthCones+1; ++n) {

        const SpatialGraphSelection nodeSel = FilopodiaFunctions::getNodesWithGcId(centerGraph, n);
        const SpatialGraphSelection rootSel = FilopodiaFunctions::getNodesOfTypeInSelection(centerGraph, nodeSel, ROOT_NODE);

        McHandle<HxSpatialGraph> rootGraph = HxSpatialGraph::createInstance();
        rootGraph->setLabel(QString("rootNodes_gc%1").arg(n));

        rootGraph = centerGraph->getSubgraph(rootSel);

        FilopodiaFunctions::addTypeLabelAttribute(rootGraph);
        const char* typeAttName = FilopodiaFunctions::getTypeLabelAttributeName();
        EdgeVertexAttribute* typeAtt = rootGraph->findVertexAttribute(typeAttName);
        const int rootNodeLabel = FilopodiaFunctions::getTypeLabelId(rootGraph, ROOT_NODE);

        for (int v=0; v<rootGraph->getNumVertices(); ++v) {
            typeAtt->setIntDataAtIdx(v, rootNodeLabel);
        }

        theObjectPool->addObject(rootGraph);
        rootGraph->saveAmiraMeshASCII(qPrintable(QString(rootFileDir + "rootNodes_gc%1.am").arg(n)));
        rootGraph->saveAmiraMeshASCII(qPrintable(QString(rootFileDir + "resultTree_gc%1.am").arg(n)));

    }

    theObjectPool->addObject(centerGraph);
    centerGraph->saveAmiraMeshASCII(qPrintable(centerFileName));
}


/// Compute the label overlap and volume of the input label fields in one go,
/// to avoid having the read the label fields from disk twice.
void getOverlapAndLabelVolumePerTimeStep(const FileInfoMap& fileMap,
                                         OverlapPerTimeStep& overlaps,
                                         VolumePerLabelPerTimeStep& volumes)
{
    overlaps.clear();
    volumes.clear();

    const QList<int> timeStamps = fileMap.keys();

    for (int t=0; t<timeStamps.size()-1; ++t) {
        McHandle<HxUniformLabelField3> first = loadLabelField(fileMap.value(timeStamps[t]).absoluteFilePath());
        McHandle<HxUniformLabelField3> second = loadLabelField(fileMap.value(timeStamps[t+1]).absoluteFilePath());

        HxLabelLattice3& latFirst  = first->labelLattice();
        HxLabelLattice3& latSecond = second->labelLattice();

        const McDim3l& dimsFirst = latFirst.getDims();
        const McDim3l& dimsSecond = latSecond.getDims();

        if (dimsFirst != dimsSecond) {
            throw McException(QString("Dimension mismatch"));
        }

        McPrimType T1 = latFirst.primType();
        McPrimType T2 = latSecond.primType();

        if (T1 == McPrimType::MC_UINT8 && T2 == McPrimType::MC_UINT8) {
            const mcuint8* L1 = latFirst.getLabels<mcuint8>();
            const mcuint8* L2 = latSecond.getLabels<mcuint8>();
            overlaps.insert(t, getOverlap<mcuint8, mcuint8>(L1, L2, dimsFirst));
        }
        else if (T1 == McPrimType::MC_UINT8 && T2 == McPrimType::MC_UINT16) {
            const mcuint8* L1 = latFirst.getLabels<mcuint8>();
            const mcuint16* L2 = latSecond.getLabels<mcuint16>();
            overlaps.insert(t, getOverlap<mcuint8, mcuint16>(L1, L2, dimsFirst));
        }
        else if (T1 == McPrimType::MC_UINT8 && T2 == McPrimType::MC_INT32) {
            const mcuint8* L1 = latFirst.getLabels<mcuint8>();
            const mcint32* L2 = latSecond.getLabels<mcint32>();
            overlaps.insert(t, getOverlap<mcuint8, mcint32>(L1, L2, dimsFirst));
        }

        else if (T1 == McPrimType::MC_UINT16 && T2 == McPrimType::MC_UINT8) {
            const mcuint16* L1 = latFirst.getLabels<mcuint16>();
            const mcuint8* L2 = latSecond.getLabels<mcuint8>();
            overlaps.insert(t, getOverlap<mcuint16, mcuint8>(L1, L2, dimsFirst));
        }
        else if (T1 == McPrimType::MC_UINT16 && T2 == McPrimType::MC_UINT16) {
            const mcuint16* L1 = latFirst.getLabels<mcuint16>();
            const mcuint16* L2 = latSecond.getLabels<mcuint16>();
            overlaps.insert(t, getOverlap<mcuint16, mcuint16>(L1, L2, dimsFirst));
        }
        else if (T1 == McPrimType::MC_UINT16 && T2 == McPrimType::MC_INT32) {
            const mcuint16* L1 = latFirst.getLabels<mcuint16>();
            const mcuint32* L2 = latSecond.getLabels<mcuint32>();
            overlaps.insert(t, getOverlap<mcuint16, mcuint32>(L1, L2, dimsFirst));
        }


        else if (T1 == McPrimType::MC_INT32 && T2 == McPrimType::MC_UINT8) {
            const mcint32* L1 = latFirst.getLabels<mcint32>();
            const mcuint8* L2 = latSecond.getLabels<mcuint8>();
            overlaps.insert(t, getOverlap<mcint32, mcuint8>(L1, L2, dimsFirst));
        }
        else if (T1 == McPrimType::MC_INT32 && T2 == McPrimType::MC_UINT16) {
            const mcint32* L1 = latFirst.getLabels<mcint32>();
            const mcuint16* L2 = latSecond.getLabels<mcuint16>();
            overlaps.insert(t, getOverlap<mcint32, mcuint16>(L1, L2, dimsFirst));
        }
        else if (T1 == McPrimType::MC_INT32 && T2 == McPrimType::MC_INT32) {
            const mcint32* L1 = latFirst.getLabels<mcint32>();
            const mcint32* L2 = latSecond.getLabels<mcint32>();
            overlaps.insert(t, getOverlap<mcint32, mcint32>(L1, L2, dimsFirst));
        }
        else {
            throw McException(QString("Invalid label type"));
        }

        if (T1 == McPrimType::MC_UINT8) {
            const mcuint8* L1 = latFirst.getLabels<mcuint8>();
            volumes.insert(t, getVolumes<mcuint8>(L1, dimsFirst));
        }
        else if (T1 == McPrimType::MC_UINT16) {
            const mcuint16* L1 = latFirst.getLabels<mcuint16>();
            volumes.insert(t, getVolumes<mcuint16>(L1, dimsFirst));
        }
        else if (T1 == McPrimType::MC_INT32) {
            const mcint32* L1 = latFirst.getLabels<mcint32>();
            volumes.insert(t, getVolumes<mcint32>(L1, dimsFirst));
        }

        if (t == timeStamps.size() - 2) {
            if (T2 == McPrimType::MC_UINT8) {
                const mcuint8* L2 = latSecond.getLabels<mcuint8>();
                volumes.insert(t+1, getVolumes<mcuint8>(L2, dimsFirst));
            }
            else if (T2 == McPrimType::MC_UINT16) {
                const mcuint16* L2 = latSecond.getLabels<mcuint16>();
                volumes.insert(t+1, getVolumes<mcuint16>(L2, dimsFirst));
            }
            else if (T1 == McPrimType::MC_INT32) {
                const mcint32* L2 = latSecond.getLabels<mcint32>();
                volumes.insert(t+1, getVolumes<mcint32>(L2, dimsFirst));
            }
        }


        theObjectPool->removeObject(first);
        theObjectPool->removeObject(second);
    }
}


/// Compute the actual mapping between labels in different time steps.
/// Condition for correspondence is that the overlap between the label
/// regions is larger than a certain fraction of the total volume of the region.
LabelMapping getLabelMapping(const OverlapStatistics& overlaps,
                             const VolumePerLabel& volumesFirst,
                             const VolumePerLabel& volumesSecond,
                             const float fraction)
{
    LabelMapping mapping;

    QSet<int> labelsInFirst, labelsInSecond;
    QSet<int> mappedInFirst, mappedInSecond;

    for (OverlapStatistics::const_iterator it=overlaps.begin(); it!=overlaps.end(); ++it) {
        const LabelPair& pair = it.key();

        if (pair.first == 0 || pair.second == 0) {
            continue;
        }

        const int overlap = it.value();
        const int volumeFirst  = volumesFirst.value(pair.first);
        const int volumeSecond = volumesSecond.value(pair.second);

        labelsInFirst.insert(pair.first);
        labelsInSecond.insert(pair.second);

        if ((float(overlap) > fraction*float(volumeFirst)) &&
             float(overlap) > fraction*float(volumeSecond))
        {
            mapping.insert(pair.first, pair.second);
            mappedInFirst.insert(pair.first);
            mappedInSecond.insert(pair.second);
        }
    }

    for (QSet<int>::const_iterator it=labelsInFirst.begin(); it!=labelsInFirst.end(); ++it) {
        const int& labelInFirst = *it;
        if (!mappedInFirst.contains(labelInFirst)) {
            qDebug().nospace() << "Label " << labelInFirst << " could not be mapped (1st)";
        }
    }
    for (QSet<int>::const_iterator it=labelsInSecond.begin(); it!=labelsInSecond.end(); ++it) {
        const int& labelInSecond = *it;
        if (!mappedInSecond.contains(labelInSecond)) {
            qDebug().nospace() << "Label " << labelInSecond << " could not be mapped (2nd)";
        }
    }

    return mapping;
}


LabelMappingPerTimeStep getLabelMappingPerTimeStep(const OverlapPerTimeStep& overlaps,
                                                   const VolumePerLabelPerTimeStep& volumes,
                                                   const float fraction)
{
    LabelMappingPerTimeStep mappingPerTimeStep;
    for (OverlapPerTimeStep::const_iterator it=overlaps.begin(); it!=overlaps.end(); ++it) {
        const int t = it.key();
        const OverlapStatistics& stats = it.value();
        const VolumePerLabel& volumeFirst = volumes.value(t);
        const VolumePerLabel& volumeSecond = volumes.value(t+1);
        const LabelMapping mapping = getLabelMapping(stats, volumeFirst, volumeSecond, fraction);
        mappingPerTimeStep.insert(t, mapping);
    }
    return mappingPerTimeStep;
}


LabelMappingPerTimeStep getRelabelingPerTimeStep(const LabelMappingPerTimeStep& mappingPerTS) {
    LabelMappingPerTimeStep relabelingPerTimeStep;

    int nextUnusedLabel = 1;
    for (LabelMappingPerTimeStep::const_iterator it=mappingPerTS.begin(); it!=mappingPerTS.end(); ++it) {
        const int t = it.key();

        if (it == mappingPerTS.begin()) {
            LabelMapping relabelingFirst;
            const LabelMapping& mapping = it.value();
            for (LabelMapping::const_iterator mapIt=mapping.begin(); mapIt!=mapping.end(); ++mapIt) {
                const int oldLabel = mapIt.key();
                relabelingFirst.insert(oldLabel, nextUnusedLabel);
                qDebug() << "Starting new label" << nextUnusedLabel;
                ++nextUnusedLabel;
            }
            relabelingPerTimeStep.insert(t, relabelingFirst);
        }

        const LabelMapping& mapping = it.value();
        const LabelMapping& relabelingFirst = relabelingPerTimeStep.value(t);
        LabelMapping relabelingSecond;
        for (LabelMapping::const_iterator mapIt=mapping.begin(); mapIt!=mapping.end(); ++mapIt) {
            const int oldLabelFirst = mapIt.key();
            const int oldLabelSecond = mapIt.value();

            if (relabelingFirst.contains(oldLabelFirst)) {
                relabelingSecond.insert(oldLabelSecond, relabelingFirst.value(oldLabelFirst));
            }
            else {
                relabelingSecond.insert(oldLabelSecond, nextUnusedLabel);
                qDebug() << "Starting new label" << nextUnusedLabel;
                ++nextUnusedLabel;
            }
        }

        relabelingPerTimeStep.insert(t+1, relabelingSecond);
    }
    return relabelingPerTimeStep;
}


void HxGrowthConeFieldTrack::compute()
{
    if (portAction.wasHit()) {
        QDir outputDir(portOutputDir.getValue());
        if (!outputDir.exists()) {
            throw McException(QString("Output directory does not exist"));
        }

        FileInfoMap inputFiles = getInputFiles(portInputDir.getValue());

        OverlapPerTimeStep overlapsPerTimeStep;
        VolumePerLabelPerTimeStep volumesPerTimeStep;
        getOverlapAndLabelVolumePerTimeStep(inputFiles, overlapsPerTimeStep, volumesPerTimeStep);
        const float minOverlapFraction = portMinOverlapFraction.getValue();
        LabelMappingPerTimeStep mappingPerTimeStep = getLabelMappingPerTimeStep(overlapsPerTimeStep, volumesPerTimeStep, minOverlapFraction);
        LabelMappingPerTimeStep relabelingPerTimeStep = getRelabelingPerTimeStep(mappingPerTimeStep);

        /*
        const QList<int> timeSteps = inputFiles.keys();
        for (int t=0; t<timeSteps.size()-1; ++t) {
            qDebug().nospace() << "--------Mapping: " << t << " -> " << t+1 << "-------------\n";
            qDebug().nospace() << "Overlaps:\n" << overlapsPerTimeStep[t] << "\n";
            printOverlapPercentages(overlapsPerTimeStep[t], volumesPerTimeStep[t], volumesPerTimeStep[t+1]);
            qDebug().nospace() << "Volumes 1st:\n" << volumesPerTimeStep[t] << "\n";
            qDebug().nospace() << "Volumes 2nd:\n" << volumesPerTimeStep[t+1] << "\n";
            qDebug().nospace() << "Mapping:\n" << mappingPerTimeStep[t] << "\n";
            qDebug().nospace() << "Relabeling 1st:\n" << relabelingPerTimeStep[t] << "\n";
            qDebug().nospace() << "Relabeling 2nd:\n" << relabelingPerTimeStep[t+1] << "\n";
        }
        */

        const QString centerFileName = portOutputGraph.getValue();
        createRelabeledFieldsAndSpatialGraph(inputFiles, relabelingPerTimeStep, outputDir, centerFileName);
    }
}

