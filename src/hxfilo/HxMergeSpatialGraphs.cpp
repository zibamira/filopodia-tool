#include "HxMergeSpatialGraphs.h"
#include "FilopodiaFunctions.h"
#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <mclib/McException.h>
#include <QDir>


typedef QMap<int, QFileInfo> FileInfoMap; /// Key is time step


HX_INIT_CLASS(HxMergeSpatialGraphs,HxCompModule)

HxMergeSpatialGraphs::HxMergeSpatialGraphs() :
    HxCompModule(HxSpatialGraph::getClassTypeId()),
    portInputDir(this, tr("InputDirectory"), tr("Input directory"), HxPortFilename::LOAD_DIRECTORY),
    portAction(this, tr("action"), tr("Action"))
{
    portData.hide();
    portInputDir.setValue("Please select");
}

HxMergeSpatialGraphs::~HxMergeSpatialGraphs()
{
}

void mergeGraphs(HxSpatialGraph* mergedGraph, const QStringList files) {

    FileInfoMap map;
    for (int f=0; f<files.size(); ++f) {

        int time = FilopodiaFunctions::getTimeFromFileNameNoGC(files[f]);

        if (map.contains(time)) {
            throw McException(QString("Duplicate timestamp %1").arg(time));
        }
        map.insert(time, files[f]);
    }

    TimeMinMax tMinMax;
    tMinMax.minT = map.keys().first();
    tMinMax.maxT = map.keys().last();

    const int missing = (tMinMax.maxT-tMinMax.minT + 1) - map.size();
    if (missing != 0) {
        throw McException(QString("Missing %1 input labels fields").arg(missing));
    }

    for (FileInfoMap::const_iterator it=map.begin(); it!=map.end(); ++it) {
        const int time = it.key();

        McHandle<HxSpatialGraph> graph = FilopodiaFunctions::readSpatialGraph(it.value().absoluteFilePath());
        FilopodiaFunctions::addTimeLabelAttribute(graph, tMinMax);

        SpatialGraphSelection addedSel;

        if (it == map.begin()) {
            mergedGraph->copyFrom(graph);
            addedSel = SpatialGraphSelection(mergedGraph);
            addedSel.selectAllVerticesAndEdges();
        }
        else {
            addedSel = mergedGraph->merge(graph);
        }

        int labelId = FilopodiaFunctions::getTimeIdFromTimeStep(mergedGraph, time);

        SpatialGraphSelection::Iterator iter(addedSel);
        for (int v=iter.vertices.nextSelected(); v!=-1; v=iter.vertices.nextSelected()) {
            FilopodiaFunctions::setTimeIdOfNode(mergedGraph, v, labelId);
        }
        for (int e=iter.edges.nextSelected(); e!=-1; e=iter.edges.nextSelected()) {
            FilopodiaFunctions::setTimeIdOfEdge(mergedGraph, e, labelId);
        }
    }
}


void HxMergeSpatialGraphs::compute() {

    if (!portAction.wasHit()) return;

    if (portInputDir.getFilename().isEmpty() || portInputDir.getFilename() == "Please select") {
        throw McException(QString("Specify input directory!"));
    }

    McHandle<HxSpatialGraph> mergedGraph = dynamic_cast<HxSpatialGraph*>(getResult());
    if (mergedGraph) {
        mergedGraph->clear();
    }
    else {
        mergedGraph = HxSpatialGraph::createInstance();
        mergedGraph->setLabel("MergedGraph");
        setResult(mergedGraph);
    }

    const QString inputDir(portInputDir.getFilename());

    QStringList files = FilopodiaFunctions::getFileListFromDirectory(inputDir, "AM");
    if (files.size() == 0) {
        throw McException(QString("No am files in directory!"));
    }

    mergeGraphs(mergedGraph, files);
}

