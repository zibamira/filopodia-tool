#ifndef HXCOMPAREFILOPODIASPATIALGRAPHS_H
#define HXCOMPAREFILOPODIASPATIALGRAPHS_H

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>
#include <hxcore/HxConnection.h>
#include <hxcore/HxPortFloatTextN.h>
#include <hxcore/HxPortRadioBox.h>
#include <hxcore/HxPortFilename.h>
#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <vector>

class HxSpreadSheet;


class HXFILOPODIA_API HxCompareFilopodiaSpatialGraphs : public HxCompModule {
    HX_HEADER(HxCompareFilopodiaSpatialGraphs);

  public:

    void compute();
    void update();

    HxConnection     portTargetGraph;
    HxPortRadioBox   portTwoOrNWayComparison;
    HxPortFilename   portFileNames;
    HxPortFloatTextN portMaxSamplingDistance;
    HxPortFloatTextN portMaxCorrespondenceDistance;
    HxPortDoIt       portAction;

private:
    enum ComparisonNumber {TWO_WAY_COMPARISON=0, N_WAY_COMPARISON=1};

    class MatchStatistics {
        public:
            MatchStatistics(const int totalNumberOfGraphs) :
                mTotalNumberOfGraphs(totalNumberOfGraphs)
            {
                mLengthInCommonWithNOtherGraphs.resize(totalNumberOfGraphs);
                std::fill(mLengthInCommonWithNOtherGraphs.begin(),mLengthInCommonWithNOtherGraphs.end(),0.0);
                mNumberFilamentsInCommonWithNOtherGraphs.resize(totalNumberOfGraphs);
                std::fill(mNumberFilamentsInCommonWithNOtherGraphs.begin(),mNumberFilamentsInCommonWithNOtherGraphs.end(),0);
            }

            double getLengthInCommonWithNOtherGraphs(const int n) const {
                return mLengthInCommonWithNOtherGraphs[n];
            }

            int getNumberFilamentsInCommonWithNOtherGraphs(const int n) const {
                return mNumberFilamentsInCommonWithNOtherGraphs[n];
            }

            double getTotalLength() const {
                double sum = 0.0;
                for (int i=0; i<mLengthInCommonWithNOtherGraphs.size(); ++i) {
                    sum += mLengthInCommonWithNOtherGraphs[i];
                }
                return sum;
            }

            int getTotalNumberFilaments() const {
                int sum = 0;
                for (int i=0; i<mNumberFilamentsInCommonWithNOtherGraphs.size(); ++i) {
                    sum += mNumberFilamentsInCommonWithNOtherGraphs[i];
                }
                return sum;
            }

            void addToLengthInCommonWithNOtherGraphs(const int n, const double length) {
                mLengthInCommonWithNOtherGraphs[n] += length;
            }

            void addToNumberFilamentsInCommonWithNOtherGraphs(const int n, const int number) {
                mNumberFilamentsInCommonWithNOtherGraphs[n] += number;
            }

        private:
            const int mTotalNumberOfGraphs;
            std::vector<double> mLengthInCommonWithNOtherGraphs;
            std::vector<int> mNumberFilamentsInCommonWithNOtherGraphs;
    };


    void twoWayComparison();
    void nWayComparison();

    std::vector<const HxSpatialGraph*> readSpatialGraphs();
    void outputGraphAndViewer(HxSpatialGraph* graph, const int totalNumberOfGraphs);

    void cleanSpatialGraph(HxSpatialGraph* graph) const;
    void removeContours(HxSpatialGraph* graph) const;
    void removeIntermediateNodes(HxSpatialGraph* graph) const;

    std::vector<HxSpatialGraph*> cleanAndResampleGraphs(std::vector<const HxSpatialGraph*>& spatialGraphs) const;

    std::vector<HxSpatialGraph*> matchGraphs(const std::vector<HxSpatialGraph*>& graphs,
                                             const float maxCorrespondenceDistance);

    HxSpreadSheet* createMatchStatisticsSpreadsheet(const std::vector<HxSpatialGraph*>& graphs) const;
    MatchStatistics getGraphStatisticsForGraph(const HxSpatialGraph* graph,
                                               const int totalNumberOfGraphs) const;

    const McString cNumberOfMatchesAttributeName;
};

#endif

