#include "HxSplitLabelField.h"
#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <hxfield/HxUniformLabelField3.h>
#include <hxfield/HxLoc3Regular.h>
#include <hxcore/HxMessage.h>
#include <QSetIterator>

HX_INIT_CLASS(HxSplitLabelField, HxCompModule)

HxSplitLabelField::HxSplitLabelField()
    : HxCompModule(HxUniformLabelField3::getClassTypeId())
    , portImage(this, "Image", tr("Image"), HxUniformScalarField3::getClassTypeId())
    , portSeeds(this, "Seeds", tr("Seeds"), HxSpatialGraph::getClassTypeId())
    , portBoundarySize(this, "BoundarySize", tr("Boundary size"), 1)
    , portImageBackground(this, "ImageBackground", tr("Image background"), 2)
    , portDoIt(this, "DoIt", tr("DoIt"), 1)
{
    portBoundarySize.setValue(1);

    portImageBackground.setLabel(KEEP_INTENSITY, "Keep intensity");
    portImageBackground.setLabel(SET_TO_BACKGROUND, "Set to background");
    portImageBackground.setValue(KEEP_INTENSITY);
}

HxSplitLabelField::~HxSplitLabelField()
{
}

void
HxSplitLabelField::update()
{

    const HxUniformScalarField3* image = hxconnection_cast<HxUniformScalarField3>(portImage);
    if (image)
    {
	 portImageBackground.show();
    }
    else
    {
        portImageBackground.hide();
    }
}

HxSplitLabelField::IntensityPolicy
HxSplitLabelField::getIntensityPolicy()
{
    return static_cast<IntensityPolicy>(portImageBackground.getValue());
}

void
crop(HxLabelLattice3* lat, HxLattice3* imageLat, const int boundarySize)
{
    const McDim3l& dims = lat->getDims();

    McVec3i min(dims[0] - 1, dims[1] - 1, dims[2] - 1);
    McVec3i max(0, 0, 0);

    float val;
    for (int z = 0; z < dims[2]; ++z)
    {
        for (int y = 0; y < dims[1]; ++y)
        {
            for (int x = 0; x < dims[0]; ++x)
            {
                lat->eval(x, y, z, &val);
                if (val > 0.0f)
                {
                    if (x < min[0])
                        min[0] = x;
                    if (y < min[1])
                        min[1] = y;
                    if (z < min[2])
                        min[2] = z;

                    if (x > max[0])
                        max[0] = x;
                    if (y > max[1])
                        max[1] = y;
                    if (z > max[2])
                        max[2] = z;
                }
            }
        }
    }

    min[0] = MC_MAX2(0, min[0] - boundarySize);
    min[1] = MC_MAX2(0, min[1] - boundarySize);
    min[2] = MC_MAX2(0, min[2] - boundarySize);

    max[0] = MC_MIN2(dims[0] - 1, max[0] + boundarySize);
    max[1] = MC_MIN2(dims[1] - 1, max[1] + boundarySize);
    max[2] = MC_MIN2(dims[2] - 1, max[2] + boundarySize);

    lat->crop(&min[0], &max[0], NULL);
    if (imageLat)
    {
        imageLat->crop(&min[0], &max[0], NULL);
    }
}

template <typename T>
QSet<T>
getLabelsToKeep(const McDim3l dims,
                HxLoc3Regular* location,
                const T* ptr,
                const HxSpatialGraph* seeds)
{
    QSet<T> labelValues;

    if (seeds)
    {
        for (int v = 0; v < seeds->getNumVertices(); ++v)
        {
            const McVec3f seedCoords = seeds->getVertexCoords(v);
            location->move(seedCoords);
            const McVec3i gridCoords(location->getIx(), location->getIy(), location->getIz());
            if (gridCoords.i < 0 || gridCoords.i >= dims.nx ||
                gridCoords.j < 0 || gridCoords.j >= dims.ny ||
                gridCoords.k < 0 || gridCoords.k >= dims.nz)
            {
                theMsg->printf("Invalid coords for vertex %d. Skipping.", v);
                continue;
            }
            const mclong index = dims.nx * dims.ny * gridCoords.k + dims.nx * gridCoords.j + gridCoords.i;
            T labelValue = ptr[index];
            if (labelValue == 0)
            {
                theMsg->printf("Vertex %d in exterior. Skipping.", v);
                continue;
            }
            labelValues.insert(labelValue);
        }
    }
    else
    {
        const mculong numVoxels = dims.nx * dims.ny * dims.nz;
        for (mculong i = 0; i < numVoxels; ++i)
        {
            const T v = ptr[i];
            if (v > 0)
            { // Exclude Exterior
                labelValues.insert(v);
            }
        }
    }

    return labelValues;
}

template <typename T>
std::vector<HxData*>
createFieldPerLabel(const McDim3l dims,
                    const QSet<T>& labelsToKeep,
                    const HxUniformLabelField3* inputLabels,
                    const HxUniformScalarField3* inputImage,
                    const HxSplitLabelField::IntensityPolicy intensityPolicy,
                    const int boundarySize)
{
    QSetIterator<T> it(labelsToKeep);

    const float zero = 0.0f;
    const mcint64 numVoxels = dims.nbVoxel();

    std::vector<HxData*> results;

    while (it.hasNext())
    {
        const T label = it.next();

        HxUniformLabelField3* outputLabels = inputLabels->duplicate();
        HxLabelLattice3* outputLabelLat = (HxLabelLattice3*)outputLabels->getInterface(HxLabelLattice3::getClassTypeId());
        T* ptr = (T*)outputLabelLat->getLabels();

        HxUniformScalarField3* outputImage = 0;
        HxLattice3* outputImageLat = 0;

        if (inputImage)
        {
            outputImage = inputImage->duplicate();
            outputImageLat = &(outputImage->lattice());
        }

        for (mcint64 i = 0; i < numVoxels; ++i)
        {
            if (ptr[i] != label)
            {
                ptr[i] = 0;
                if (outputImage && intensityPolicy == HxSplitLabelField::SET_TO_BACKGROUND)
                {
                    outputImageLat->set(i, &zero);
                }
            }
        }

        HxParamBundle* materials = outputLabelLat->materials(0);
        if (materials)
        {
            const int bundleNum = int(label);
            HxParamBundle* material = materials->getBundle(bundleNum);
            if (material)
            {
                const int idx = material->getName().remove("Material").toInt();
                const QString nameLabel = QString("Object%1.Label").arg(idx);
                outputLabels->composeLabel(inputLabels->getLabel(), nameLabel);
                if (outputImage)
                {
                    const QString nameImage = QString("Object%1.Image").arg(idx);
                    outputImage->composeLabel(inputImage->getLabel(), nameImage);
                }
                theMsg->printf(QString("Cropping object%1").arg(idx));
                crop(outputLabelLat, outputImageLat, boundarySize);
            }
        }

        results.push_back(outputLabels);
        if (outputImage)
        {
            results.push_back(outputImage);
        }
    }

    return results;
}

void
HxSplitLabelField::compute()
{
    if (!portDoIt.wasHit()) return;

    const HxUniformLabelField3* inputLabels = hxconnection_cast<HxUniformLabelField3>(portData);
    if (!inputLabels)
    {
        return;
    }

    HxLabelLattice3* inputLabelLat = (HxLabelLattice3*)inputLabels->getInterface(HxLabelLattice3::getClassTypeId());
    HxLoc3Regular* location = inputLabelLat->coords()->createLocation();
    const McDim3l dims = inputLabelLat->getDims();

    const HxUniformScalarField3* inputImage = hxconnection_cast<HxUniformScalarField3>(portImage);

    const HxSpatialGraph* seeds = hxconnection_cast<HxSpatialGraph>(portSeeds);

    const int boundarySize = MC_MAX2(portBoundarySize.getValue(), 0);

    const IntensityPolicy intensityPolicy = getIntensityPolicy();

    std::vector<HxData*> results;

    if (inputLabelLat->primType() == McPrimType::MC_UINT8)
    {
        unsigned char* ptr = inputLabelLat->getLabels();
        QSet<unsigned char> labelValues = getLabelsToKeep(dims, location, ptr, seeds);
        results = createFieldPerLabel(dims, labelValues, inputLabels, inputImage, intensityPolicy, boundarySize);
    }
    else if (inputLabelLat->primType() == McPrimType::MC_INT32)
    {
        int* ptr = (int*)inputLabelLat->getLabels();
        QSet<int> labelValues = getLabelsToKeep(dims, location, ptr, seeds);
        results = createFieldPerLabel(dims, labelValues, inputLabels, inputImage, intensityPolicy, boundarySize);
    }

    for (int i = 0; i < results.size(); ++i)
    {
        setResult(i, results[i]);
    }

    delete location;
}
