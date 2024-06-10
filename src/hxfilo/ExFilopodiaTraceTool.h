#ifndef EXFILOPODIATRACETOOL_H
#define EXFILOPODIATRACETOOL_H

#include "api.h"
#include "QxFilopodiaTool.h"
#include <hxneuroneditor/internal/QxNeuronEditorTool.h>
#include <hxvisageviewer/internal/ExShapeTool.h>
#include <mclib/McVec3.h>
#include <mclib/McVec2i.h>

class VsVolume;
class VsSlice;
class HxNeuronEditorSubApp;
struct SpatialGraphPoint;
class HxUniformScalarField3;
class QxFilopodiaTool;
class SoEventCallback;
class McLine;

/**
    @brief The ExFilopodiaTraceTool class allows user to interract with graphical
           scene elements. Method is called on the mouse move event.
*/

class ExFilopodiaTraceTool : public ExShapeTool
{
public:
    enum PickedItem
    {
        NODE,
        SEGMENT,
        POINT,
        VALID_INTENSITY,
        INVALID_INTENSITY
    };

    ExFilopodiaTraceTool(QxFilopodiaTool* filopodiaTool, int surroundWidthInVoxels);

    void
    setEditor(HxNeuronEditorSubApp* editor)
    {
        mEditor = editor;
    }

    void
    setSnapping(int enableSnapping)
    {
        mSnapToGraph = enableSnapping;
    }

    virtual bool supports(Ex::ViewerType) const;

    virtual bool pick(ExBaseViewer*, int, int, float&);

    // Method processes mouse activities in the GUI.
    // Assigns appropriate action according to the button pressed.
    virtual bool processMouseEvent(ExBaseViewer* viewer,
                                   Vs::MouseEvent inEvent,
                                   int inX,
                                   int inY,
                                   Vs::ButtonState inState,
                                   Ex::CursorShape& outCursor);

private:
    bool nodePicked(ExBaseViewer* viewer, const McVec2i& pickedScreenPoint, McVec3f& pickedPoint, int& nodeIdx);
    bool pointPicked(ExBaseViewer* viewer, const McVec2i& pickedScreenPoint, McVec3f& pickedPoint, SpatialGraphPoint& edgePoint);
    bool edgePicked(ExBaseViewer* viewer, const McVec2i& pickedScreenPoint, McVec3f& pickedPoint, SpatialGraphPoint& pointBefore, int& edgeIdx);
    bool validIntensityPicked(ExBaseViewer* viewer, VsSlice* slice, const McVec2i& pickedScreenPoint, McVec3f& pickedPoint);

    Ex::CursorShape getCursor(const PickedItem pickedItem, const bool appending);

    HxUniformScalarField3* getImageOrThrow() const;

    virtual const McString identifier() const;

    HxNeuronEditorSubApp* mEditor;
    const QxFilopodiaTool* mToolbox;
    int mSurroundWidthInVoxels;

    int mSnapToGraph;
};

////////////////////////////////////////////////////////////////////////////////
/// \brief The QxFilopodiaTraceTool class contains interractive tools. These tools provide
///        user some interraction withing the filament editor viewers.
///
class QxFilopodiaTraceTool : public QxNeuronEditorModalTool
{
    //    Q_OBJECT

public:
    QxFilopodiaTraceTool(HxNeuronEditorSubApp* editor, QxFilopodiaTool* filopodiaTool);
    bool supportsViewer(HxViewerBase* viewer);
    void onActivate(HxViewerBase*);
    void onDeactivate(HxViewerBase*);

    static void pickCB(void *userData, SoEventCallback* node);

private:
    McHandle<ExFilopodiaTraceTool> mFilopodiaTraceTool;
    const QxFilopodiaTool* mFilopodiaToolbox;
    int mSurroundWidthInVoxels;
};

#ifdef __cplusplus
extern "C" {
#endif
void HXFILOPODIA_API QxFilopodiaTraceTool_init_plugin(HxNeuronEditorSubApp* editor);
#ifdef __cplusplus
}
#endif

#endif
