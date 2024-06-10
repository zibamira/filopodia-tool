#ifndef QxDeleteFilopodiaTool_H
#define QxDeleteFilopodiaTool_H

#include "api.h"
#include <hxneuroneditor/internal/QxNeuronEditorTool.h>


class HxNeuronEditorSubApp;

class QxDeleteFilopodiaTool : public QxNeuronEditorTriggerTool {
    
public:

    QxDeleteFilopodiaTool(HxNeuronEditorSubApp* editor);
    ~QxDeleteFilopodiaTool();

    bool supportsViewer(HxViewerBase* baseViewer);
    void onTrigger(HxViewerBase* activeViewer, int modifiers);

protected:

    void deleteFilopodia();
};



#ifdef __cplusplus
extern "C" {
#endif
    void HXFILOPODIA_API QxDeleteFilopodiaTool_init_plugin(HxNeuronEditorSubApp* editor);
#ifdef __cplusplus
}
#endif




#endif
