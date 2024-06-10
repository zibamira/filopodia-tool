#ifndef QXFILOPODIATOOL_H
#define QXFILOPODIATOOL_H

#include "api.h"
#include "FilopodiaFunctions.h"
#include <hxfilopodia/ui_QxFilopodiaTool.h>
#include <hxneuroneditor/internal/QxNeuronEditorToolBox.h>
#include <hxneuroneditor/internal/HxNeuronEditorSubApp.h>
#include <hxspreadsheet/internal/HxSpreadSheet.h>
#include <hxfield/HxUniformScalarField3.h>


class HxNeuronEditorSubApp;
class QWidget;
class SoTabBoxDraggerVR;
class SoDragger;

struct BoxSpec {
    BoxSpec() : offset(0, 0, 0), size(1, 1, 1) {}

    bool isUndefined() const {
        return (offset == McVec3i(0, 0, 0)) && (size == McVec3i(1, 1, 1));
    }

    McVec3i offset;
    McVec3i size;
};


class HXFILOPODIA_API QxFilopodiaTool  : public QObject, public QxNeuronEditorToolBox {
    
    Q_OBJECT

    public:

        QxFilopodiaTool(HxNeuronEditorSubApp* editor);
        ~QxFilopodiaTool();
        virtual QWidget* toolcard();
        int getCurrentTime() const;
        HxUniformScalarField3 *getDijkstraMap() const;
        void boxDraggerChanged();
        int getTopBrightness() const;
        int getThreshold() const;
        int getPersistence() const;
        float getIntensityWeight() const;


    public slots:
        virtual void updateToolcard();
        void selectDir();
        void selectOutputDir();
        void setTime(int time);
        void updateAfterSpatialGraphChange(HxNeuronEditorSubApp::SpatialGraphChanges changes);
        void updateAfterImageChanged();
        void updateGraphVisibility();
        void updateLocationLabels();
        void updateStatistic();
        void addBulbousLabel();
        void extendBulbousLabel();
        void updateTracking();
        void propagateAllFilopodia();
        void propagateSelFilopodia();
        void propagateRoots();
        void addFilopodiaLabels();
        void startLog();
        void stopLog();
        void prepareFiles();
        void calculateDijkstra();
        void updateConsistencyTable();
        void updateConsistencySelection(int row, int column);

        // Slots for interactive bounding box specification
        void toggleBoxDragger(bool value);
        void updateGrowthConeList();
        void updateBoxDragger();
        void setBoxSpinBoxes();
        void boxSpinBoxChanged();

        // Estimates the bounding boxes using the root node and image of the current time step
        void autoEstimateBoundingBoxes();
        void computeCropAndDijkstra();

private:

        void updateLabelAfterGeometryChange(HxSpatialGraph* graph);
        void checkConsistencyAfterGeometryChange(const HxSpatialGraph* graph, const int currentTime, McHandle<HxSpreadSheet> ss = 0);
        void createConsistencyTab(const HxSpatialGraph* graph, McHandle<HxSpreadSheet> ss);
        void connectConsistency();

        QWidget*                        mUiParent;
        Ui::FilopodiaToolUi             mUi;

        QString                           mDir;
        QString                           mOutputDir;
        QString                           mImageDir;
        QString                           mDijkstraDir;
        TimeMinMax                        mTimeMinMax;
        int                               mCurrentTime;
        QMap<int, McHandle<HxUniformScalarField3> > mImages;
        QMap<int, McHandle<HxUniformScalarField3> > mDijkstras;

        HxSpatialGraph*                 mGraph;
        McHandle<HxUniformScalarField3> mImage;
        McHandle<HxUniformScalarField3> mDijkstra;

        McHandle<HxSpreadSheet>         mFilamentStats;
        McHandle<HxSpreadSheet>         mLengthStats;
        McHandle<HxSpreadSheet>         mFilopodiaStats;
        McHandle<HxSpreadSheet>         mConsistency;

        McHandle<SoTabBoxDraggerVR>     mBoxDragger;
        QMap<int, BoxSpec>              mBoxSpecs;

        void updateFiles();
        void updateTimeMinMax();
        void setImageForCurrentTime();
        void setEmptyImage();
        void updateToolboxState();

        static void boxDraggerChangedCB(void* userData, SoDragger*);
        BoxSpec fixBoxSpec(const BoxSpec& spec);
};

#endif
