#include "HxLabelPSFCorrection.h"
#include <mclib/McException.h>
#include <mclib/internal/McAssert.h>

HX_INIT_CLASS(HxLabelPSFCorrection,HxCompModule)


HxLabelPSFCorrection::HxLabelPSFCorrection() :
    HxCompModule(HxUniformLabelField3::getClassTypeId()),
    portImage(this, "Image", tr("Image"), HxUniformScalarField3::getClassTypeId()),
    portNumIterations(this, "NumberOfIteration", tr("NumberOfIteration"), 1),
    portDoIt(this, "DoIt", tr("DoIt"), 1)
{
    portNumIterations.setValue(1);
}

HxLabelPSFCorrection::~HxLabelPSFCorrection()
{
}

mclong getIndexFromXYZ(const McVec3i& pos, const McDim3l& dims) {
    return pos[2]*dims[0]*dims[1] + pos[1]*dims[0] + pos[0];
}


bool isObjectVoxel(const McVec3i& pos, HxLabelLattice3* labelLat) {
    const unsigned char* ptr = labelLat->getLabels();
    const McDim3l dims = labelLat->getDims();

    if (pos[0] < 0 || pos[0] >= dims[0]) return false;
    if (pos[1] < 0 || pos[1] >= dims[1]) return false;
    if (pos[2] < 0 || pos[2] >= dims[2]) return false;

    const mclong index = getIndexFromXYZ(pos, dims);
    return ptr[index] >= 1;
}


bool isObjectTopBoundary(const McVec3i& pos, HxLabelLattice3* labelLat) {
    const McVec3i neighborAbovePos(pos[0], pos[1], pos[2]-1);
    const McVec3i neighborBelowPos(pos[0], pos[1], pos[2]+1);
    return isObjectVoxel(pos, labelLat) &&
           isObjectVoxel(neighborBelowPos, labelLat) &&
           !isObjectVoxel(neighborAbovePos, labelLat);
}


bool isObjectBottomBoundary(const McVec3i& pos, HxLabelLattice3* labelLat) {
    const McVec3i neighborAbovePos(pos[0], pos[1], pos[2]-1);
    const McVec3i neighborBelowPos(pos[0], pos[1], pos[2]+1);
    return isObjectVoxel(pos, labelLat) &&
           !isObjectVoxel(neighborBelowPos, labelLat) &&
           isObjectVoxel(neighborAbovePos, labelLat);
}


bool neighborHasHigherIntensity(const McVec3i& pos,
                                const McVec3i& neighborPos,
                                HxLattice3* imageLat)
{
    float val, neighborVal;
    imageLat->eval(pos[0], pos[1], pos[2], &val);
    imageLat->eval(neighborPos[0], neighborPos[1], neighborPos[2], &neighborVal);
    return neighborVal > val;
}


bool hasHigherIntensityBelow(const McVec3i& pos, HxLattice3* imageLat) {
    const McVec3i neighborBelowPos(pos[0], pos[1], pos[2]+1);
    mcassert(neighborBelowPos[2] != imageLat->getDims()[2]);
    return neighborHasHigherIntensity(pos, neighborBelowPos, imageLat);
}


bool hasHigherIntensityAbove(const McVec3i& pos, HxLattice3* imageLat) {
    const McVec3i neighborAbovePos(pos[0], pos[1], pos[2]-1);
    mcassert(neighborAbovePos[2] != 0);
    return neighborHasHigherIntensity(pos, neighborAbovePos, imageLat);
}


void HxLabelPSFCorrection::compute() {
    if (portDoIt.wasHit()) {

        const HxUniformLabelField3* input = hxconnection_cast<HxUniformLabelField3>(portData);
        if (!input || input->lattice().primType() != McPrimType::MC_UINT8) {
            throw McException(QString("No label field of type unsigned char provided"));
        }

        const HxUniformScalarField3* image = hxconnection_cast<HxUniformScalarField3>(portImage);
        if (!image ||image->lattice().primType() != McPrimType::MC_UINT8) {
            throw McException(QString("No image of type unsigned char provided"));
        }

        McHandle<HxUniformLabelField3> output = dynamic_cast<HxUniformLabelField3*>(getResult());
        if (output) {
            const HxLattice3& inLat = input->lattice();
            output->lattice().init(inLat.nDataVar(), inLat.primType(), inLat.coords());
            output->lattice().copyData(inLat);
        }
        else {
            output = input->duplicate();
            output->composeLabel(input->getLabel(), "corrected");
            setResult(output);
        }

        HxLattice3* imageLat = dynamic_cast<HxLattice3*>(image->getInterface(HxLattice3::getClassTypeId()));
        HxLabelLattice3* outputLat = dynamic_cast<HxLabelLattice3*>(output->getInterface(HxLabelLattice3::getClassTypeId()));

        const int numIter = portNumIterations.getValue();
        const float zero = 0.0f;

        for (int iter=0; iter<numIter; ++iter) {
            McHandle<HxUniformLabelField3> tmpLabels = output->duplicate();
            HxLabelLattice3* inputLat = dynamic_cast<HxLabelLattice3*>(tmpLabels->getInterface(HxLabelLattice3::getClassTypeId()));
            const McDim3l& dims = inputLat->getDims();
            for (int z=0; z<dims[2]; ++z) {
                for (int y=0; y<dims[1]; ++y) {
                    for (int x=0; x<dims[0]; ++x) {
                        McVec3i pos(x, y, z);
                        if (isObjectTopBoundary(pos, inputLat)) {
                            if (hasHigherIntensityBelow(pos, imageLat)) {
                                outputLat->set(x, y, z, &zero);
                            }
                        }
                        else if (isObjectBottomBoundary(pos, inputLat)) {
                            if (hasHigherIntensityAbove(pos, imageLat)) {
                                outputLat->set(x, y, z, &zero);
                            }
                        }
                    }
                }
            }
        }
    }
}

