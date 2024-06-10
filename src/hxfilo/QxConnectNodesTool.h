#ifndef QxConnectNodesTool_H
#define QxConnectNodesTool_H

#include "api.h"
#include <hxneuroneditor/internal/QxNeuronEditorTool.h>
#include <hxneuroneditor/internal/SelectionHelpers.h>
#include <hxspatialgraph/internal/SpatialGraphSelection.h>

class HxNeuronEditorSubApp;

class QxConnectNodesTool : public QxNeuronEditorTriggerTool {
    
public:

    QxConnectNodesTool(HxNeuronEditorSubApp* editor);
    ~QxConnectNodesTool();

    bool supportsViewer(HxViewerBase* baseViewer);
    void onTrigger(HxViewerBase* activeViewer, int modifiers);

protected:

    void connectNodes();
};



#ifdef __cplusplus
extern "C" {
#endif
    void HXFILOPODIA_API QxConnectNodesTool_init_plugin(HxNeuronEditorSubApp* editor);
#ifdef __cplusplus
}
#endif




#endif
