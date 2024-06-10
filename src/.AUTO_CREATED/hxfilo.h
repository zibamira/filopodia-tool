#ifndef HXFILO_H
#define HXFILO_H

#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>

#include "api.h"

/**
 * @brief Brief description of this compute module.
 *
 *  Detailed description of this compute module.
 */
class HXFILO_API hxfilo : public HxCompModule
{
    HX_HEADER(hxfilo);

public:
    /**
     * @brief Port hit when the Apply button is clicked.
     */
    HxPortDoIt portAction;

    /**
     * @copydoc HxObject::update()
     */
    virtual void update();

    /**
     * @copydoc HxObject::compute()
     *
     * Creates an empty output data module with the same dimensions as the input data.
     */
    virtual void compute();
};

#endif // HXFILO_H
