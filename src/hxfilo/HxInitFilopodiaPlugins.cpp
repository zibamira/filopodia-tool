#ifndef HX_INIT_FILOPODIA_PLUGINS_H
#define HX_INIT_FILOPODIA_PLUGINS_H

#include "api.h"
#include "QxFilopodiaTool.h"
#include "ExFilopodiaTraceTool.h"
#include "QxSelectFilopodiaTool.h"
#include "QxDeleteFilopodiaTool.h"
#include "QxMatchFilopodiaTool.h"
#include "QxMoveNodeTool.h"
#include "QxConnectNodesTool.h"

/** Function to register the FilopodiaTools at the HxNeuronEditorSubApp.
 *  We use extern "C" to avoid name mangling so HxDSO can find it. */
#ifdef __cplusplus
extern "C" {
#endif

void HXFILOPODIA_API QxFilopodiaTools_init_plugin(HxNeuronEditorSubApp* editor) {

    // Filopodia toolbox
    QxFilopodiaTool* filopodiaTool = new QxFilopodiaTool(editor);
    filopodiaTool->setText("Filopodia");
    filopodiaTool->setToolTip("Filopodia analysis tools");

    editor->registerToolBox(filopodiaTool);

    // TraceFilopodiaTool
    QxFilopodiaTraceTool *traceTool = new QxFilopodiaTraceTool(editor, filopodiaTool);
    traceTool->setText("Trace filopodia");
    traceTool->setToolTip("Trace filopodia (shift + t)");
    traceTool->setIconFile("neuronEditorTraceFilopodia.png");
    traceTool->setKeySequence(QKeySequence(Qt::SHIFT + Qt::Key_T));

    editor->registerTool(traceTool, "FilopodiaTools");

    // MoveFilopodiaTool
    QxMoveNodeTool *moveNodeTool = new QxMoveNodeTool(editor);
    moveNodeTool->setText("Move filopodia base or tip");
    moveNodeTool->setToolTip("Move filopodia base or tip (shift + s)");
    moveNodeTool->setIconFile("neuronEditorMoveFilopodia.png");
    moveNodeTool->setKeySequence(QKeySequence(Qt::SHIFT + Qt::Key_S));

    editor->registerTool(moveNodeTool, "FilopodiaTools");

    // DeleteFilopodiaTool
    QxDeleteFilopodiaTool *deleteSegmentsTool = new QxDeleteFilopodiaTool(editor);
    deleteSegmentsTool->setText("Delete filopodium");
    deleteSegmentsTool->setToolTip("Delete filopodium (shift + d)");
    deleteSegmentsTool->setIconFile("neuronEditorDeleteFilopodia.png");
    deleteSegmentsTool->setKeySequence(QKeySequence(Qt::SHIFT + Qt::Key_D));

    editor->registerTool(deleteSegmentsTool, "FilopodiaTools");

    // SelectFilopodiaTool
    QxSelectFilopodiaTool *filopodiaSelectionTool = new QxSelectFilopodiaTool(editor);
    filopodiaSelectionTool->setText("Select filopodia");
    filopodiaSelectionTool->setToolTip("Select filopodia");
    filopodiaSelectionTool->setIconFile("neuronEditorSelectFilopodia.png");

    editor->registerTool(filopodiaSelectionTool, "FilopodiaTools");

    // MatchFilopodiaTool
    QxMatchFilopodiaTool *labelFilopodiaTool = new QxMatchFilopodiaTool(editor);
    labelFilopodiaTool->setText("Selected Filopodia get same match ID");
    labelFilopodiaTool->setToolTip("Match selected filopodia");
    labelFilopodiaTool->setIconFile("neuronEditorMatchFilopodia.png");

    editor->registerTool(labelFilopodiaTool, "FilopodiaTools");  
}

#ifdef __cplusplus
}
#endif

#endif //HX_INIT_FILOPODIA_PLUGINS_H
