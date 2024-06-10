/////////////////////////////////////////////////////////////////
//
// McCliqueDetection.h
//
// Main Authors: Baum
// 
/////////////////////////////////////////////////////////////////

#ifndef MC_CLIQUE_DETECTION_H
#define MC_CLIQUE_DETECTION_H

#include <mclib/McBitfield.h>

#include "api.h"

#include <vector>

class POINTMATCHING_API McCliqueDetection {

public:
    static void computeCliquesBasic(
                    std::vector<int>        & clique, 
                    std::vector<int>        & notList,
                    std::vector<int>        & cand, 
                    std::vector<McBitfield> & connected,
                    int                  & nCliques);
    
    static void computeCliquesBasicImproved(
                    std::vector<int>        & clique, 
                    std::vector<int>        & notAndCand, 
                    int                    endNot, 
                    int                    endCand,
                    std::vector<McBitfield> & connected,
                    int                  & nCliques);
    
    static bool computeCliquesBronKerbosch(
                    std::vector<int>            & clique, 
                    std::vector<int>            & notAndCand, 
                    int                        endNot, 
                    int                        endCand,
                    std::vector<McBitfield>     & connected,
                    int                      & nCliques,
                    std::vector<std::vector<int> > & cliques, 
                    int                        minCliqueSize=1,
                    int                        maxNumCliques=-1);
};

#endif
