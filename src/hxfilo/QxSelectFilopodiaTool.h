#ifndef QxSelectFilopodiaTool_H
#define QxSelectFilopodiaTool_H

#include "api.h"
#include <hxneuroneditor/internal/QxNeuronEditorTool.h>
#include <hxneuroneditor/internal/SelectionHelpers.h>


class HxNeuronEditorSubApp;

enum SelectionMode {SELECT_FILOPODIA_SINGLE_TIMESTEP = 0,
                    TOGGLE_FILOPODIA_SINGLE_TIMESTEP,
                    SELECT_FILOPODIA_ALL_TIMESTEPS,
                    TOGGLE_FILOPODIA_ALL_TIMESTEPS};


class QxSelectFilopodiaTool : public QxNeuronEditorModalTool {

public:

    QxSelectFilopodiaTool(HxNeuronEditorSubApp* editor);
    ~QxSelectFilopodiaTool();

    bool supportsViewer(HxViewerBase* baseViewer);
    void onActivate(HxViewerBase* activeViewer);
    void onDeactivate(HxViewerBase* activeViewer);
    void pickCallback(const GraphViewNodeInterface *viewNode, SoEventCallback *node);

protected:

    void doSelection(const int vertexId,
                     const int edgeId,
                     const SpatialGraphPoint &pointId,
                     const SelectionHelpers::Modifier modKeys) const;
};



#ifdef __cplusplus
extern "C" {
#endif
    void HXFILOPODIA_API QxFilopodiaSelectionTool_init_plugin(HxNeuronEditorSubApp* editor);
#ifdef __cplusplus
}
#endif




#endif
