#include "HxFilopodiaStats.h"
#include "FilopodiaFunctions.h"
#include <mclib/McException.h>
#include <mclib/McVec2.h>

#include "ConvertVectorAndMcDArray.h"

HX_INIT_CLASS(HxFilopodiaStats,HxCompModule)

HxFilopodiaStats::HxFilopodiaStats() :
    HxCompModule(HxSpatialGraph::getClassTypeId()),
    portOutput(this, "output", QApplication::translate("HxSpatialGraphStats", "Output"), 3),
    portFilter(this, tr("StabilizationThreshold"), tr("Stabilization Threshold")),
    portAction(this, tr("action"), tr("Action"))
{
    portOutput.setLabel(OUTPUT_SPREADSHEET_FILAMENT,"Filament Statistic");
    portOutput.setLabel(OUTPUT_SPREADSHEET_FILOPODIA,"Filopodia Statistic");
    portOutput.setLabel(OUTPUT_SPREADSHEET_LENGTH,"Length Statistic");

    portFilter.setValue(0.1f);

    portOutput.setValue(OUTPUT_SPREADSHEET_FILAMENT, 1);
    portOutput.setValue(OUTPUT_SPREADSHEET_FILOPODIA, 1);
    portOutput.setValue(OUTPUT_SPREADSHEET_LENGTH, 1);
}

HxFilopodiaStats::~HxFilopodiaStats() {
}


int getBaseFromFilopodiaWithTimeId(const HxSpatialGraph* graph, const int filoId, const int timeId) {

    int firstBase = -1;

    for (int v=0; v<graph->getNumVertices(); ++v) {

        if ( (FilopodiaFunctions::getFilopodiaIdOfNode(graph, v) == filoId) && (FilopodiaFunctions::getTimeIdOfNode(graph, v) == timeId) && (FilopodiaFunctions::hasNodeType(graph, BASE_NODE, v)) ) {
            firstBase = v;
        }
    }

    if (firstBase == -1) {
        throw McException(QString("Filopodia %1 has no firstBase").arg(filoId));
    } else {

        return firstBase;
    }
}


float getAngleFromFilopodia(const HxSpatialGraph* graph, const int filoId, const int startTime) {

    // Calculates angle of filopodia by projecting root and base node of filopodium into XY plane
    // and calculating the 2D angle between the vector spanned by root and base and the unit vector [0,1]
    const int timeId = FilopodiaFunctions::getTimeIdFromTimeStep(graph, startTime);
    const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, startTime);
    const int baseNode = getBaseFromFilopodiaWithTimeId(graph, filoId, timeId);

    const McVec3f rootPoint = graph->getVertexCoords(rootNode);
    const McVec3f basePoint = graph->getVertexCoords(baseNode);

    const McVec2f root2D(rootPoint[0],rootPoint[1]);
    const McVec2f base2D(basePoint[0],basePoint[1]);

    const McVec2f vector = (root2D - base2D) / (root2D - base2D).length();
    const float scalar = vector.dot(McVec2f(0,1));

    float angle = acos(scalar/vector.length())*180.0f/ M_PI;

    if (vector[0] > 0) {
        angle = 360 - angle;
    }

    return angle;
}


float getFilamentLength(const HxSpatialGraph* graph, const SpatialGraphSelection sel) {

    // Filament is the sum of length of all edges related to the filament
    // if the filament contains branches the branch will be added
    float filamentLength = 0;

    for (int sele=0; sele<sel.getNumSelectedEdges(); ++sele) {
        int currentEdge = sel.getSelectedEdge(sele);
        filamentLength += FilopodiaFunctions::getEdgeLength(graph, currentEdge);
    }

    return filamentLength;
}


void HxFilopodiaStats::createFilamentTab(const HxSpatialGraph* graph, HxSpreadSheet* ss) {

    // Filament is defined as tree-like structure, which location is "filopodium" and contains at least one base and one tip
    // You can say that a filament is a filopodium at a specific time point
    // Each base indicates a filament
    ss->clear();

    SpatialGraphSelection baseSel = FilopodiaFunctions::getNodesOfType(graph, BASE_NODE);
    const int numberFilaments = baseSel.getNumSelectedVertices();

    ss->setTableName("Filament Statistic", 0);

    ss->setNumRows(numberFilaments, 0);
    ss->addColumn( "Filament ID",         HxSpreadSheet::Column::INT,    0);
    ss->addColumn( "Time Step",           HxSpreadSheet::Column::INT,    0);
    ss->addColumn( "Filopodia ID",        HxSpreadSheet::Column::INT,    0);
    ss->addColumn( "Filopodia Name",      HxSpreadSheet::Column::STRING, 0);
    ss->addColumn( "Total Length [mum]",  HxSpreadSheet::Column::FLOAT, 0);
    ss->addColumn( "Angle",               HxSpreadSheet::Column::FLOAT,  0);
    ss->addColumn( "Bulbous",             HxSpreadSheet::Column::INT,    0);
    ss->addColumn( "Base ID",             HxSpreadSheet::Column::INT,    0);
    ss->addColumn( "Base x-coord",        HxSpreadSheet::Column::FLOAT,  0);
    ss->addColumn( "Base y-coord",        HxSpreadSheet::Column::FLOAT,  0);
    ss->addColumn( "Base z-coord",        HxSpreadSheet::Column::FLOAT,  0);
    ss->addColumn( "# Branching Nodes",   HxSpreadSheet::Column::INT,    0);
    ss->addColumn( "# Tip",               HxSpreadSheet::Column::INT,    0);

    for (int i=0; i<numberFilaments; ++i) {

        const int baseNode = baseSel.getSelectedVertex(i);
        std::vector<int> tipNodes;

        SpatialGraphSelection sel = FilopodiaFunctions::getFilopodiumSelectionFromNode(graph, baseNode);
        int numBranchingNodes = 0;
        if (sel.isEmpty()) {
            printf("Filopodium Selection of base %i is empty. \n", baseNode);
        } else if (sel.getNumSelectedEdges() == 0) {
            printf("Filopodium Selection of base %i has no edges. \n", baseNode);
        } else if (sel.getNumSelectedVertices() == 0) {
            printf("Filopodium Selection of base %i has no vertices. \n", baseNode);
        } else {

            for (int selv=0; selv<sel.getNumSelectedVertices(); ++selv) {

                const int currentNode = sel.getSelectedVertex(selv);

                if (currentNode < 0 || currentNode >= graph->getNumVertices()) {
                    printf("currentNode: %i\n", currentNode);
                }

                if (FilopodiaFunctions::hasNodeType(graph, TIP_NODE, currentNode)) {
                    tipNodes.push_back(currentNode);
                } else if (FilopodiaFunctions::hasNodeType(graph, BRANCHING_NODE, currentNode)) {
                    ++numBranchingNodes;
                }
            }

            float filamentLength = getFilamentLength(graph, sel);

            const int time = FilopodiaFunctions::getTimeOfNode(graph, baseNode);
            const int filoId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, baseNode);
            const QString filoName = FilopodiaFunctions::getFilopodiaLabelNameFromID(graph, filoId);
            const float filamentAngle = getAngleFromFilopodia(graph, filoId, time);
            const int bulbous = FilopodiaFunctions::getBulbousIdOfNode(graph, baseNode) - 1;

            McVec3f baseCoords = graph->getVertexCoords(baseNode);

            ss->column(0,0)->setValue(i, i);
            ss->column(1,0)->setValue(i, time);
            ss->column(2,0)->setValue(i, filoId);
            ss->column(3,0)->setValue(i, qPrintable(filoName));
            ss->column(4,0)->setValue(i, filamentLength);
            ss->column(5,0)->setValue(i, filamentAngle);
            ss->column(6,0)->setValue(i, bulbous);
            ss->column(7,0)->setValue(i, baseNode);
            ss->column(8,0)->setValue(i, baseCoords.x);
            ss->column(9,0)->setValue(i, baseCoords.y);
            ss->column(10,0)->setValue(i, baseCoords.z);
            ss->column(11,0)->setValue(i, numBranchingNodes);
            ss->column(12,0)->setValue(i, tipNodes.size());

            for (int n = 0; n < tipNodes.size(); ++n) {

                if (ss->findColumn(McString().printf("Tip %i Id", n+1), HxSpreadSheet::Column::INT) == -1) {
                    ss->addColumn(McString().printf("Tip %i Id", n+1),         HxSpreadSheet::Column::INT,    0);
                    ss->addColumn(McString().printf("Tip %i x-coords", n+1),   HxSpreadSheet::Column::FLOAT,  0);
                    ss->addColumn(McString().printf("Tip %i y-coords", n+1),   HxSpreadSheet::Column::FLOAT,  0);
                    ss->addColumn(McString().printf("Tip %i z-coords", n+1),   HxSpreadSheet::Column::FLOAT,  0);
                }

                ss->column(13 + 4*n,0)->setValue(i, tipNodes[n]);
                McVec3f tipCoords = graph->getVertexCoords(tipNodes[n]);

                ss->column(14 + 4*n,0)->setValue(i, tipCoords.x);
                ss->column(15 + 4*n,0)->setValue(i, tipCoords.y);
                ss->column(16 + 4*n,0)->setValue(i, tipCoords.z);
            }
        }
    }
}


int findFilamentsOfFilopodia(const HxSpatialGraph* graph, std::vector<int>& filaments, const HxSpreadSheet* ss,const int filoId) {

    // Returns all filament IDs of filaments with given filopodia ID as well as the number of filaments with bulbous label

    int bulbous = 0;

    const int tableID = ss->findTable("Filament Statistic");
    if (tableID == -1) {
        throw McException(QString("Filament table not found"));
    }

    const int baseColumnID = ss->findColumn("Base ID", HxSpreadSheet::Column::INT);
    const int filamentColumnID = ss->findColumn("Filament ID", HxSpreadSheet::Column::INT);
    if ( (baseColumnID == -1) || (filamentColumnID == -1)){
        throw McException(QString("Column not found"));
    }

    const HxSpreadSheet::Column* baseColumn = ss->column(baseColumnID,tableID);
    const HxSpreadSheet::Column* filamentColumn = ss->column(filamentColumnID,tableID);

    const TimeMinMax lifeTime = FilopodiaFunctions::getFilopodiumLifeTime(graph, filoId);
    std::vector<int> timeCheck(lifeTime.maxT + 1);
    std::fill(timeCheck.begin(), timeCheck.end(),0);

    for (int r=0; r<ss->nRows();++r) {

        const int baseNode = baseColumn->intValue(r);
        const int currentFilID = FilopodiaFunctions::getFilopodiaIdOfNode(graph, baseNode);

        if (currentFilID == filoId) {

            const int currentTimeId = FilopodiaFunctions::getTimeIdOfNode(graph, baseNode);
            const int currentTime = FilopodiaFunctions::getTimeStepFromTimeId(graph, currentTimeId);

            if (timeCheck[currentTime] == 0) {
                const int filamentID = filamentColumn->intValue(r);

                if (FilopodiaFunctions::hasBulbousLabel(graph, BULBOUS, baseNode)) {
                    bulbous += 1;
                }

                filaments.push_back(filamentID);
                timeCheck[currentTime] = 1;
            } else {
                if (filoId > 2) {
                    printf("Multiple filaments with filopodia ID %i in time step %i. \n", filoId, currentTime);
                    return 0;
                }
            }
        }
    }
    return bulbous;
}


std::vector<float> getFilopodiaLength(const HxSpatialGraph* graph, const int filoId, const TimeMinMax lifeTime) {

    const int age = lifeTime.maxT - lifeTime.minT + 1;
    std::vector<float> filopodiaLength(age);

    for (int t=lifeTime.minT; t<lifeTime.maxT+1; ++t) {

        const int timeId = FilopodiaFunctions::getTimeIdFromTimeStep(graph, t);
        const int baseNode = getBaseFromFilopodiaWithTimeId(graph, filoId, timeId);
        SpatialGraphSelection filoSel = FilopodiaFunctions::getFilopodiumSelectionFromNode(graph, baseNode);
        filopodiaLength[t - lifeTime.minT] = getFilamentLength(graph, filoSel);
    }

    return filopodiaLength;
}


QString filamentsToString(const std::vector<int> filaments) {

    // Write std::vector of filament IDs to string

    QString filamentsString = QString("%1").arg(filaments[0]);

    for (int i=1; i<filaments.size();++i) {
        const QString fil = QString(", %1").arg(filaments[i]);
        filamentsString += fil;
    }
    return filamentsString;
}

std::vector<int> getValidFilopodiaIds(const HxSpatialGraph* graph) {

    // Returns std::vector of filopodia IDs excluding unassigned, ignored, axon or parent ID

    std::vector<int> validFilopodiaLabelIds;
    const char* filopodiaAttName = FilopodiaFunctions::getFilopodiaAttributeName();
    const HierarchicalLabels* filoLabelGroup = graph->getLabelGroup(filopodiaAttName);
    if (!filoLabelGroup) {
        printf("Filopodia label group does not exist.\n");
        return validFilopodiaLabelIds;
    }

    const McDArray<int> allFilopodiaLabelIds = filoLabelGroup->getChildIds(0);
    for (int i=0; i<allFilopodiaLabelIds.size(); ++i) {
        const int id = allFilopodiaLabelIds[i];
        const int nextAvailable = FilopodiaFunctions::getNextAvailableFilopodiaId(graph);
        if ( FilopodiaFunctions::isRealFilopodium(graph, id) && (id != nextAvailable))
        {
            validFilopodiaLabelIds.push_back(id);
        }
    }

    return validFilopodiaLabelIds;
}


void HxFilopodiaStats::createLengthTab(const HxSpatialGraph* graph, HxSpreadSheet* ss, const HxSpreadSheet* ssFilament) {

    // Length tab offers values for length vs. time vs. angle heat maps
    // For each filopodium the initial angle and length at each timestep is stored
    ss->clear();
    ss->setTableName("Filopodia Length", 0);

    // Consider only valid filo IDs excluding unasigned, ignored, axon ID
    const std::vector<int> validFilopodiaLabelIds = getValidFilopodiaIds(graph);

    ss->setNumRows(validFilopodiaLabelIds.size(), 0);

    ss->addColumn( "Filopodia Name",            HxSpreadSheet::Column::STRING,    0);
    ss->addColumn( "Filopodia ID",              HxSpreadSheet::Column::INT,       0);
    ss->addColumn( "First Occurence",           HxSpreadSheet::Column::INT,       0);
    ss->addColumn( "Last Occurence",            HxSpreadSheet::Column::INT,       0);
    ss->addColumn( "Life Time [min]",           HxSpreadSheet::Column::INT,       0);
    ss->addColumn( "Angle",                     HxSpreadSheet::Column::FLOAT,     0);
    ss->addColumn( "Filaments",                 HxSpreadSheet::Column::STRING,    0);

    const int nScipCol = ss->nCols();

    // Add column for each time step
    TimeMinMax tMinMax = FilopodiaFunctions::getTimeMinMaxFromGraphLabels(graph);

    for (int t=tMinMax.minT; t<tMinMax.maxT+1; ++t) {
        ss->addColumn( McString().printf("Length t%02d [mum]", t),     HxSpreadSheet::Column::FLOAT,  0);
    }

    for (int f=0; f<validFilopodiaLabelIds.size();++f) {
        const int filoId = validFilopodiaLabelIds[f];

        SpatialGraphSelection filoSel = FilopodiaFunctions::getFilopodiaSelectionForAllTimeSteps(graph, filoId);
        SpatialGraphSelection baseSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, filoSel, BASE_NODE);

        if (baseSel.getNumSelectedVertices() > 0) {
            const TimeMinMax lifeTime = FilopodiaFunctions::getFilopodiumLifeTime(graph, filoId);
            const int startTime = lifeTime.minT;
            const int endTime = lifeTime.maxT;
            const int age = endTime - startTime + 1;
            const float angle = getAngleFromFilopodia(graph, filoId, startTime);

            if (baseSel.getNumSelectedVertices() > age) {
                printf("%s has more than one filament per timestep.\n", qPrintable(FilopodiaFunctions::getFilopodiaLabelNameFromID(graph, filoId)));
            }
            else if (baseSel.getNumSelectedVertices() < age) {
                printf("%s has less than one filament per timestep. Num. filaments: %d. Age: %d (%d-%d)\n",
                       qPrintable(FilopodiaFunctions::getFilopodiaLabelNameFromID(graph, filoId)), int(baseSel.getNumSelectedVertices()),
                       age, startTime, endTime);
            }

            std::vector<int> filaments;
            findFilamentsOfFilopodia(graph, filaments, ssFilament, filoId);
            QString filopodiaLabelName = FilopodiaFunctions::getFilopodiaLabelNameFromID(graph, filoId);
            QString filamentsString = filamentsToString(filaments);

            ss->column(0,0)->setValue(f, qPrintable(filopodiaLabelName));
            ss->column(1,0)->setValue(f, filoId);
            ss->column(2,0)->setValue(f, startTime);
            ss->column(3,0)->setValue(f, endTime);
            ss->column(4,0)->setValue(f, age);
            ss->column(5,0)->setValue(f, angle);
            ss->column(6,0)->setValue(f, qPrintable(filamentsString));

            for (int l=0; l<baseSel.getNumSelectedVertices();++l) {
                SpatialGraphSelection filamentSel = FilopodiaFunctions::getFilopodiumSelectionFromNode(graph, baseSel.getSelectedVertex(l));
                const float length = getFilamentLength(graph, filamentSel);
                const int columnID = nScipCol + startTime + l;
                ss->column(columnID,0)->setValue(f,length);
            }
        }
        else {
            theMsg->printf("no filaments for filopodia %i", filoId);
        }
    }
}

std::vector<float> getSpeed(std::vector<float> filoLength) {

    // Calculates differences of length between time steps
    std::vector<float> speed;

    for (int i=0; i<filoLength.size()-1; ++i) {
        const float currentSpeed = filoLength[i]-filoLength[i+1];
        speed.push_back(currentSpeed);
    }

    return speed;
}

float getMean(const std::vector<float> array) {

    // Calculates the mean of a given array but using absolute values
    if (array.size() == 0) {
        return NAN;
    } else {
        float sum = 0.0;
        for (int i=0; i<array.size(); ++i) {
            sum += fabs(array[i]);
        }

        return sum/float(array.size());
    }
}

float getStd(const std::vector<float> array, const float mean) {

    // Calculates the standard deviation of a given array but using absolute values
    if (array.size() == 0) {
        return NAN;
    } else {
        float sum = 0.0;
        for (int i=0; i<array.size(); ++i) {
            sum += ((fabs(array[i])-mean)*(fabs(array[i])-mean));
        }

        return sqrt(sum/float(array.size()));
    }
}


std::vector<float> filterSpeed(std::vector<float> speed, const float filter, std::vector<float>& extensions, std::vector<float>& retractions) {

    // Returns array of speed values > given stabilization threshold
    std::vector<float> speedFilter;

    for (int i=0; i<speed.size(); ++i) {
        if (fabs(speed[i]) > filter) {
            speedFilter.push_back(fabs(speed[i]));
            if (speed[i] < 0) {
                extensions.push_back(fabs(speed[i]));
            } else {
                retractions.push_back(fabs(speed[i]));
            }
        }
    }
    return speedFilter;
}


void HxFilopodiaStats::createFilopodiaTab(const HxSpatialGraph* graph, HxSpreadSheet* ss, const HxSpreadSheet* ssFilament, const float filter) {

    ss->clear();

    const std::vector<int> validFilopodiaIds = getValidFilopodiaIds(graph);
    const TimeMinMax timeMinMax = FilopodiaFunctions::getTimeMinMaxFromGraphLabels(graph);
    const int maxTime = timeMinMax.maxT;

    ss->setTableName("Filopodia Statistic", 0);
    ss->setNumRows(validFilopodiaIds.size(), 0);

    ss->addColumn( "Filopodia Name",                                HxSpreadSheet::Column::STRING,  0);
    ss->addColumn( "Filopodia ID",                                  HxSpreadSheet::Column::INT,     0);
    ss->addColumn( "# Bulbous",                                     HxSpreadSheet::Column::INT,     0);
    ss->addColumn( "Bulbous [%]",                                   HxSpreadSheet::Column::FLOAT,   0);
    ss->addColumn( "First occurence",                               HxSpreadSheet::Column::INT,     0);
    ss->addColumn( "Last occurence",                                HxSpreadSheet::Column::INT,     0);
    ss->addColumn( "Life Time [min]",                               HxSpreadSheet::Column::INT,     0);
    ss->addColumn( "Length (mean) [mum]",                           HxSpreadSheet::Column::FLOAT,   0);
    ss->addColumn( "Length (std) [mum]",                            HxSpreadSheet::Column::FLOAT,   0);
    ss->addColumn( "Final Length [mum]",                            HxSpreadSheet::Column::FLOAT,   0);
    ss->addColumn( "# Total Events",                                HxSpreadSheet::Column::INT,     0);
    ss->addColumn( "Total Speed (mean) [mum/min]",                  HxSpreadSheet::Column::FLOAT,   0);
    ss->addColumn( "Total Speed (std) [mum/min]",                   HxSpreadSheet::Column::FLOAT,   0);
    ss->addColumn( McString().printf("Filter = %1.2f", filter),     HxSpreadSheet::Column::STRING,  0);
    ss->addColumn( "# Filtered Events",                             HxSpreadSheet::Column::INT,     0);
    ss->addColumn( "Filtered Speed (mean) [mum/min]",               HxSpreadSheet::Column::FLOAT,   0);
    ss->addColumn( "Filtered Speed (std) [mum/min]",                HxSpreadSheet::Column::FLOAT,   0);
    ss->addColumn( "# Filtered Extensions",                         HxSpreadSheet::Column::INT,     0);
    ss->addColumn( "Extension Speed (mean) [mum/min]",              HxSpreadSheet::Column::FLOAT,   0);
    ss->addColumn( "Extension Speed (std) [mum/min]",               HxSpreadSheet::Column::FLOAT,   0);
    ss->addColumn( "# Retractions",                                 HxSpreadSheet::Column::INT,     0);
    ss->addColumn( "Retraction Speed (mean) [mum/min]",             HxSpreadSheet::Column::FLOAT,   0);
    ss->addColumn( "Retraction Speed (std) [mum/min]",              HxSpreadSheet::Column::FLOAT,   0);
    ss->addColumn( "# Static",                                      HxSpreadSheet::Column::INT,     0);
    ss->addColumn( "Static [%]",                                    HxSpreadSheet::Column::FLOAT,   0);

    for (int f=0; f<validFilopodiaIds.size(); ++f) {

        const int filoId = validFilopodiaIds[f];
        const TimeMinMax lifeTime = FilopodiaFunctions::getFilopodiumLifeTime(graph, filoId);
        const int startTime = lifeTime.minT;
        const int endTime = lifeTime.maxT;
        const int age = endTime - startTime + 1;

        std::vector<float> filoLength;
        const std::vector<float> filoLengthS = getFilopodiaLength(graph, filoId, lifeTime);
        const float meanLength = getMean(filoLengthS);
        const float stdLength = getStd(filoLengthS, meanLength);
        const float finalLength = filoLengthS.back();


        // push_back length value = 0 to start and end of list if filopodia emerge or retract in middel of timeline to point out first event of growing and event of disappearing
        if (startTime != 0) {
            filoLength.push_back(0.0);
            for (int i=0; i<filoLengthS.size(); ++i) {
                filoLength.push_back(filoLengthS[i]);
            }
        } else {
            filoLength = filoLengthS;
        }

        if (endTime < (maxTime)) {
            filoLength.push_back(0.0);
        }

        const QString filopodiaName = FilopodiaFunctions::getFilopodiaLabelNameFromID(graph, filoId);

        std::vector<int> filaments;
        const int bulbous = findFilamentsOfFilopodia(graph, filaments, ssFilament, filoId);
        if (bulbous > 0) {
            printf("%s is bulbous\n", qPrintable(filopodiaName));
        }

        const std::vector<float> filoSpeed = getSpeed(filoLength);

        const int numberEvents = filoSpeed.size();
        const float meanSpeed = getMean(filoSpeed);
        const float stdSpeed = getStd(filoSpeed, meanSpeed);

        std::vector<float> extensions;
        std::vector<float> retractions;
        const std::vector<float> filoSpeedFilter = filterSpeed(filoSpeed, filter, extensions, retractions);
        const int numberEventsFilter = filoSpeedFilter.size();
        const float meanSpeedFilter = getMean(filoSpeedFilter);
        const float stdSpeedFilter = getStd(filoSpeedFilter, meanSpeedFilter);

        const int numberExt = extensions.size();
        const float meanExt = getMean(extensions);
        const float stdExt = getStd(extensions, meanExt);

        const int numberRetr = retractions.size();
        const float meanRetr = getMean(retractions);
        const float stdRetr = getStd(retractions, meanExt);

        const int stable = numberEvents - numberEventsFilter;
        float stableP = 0.0;
        if (numberEvents > 0) {
            stableP = float(stable)/float(numberEvents);
        }

        ss->column(0,0)->setValue(f, qPrintable(filopodiaName));
        ss->column(1,0)->setValue(f, filoId);
        ss->column(2,0)->setValue(f, bulbous);
        ss->column(3,0)->setValue(f, float(bulbous)/age);
        ss->column(4,0)->setValue(f, startTime);
        ss->column(5,0)->setValue(f, endTime);
        ss->column(6,0)->setValue(f, age);
        ss->column(7,0)->setValue(f, meanLength);
        ss->column(8,0)->setValue(f, stdLength);
        ss->column(9,0)->setValue(f, finalLength);
        ss->column(10,0)->setValue(f, numberEvents);
        ss->column(11,0)->setValue(f, meanSpeed);
        ss->column(12,0)->setValue(f, stdSpeed);
        ss->column(13,0)->setValue(f, "");
        ss->column(14,0)->setValue(f, numberEventsFilter);
        ss->column(15,0)->setValue(f, meanSpeedFilter);
        ss->column(16,0)->setValue(f, stdSpeedFilter);
        ss->column(17,0)->setValue(f, numberExt);
        ss->column(18,0)->setValue(f, meanExt);
        ss->column(19,0)->setValue(f, stdExt);
        ss->column(20,0)->setValue(f, numberRetr);
        ss->column(21,0)->setValue(f, meanRetr);
        ss->column(22,0)->setValue(f, stdRetr);
        ss->column(23,0)->setValue(f, stable);
        ss->column(24,0)->setValue(f, stableP);

    }
}

void HxFilopodiaStats::compute()
{
    if (portAction.wasHit()) {
        HxSpatialGraph* inputGraph = hxconnection_cast<HxSpatialGraph> (portData);
        if (!inputGraph) {
            throw McException(QString("No input graph"));
        }

        if (!FilopodiaFunctions::isFilopodiaGraph(inputGraph)) {
            throw McException(QString("Graph has to be filopodia graph"));
        }

        if ((portOutput.getValue(OUTPUT_SPREADSHEET_FILAMENT) || portOutput.getValue(OUTPUT_SPREADSHEET_FILOPODIA) || portOutput.getValue(OUTPUT_SPREADSHEET_LENGTH))) {
            HxSpreadSheet* ssFilament = mcinterface_cast<HxSpreadSheet>(getResult(OUTPUT_SPREADSHEET_FILAMENT));

            if (!ssFilament) {
                ssFilament = HxSpreadSheet::createInstance();
                ssFilament->composeLabel(inputGraph->getLabel(), "FilamentStatistics");
            } else {
                ssFilament->clear();
            }        

            HxSpreadSheet* ssFilopodia = mcinterface_cast<HxSpreadSheet>(getResult(OUTPUT_SPREADSHEET_FILOPODIA));

            if (!ssFilopodia) {
                ssFilopodia = HxSpreadSheet::createInstance();
                ssFilopodia->composeLabel(inputGraph->getLabel(), "FilopodiaStatistics");
            } else {
                ssFilopodia->clear();
            }

            HxSpreadSheet* ssLength = mcinterface_cast<HxSpreadSheet>(getResult(OUTPUT_SPREADSHEET_LENGTH));

            if (!ssLength) {
                ssLength = HxSpreadSheet::createInstance();
                ssLength->composeLabel(inputGraph->getLabel(), "LengthStatistics");
            } else {
                ssLength->clear();
            }

            const float filter = portFilter.getValue(); // Changes in length smaller than filter classified as stable

            createFilamentTab(inputGraph, ssFilament);
            createLengthTab(inputGraph,ssLength,ssFilament);
            createFilopodiaTab(inputGraph, ssFilopodia, ssFilament, filter);

            // Show only spreadsheets of interest
            if (portOutput.getValue(OUTPUT_SPREADSHEET_FILAMENT)) {
                setResult(OUTPUT_SPREADSHEET_FILAMENT, ssFilament);
            }

            if (portOutput.getValue(OUTPUT_SPREADSHEET_FILOPODIA)) {
                setResult(OUTPUT_SPREADSHEET_FILOPODIA, ssFilopodia);
            }

            if (portOutput.getValue(OUTPUT_SPREADSHEET_LENGTH)) {
                setResult(OUTPUT_SPREADSHEET_LENGTH, ssLength);
            }
        } else {
            printf("no spreadsheet");
        }

    }
}
