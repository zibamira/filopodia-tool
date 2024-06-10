#include "HxExtractSlicesFrom3DImage.h"
#include <hxcore/HxObjectPool.h>
#include <hxfield/HxUniformScalarField3.h>
#include <hxcore/HxProgressInterface.h>
#include <hxcore/HxMessage.h>
#include <vector>
#include <QDebug>

HX_INIT_CLASS(HxExtractSlicesFrom3DImage, HxCompModule);

HxExtractSlicesFrom3DImage::HxExtractSlicesFrom3DImage()
    : HxCompModule(HxUniformScalarField3::getClassTypeId())
    , z_step(this, "Z_step", tr("Slice step:"))
    , portAction(this, "action", tr("Action"))
{
    z_step.setValue(0,3);
}

HxExtractSlicesFrom3DImage::~HxExtractSlicesFrom3DImage(){}

template <typename T>
void sliceData(T* inputData, T* outputData, McDim3l dims, int s, int step){
    int i = 0;
    for(int z = 0; z < dims.nz; z++){
        if((z+s)%step != 0) continue;
        for(int y = 0; y < dims.ny; y++){
            for(int x = 0; x < dims.nx; x++){
                auto idx = x + y*dims.nx + z*dims.nx*dims.ny;
                outputData[i] = inputData[idx];
                i++;
            }
        }
    }
}

void HxExtractSlicesFrom3DImage::compute()
{
    if (!portAction.wasHit()) return;

    HxUniformScalarField3* inputField = hxconnection_cast<HxUniformScalarField3>(portData);

    if (inputField == nullptr) return;

    int step = z_step.getValue();

    const McDim3l dims = inputField->lattice().getDims();
    const McDim3l dims_new(dims.nx, dims.ny, dims.nz/step);

    if (dims.nz%step != 0)
    {
        theMsg->warning(QString("Number of slices in z is not divisible by step number."));
        return;
    }

    theProgress->startWorkingNoStop("Splitting data...");

    std::vector<HxUniformScalarField3*> out(step);

    for (int i = 0; i < step; i++){
        out[i] = dynamic_cast<HxUniformScalarField3*>(getResult(i));

        if (out[i] == nullptr)
            out[i] = new HxUniformScalarField3(dims_new, inputField->primType());
        else{
            out[i]->lattice().setPrimType(inputField->primType());
            out[i]->lattice().resize(dims_new);
        }
    }

    // Adjust new bounding box size
    McBox3f bbox = inputField->getBoundingBox(); // Get old bounding box
    float x1, x2, y1, y2, z1, z2, z_new;
    bbox.getBounds(x1, y1, z1, x2, y2, z2);
    z_new = (z2-z1+1)/step+z1-1;
    bbox.setValue(x1, x2, y1, y2, z1, z_new);

    void* inputData;
    std::vector<void*> outputData(step);

    inputData = inputField->lattice().dataPtr();

    for (int i = 0; i< step; i++){
        out[i]->lattice().setBoundingBox(bbox);
        outputData[i] = out[i]->lattice().dataPtr();
    }

    auto dataType = inputField->lattice().primType();

    switch(dataType){
        case McPrimType::Type::MC_UNKNOWN:
        {
            theMsg->warning(QString("Unknow type of input data."));
            theProgress->stopWorking();
            return;
        }
        case McPrimType::Type::MC_UINT8:
        {
            mcuint8* inputDataCasted = static_cast<mcuint8*>(inputData);
            std::vector<mcuint8*> outputDataCasted(step);
            for (int i = 0; i < step; i++){
                outputDataCasted[i] = static_cast<mcuint8*>(outputData[i]);
                sliceData(inputDataCasted, outputDataCasted[i], dims, i, step);
            }
            break;
        }
        case McPrimType::Type::MC_INT16:
        {
            mcint16* inputDataCasted = static_cast<mcint16*>(inputData);
            std::vector<mcint16*> outputDataCasted(step);
            for (int i = 0; i < step; i++){
                outputDataCasted[i] = static_cast<mcint16*>(outputData[i]);
                sliceData(inputDataCasted, outputDataCasted[i], dims, i, step);
            }
            break;
        }
        case McPrimType::Type::MC_INT32:
        {
            mcint32* inputDataCasted = static_cast<mcint32*>(inputData);
            std::vector<mcint32*> outputDataCasted(step);
            for (int i = 0; i < step; i++){
                outputDataCasted[i] = static_cast<mcint32*>(outputData[i]);
                sliceData(inputDataCasted, outputDataCasted[i], dims, i, step);
            }
            break;
        }
        case McPrimType::Type::MC_FLOAT:
        {
            float* inputDataCasted = static_cast<float*>(inputData);
            std::vector<float*> outputDataCasted(step);
            for (int i = 0; i < step; i++){
                outputDataCasted[i] = static_cast<float*>(outputData[i]);
                sliceData(inputDataCasted, outputDataCasted[i], dims, i, step);
            }
            break;
        }
        case McPrimType::Type::MC_STRING:
        {
            mcstring* inputDataCasted = static_cast<mcstring*>(inputData);
            std::vector<mcstring*> outputDataCasted(step);
            for (int i = 0; i < step; i++){
                outputDataCasted[i] = static_cast<mcstring*>(outputData[i]);
                sliceData(inputDataCasted, outputDataCasted[i], dims, i, step);
            }
            break;
            }
        case McPrimType::Type::MC_UINT16:
        {
            mcuint16* inputDataCasted = static_cast<mcuint16*>(inputData);
            std::vector<mcuint16*> outputDataCasted(step);
            for (int i = 0; i < step; i++){
                outputDataCasted[i] = static_cast<mcuint16*>(outputData[i]);
                sliceData(inputDataCasted, outputDataCasted[i], dims, i, step);
            }
            break;
        }
        case McPrimType::Type::MC_UINT32:
        {
            mcuint32* inputDataCasted = static_cast<mcuint32*>(inputData);
            std::vector<mcuint32*> outputDataCasted(step);
            for (int i = 0; i < step; i++){
                outputDataCasted[i] = static_cast<mcuint32*>(outputData[i]);
                sliceData(inputDataCasted, outputDataCasted[i], dims, i, step);
            }
            break;
        }
        case McPrimType::Type::MC_INT64:
        {
            mcint64* inputDataCasted = static_cast<mcint64*>(inputData);
            std::vector<mcint64*> outputDataCasted(step);
            for (int i = 0; i < step; i++){
                outputDataCasted[i] = static_cast<mcint64*>(outputData[i]);
                sliceData(inputDataCasted, outputDataCasted[i], dims, i, step);
            }
            break;
        }
        case McPrimType::Type::MC_UINT64:
        {
            mcuint64* inputDataCasted = static_cast<mcuint64*>(inputData);
            std::vector<mcuint64*> outputDataCasted(step);
            for (int i = 0; i < step; i++){
                outputDataCasted[i] = static_cast<mcuint64*>(outputData[i]);
                sliceData(inputDataCasted, outputDataCasted[i], dims, i, step);
            }
            break;
        }
    }

    QStringList oldName = inputField->getLabel().split(".");
    for (int i = 0; i < step; i++){
        out[i]->setLabel(oldName[0] + "_CH" + QString::number(i+1) + "." + oldName[1]);
        setResult(out[i]);
    }

    theProgress->stopWorking();
}
