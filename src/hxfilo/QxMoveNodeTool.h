#ifndef QXMOVENODETOOL_H
#define QXMOVENODETOOL_H

#include "api.h"
#include <hxneuroneditor/internal/QxNeuronEditorTool.h>
#include <hxneuroneditor/internal/SelectionHelpers.h>
#include <hxspatialgraph/internal/SpatialGraphSelection.h>

class HxNeuronEditorSubApp;

class QxMoveNodeTool : public QxNeuronEditorTriggerTool {
    
public:

    QxMoveNodeTool(HxNeuronEditorSubApp* editor);
    ~QxMoveNodeTool();

    bool supportsViewer(HxViewerBase* baseViewer);
    void onTrigger(HxViewerBase* activeViewer, int modifiers);

protected:

    void moveNode();
};



#ifdef __cplusplus
extern "C" {
#endif
    void HXFILOPODIA_API QxMoveNodeTool_init_plugin(HxNeuronEditorSubApp* editor);
#ifdef __cplusplus
}
#endif




#endif
