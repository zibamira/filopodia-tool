/*
 *  Template of a compute module
 */

#include <hxcore/HxApplication.h>
#include <hxcore/HxProgressInterface.h>
#include <hxfield/HxUniformScalarField3.h>

#include "hxfilo.h"

HX_INIT_CLASS(hxfilo, HxCompModule)

hxfilo::hxfilo()
    : HxCompModule(HxUniformScalarField3::getClassTypeId())
    , portAction(this, "action", tr("Apply"))
{
}

hxfilo::~hxfilo()
{
}

void
hxfilo::update()
{
    // If you need to do some actions on ports (ie. make it visible/invisible)
    // write your code here.
    HxCompModule::update();
}

void
hxfilo::compute()
{
    if (!portAction.wasHit())
    {
        preserveTouchOnce();
        return;
    }

    // Skip the computation if result for current configuration has already been generated.
    // If you add a new port (i.e portA) and the computation is required
    // when this port is new, the if brackets must become:
    // if ( getResult() && !portData.isNew() && !portA.isNew() )
    if (getResult() && !portData.isNew())
        return;

    HxUniformScalarField3* inputData = hxconnection_cast<HxUniformScalarField3>(portData);

    if (!inputData)
        return;

    theProgress->startWorking(tr("Computing..."));

    // Retrieve the input data properties.
    const McDim3l& dims = inputData->lattice().getDims();
    const McPrimType primType = inputData->lattice().primType();

    // Generate an empty output data with the same properties as the input data.
    HxUniformScalarField3* outputData = new HxUniformScalarField3(dims, primType);

    // Set the name of the output data module: inputDataName.ComputeModuleName.
    outputData->composeLabel(inputData->getLabel(), getLabel());

    // Registers the output data object as the result object to make it appear in the
    // Project View panel.
    setResult(outputData);

    theProgress->stopWorking();
}
