#include "HxLandweberDeconvolution.h"
#include <hxitk/HxItkImageImporter.h>
#include <hxitk/HxItkImageExporter.h>
#include <hxitk/HxItkProgressObserver.h>
#include <itkProjectedLandweberDeconvolutionImageFilter.h>
#include <mclib/McException.h>


HX_INIT_CLASS(HxLandweberDeconvolution, HxCompModule)

HxLandweberDeconvolution::HxLandweberDeconvolution():
    HxCompModule(HxUniformScalarField3::getClassTypeId()),
    portTemplate(this, "Template", tr("Template"), HxUniformScalarField3::getClassTypeId()),
    portIterations(this, "Iterations", tr("Iterations"), 1),
    portAction(this, "action", tr("Action"))
{
    portIterations.setValue(0,1);
}

HxLandweberDeconvolution::~HxLandweberDeconvolution()
{
}

void HxLandweberDeconvolution::compute() {
    if (portAction.wasHit()) {
        McHandle<HxUniformScalarField3> inputField = hxconnection_cast<HxUniformScalarField3>(portData);
        if (!inputField) {
            return;
        }
        if (inputField->lattice().primType() != McPrimType::MC_UINT8)
        {
            throw McException(QString("Input must be scalar field with unsigned chars"));
        }

        McHandle<HxUniformScalarField3> templateField = hxconnection_cast<HxUniformScalarField3>(portTemplate);
        if (!templateField) {
            return;
        }
        if (templateField->lattice().primType() != McPrimType::MC_FLOAT)
        {
            throw McException(QString("Template must be scalar field with floats"));
        }

        const McDim3l& templateDims = templateField->lattice().getDims();
        for (int i=0; i<=2; ++i) {
            if (templateDims[i] % 2 == 0) {
                throw McException(QString("Template dimensions must be odd"));
            }
        }


        const McDim3l& inDims = inputField->lattice().getDims();

        McHandle<HxUniformScalarField3> resultField = mcinterface_cast<HxUniformScalarField3>( getResult() );
        if (resultField && !resultField->isOfType(HxUniformScalarField3::getClassTypeId())) {
            resultField = 0;
        }
        if (resultField) {
            const McDim3l& outdims = resultField->lattice().getDims();
            if (inDims[0]!=outdims[0] || inDims[1]!=outdims[1] || inDims[2]!=outdims[2] ||
                inputField->primType() != McPrimType::MC_FLOAT)
            {
                resultField=0;
            }
        }

        if (!resultField) {
            resultField = new HxUniformScalarField3(inDims, McPrimType::MC_FLOAT);
        }
        resultField->composeLabel(inputField->getLabel(), "correlation");

        const unsigned int Dimension = 3;
        typedef itk::Image<unsigned char, Dimension> InputImageType;
        typedef itk::Image<float, Dimension>         TemplateImageType;
        typedef itk::Image<float, Dimension>         OutputImageType;

        HxItkImageImporter<unsigned char> importer(inputField);
        HxItkImageImporter<float> templateImporter(templateField);

        typedef itk::ProjectedLandweberDeconvolutionImageFilter<InputImageType, TemplateImageType, OutputImageType> DeconvolutionFilterType;
        DeconvolutionFilterType::Pointer deconvolutionFilter = DeconvolutionFilterType::New();
        deconvolutionFilter->SetInput(importer.GetOutput());
        deconvolutionFilter->SetKernelImage(templateImporter.GetOutput());
        deconvolutionFilter->NormalizeOn();
        deconvolutionFilter->SetNumberOfIterations(portIterations.getValue());
        
        HxItkImageExporter<OutputImageType> exporter(deconvolutionFilter->GetOutput(), resultField.ptr());

        HxItkProgressObserver progress;
        progress.setProgressMessage("Applying deconvolution filter...");
        progress.startObservingProcessObject(deconvolutionFilter);
        
        deconvolutionFilter->Update();

        progress.stopObservingProcessObject();

        resultField = exporter.getResult();
        const McBox3f& box = inputField->getBoundingBox();
        resultField->lattice().setBoundingBox(box);

        setResult(resultField);
    }
}

