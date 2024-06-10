#include "HxFieldCorrelation.h"
#include <hxitk/HxItkImageImporter.h>
#include <hxitk/HxItkImageExporter.h>
#include <hxitk/HxItkProgressObserver.h>
#include <itkNormalizedCorrelationImageFilter.h>
#include <itkImageKernelOperator.h>
#include <mclib/McException.h>
#include <QDebug>

HX_INIT_CLASS(HxFieldCorrelation, HxCompModule)


HxFieldCorrelation::HxFieldCorrelation():
    HxCompModule(HxUniformScalarField3::getClassTypeId()),
    portTemplate(this, "Template", tr("Template"), HxUniformScalarField3::getClassTypeId()),
    portStart(this,"Start",tr("Start"),3),
    portSize(this, "Size", tr("Size"),3),
    portAction(this, "action", tr("Action"))
{
    portStart.setValue(0, 0);
    portStart.setValue(1, 0);
    portStart.setValue(2, 0);

    portSize.setValue(0, 10);
    portSize.setValue(1, 10);
    portSize.setValue(2, 5);
}

HxFieldCorrelation::~HxFieldCorrelation()
{
}

void HxFieldCorrelation::compute() {

    if (!portAction.wasHit()) return;

    McHandle<HxUniformScalarField3> inputField = hxconnection_cast<HxUniformScalarField3>(portData);
    if (!inputField) {
        return;
    }
    if (inputField->lattice().primType() != McPrimType::MC_UINT8 &&
        inputField->lattice().primType() != McPrimType::MC_FLOAT)
    {
        throw McException(QString("Input must be scalar field with unsigned chars or floats"));
    }

    McHandle<HxUniformScalarField3> templateField = hxconnection_cast<HxUniformScalarField3>(portTemplate);
    if (!templateField) {
        throw McException(QString("Please connect template"));
    }
    if (inputField->lattice().primType() != McPrimType::MC_UINT8 &&
            templateField->lattice().primType() != McPrimType::MC_FLOAT)
    {
        throw McException(QString("Template must be scalar field with unsigned chars or floats"));
    }

    const McDim3l& templateDims = templateField->lattice().getDims();
    for (int i=0; i<=2; ++i) {
        if (templateDims[i] % 2 == 0) {
            throw McException(QString("Template dimensions must be odd"));
        }
    }

    McHandle<HxUniformScalarField3> floatTemplate;
    if (templateField->lattice().primType() == McPrimType::MC_UINT8) {
        floatTemplate = new HxUniformScalarField3(templateDims, McPrimType::MC_FLOAT);
        HxLattice3& floatLattice = floatTemplate->lattice();
        HxLattice3& templateLattice = templateField->lattice();
        float val;
        for (int x=0; x<templateDims.nx; ++x) {
            for (int y=0; y<templateDims.ny; ++y) {
                for (int z=0; z<templateDims.nz; ++z) {
                    templateLattice.eval(x, y, z, &val);
                    floatLattice.set(x, y, z, &val);
                }
            }
        }
    } else {
        floatTemplate = templateField->duplicate();
    }

    const McDim3l outDims(portSize.getValue(0), portSize.getValue(1), portSize.getValue(2));
    const McDim3l startIndex(portStart.getValue(0), portStart.getValue(1), portStart.getValue(2));

    McHandle<HxUniformScalarField3> resultField = mcinterface_cast<HxUniformScalarField3>( getResult() );
    if (resultField && !resultField->isOfType(HxUniformScalarField3::getClassTypeId())) {
        resultField = 0;
    }
    if (resultField) {
        const McDim3l& previousOutDims = resultField->lattice().getDims();
        if (outDims[0]!=previousOutDims[0] || outDims[1]!=previousOutDims[1] || outDims[2]!=previousOutDims[2] ||
            inputField->primType() != McPrimType::MC_FLOAT)
        {
            resultField=0;
        }
    }

    if (!resultField) {
        resultField = new HxUniformScalarField3(outDims, McPrimType::MC_FLOAT);
    }
    resultField->composeLabel(inputField->getLabel(), "correlation");

    const unsigned int Dimension = 3;
    typedef itk::Image<float, Dimension> OutputImageType;

    HxItkImageImporter<float> templateImporter(floatTemplate);
    itk::ImageKernelOperator<float, Dimension> kernelOperator;
    kernelOperator.SetImageKernel(templateImporter.GetOutput());

    // The radius of the kernel must be the radius of the patch, NOT the size of the patch
    itk::Size<Dimension> radius = templateImporter.GetOutput()->GetLargestPossibleRegion().GetSize();
    radius[0] = (radius[0]-1) / 2;
    radius[1] = (radius[1]-1) / 2;
    radius[2] = (radius[2]-1) / 2;
    kernelOperator.CreateToRadius(radius);

    // Set output image region
    OutputImageType::SizeType size;
    size[0] = outDims[0];
    size[1] = outDims[1];
    size[2] = outDims[2];

    OutputImageType::IndexType start;
    start[0] = startIndex[0];
    start[1] = startIndex[1];
    start[2] = startIndex[2];

    OutputImageType::RegionType outputRegion;
    outputRegion.SetSize( size );
    outputRegion.SetIndex( start );

    if (inputField->lattice().primType() == McPrimType::MC_UINT8) {
        HxItkImageImporter<unsigned char> importer(inputField);

        typedef itk::Image<unsigned char, Dimension> InputImageType;
        typedef itk::NormalizedCorrelationImageFilter<InputImageType, InputImageType, OutputImageType> CorrelationFilterType;

        CorrelationFilterType::Pointer correlationFilter = CorrelationFilterType::New();
        correlationFilter->SetInput(importer.GetOutput());
        correlationFilter->SetTemplate(kernelOperator);
        correlationFilter->GetOutput()->SetRegions(outputRegion);

        HxItkImageExporter<OutputImageType> exporter(correlationFilter->GetOutput(),resultField.ptr(), startIndex);

        HxItkProgressObserver progress;
        progress.setProgressMessage("Applying NormalizedCorrelationImage filter...");
        progress.startObservingProcessObject(correlationFilter);

        correlationFilter->Update();

        progress.stopObservingProcessObject();
        resultField = exporter.getResult();
    }

    else if (inputField->lattice().primType() == McPrimType::MC_FLOAT) {
        HxItkImageImporter<float> importer(inputField);

        typedef itk::Image<float, Dimension> InputImageType;
        typedef itk::NormalizedCorrelationImageFilter<InputImageType, InputImageType, OutputImageType> CorrelationFilterType;

        CorrelationFilterType::Pointer correlationFilter = CorrelationFilterType::New();
        correlationFilter->SetInput(importer.GetOutput());
        correlationFilter->SetTemplate(kernelOperator);
        correlationFilter->GetOutput()->SetRegions(outputRegion);

        HxItkImageExporter<OutputImageType> exporter(correlationFilter->GetOutput(), resultField.ptr(), startIndex);

        HxItkProgressObserver progress;
        progress.setProgressMessage("Applying NormalizedCorrelationImage filter...");
        progress.startObservingProcessObject(correlationFilter);

        correlationFilter->Update();

        progress.stopObservingProcessObject();
        resultField = exporter.getResult();
    }

    const McBox3f box = inputField->getBoundingBox();
    const McVec3f voxelSize = inputField->getVoxelSize();
    McVec3f outBoxMin, outBoxMax;

    outBoxMin.x = box.getMin().x + startIndex.nx * voxelSize.x;
    outBoxMin.y = box.getMin().y + startIndex.ny * voxelSize.y;
    outBoxMin.z = box.getMin().z + startIndex.nz * voxelSize.z;

    outBoxMax.x = outBoxMin.x + (outDims.nx-1) * voxelSize.x;
    outBoxMax.y = outBoxMin.y + (outDims.ny-1) * voxelSize.y;
    outBoxMax.z = outBoxMin.z + (outDims.nz-1) * voxelSize.z;

    const McBox3f outputBox(outBoxMin, outBoxMax);
    resultField->lattice().setBoundingBox(outputBox);
    setResult(resultField);;
}
