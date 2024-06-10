#ifndef QxMatchFilopodiaTool_H
#define QxMatchFilopodiaTool_H

#include "api.h"
#include <hxneuroneditor/internal/QxNeuronEditorTool.h>
#include <hxneuroneditor/internal/SelectionHelpers.h>
#include <hxspatialgraph/internal/SpatialGraphSelection.h>

class HxNeuronEditorSubApp;

class QxMatchFilopodiaTool : public QxNeuronEditorTriggerTool {

public:

    QxMatchFilopodiaTool(HxNeuronEditorSubApp* editor);
    ~QxMatchFilopodiaTool();

    bool supportsViewer(HxViewerBase* baseViewer);
    void onTrigger(HxViewerBase* activeViewer, int modifiers);

protected:

    void matchFilopodia(const int modifiers);
};



#ifdef __cplusplus
extern "C" {
#endif
    void HXFILOPODIA_API QxMatchFilopodiaTool_init_plugin(HxNeuronEditorSubApp* editor);
#ifdef __cplusplus
}
#endif




#endif
